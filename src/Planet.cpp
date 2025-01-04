#include "headers/Planet.h"
#include <iostream>
#include <unordered_set>
#include <GL/glew.h>
#include "headers/WorldGen.h"

Planet *Planet::planet = nullptr;

// Public
Planet::Planet(Shader *solidShader, Shader *waterShader, Shader *billboardShader)
    : solidShader(solidShader), waterShader(waterShader), billboardShader(billboardShader) {
    chunkThread = std::thread(&Planet::chunkThreadUpdate, this);
}

Planet::~Planet() {
    shouldEnd = true;
    chunkThread.join();
}

void Planet::update(const glm::vec3& cameraPos) {
    // Check if the seed has changed
    if (SEED != last_seed) {
        last_seed = SEED;
        chunks.clear();
        chunkData.clear();
        return;
    }

    // Calculate camera chunk position
    camChunkX = static_cast<int>(floor(cameraPos.x / CHUNK_SIZE));
    camChunkY = static_cast<int>(floor(cameraPos.y / CHUNK_SIZE));
    camChunkZ = static_cast<int>(floor(cameraPos.z / CHUNK_SIZE));

    glDisable(GL_BLEND);

    // Reset counters
    chunksLoading = 0;
    numChunks = 0;
    numChunksRendered = 0;

    // Lock the chunk mutex
    std::lock_guard lock(chunkMutex);

    for (auto it = chunks.begin(); it != chunks.end();) {
        Chunk *chunk = it->second;
        const int chunkX = chunk->chunkPos.x;
        const int chunkY = chunk->chunkPos.y;
        const int chunkZ = chunk->chunkPos.z;

        ++numChunks;

        if (!chunk->ready) {
            ++chunksLoading;
        }

        // Check if the chunk is out of the render distance
        if (chunk->ready &&
            (abs(chunkX - camChunkX) > renderDistance ||
             abs(chunkY - camChunkY) > renderDistance ||
             abs(chunkZ - camChunkZ) > renderDistance)) {
            // Queue neighboring chunk data for deletion
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        if (abs(dx) + abs(dy) + abs(dz) == 1) {
                            chunkDataDeleteQueue.emplace(chunkX + dx, chunkY + dy, chunkZ + dz);
                        }
                    }
                }
            }

            // Delete the chunk and erase it from the map
            delete chunk;
            it = chunks.erase(it);
        } else {
            // Render the chunk
            ++numChunksRendered;
            chunk->render(solidShader, billboardShader);
            ++it;
        }
    }

    glEnable(GL_BLEND);

    // Render water for all chunks
    waterShader->use();
    for (auto &[_, chunk]: chunks) {
        chunk->renderWater(waterShader);
    }
}

void Planet::chunkThreadUpdate() {
    while (!shouldEnd) {
        for (auto it = chunkData.begin(); it != chunkData.end();) {
            ChunkPos pos = it->first;

            if (chunks.find(pos) == chunks.end() &&
                chunks.find({pos.x + 1, pos.y, pos.z}) == chunks.end() &&
                chunks.find({pos.x - 1, pos.y, pos.z}) == chunks.end() &&
                chunks.find({pos.x, pos.y + 1, pos.z}) == chunks.end() &&
                chunks.find({pos.x, pos.y - 1, pos.z}) == chunks.end() &&
                chunks.find({pos.x, pos.y, pos.z + 1}) == chunks.end() &&
                chunks.find({pos.x, pos.y, pos.z - 1}) == chunks.end()) {
                delete chunkData.at(pos);
                it = chunkData.erase(it);
            } else {
                ++it;
            }
        }

        // Check if camera moved to new chunk
        if (camChunkX != lastCamX || camChunkY != lastCamY || camChunkZ != lastCamZ) {
            // Player moved chunks, start new chunk queue
            lastCamX = camChunkX;
            lastCamY = camChunkY;
            lastCamZ = camChunkZ;

            // Current chunk
            chunkMutex.lock();
            chunkQueue = {};
            if (chunks.find({camChunkX, camChunkY, camChunkZ}) == chunks.end())
                chunkQueue.emplace(camChunkX, camChunkY, camChunkZ);

            for (int r = 0; r < renderDistance; r++) {
                // Add middle chunks
                for (int y = 0; y <= renderHeight; y++) {
                    chunkQueue.emplace(camChunkX, camChunkY + y, camChunkZ + r);
                    chunkQueue.emplace(camChunkX + r, camChunkY + y, camChunkZ);
                    chunkQueue.emplace(camChunkX, camChunkY + y, camChunkZ - r);
                    chunkQueue.emplace(camChunkX - r, camChunkY + y, camChunkZ);

                    if (y > 0) {
                        chunkQueue.emplace(camChunkX, camChunkY - y, camChunkZ + r);
                        chunkQueue.emplace(camChunkX + r, camChunkY - y, camChunkZ);
                        chunkQueue.emplace(camChunkX, camChunkY - y, camChunkZ - r);
                        chunkQueue.emplace(camChunkX - r, camChunkY - y, camChunkZ);
                    }
                }

                // Add edges
                for (int e = 1; e < r; e++) {
                    for (int y = 0; y <= renderHeight; y++) {
                        chunkQueue.emplace(camChunkX + e, camChunkY + y, camChunkZ + r);
                        chunkQueue.emplace(camChunkX - e, camChunkY + y, camChunkZ + r);
                        chunkQueue.emplace(camChunkX + r, camChunkY + y, camChunkZ + e);
                        chunkQueue.emplace(camChunkX + r, camChunkY + y, camChunkZ - e);
                        chunkQueue.emplace(camChunkX + e, camChunkY + y, camChunkZ - r);
                        chunkQueue.emplace(camChunkX - e, camChunkY + y, camChunkZ - r);
                        chunkQueue.emplace(camChunkX - r, camChunkY + y, camChunkZ + e);
                        chunkQueue.emplace(camChunkX - r, camChunkY + y, camChunkZ - e);

                        if (y > 0) {
                            chunkQueue.emplace(camChunkX + e, camChunkY - y, camChunkZ + r);
                            chunkQueue.emplace(camChunkX - e, camChunkY - y, camChunkZ + r);
                            chunkQueue.emplace(camChunkX + r, camChunkY - y, camChunkZ + e);
                            chunkQueue.emplace(camChunkX + r, camChunkY - y, camChunkZ - e);
                            chunkQueue.emplace(camChunkX + e, camChunkY - y, camChunkZ - r);
                            chunkQueue.emplace(camChunkX - e, camChunkY - y, camChunkZ - r);
                            chunkQueue.emplace(camChunkX - r, camChunkY - y, camChunkZ + e);
                            chunkQueue.emplace(camChunkX - r, camChunkY - y, camChunkZ - e);
                        }
                    }
                }

                // Add corners
                for (int y = 0; y <= renderHeight; y++) {
                    chunkQueue.emplace(camChunkX + r, camChunkY + y, camChunkZ + r);
                    chunkQueue.emplace(camChunkX + r, camChunkY + y, camChunkZ - r);
                    chunkQueue.emplace(camChunkX - r, camChunkY + y, camChunkZ + r);
                    chunkQueue.emplace(camChunkX - r, camChunkY + y, camChunkZ - r);

                    if (y > 0) {
                        chunkQueue.emplace(camChunkX + r, camChunkY - y, camChunkZ + r);
                        chunkQueue.emplace(camChunkX + r, camChunkY - y, camChunkZ - r);
                        chunkQueue.emplace(camChunkX - r, camChunkY - y, camChunkZ + r);
                        chunkQueue.emplace(camChunkX - r, camChunkY - y, camChunkZ - r);
                    }
                }
            }

            chunkMutex.unlock();
        } else {
            chunkMutex.lock();
            if (!chunkDataQueue.empty()) {
                ChunkPos chunkPos = chunkDataQueue.front();

                if (chunkData.find(chunkPos) != chunkData.end()) {
                    chunkDataQueue.pop();
                    chunkMutex.unlock();
                    continue;
                }

                chunkMutex.unlock();

                auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
                auto *data = new ChunkData(d);

                WorldGen::generateChunkData(chunkPos, d, SEED);

                chunkMutex.lock();
                chunkData[chunkPos] = data;
                chunkDataQueue.pop();
                chunkMutex.unlock();
            } else {
                if (!chunkQueue.empty()) {
                    // Check if chunk exists
                    ChunkPos chunkPos = chunkQueue.front();
                    if (chunks.find(chunkPos) != chunks.end()) {
                        chunkQueue.pop();
                        chunkMutex.unlock();
                        continue;
                    }

                    chunkMutex.unlock();

                    // Create chunk object
                    auto *chunk = new Chunk(chunkPos, solidShader, waterShader);

                    // Set chunk data
                    {
                        chunkMutex.lock();
                        if (chunkData.find(chunkPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(chunkPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->chunkData = data;

                            chunkMutex.lock();
                            chunkData[chunkPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->chunkData = chunkData.at(chunkPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set top data
                    {
                        ChunkPos topPos(chunkPos.x, chunkPos.y + 1, chunkPos.z);
                        chunkMutex.lock();
                        if (chunkData.find(topPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(topPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->upData = data;

                            chunkMutex.lock();
                            chunkData[topPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->upData = chunkData.at(topPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set bottom data
                    {
                        ChunkPos bottomPos(chunkPos.x, chunkPos.y - 1, chunkPos.z);
                        chunkMutex.lock();
                        if (chunkData.find(bottomPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(bottomPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->downData = data;

                            chunkMutex.lock();
                            chunkData[bottomPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->downData = chunkData.at(bottomPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set north data
                    {
                        ChunkPos northPos(chunkPos.x, chunkPos.y, chunkPos.z - 1);
                        chunkMutex.lock();
                        if (chunkData.find(northPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(northPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->northData = data;

                            chunkMutex.lock();
                            chunkData[northPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->northData = chunkData.at(northPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set south data
                    {
                        ChunkPos southPos(chunkPos.x, chunkPos.y, chunkPos.z + 1);
                        chunkMutex.lock();
                        if (chunkData.find(southPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(southPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->southData = data;

                            chunkMutex.lock();
                            chunkData[southPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->southData = chunkData.at(southPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set east data
                    {
                        ChunkPos eastPos(chunkPos.x + 1, chunkPos.y, chunkPos.z);
                        chunkMutex.lock();
                        if (chunkData.find(eastPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(eastPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->eastData = data;

                            chunkMutex.lock();
                            chunkData[eastPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->eastData = chunkData.at(eastPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Set west data
                    {
                        ChunkPos westPos(chunkPos.x - 1, chunkPos.y, chunkPos.z);
                        chunkMutex.lock();
                        if (chunkData.find(westPos) == chunkData.end()) {
                            chunkMutex.unlock();
                            auto *d = new uint32_t[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

                            WorldGen::generateChunkData(westPos, d, SEED);

                            auto *data = new ChunkData(d);

                            chunk->westData = data;

                            chunkMutex.lock();
                            chunkData[westPos] = data;
                            chunkMutex.unlock();
                        } else {
                            chunk->westData = chunkData.at(westPos);
                            chunkMutex.unlock();
                        }
                    }

                    // Generate chunk mesh
                    chunk->generateChunkMesh();

                    // Finish
                    chunkMutex.lock();
                    chunks[chunkPos] = chunk;
                    chunkQueue.pop();
                    chunkMutex.unlock();
                } else {
                    chunkMutex.unlock();

                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
    }
}


Chunk *Planet::getChunk(ChunkPos chunkPos) {
    chunkMutex.lock();
    if (chunks.find(chunkPos) == chunks.end()) {
        chunkMutex.unlock();
        return nullptr;
    } else {
        chunkMutex.unlock();
        return chunks.at(chunkPos);
    }
}

void Planet::clearChunkQueue() {
    lastCamX++;
}
