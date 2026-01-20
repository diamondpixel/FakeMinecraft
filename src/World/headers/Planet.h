#pragma once

#include <vector>
#include <unordered_map>
#include <queue>
#include <glm/glm.hpp>
#include <thread>
#include <mutex>

#include "ChunkPos.h"
#include "ChunkData.h"
#include "Chunk.h"
#include "ChunkPosHash.h"
#include "Frustum.h"
#include "ThreadPool.h"

#include "WorldConstants.h"

// Hardware Occlusion Query Smoothing Methods
enum class OcclusionMethod {
    VOTING,  // Discrete voting: N consecutive failures required
    EMA      // Exponential Moving Average: smooth interpolation
};

class Planet
{
    // Methods
public:
    Planet(Shader* solidShader, Shader* waterShader, Shader* billboardShader, Shader* bboxShader);
    ~Planet();
    // updateOcclusion: If true, issues new queries. If false, freezes current visibility state.
    void update(const glm::vec3& cameraPos, bool updateOcclusion = true);
    void updateFrustum(const glm::mat4& frustumVP, const glm::mat4& renderingVP);
    Chunk* getChunk(ChunkPos chunkPos);
    void clearChunkQueue();

private:
    void chunkThreadUpdate();

    // Helper methods for improved organization and performance
    [[nodiscard]] bool isOutOfRenderDistance(const ChunkPos& pos) const;
    static std::pair<glm::vec3, glm::vec3> getChunkBounds(const ChunkPos& pos);
    void cleanupUnusedChunkData();

    void updateChunkQueue();
    void buildChunkQueue();
    void addChunkRing(int radius);
    void addChunkEdges(int radius, int edge, int y);
    void addChunkCorners(int radius, int y);
    void addChunkIfMissing(int x, int y, int z);

    void processChunkDataQueue();
    void processChunkQueue();
    void processRegenQueue();
    bool populateChunkData(Chunk* chunk, const ChunkPos& chunkPos);
    std::shared_ptr<ChunkData> getOrCreateChunkData(const ChunkPos& pos);

    // Variables
public:
    static Planet* planet;
    unsigned int numChunks = 0, numChunksRendered = 0;
    int renderDistance = 30;
    int renderHeight = 2;

private:
    std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> chunks;
    std::vector<Chunk*> chunkList; // Contiguous list for fast iteration
    std::unordered_map<ChunkPos, std::shared_ptr<ChunkData>, ChunkPosHash> chunkData;
    std::queue<ChunkPos> chunkQueue;
    std::queue<ChunkPos> chunkDataQueue;
    std::queue<ChunkPos> regenQueue;
    unsigned int chunksLoading = 0;
    int lastCamX = -100, lastCamY = -100, lastCamZ = -100;
    int camChunkX = -100, camChunkY = -100, camChunkZ = -100;
    long last_seed = 0;

    Shader* solidShader;
    Shader* waterShader;
    Shader* billboardShader;
    Shader* bboxShader;

    std::thread chunkThread;
    std::mutex chunkMutex;

    bool shouldEnd = false;

    Frustum frustum;
    glm::mat4 lastViewProjection = glm::mat4(1.0f);
    ThreadPool chunkGenPool;  // Thread pool for parallel chunk data generation
    
    // Sort caching - skip re-sorting when camera is stationary
    int prevSortCamX = -1000, prevSortCamZ = -1000;
    
    // Pre-allocated render lists (avoid per-frame allocation)
    std::vector<Chunk*> solidChunks;
    std::vector<Chunk*> billboardChunks;
    std::vector<Chunk*> waterChunks;
    std::vector<Chunk*> frustumVisibleChunks; // ALL chunks in frustum (for HOQ)
    std::vector<ChunkPos> toDeleteList;

    // Chunk object pool (recycle instead of new/delete)
    std::vector<Chunk*> chunkPool;
    std::vector<Chunk*> renderChunks; // Double-buffered list for rendering (only updated when dirty)
    bool renderChunksDirty = false;
    
    // HOQ Smoothing Configuration
    OcclusionMethod occlusionMethod = OcclusionMethod::VOTING;

    // Hardware Occlusion Bounding Box
    unsigned int bboxVAO = 0, bboxVBO = 0, bboxEBO = 0;
    void initBoundingBoxMesh();
    
public:
    void drawBoundingBox(const glm::vec3& center, const glm::vec3& extents);
};