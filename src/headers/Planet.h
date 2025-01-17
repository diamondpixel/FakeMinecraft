#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <queue>
#include <glm/glm.hpp>
#include <thread>
#include <mutex>

#include "../Chunk/headers/ChunkPos.h"
#include "../Chunk/headers/ChunkData.h"
#include "../Chunk/headers/Chunk.h"
#include "./../Chunk/headers/ChunkPosHash.h"6

inline int MAX_HEIGHT = 256;
constexpr unsigned int CHUNK_SIZE = 32;
static int WATER_LEVEL = MAX_HEIGHT - 3;
inline int MIN_HEIGHT = -256;
inline long SEED = 0;

class Planet
{
    // Methods
public:
    Planet(Shader* solidShader, Shader* waterShader, Shader* billboardShader);
    ~Planet();
    void update(const glm::vec3& cameraPos);
    Chunk* getChunk(ChunkPos chunkPos);
    void clearChunkQueue();

private:
    void chunkThreadUpdate();
    // Variables
public:
    static Planet* planet;
    unsigned int numChunks = 0, numChunksRendered = 0;
    int renderDistance = 5;
    int renderHeight = 2;



private:
    std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> chunks;
    std::unordered_map<ChunkPos, ChunkData*, ChunkPosHash> chunkData;
    std::queue<ChunkPos> chunkQueue;
    std::queue<ChunkPos> chunkDataQueue;
    std::queue<ChunkPos> chunkDataDeleteQueue;
    unsigned int chunksLoading = 0;
    int lastCamX = -100, lastCamY = -100, lastCamZ = -100;
    int camChunkX = -100, camChunkY = -100, camChunkZ = -100;
    long last_seed = 0;

    Shader* solidShader;
    Shader* waterShader;
    Shader* billboardShader;

    std::thread chunkThread;
    std::mutex chunkMutex;

    bool shouldEnd = false;
};