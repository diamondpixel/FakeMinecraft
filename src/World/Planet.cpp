/**
 * @file Planet.cpp
 * @brief Implementation of the Planet class, managing world lifecycle and rendering.
 */

#include "Planet.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <GL/glew.h>
#include "WorldGen.h"
#include "glm/gtc/matrix_transform.hpp"

// Branch prediction hints for performance-critical code paths.
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

Planet *Planet::planet = nullptr;

// Pre-computed neighbor offsets (calculated once, reused everywhere)
static constexpr int NEIGHBOR_OFFSETS[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
};

// Tuning constants for pre-allocating memory to minimize runtime reallocations.
static constexpr size_t EXPECTED_CHUNK_COUNT = 4000;
static constexpr size_t EXPECTED_VISIBLE_CHUNKS = 800;

Planet::Planet(Shader *solidShader, Shader *waterShader, Shader *billboardShader, Shader* bboxShader)
    : solidShader(solidShader), waterShader(waterShader), billboardShader(billboardShader), bboxShader(bboxShader) {

    // Reserve capacity upfront to avoid reallocations
    chunks.reserve(EXPECTED_CHUNK_COUNT);
    chunkList.reserve(EXPECTED_CHUNK_COUNT);
    chunkData.reserve(EXPECTED_CHUNK_COUNT);
    chunkPool.reserve(200);

    renderChunks.reserve(EXPECTED_CHUNK_COUNT);
    solidChunks.reserve(EXPECTED_VISIBLE_CHUNKS);
    billboardChunks.reserve(EXPECTED_VISIBLE_CHUNKS / 2);
    waterChunks.reserve(EXPECTED_VISIBLE_CHUNKS / 4);
    frustumVisibleChunks.reserve(EXPECTED_VISIBLE_CHUNKS);
    toDeleteList.reserve(100);

    chunkThread = std::thread(&Planet::chunkThreadUpdate, this);
}

Planet::~Planet() {
    shouldEnd.store(true, std::memory_order_release);
    if (chunkThread.joinable()) {
        chunkThread.join();
    }
}

//==============================================================================
// MAIN UPDATE - Called every frame
//==============================================================================

void Planet::update(const glm::vec3 &cameraPos, bool updateOcclusion) {
    // Use atomic load with relaxed ordering for better performance.
    const long currentSeed = SEED;
    const long cachedSeed = last_seed.load(std::memory_order_relaxed);

    // Early seed change check (no lock needed for read-only check).
    if (UNLIKELY(currentSeed != cachedSeed)) {
        std::unique_lock lock(chunkMutex);

        if (currentSeed != last_seed.load(std::memory_order_relaxed)) {
            last_seed.store(currentSeed, std::memory_order_relaxed);

            for (auto &[pos, chunk]: chunks) {
                delete chunk;
            }
            chunks.clear();
            
            for (Chunk* chunk : chunkPool) {
                delete chunk;
            }
            chunkPool.clear();

            chunkData.clear();
            chunkList.clear();

            renderChunks.clear();
            solidChunks.clear();
            billboardChunks.clear();
            waterChunks.clear();
            frustumVisibleChunks.clear();
            toDeleteList.clear();
            renderChunksDirty.store(true, std::memory_order_release);

            chunkQueue = {};
            chunkDataQueue = {};
            regenQueue = {};
        }
        return;
    }

    const int newCamChunkX = static_cast<int>(std::floor(cameraPos.x / CHUNK_WIDTH));
    const int newCamChunkY = static_cast<int>(std::floor(cameraPos.y / CHUNK_HEIGHT));
    const int newCamChunkZ = static_cast<int>(std::floor(cameraPos.z / CHUNK_WIDTH));

    // Use atomics for lock-free updates.
    camChunkX.store(newCamChunkX, std::memory_order_relaxed);
    camChunkY.store(newCamChunkY, std::memory_order_relaxed);
    camChunkZ.store(newCamChunkZ, std::memory_order_relaxed);

    chunksLoading.store(0, std::memory_order_relaxed);
    numChunks.store(0, std::memory_order_relaxed);
    numChunksRendered.store(0, std::memory_order_relaxed);

    // Clear without deallocation (capacity preserved).
    solidChunks.clear();
    billboardChunks.clear();
    waterChunks.clear();
    frustumVisibleChunks.clear();
    toDeleteList.clear();

    glDisable(GL_BLEND);

    // Use atomic load for lock-free check.
    if (UNLIKELY(renderChunksDirty.load(std::memory_order_acquire))) {
        std::unique_lock lock(chunkMutex);
        if (renderChunksDirty.load(std::memory_order_relaxed)) {
            renderChunks = chunkList;
            renderChunksDirty.store(false, std::memory_order_release);
        }
    }

    int chunksUploadedThisFrame = 0;

    // Cache commonly used values to avoid recomputation in loops.
    const float maxDist = static_cast<float>(renderDistance) * CHUNK_WIDTH;
    const float maxDistSq = maxDist * maxDist;
    cameraCache.maxDistSq = maxDistSq;

    constexpr float billboardMaxDist = 10.0f * CHUNK_WIDTH;
    constexpr float billboardMaxDistSq = billboardMaxDist * billboardMaxDist;

    const float camPosX = cameraPos.x;
    const float camPosY = cameraPos.y;
    const float camPosZ = cameraPos.z;

    // Loop through all active chunks. NO LOCK HELD during iteration.
    // Use size_t for better optimization on 64-bit systems.
    const size_t renderChunksSize = renderChunks.size();
    for (size_t i = 0; i < renderChunksSize; ++i) {
        Chunk * const chunk = renderChunks[i];
        const ChunkPos &pos = chunk->chunkPos;
        numChunks.fetch_add(1, std::memory_order_relaxed);

        if (UNLIKELY(isOutOfRenderDistance(pos))) {
            toDeleteList.push_back(pos);
            continue;
        }

        const bool needsUpload = chunk->generated && !chunk->ready;
        if (UNLIKELY(needsUpload)) {
            // Aggressive upload: Ensure mesh is uploaded even if not visible.
            // Limit to 4 uploads per frame to prevent frame-time spikes.
            if (chunksUploadedThisFrame < 4) {
                chunk->uploadMesh();
                ++chunksUploadedThisFrame;
            }
            chunksLoading.fetch_add(1, std::memory_order_relaxed);
        }

        if (LIKELY(chunk->ready && frustum.isBoxVisible(chunk->cullingCenter, chunk->cullingExtents))) {
            const float dx = chunk->cullingCenter.x - camPosX;
            const float dy = chunk->cullingCenter.y - camPosY;
            const float dz = chunk->cullingCenter.z - camPosZ;

            const float distSq = dx*dx + dy*dy + dz*dz;

            if (UNLIKELY(distSq > maxDistSq)) continue;

            chunk->cachedDistSq = distSq;
            frustumVisibleChunks.push_back(chunk);

            // Chunks in the 3x3x3 Safe Zone around camera are ALWAYS rendered to prevent flickering.
            const int distCX = std::abs(pos.x - newCamChunkX);
            const int distCY = std::abs(pos.y - newCamChunkY);
            const int distCZ = std::abs(pos.z - newCamChunkZ);
            const bool inSafeZone = (distCX <= 1) & (distCY <= 1) & (distCZ <= 1);

            if (LIKELY(chunk->occlusionVisible || inSafeZone)) {
                numChunksRendered.fetch_add(1, std::memory_order_relaxed);

                if (chunk->hasSolid()) solidChunks.push_back(chunk);

                // Billboard LOD: Only render decorations within a specific range.
                if (chunk->hasBillboard() && distSq <= billboardMaxDistSq) {
                    billboardChunks.push_back(chunk);
                }

                if (chunk->hasWater()) waterChunks.push_back(chunk);
            }
        }
    }

    // Sort render lists for optimal depth testing and blending.
    const bool cameraMoved = (newCamChunkX != prevSortCamX) || (newCamChunkZ != prevSortCamZ);
    const bool shouldSort = cameraMoved || !toDeleteList.empty();

    if (LIKELY(shouldSort)) {
        prevSortCamX = newCamChunkX;
        prevSortCamZ = newCamChunkZ;

        auto frontToBackSort = [](const Chunk * const a, const Chunk * const b) noexcept {
            return a->cachedDistSq < b->cachedDistSq;
        };

        auto backToFrontSort = [](const Chunk * const a, const Chunk * const b) noexcept {
            return a->cachedDistSq > b->cachedDistSq;
        };

        std::sort(frustumVisibleChunks.begin(), frustumVisibleChunks.end(), frontToBackSort);
        std::sort(solidChunks.begin(), solidChunks.end(), frontToBackSort);
        std::sort(waterChunks.begin(), waterChunks.end(), backToFrontSort);
        std::sort(billboardChunks.begin(), billboardChunks.end(), frontToBackSort);
    }

    if (UNLIKELY(!toDeleteList.empty())) {
        std::unique_lock lock(chunkMutex);

        for (const ChunkPos &pos: toDeleteList) {
            auto it = chunks.find(pos);
            if (it != chunks.end()) {
                Chunk* const chunk = it->second;

                // O(1) remove from chunkList using swap-and-pop.
                const size_t index = chunk->listIndex;
                if (LIKELY(index < chunkList.size())) {
                    Chunk* const lastChunk = chunkList.back();
                    chunkList[index] = lastChunk;
                    lastChunk->listIndex = index;
                    chunkList.pop_back();
                }

                chunkPool.push_back(chunk);
                chunks.erase(it);
                renderChunksDirty.store(true, std::memory_order_release);
            }
        }
    }

    // Hardware Occlusion Query result processing.
    static int frameCounter = 0;
    if (updateOcclusion) {
        ++frameCounter;

        const size_t visibleCount = frustumVisibleChunks.size();
        for (size_t i = 0; i < visibleCount; ++i) {
            Chunk * const chunk = frustumVisibleChunks[i];

            const size_t chunkHash = chunk->chunkPos.x * 73856093 ^
                                    chunk->chunkPos.y * 19349663 ^
                                    chunk->chunkPos.z * 83492791;
            const bool shouldQuery = (chunkHash + frameCounter) % 3 == 0;

            if (chunk->queryID != 0 && chunk->queryIssued && shouldQuery) {
                GLint available = GL_FALSE;
                glGetQueryObjectiv(chunk->queryID, GL_QUERY_RESULT_AVAILABLE, &available);

                if (LIKELY(available == GL_TRUE)) {
                    GLuint result = 0;
                    glGetQueryObjectuiv(chunk->queryID, GL_QUERY_RESULT, &result);
                    const bool rawVisible = (result > 0);

                    // Hysteresis (Discrete Voting): Prevent rapid flickering of visibility state.
                    if (LIKELY(rawVisible)) {
                        chunk->occlusionVisible = true;
                        chunk->occlusionCounter = 0;
                    } else {
                        if (++chunk->occlusionCounter >= 4) {
                            chunk->occlusionVisible = false;
                        }
                    }
                }
            }
        }
    }

    // Solid rendering pass.
    solidShader->use();
    const size_t solidCount = solidChunks.size();
    for (size_t i = 0; i < solidCount; ++i) {
        solidChunks[i]->renderAllSolid();
    }

    // Hardware Occlusion Query issuance pass.
    if (updateOcclusion) {
        bboxShader->use();
        (*bboxShader)["viewProjection"] = lastViewProjection;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        if (UNLIKELY(bboxVAO == 0)) initBoundingBoxMesh();
        glBindVertexArray(bboxVAO);

        const int safeZoneMinX = newCamChunkX - 1;
        const int safeZoneMaxX = newCamChunkX + 1;
        const int safeZoneMinY = newCamChunkY - 1;
        const int safeZoneMaxY = newCamChunkY + 1;
        const int safeZoneMinZ = newCamChunkZ - 1;
        const int safeZoneMaxZ = newCamChunkZ + 1;

        const size_t visibleCount = frustumVisibleChunks.size();
        for (size_t i = 0; i < visibleCount; ++i) {
            Chunk * const chunk = frustumVisibleChunks[i];

            if (chunk->queryID != 0) {
                const int posX = chunk->chunkPos.x;
                const int posY = chunk->chunkPos.y;
                const int posZ = chunk->chunkPos.z;

                const bool inSafeZone = (posX >= safeZoneMinX && posX <= safeZoneMaxX) &&
                                       (posY >= safeZoneMinY && posY <= safeZoneMaxY) &&
                                       (posZ >= safeZoneMinZ && posZ <= safeZoneMaxZ);

                if (inSafeZone) continue;

                const size_t chunkHash = posX * 73856093 ^ posY * 19349663 ^ posZ * 83492791;

                if ((chunkHash + frameCounter) % 3 == 0) {
                    glBeginQuery(GL_SAMPLES_PASSED, chunk->queryID);

                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, chunk->cullingCenter);
                    model = glm::scale(model, (chunk->cullingExtents - 0.01f) * 2.0f);
                    (*bboxShader)["model"] = model;
                    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

                    glEndQuery(GL_SAMPLES_PASSED);
                    chunk->queryIssued = true;
                }
            }
        }

        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_GEQUAL);
    }

    // Billboard rendering pass.
    billboardShader->use();
    glDisable(GL_CULL_FACE);
    const size_t billboardCount = billboardChunks.size();
    for (size_t i = 0; i < billboardCount; ++i) {
        billboardChunks[i]->renderAllBillboard();
    }
    glEnable(GL_CULL_FACE);

    // Water rendering pass.
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    waterShader->use();
    const size_t waterCount = waterChunks.size();
    for (size_t i = 0; i < waterCount; ++i) {
        waterChunks[i]->renderAllWater();
    }
    glEnable(GL_CULL_FACE);
}

//==============================================================================
// HELPER METHODS - Inlined for performance
//==============================================================================

inline bool Planet::isOutOfRenderDistance(const ChunkPos &pos) const noexcept {
    const int camX = camChunkX.load(std::memory_order_relaxed);
    const int camZ = camChunkZ.load(std::memory_order_relaxed);

    // OPTIMIZATION: Use bitwise operations for abs() when possible
    const int diffX = pos.x - camX;
    const int diffZ = pos.z - camZ;
    const int absX = (diffX < 0) ? -diffX : diffX;
    const int absZ = (diffZ < 0) ? -diffZ : diffZ;

    return (absX > renderDistance) | (absZ > renderDistance);
}

std::pair<glm::vec3, glm::vec3> Planet::getChunkBounds(const ChunkPos &pos) noexcept {
    const float worldX = static_cast<float>(pos.x) * CHUNK_WIDTH;
    const float worldY = static_cast<float>(pos.y) * CHUNK_HEIGHT;
    const float worldZ = static_cast<float>(pos.z) * CHUNK_WIDTH;
    return {
        glm::vec3(worldX, worldY, worldZ),
        glm::vec3(worldX + CHUNK_WIDTH, worldY + CHUNK_HEIGHT, worldZ + CHUNK_WIDTH)
    };
}

void Planet::updateFrustum(const glm::mat4 &frustumVP, const glm::mat4 &renderingVP) {
    lastViewProjection = renderingVP;
    frustum.update(frustumVP);
}

//==============================================================================
// CHUNK THREAD - Background processing
//==============================================================================

void Planet::chunkThreadUpdate() {
    static int cleanupCounter = 0;

    while (!shouldEnd.load(std::memory_order_acquire)) {
        // Abort if seed changed
        if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Cleanup unused chunk data (throttled to avoid lock contention)
        if (++cleanupCounter > 5000) {
            cleanupUnusedChunkData();
            cleanupCounter = 0;
        }

        // Use atomic loads for lock-free reads to minimize synchronization overhead.
        const int currentCamX = camChunkX.load(std::memory_order_relaxed);
        const int currentCamY = camChunkY.load(std::memory_order_relaxed);
        const int currentCamZ = camChunkZ.load(std::memory_order_relaxed);
        const int prevCamX = lastCamX.load(std::memory_order_relaxed);
        const int prevCamY = lastCamY.load(std::memory_order_relaxed);
        const int prevCamZ = lastCamZ.load(std::memory_order_relaxed);

        // Check if camera moved to rebuild queue
        if ((currentCamX != prevCamX) | (currentCamY != prevCamY) | (currentCamZ != prevCamZ)) {
            updateChunkQueue();
        } else {
            // Process task queues if camera is stationary.
            const bool allQueuesEmpty = chunkQueue.empty() &&
                                       chunkDataQueue.empty() &&
                                       regenQueue.empty();

            if (UNLIKELY(allQueuesEmpty)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else {
                processChunkDataQueue();
                processRegenQueue();
                processChunkQueue();
            }
        }
    }
}

void Planet::processRegenQueue() {
    if (regenQueue.empty()) return;

    std::unique_lock lock(chunkMutex);

    const ChunkPos chunkPos = regenQueue.front();
    regenQueue.pop();

    auto it = chunks.find(chunkPos);
    if (it != chunks.end()) {
        Chunk* const chunk = it->second;

        lock.unlock();
        chunk->generateChunkMesh();
        lock.lock();

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

    // Use static thread_local to avoid repeated allocations in the cleanup thread.
    static thread_local std::vector<ChunkPos> toRemove;
    toRemove.clear();
    toRemove.reserve(chunkData.size() / 10); // Estimate 10% cleanup

    for (const auto &[pos, data]: chunkData) {
        // Check reference count: 1 ref = held by chunkData map ONLY.
        if (LIKELY(data.use_count() == 1)) {
            toRemove.push_back(pos);
        }
    }

    for (const ChunkPos &pos: toRemove) {
        chunkData.erase(pos);
    }
}

//==============================================================================
// CHUNK QUEUE MANAGEMENT
//==============================================================================

void Planet::updateChunkQueue() {
    const int currentCamX = camChunkX.load(std::memory_order_relaxed);
    const int currentCamY = camChunkY.load(std::memory_order_relaxed);
    const int currentCamZ = camChunkZ.load(std::memory_order_relaxed);

    lastCamX.store(currentCamX, std::memory_order_relaxed);
    lastCamY.store(currentCamY, std::memory_order_relaxed);
    lastCamZ.store(currentCamZ, std::memory_order_relaxed);

    std::unique_lock lock(chunkMutex);
    buildChunkQueue();
}

void Planet::buildChunkQueue() {
    chunkQueue = {};

    const int camX = lastCamX.load(std::memory_order_relaxed);
    const int camZ = lastCamZ.load(std::memory_order_relaxed);

    // Add camera chunk (clamped to Y=0 for this single-layer world).
    addChunkIfMissing(camX, 0, camZ);

    // Build in expanding rings (closest first) to prioritize player vicinity.
    for (int r = 1; r <= renderDistance; ++r) {
        addChunkRing(r);
    }
}

void Planet::addChunkRing(int radius) {
    const int camX = lastCamX.load(std::memory_order_relaxed);
    const int camZ = lastCamZ.load(std::memory_order_relaxed);
    constexpr int actualY = 0;

    // Cardinal directions
    addChunkIfMissing(camX, actualY, camZ + radius);
    addChunkIfMissing(camX, actualY, camZ - radius);
    addChunkIfMissing(camX + radius, actualY, camZ);
    addChunkIfMissing(camX - radius, actualY, camZ);

    // Edges (between corners)
    for (int e = 1; e < radius; ++e) {
        addChunkEdges(radius, e, actualY);
    }

    // Corners
    addChunkCorners(radius, actualY);
}

inline void Planet::addChunkEdges(int radius, int edge, int y) noexcept {
    const int camX = lastCamX.load(std::memory_order_relaxed);
    const int camZ = lastCamZ.load(std::memory_order_relaxed);

    addChunkIfMissing(camX + edge, y, camZ + radius);
    addChunkIfMissing(camX - edge, y, camZ + radius);
    addChunkIfMissing(camX + radius, y, camZ + edge);
    addChunkIfMissing(camX + radius, y, camZ - edge);
    addChunkIfMissing(camX + edge, y, camZ - radius);
    addChunkIfMissing(camX - edge, y, camZ - radius);
    addChunkIfMissing(camX - radius, y, camZ + edge);
    addChunkIfMissing(camX - radius, y, camZ - edge);
}

inline void Planet::addChunkCorners(int radius, int y) noexcept {
    const int camX = lastCamX.load(std::memory_order_relaxed);
    const int camZ = lastCamZ.load(std::memory_order_relaxed);

    addChunkIfMissing(camX + radius, y, camZ + radius);
    addChunkIfMissing(camX + radius, y, camZ - radius);
    addChunkIfMissing(camX - radius, y, camZ + radius);
    addChunkIfMissing(camX - radius, y, camZ - radius);
}

inline void Planet::addChunkIfMissing(int x, int y, int z) {
    const ChunkPos pos(x, y, z);
    if (chunks.find(pos) == chunks.end()) {
        chunkQueue.push(pos);
    }
}

//==============================================================================
// CHUNK DATA PROCESSING
//==============================================================================

void Planet::processChunkDataQueue() {
    std::unique_lock lock(chunkMutex);

    if (chunkQueue.empty()) return;

    // Process multiple chunks in parallel to saturate generator worker threads.
    const size_t maxBatch = std::min(chunkDataQueue.size(),
                                    chunkGenPool.workerCount() * 2);

    // Use static thread_local to avoid repeated allocations of the generation task list.
    static thread_local std::vector<ChunkPos> toGenerate;
    toGenerate.clear();
    toGenerate.reserve(maxBatch);

    for (size_t i = 0; i < maxBatch && !chunkDataQueue.empty(); ++i) {
        const ChunkPos chunkPos = chunkDataQueue.front();
        chunkDataQueue.pop();

        if (chunkData.find(chunkPos) == chunkData.end()) {
            toGenerate.push_back(chunkPos);
        }
    }

    lock.unlock();

    if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) return;

    // Submit all chunks to the thread pool for parallel terrain generation.
    for (const ChunkPos &pos: toGenerate) {
        chunkGenPool.submit([this, pos]() {
            if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) return;

            // Use aligned allocation for better cache performance; raw pointer wrapped in shared_ptr.
            auto *d = new uint8_t[CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH];
            WorldGen::generateChunkData(pos, d, SEED);
            const auto data = std::make_shared<ChunkData>(d);

            std::unique_lock dataLock(chunkMutex);
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

    lock.unlock();

    // Check seed before expensive generation
    if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) return;

    // Get chunk from pool or create if needed. Using an object pool reduces frequent new/delete operations.
    Chunk *chunk;
    {
        std::unique_lock poolLock(chunkMutex);
        if (!chunkPool.empty()) {
            chunk = chunkPool.back();
            chunkPool.pop_back();
            chunk->reset(chunkPos);
        } else {
            chunk = new Chunk(chunkPos);
        }
    }

    if (!populateChunkData(chunk, chunkPos)) {
        std::unique_lock poolLock(chunkMutex);
        chunkPool.push_back(chunk);
        return;
    }

    chunk->generateChunkMesh();

    lock.lock();
    if (chunks.find(chunkPos) == chunks.end()) {
        chunks[chunkPos] = chunk;
        chunk->listIndex = chunkList.size();
        chunkList.push_back(chunk);
        renderChunksDirty.store(true, std::memory_order_release);

        // Batched neighbor notifications: Alert neighbors that a new data source is available.
        constexpr int neighborOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}
        };

        std::shared_ptr<ChunkData> Chunk::* const dataPtrs[6] = {
            &Chunk::westData, &Chunk::eastData, &Chunk::northData,
            &Chunk::southData, &Chunk::downData, &Chunk::upData
        };

        for (int i = 0; i < 6; ++i) {
            const ChunkPos nPos(chunkPos.x + neighborOffsets[i][0],
                               chunkPos.y + neighborOffsets[i][1],
                               chunkPos.z + neighborOffsets[i][2]);
            auto it = chunks.find(nPos);
            if (it != chunks.end()) {
                it->second->*dataPtrs[i] = chunk->chunkData;
                regenQueue.push(nPos);
            }
        }
    } else {
        delete chunk;
    }
    chunkQueue.pop();
}

bool Planet::populateChunkData(Chunk *chunk, const ChunkPos &chunkPos) {
    // Use stack array for better cache locality during neighbor data acquisition.
    struct NeighborInfo {
        int dx, dy, dz;
        std::shared_ptr<ChunkData> *target;
    };

    constexpr NeighborInfo neighbors[] = {
        {0, 0, 0, nullptr},    // Will be assigned below
        {0, 1, 0, nullptr},
        {0, -1, 0, nullptr},
        {0, 0, -1, nullptr},
        {0, 0, 1, nullptr},
        {1, 0, 0, nullptr},
        {-1, 0, 0, nullptr}
    };

    std::shared_ptr<ChunkData>* targets[] = {
        &chunk->chunkData, &chunk->upData, &chunk->downData,
        &chunk->northData, &chunk->southData, &chunk->eastData, &chunk->westData
    };

    for (size_t i = 0; i < 7; ++i) {
        if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) return false;

        const ChunkPos neighborPos(chunkPos.x + neighbors[i].dx,
                                  chunkPos.y + neighbors[i].dy,
                                  chunkPos.z + neighbors[i].dz);
        *targets[i] = getOrCreateChunkData(neighborPos);

        if (!*targets[i]) return false;
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

    if (UNLIKELY(SEED != last_seed.load(std::memory_order_relaxed))) return nullptr;

    // Perform expensive procedural generation outside of the mutex lock.
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
    // Use shared_lock for read-only access to allow multiple concurrent readers.
    std::shared_lock lock(chunkMutex);
    const auto it = chunks.find(chunkPos);
    return (it != chunks.end()) ? it->second : nullptr;
}

void Planet::clearChunkQueue() {
    lastCamX.fetch_add(1, std::memory_order_relaxed);
}

//==============================================================================
// Hardware Occlusion Helper Methods
//==============================================================================

void Planet::initBoundingBoxMesh() {
    // Use constexpr for compile-time data to avoid runtime initialization overhead.
    constexpr float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
    };

    constexpr unsigned int indices[] = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
        0, 1, 5, 5, 4, 0, 2, 3, 7, 7, 6, 2,
        0, 3, 7, 7, 4, 0, 1, 2, 6, 6, 5, 1
    };

    glGenVertexArrays(1, &bboxVAO);
    glGenBuffers(1, &bboxVBO);
    glGenBuffers(1, &bboxEBO);

    glBindVertexArray(bboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, bboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Planet::drawBoundingBox(const glm::vec3& center, const glm::vec3& extents) {
    if (UNLIKELY(bboxVAO == 0)) initBoundingBoxMesh();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, extents * 2.0f);

    (*bboxShader)["model"] = model;

    glBindVertexArray(bboxVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}