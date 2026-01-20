#pragma once

#include <Shader.h>
#include <thread>
#include <vector>
#include <glm/glm.hpp>
#include "WorldVertex.h"
#include "FluidVertex.h"
#include "BillboardVertex.h"
#include "Block.h"
#include "ChunkPos.h"
#include "ChunkData.h"
#include <memory>

constexpr int SUBCHUNK_HEIGHT = 16;
constexpr int NUM_SUBCHUNKS = 16; // CHUNK_HEIGHT / SUBCHUNK_HEIGHT

struct LiquidFace {
    char x, y, z;
    FACE_DIRECTION dir;
    const Block *block;
    char waterTopValue;
};

// Sub-Chunk for fine-grained culling (16 blocks high)
struct SubChunk {
    unsigned int worldVAO{}, waterVAO{}, billboardVAO{};
    unsigned int worldVBO{}, worldEBO{}, liquidVBO{}, liquidEBO{}, billboardVBO{}, billboardEBO{};
    unsigned int numTrianglesWorld{}, numTrianglesLiquid{}, numTrianglesBillboard{};
    glm::vec3 cullingCenter;
    glm::vec3 cullingExtents;
    bool ready = false;

    [[nodiscard]] bool hasSolid() const { return numTrianglesWorld > 0 && worldVAO != 0; }
    [[nodiscard]] bool hasBillboard() const { return numTrianglesBillboard > 0 && billboardVAO != 0; }
    [[nodiscard]] bool hasWater() const { return numTrianglesLiquid > 0 && waterVAO != 0; }
};

class Chunk {
public:
    explicit Chunk(ChunkPos chunkPos);

    ~Chunk();

    void generateChunkMesh();

    void generateWorldFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block,
                            unsigned int &currentVertex, int subChunkIndex);

    void generateBillboardFaces(int x, int y, int z, const Block *block, unsigned int &currentVertex, int subChunkIndex);

    void generateLiquidFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block,
                             unsigned int &currentVertex, char liquidTopValue, int subChunkIndex);

    void renderSolid(int subChunkIndex);
    void renderBillboard(int subChunkIndex) const;
    void renderWater(int subChunkIndex) const;
    
    // Render all sub-chunks in one call (reduces draw call overhead)
    void renderAllSolid();
    void renderAllBillboard() const;
    void renderAllWater() const;

    [[nodiscard]] uint16_t getBlockAtPos(int x, int y, int z) const;

    void updateBlock(int x, int y, int z, uint8_t newBlock);

    void updateChunk();

    void uploadMesh();
    
    // Reset chunk for reuse in object pool
    void reset(ChunkPos newPos);
    
    [[nodiscard]] bool hasSolid() const { return mergedWorldTriangles > 0; }
    [[nodiscard]] bool hasBillboard() const { return mergedBillboardTriangles > 0; }
    [[nodiscard]] bool hasWater() const { return mergedWorldTriangles > 0; }


    std::shared_ptr<ChunkData> chunkData{};
    std::shared_ptr<ChunkData> northData{};
    std::shared_ptr<ChunkData> southData{};
    std::shared_ptr<ChunkData> upData{};
    std::shared_ptr<ChunkData> downData{};
    std::shared_ptr<ChunkData> eastData{};
    std::shared_ptr<ChunkData> westData{};
    ChunkPos chunkPos;
    bool ready;
    bool generated;
    size_t listIndex = 0; // For O(1) removal from Planet::chunkList
    float cachedDistSq = 0.0f;  // Cached distance squared for sorting (updated once per frame)
    
    // Chunk-level culling data (for frustum culling)
    glm::vec3 cullingCenter;
    glm::vec3 cullingExtents;
    
    // Merged VAOs - single draw call per geometry type (all sub-chunks combined)
    unsigned int mergedWorldVAO{}, mergedWorldVBO{}, mergedWorldEBO{};
    unsigned int mergedBillboardVAO{}, mergedBillboardVBO{}, mergedBillboardEBO{};
    unsigned int mergedWaterVAO{}, mergedWaterVBO{}, mergedWaterEBO{};
    unsigned int mergedWorldTriangles{}, mergedBillboardTriangles{}, mergedWaterTriangles{};
    
    // Sub-chunk data (kept for potential future per-sub-chunk culling)
    SubChunk subChunks[NUM_SUBCHUNKS];

    // Hardware Occlusion Query
    unsigned int queryID = 0;
    bool queryIssued = false; // True after first glBeginQuery/glEndQuery cycle
    bool occlusionVisible = true; // Result of previous frame's query (default visible)
    
    // HOQ Smoothing
    int occlusionCounter = 0;      // Discrete Voting: frames of consecutive occlusion
    float occlusionScore = 1.0f;   // EMA: visibility score (0.0 = hidden, 1.0 = visible)

private:
    glm::vec3 worldPos;
    std::thread chunkThread;
    
    // Temporary buffers per sub-chunk (used during generation, cleared after upload)
    std::vector<WorldVertex> worldVertices[NUM_SUBCHUNKS];
    std::vector<unsigned int> worldIndices[NUM_SUBCHUNKS];
    std::vector<FluidVertex> liquidVertices[NUM_SUBCHUNKS];
    std::vector<unsigned int> liquidIndices[NUM_SUBCHUNKS];
    std::vector<BillboardVertex> billboardVertices[NUM_SUBCHUNKS];
    std::vector<unsigned int> billboardIndices[NUM_SUBCHUNKS];
};
