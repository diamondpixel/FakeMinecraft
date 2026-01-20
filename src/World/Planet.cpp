#include "Planet.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <GL/glew.h>
#include "WorldGen.h"
#include "glm/gtc/matrix_transform.hpp"

Planet *Planet::planet = nullptr;

// Pre-computed neighbor offsets (calculated once, reused everywhere)
static constexpr int NEIGHBOR_OFFSETS[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
};

Planet::Planet(Shader *solidShader, Shader *waterShader, Shader *billboardShader, Shader* bboxShader)
    : solidShader(solidShader), waterShader(waterShader), billboardShader(billboardShader), bboxShader(bboxShader) {
    chunkThread = std::thread(&Planet::chunkThreadUpdate, this);
}

Planet::~Planet() {
    shouldEnd = true;
    chunkThread.join();
}

//==============================================================================
// MAIN UPDATE - Called every frame
//==============================================================================

void Planet::update(const glm::vec3 &cameraPos, bool updateOcclusion) {
    // Early seed change check (no lock needed for read-only check)
    if (SEED != last_seed) {
        std::lock_guard lock(chunkMutex);
        if (SEED != last_seed) {
            last_seed = SEED;

            // Batch delete all chunks
            for (auto &[pos, chunk]: chunks) delete chunk;
            chunks.clear();
            
            // Clear pool - chunks from old seed are invalid
            for (Chunk* chunk : chunkPool) delete chunk;
            chunkPool.clear();

            // Clear chunk data - shared_ptr handles deletion
            chunkData.clear();
            chunkList.clear();

            // Clear all queues efficiently
            chunkQueue = {};
            chunkDataQueue = {};
            regenQueue = {};
        }
        return;
    }

    // Update camera chunk position (computed once per frame)
    camChunkX = static_cast<int>(floor(cameraPos.x / CHUNK_WIDTH));
    camChunkY = static_cast<int>(floor(cameraPos.y / CHUNK_HEIGHT));
    camChunkZ = static_cast<int>(floor(cameraPos.z / CHUNK_WIDTH));

    // Reset per-frame counters
    chunksLoading = 0;
    numChunks = 0;
    numChunksRendered = 0;

    // OPTIMIZATION: Use pre-allocated member vectors instead of thread_local
    solidChunks.clear();
    billboardChunks.clear();
    waterChunks.clear();
    frustumVisibleChunks.clear();
    toDeleteList.clear();

    glDisable(GL_BLEND);

    if (renderChunksDirty) {
        std::lock_guard lock(chunkMutex);
        if (renderChunksDirty) {
            renderChunks = chunkList;
            renderChunksDirty = false;
        }
    }

    int chunksUploadedThisFrame = 0;
    const float maxDist = static_cast<float>(renderDistance) * CHUNK_WIDTH;
    const float maxDistSq = maxDist * maxDist;

    // Loop through all active chunks - NO LOCK HELD during iteration
    for (Chunk *chunk: renderChunks) {
        const ChunkPos &pos = chunk->chunkPos;
        ++numChunks;

        if (isOutOfRenderDistance(pos)) {
            toDeleteList.push_back(pos);
            continue;
        }

        if (chunk->generated && !chunk->ready) {
            // Aggressive upload: Ensure mesh is uploaded and CLEARED from RAM even if not visible
            // Limit to 4 uploads per frame to prevent stutter
            if (chunksUploadedThisFrame < 4) {
                chunk->uploadMesh();
                chunksUploadedThisFrame++;
            }
            ++chunksLoading;
        }
        // Chunk-level frustum culling (single check, not 16 sub-chunk checks)
        if (chunk->ready && frustum.isBoxVisible(chunk->cullingCenter, chunk->cullingExtents)) {
            // OPTIMIZATION: Calculate distance ONCE and cache it
            const float dx = chunk->cullingCenter.x - cameraPos.x;
            const float dy = chunk->cullingCenter.y - cameraPos.y;
            const float dz = chunk->cullingCenter.z - cameraPos.z;
            const float distSq = dx*dx + dy*dy + dz*dz;
            
            if (distSq > maxDistSq) continue; // Skip chunks beyond render distance
            
            // Cache for sorting (avoid recalculating in sort comparators)
            chunk->cachedDistSq = distSq;
            
            // ALL frustum-visible chunks go here (needed for HOQ queries)
            frustumVisibleChunks.push_back(chunk);
            
            // HOQ ENABLED: Only render if occlusionVisible (Freezes with last state if queries stop)
            if (chunk->occlusionVisible) {
                ++numChunksRendered;
                
                if (chunk->hasSolid()) solidChunks.push_back(chunk);
                
                // Billboard LOD: Only render grass/flowers within 10 chunks
                constexpr float billboardMaxDist = 10.0f * CHUNK_WIDTH;
                constexpr float billboardMaxDistSq = billboardMaxDist * billboardMaxDist;
                if (chunk->hasBillboard() && distSq <= billboardMaxDistSq) {
                    billboardChunks.push_back(chunk);
                }
                
                if (chunk->hasWater()) waterChunks.push_back(chunk);
            }
        }
    }

    // OPTIMIZATION: Only sort when camera chunk position changes
    const bool cameraMoved = (camChunkX != prevSortCamX || camChunkZ != prevSortCamZ);
    if (cameraMoved) {
        prevSortCamX = camChunkX;
        prevSortCamZ = camChunkZ;
        
        // 0. Frustum-visible: Front-to-Back (for HOQ - closest first populates depth best)
        std::sort(frustumVisibleChunks.begin(), frustumVisibleChunks.end(), [](const Chunk *a, const Chunk *b) {
            return a->cachedDistSq < b->cachedDistSq;
        });
        
        // 1. Solids: Front-to-Back (closest first) -> Early-Z culling works best
        std::sort(solidChunks.begin(), solidChunks.end(), [](const Chunk *a, const Chunk *b) {
            return a->cachedDistSq < b->cachedDistSq;
        });

        // 2. Transparents (Water): Back-to-Front (farthest first) -> Required for correct Blending
        std::sort(waterChunks.begin(), waterChunks.end(), [](const Chunk *a, const Chunk *b) {
            return a->cachedDistSq > b->cachedDistSq;
        });

        // 3. Billboards: Front-to-Back (alpha tested, so Early-Z is better)
        std::sort(billboardChunks.begin(), billboardChunks.end(), [](const Chunk *a, const Chunk *b) {
            return a->cachedDistSq < b->cachedDistSq;
        });
    }

    // Batch deletion - take lock only for deletion
    if (!toDeleteList.empty()) {
        std::lock_guard lock(chunkMutex);
        for (const auto &pos: toDeleteList) {
            auto it = chunks.find(pos);
            if (it != chunks.end()) {
                const Chunk* chunk = it->second;
                
                // O(1) remove from chunkList using swap-and-pop
                const size_t index = chunk->listIndex;
                if (index < chunkList.size()) {
                    Chunk* lastChunk = chunkList.back();
                    chunkList[index] = lastChunk;
                    lastChunk->listIndex = index;
                    chunkList.pop_back();
                }

                // Return to pool instead of delete
                chunkPool.push_back(const_cast<Chunk*>(chunk));
                chunks.erase(it);
                numChunks--;
                renderChunksDirty = true;
            }
        }
    }
    // ==================== HARDWARE OCCLUSION QUERIES WITH OPTIMIZATION ====================
    // 1. Temporal Interlacing: Only query 1/3 of chunks per frame (reduces CPU overhead by 66%)
    // 2. Instant Show: If cached result says visible, show immediately.
    // 3. Latched Hide: Occlusion results persist for 3 frames (implicit hysteresis).
    
    static int frameCounter = 0;
    if (updateOcclusion) {
        frameCounter++;
    
        for (Chunk *chunk : frustumVisibleChunks) {
            // Hash position to distribute work evenly across frames
            size_t chunkHash = chunk->chunkPos.x * 73856093 ^ chunk->chunkPos.y * 19349663 ^ chunk->chunkPos.z * 83492791;
            bool shouldQuery = (chunkHash + frameCounter) % 3 == 0;

            // If we queried this chunk previously and result is ready
            if (chunk->queryID != 0 && chunk->queryIssued) {
                // Check result ONLY if we queried it (or if checking avail is cheap)
                // But to save CPU, we only check result if we plan to re-issue or if it's "our turn"
                // Actually, we must check result of the PREVIOUS query before issuing NEW one.
                // If we skip issuing, we hold the old state.
                
                if (shouldQuery) {
                    GLuint result = 0;
                    GLint available = GL_FALSE;
                    // Non-blocking check
                    glGetQueryObjectiv(chunk->queryID, GL_QUERY_RESULT_AVAILABLE, &available);
                    
                    if (available == GL_TRUE) {
                        glGetQueryObjectuiv(chunk->queryID, GL_QUERY_RESULT, &result);
                        bool rawVisible = (result > 0);
                        
                        // Logic:
                        // If Visible: Show immediately.
                        // If Occluded: Hide immediately (interlacing holds it for 3 frames).
                        chunk->occlusionVisible = rawVisible;
                        
                        // Reset smoothing counters (unused but kept for cleanliness)
                        chunk->occlusionCounter = rawVisible ? 0 : 10;
                        chunk->occlusionScore = rawVisible ? 1.0f : 0.0f;
                    }
                }
            }
        }
    }

    // Render Solid Chunks
    solidShader->use();
    for (Chunk *chunk : solidChunks) {
        chunk->renderAllSolid();
    }
    
    // ==================== ISSUE NEW QUERIES (Interlaced) ====================
    if (updateOcclusion) {
        bboxShader->use();
        (*bboxShader)["viewProjection"] = lastViewProjection;
        
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE); 
        
        // Bind VAO once
        if (bboxVAO == 0) initBoundingBoxMesh();
        glBindVertexArray(bboxVAO);

        for (Chunk *chunk : frustumVisibleChunks) {
            if (chunk->queryID != 0) {
                size_t chunkHash = chunk->chunkPos.x * 73856093 ^ chunk->chunkPos.y * 19349663 ^ chunk->chunkPos.z * 83492791;

                if ((chunkHash + frameCounter) % 3 == 0) {
                    glBeginQuery(GL_SAMPLES_PASSED, chunk->queryID);
                    drawBoundingBox(chunk->cullingCenter, chunk->cullingExtents);
                    glEndQuery(GL_SAMPLES_PASSED);
                    chunk->queryIssued = true;
                }
            }
        }
        
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE); 
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    // Billboard Pass (always visible, no occlusion for foliage)
    billboardShader->use();
    glDisable(GL_CULL_FACE);
    for (Chunk *chunk : billboardChunks) {
        chunk->renderAllBillboard();
    }
    glEnable(GL_CULL_FACE);

    // Water Pass
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    waterShader->use();
    for (Chunk *chunk : waterChunks) {
        chunk->renderAllWater();
    }
    glEnable(GL_CULL_FACE);
}

//==============================================================================
// HELPER METHODS - Inlined for performance
//==============================================================================

inline bool Planet::isOutOfRenderDistance(const ChunkPos &pos) const {
    return abs(pos.x - camChunkX) > renderDistance ||
           abs(pos.z - camChunkZ) > renderDistance;
}

inline std::pair<glm::vec3, glm::vec3> Planet::getChunkBounds(const ChunkPos &pos) {
    const float worldX = static_cast<float>(pos.x) * CHUNK_WIDTH;
    const float worldY = static_cast<float>(pos.y) * CHUNK_HEIGHT;
    const float worldZ = static_cast<float>(pos.z) * CHUNK_WIDTH;
    return {
        glm::vec3(worldX, worldY, worldZ),
        glm::vec3(worldX + CHUNK_WIDTH, worldY + CHUNK_HEIGHT, worldZ + CHUNK_WIDTH)
    };
}






void Planet::updateFrustum(const glm::mat4 &frustumVP, const glm::mat4 &renderingVP) {
    // Store RENDERING perspective for occlusion queries (matches depth buffer)
    lastViewProjection = renderingVP;
    
    // But use FRUSTUM perspective for culling (frozen during freecam)
    frustum.update(frustumVP);
}

//==============================================================================
// CHUNK THREAD - Background processing
//==============================================================================

void Planet::chunkThreadUpdate() {
    while (!shouldEnd) {
        // Abort if seed changed
        if (SEED != last_seed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Cleanup unused chunk data (throttled to avoid lock contention)
        static int cleanupCounter = 0;
        if (++cleanupCounter > 5000) {
            cleanupUnusedChunkData();
            cleanupCounter = 0;
        }

        // Check if camera moved to rebuild queue
        if (camChunkX != lastCamX || camChunkY != lastCamY || camChunkZ != lastCamZ) {
            updateChunkQueue();
        } else {
            // Process data queue first, then chunk queue
            if (chunkQueue.empty() && chunkDataQueue.empty() && regenQueue.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            processChunkDataQueue();
            processRegenQueue();
            processChunkQueue();
        }
    }
}

void Planet::processRegenQueue() {
    // Queue is thread-local to background thread, safe to check without lock
    if (regenQueue.empty()) return;

    std::unique_lock lock(chunkMutex);

    // Process one regen task per cycle to allow interleaving
    ChunkPos chunkPos = regenQueue.front();
    regenQueue.pop();
    
    auto it = chunks.find(chunkPos);
    if (it != chunks.end()) {
        Chunk* chunk = it->second;
        
        lock.unlock(); // GENERATE OUTSIDE LOCK
        chunk->generateChunkMesh();
        lock.lock();
        
        // Re-check existence just in case (though highly unlikely to be deleted if in queue)
        if (chunks.find(chunkPos) != chunks.end()) {
             chunk->ready = false;
        }
    }
}

//==============================================================================
// CHUNK DATA CLEANUP
//==============================================================================

void Planet::cleanupUnusedChunkData() {
    std::unique_lock lock(chunkMutex);

    // Use a separate vector to avoid iterator invalidation
    static thread_local std::vector<ChunkPos> toRemove;
    toRemove.clear();

    for (const auto &[pos, data]: chunkData) {
        // OPTIMIZATION: Check reference count instead of strictly checking usage
        // 1 ref = held by chunkData map ONLY (not used by any chunk or neighbor)
        if (data.use_count() == 1) {
            toRemove.push_back(pos);
        }
    }

    for (const auto &pos: toRemove) {
        chunkData.erase(pos);
    }
}

//==============================================================================
// CHUNK QUEUE MANAGEMENT
//==============================================================================

void Planet::updateChunkQueue() {
    lastCamX = camChunkX;
    lastCamY = camChunkY;
    lastCamZ = camChunkZ;

    std::lock_guard lock(chunkMutex);
    buildChunkQueue();
}

void Planet::buildChunkQueue() {
    chunkQueue = {};

    // Add camera chunk (clamped to Y=0)
    addChunkIfMissing(camChunkX, 0, camChunkZ);

    // Build in expanding rings (closest first)
    for (int r = 1; r <= renderDistance; ++r) {
        addChunkRing(r);
    }
}

void Planet::addChunkRing(int radius) {
    // Process all Y levels in render height
    // Single layer world (Tall Chunks) - Only load Y=0
    const int actualY = 0;
    {

        // Cardinal directions (N/S/E/W at this radius)
        addChunkIfMissing(camChunkX, actualY, camChunkZ + radius); // North
        addChunkIfMissing(camChunkX, actualY, camChunkZ - radius); // South
        addChunkIfMissing(camChunkX + radius, actualY, camChunkZ); // East
        addChunkIfMissing(camChunkX - radius, actualY, camChunkZ); // West

        // Edges (between corners)
        for (int e = 1; e < radius; ++e) {
            addChunkEdges(radius, e, actualY);
        }

        // Corners
        addChunkCorners(radius, actualY);
    }
}

inline void Planet::addChunkEdges(int radius, int edge, int y) {
    addChunkIfMissing(camChunkX + edge, y, camChunkZ + radius); // North edge
    addChunkIfMissing(camChunkX - edge, y, camChunkZ + radius);
    addChunkIfMissing(camChunkX + radius, y, camChunkZ + edge); // East edge
    addChunkIfMissing(camChunkX + radius, y, camChunkZ - edge);
    addChunkIfMissing(camChunkX + edge, y, camChunkZ - radius); // South edge
    addChunkIfMissing(camChunkX - edge, y, camChunkZ - radius);
    addChunkIfMissing(camChunkX - radius, y, camChunkZ + edge); // West edge
    addChunkIfMissing(camChunkX - radius, y, camChunkZ - edge);
}

inline void Planet::addChunkCorners(int radius, int y) {
    addChunkIfMissing(camChunkX + radius, y, camChunkZ + radius); // NE
    addChunkIfMissing(camChunkX + radius, y, camChunkZ - radius); // SE
    addChunkIfMissing(camChunkX - radius, y, camChunkZ + radius); // NW
    addChunkIfMissing(camChunkX - radius, y, camChunkZ - radius); // SW
}

inline void Planet::addChunkIfMissing(int x, int y, int z) {
    ChunkPos pos(x, y, z);

    // Skip if already exists
    if (chunks.find(pos) == chunks.end()) {
        chunkQueue.push(pos);
    }
    // GPU will handle frustum culling via shader - all chunks in render distance are prepared
}

//==============================================================================
// CHUNK DATA PROCESSING
//==============================================================================

void Planet::processChunkDataQueue() {
    std::unique_lock lock(chunkMutex);

    if (chunkDataQueue.empty()) return;

    // PARALLEL OPTIMIZATION: Submit multiple chunks to thread pool at once
    // Process up to N chunks in parallel based on pool size
    const size_t maxBatch = std::min(chunkDataQueue.size(), chunkGenPool.workerCount() * 2);
    std::vector<ChunkPos> toGenerate;
    toGenerate.reserve(maxBatch);

    for (size_t i = 0; i < maxBatch && !chunkDataQueue.empty(); ++i) {
        ChunkPos chunkPos = chunkDataQueue.front();
        chunkDataQueue.pop();

        // Skip if already exists
        if (chunkData.find(chunkPos) != chunkData.end()) {
            continue;
        }

        toGenerate.push_back(chunkPos);
    }

    lock.unlock();

    // Check seed before expensive generation
    if (SEED != last_seed) return;

    // Submit all chunks to thread pool for parallel generation
    for (const ChunkPos &pos: toGenerate) {
        chunkGenPool.submit([this, pos]() {
            // Check seed again in worker thread
            if (SEED != last_seed) return;

            auto *d = new uint8_t[CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH];
            WorldGen::generateChunkData(pos, d, SEED);
             const auto data = std::make_shared<ChunkData>(d);

            std::lock_guard dataLock(chunkMutex);
            // Double-check it wasn't already added
            if (chunkData.find(pos) == chunkData.end()) {
                chunkData[pos] = data;
            }
        });
    }
}

void Planet::processChunkQueue() {
    std::unique_lock lock(chunkMutex);

    if (chunkQueue.empty()) {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    const ChunkPos chunkPos = chunkQueue.front();

    // Skip if already exists
    if (chunks.find(chunkPos) != chunks.end()) {
        chunkQueue.pop();
        return;
    }

    // All chunks in render distance will be generated - GPU handles frustum culling
    lock.unlock();

    // Check seed before expensive generation
    if (SEED != last_seed) return;

    // Get chunk from pool or create if needed
    Chunk *chunk;
    {
        std::lock_guard poolLock(chunkMutex);
        if (!chunkPool.empty()) {
            chunk = chunkPool.back();
            chunkPool.pop_back();
            chunk->reset(chunkPos); // Reset to new position
        } else {
            chunk = new Chunk(chunkPos); // Pool empty, allocate new
        }
    }

    if (!populateChunkData(chunk, chunkPos)) {
        // Seed changed, return chunk to pool
        std::lock_guard poolLock(chunkMutex);
        chunkPool.push_back(chunk);
        return;
    }

    chunk->generateChunkMesh();

    lock.lock();
    if (chunks.find(chunkPos) == chunks.end()) {
        chunks[chunkPos] = chunk;
        chunk->listIndex = chunkList.size();
        chunkList.push_back(chunk);
        renderChunksDirty = true;

        // OPTIMIZATION: Batched neighbor notifications (single loop instead of 6 separate lookups)
        // Each neighbor needs a specific data pointer from the new chunk
        struct NeighborUpdate {
            int dx, dy, dz;
            std::shared_ptr<ChunkData> Chunk::*dataPtr;  // Pointer-to-member for the neighbor's data field
        };
        static constexpr int neighborOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}
        };
        // Map: neighbor offset -> which data pointer in neighbor chunk to update
        // East neighbor (dx=+1) needs my westData, etc.
        std::shared_ptr<ChunkData> Chunk::* dataPtrs[6] = {
            &Chunk::westData,   // East neighbor needs my west
            &Chunk::eastData,   // West neighbor needs my east  
            &Chunk::northData,  // South neighbor needs my north
            &Chunk::southData,  // North neighbor needs my south
            &Chunk::downData,   // Up neighbor needs my down
            &Chunk::upData      // Down neighbor needs my up
        };
        
        for (int i = 0; i < 6; ++i) {
            ChunkPos nPos(chunkPos.x + neighborOffsets[i][0], 
                          chunkPos.y + neighborOffsets[i][1], 
                          chunkPos.z + neighborOffsets[i][2]);
            auto it = chunks.find(nPos);
            if (it != chunks.end()) {
                it->second->*dataPtrs[i] = chunk->chunkData;
                regenQueue.push(nPos);
            }
        }
    } else {
        // Race condition or duplicate: Chunk already exists
        delete chunk;
    }
    chunkQueue.pop();
}

bool Planet::populateChunkData(Chunk *chunk, const ChunkPos &chunkPos) {
    // Neighbor structure for cleaner iteration
    struct NeighborInfo {
        int dx, dy, dz;
        std::shared_ptr<ChunkData> *target;
    };
    const NeighborInfo neighbors[] = {
        {0, 0, 0, &chunk->chunkData},
        {0, 1, 0, &chunk->upData},
        {0, -1, 0, &chunk->downData},
        {0, 0, -1, &chunk->northData},
        {0, 0, 1, &chunk->southData},
        {1, 0, 0, &chunk->eastData},
        {-1, 0, 0, &chunk->westData}
    };

    for (const auto &[dx, dy, dz, target]: neighbors) {
        if (SEED != last_seed) return false;

        ChunkPos neighborPos(chunkPos.x + dx, chunkPos.y + dy, chunkPos.z + dz);
        *target = getOrCreateChunkData(neighborPos);

        if (!*target) return false; // Failed to create (seed changed)
    }

    return true;
}

std::shared_ptr<ChunkData> Planet::getOrCreateChunkData(const ChunkPos &pos) {
    std::unique_lock lock(chunkMutex);

    auto it = chunkData.find(pos);
    if (it != chunkData.end()) {
        return it->second;
    }

    lock.unlock();

    // Check seed before generation
    if (SEED != last_seed) return nullptr;

    // Generate outside lock
    auto *d = new uint8_t[CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH];
    WorldGen::generateChunkData(pos, d, SEED);
    auto data = std::make_shared<ChunkData>(d);

    lock.lock();
    chunkData[pos] = data;
    return data;
}

//==============================================================================
// PUBLIC UTILITIES
//==============================================================================

Chunk *Planet::getChunk(const ChunkPos chunkPos) {
    std::lock_guard lock(chunkMutex);
    const auto it = chunks.find(chunkPos);
    return (it != chunks.end()) ? it->second : nullptr;
}

void Planet::clearChunkQueue() {
    lastCamX++;
}

// ==============================================================================
// Hardware Occlusion Helper Methods
// ==============================================================================

void Planet::initBoundingBoxMesh() {
    float vertices[] = {
        // unit cube centered at 0,0,0 with size 1 (-0.5 to 0.5)
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, // Back face
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, // Front face
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0, // Back
        4, 5, 6, 6, 7, 4, // Front
        0, 1, 5, 5, 4, 0, // Bottom
        2, 3, 7, 7, 6, 2, // Top
        0, 3, 7, 7, 4, 0, // Left
        1, 2, 6, 6, 5, 1  // Right
    };

    glGenVertexArrays(1, &bboxVAO);
    glGenBuffers(1, &bboxVBO);
    glGenBuffers(1, &bboxEBO);

    glBindVertexArray(bboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, bboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Planet::drawBoundingBox(const glm::vec3& center, const glm::vec3& extents) {
    if (bboxVAO == 0) initBoundingBoxMesh();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, extents * 2.0f); // extents are half-size, so scale by 2
    
    (*bboxShader)["model"] = model;

    // Must bind VAO here as external callers (GameObject) rely on it
    glBindVertexArray(bboxVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
