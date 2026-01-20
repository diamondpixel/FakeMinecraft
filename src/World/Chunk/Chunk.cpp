#include "Chunk.h"

#include <iostream>
#include <Shader.h>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Planet.h"
#include "WorldGen.h"
#include "Blocks.h"

Chunk::Chunk(ChunkPos chunkPos)
    : chunkPos(chunkPos) {
    worldPos = glm::vec3(static_cast<float>(chunkPos.x) * CHUNK_WIDTH, 
                         static_cast<float>(chunkPos.y) * CHUNK_HEIGHT,
                         static_cast<float>(chunkPos.z) * CHUNK_WIDTH);

    // Initialize chunk-level culling data (for pre-cull)
    cullingCenter = worldPos + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);

    // Initialize sub-chunk culling data
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        float subChunkY = worldPos.y + i * SUBCHUNK_HEIGHT;
        subChunks[i].cullingCenter = glm::vec3(
            worldPos.x + CHUNK_WIDTH * 0.5f,
            subChunkY + SUBCHUNK_HEIGHT * 0.5f,
            worldPos.z + CHUNK_WIDTH * 0.5f
        );
        subChunks[i].cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, SUBCHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    }

    ready = false;
    generated = false;
    
    // NOTE: glGenQueries moved to uploadMesh() - constructor runs on background thread!
}

Chunk::~Chunk() {
    if (chunkThread.joinable())
        chunkThread.join();

    // Clean up merged VAOs
    if (mergedWorldVAO) {
        glDeleteBuffers(1, &mergedWorldVBO);
        glDeleteBuffers(1, &mergedWorldEBO);
        glDeleteVertexArrays(1, &mergedWorldVAO);
    }
    if (mergedBillboardVAO) {
        glDeleteBuffers(1, &mergedBillboardVBO);
        glDeleteBuffers(1, &mergedBillboardEBO);
        glDeleteVertexArrays(1, &mergedBillboardVAO);
    }
    if (mergedWaterVAO) {
        glDeleteBuffers(1, &mergedWaterVBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteVertexArrays(1, &mergedWaterVAO);
    }
    
    if (queryID != 0) {
        glDeleteQueries(1, &queryID);
    }
}

void Chunk::reset(ChunkPos newPos) {
    // Join any existing thread
    if (chunkThread.joinable()) {
        chunkThread.join();
    }
    
    // Clean up existing GL resources
    if (mergedWorldVAO) {
        glDeleteBuffers(1, &mergedWorldVBO);
        glDeleteBuffers(1, &mergedWorldEBO);
        glDeleteVertexArrays(1, &mergedWorldVAO);
        mergedWorldVAO = 0;
        mergedWorldVBO = 0;
        mergedWorldEBO = 0;
    }
    if (mergedBillboardVAO) {
        glDeleteBuffers(1, &mergedBillboardVBO);
        glDeleteBuffers(1, &mergedBillboardEBO);
        glDeleteVertexArrays(1, &mergedBillboardVAO);
        mergedBillboardVAO = 0;
        mergedBillboardVBO = 0;
        mergedBillboardEBO = 0;
    }
    if (mergedWaterVAO) {
        glDeleteBuffers(1, &mergedWaterVBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteVertexArrays(1, &mergedWaterVAO);
        mergedWaterVAO = 0;
        mergedWaterVBO = 0;
        mergedWaterEBO = 0;
    }
    
    // Reset position and state
    chunkPos = newPos;
    worldPos = glm::vec3(
        static_cast<float>(newPos.x) * CHUNK_WIDTH,
        static_cast<float>(newPos.y) * CHUNK_HEIGHT,
        static_cast<float>(newPos.z) * CHUNK_WIDTH
    );
    
    cullingCenter = worldPos + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    
    // Reset sub-chunk culling
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        float subChunkY = worldPos.y + i * SUBCHUNK_HEIGHT;
        subChunks[i].cullingCenter = glm::vec3(
            worldPos.x + CHUNK_WIDTH * 0.5f,
            subChunkY + SUBCHUNK_HEIGHT * 0.5f,
            worldPos.z + CHUNK_WIDTH * 0.5f
        );
        subChunks[i].cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, SUBCHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
        subChunks[i].ready = false;
    }
    
    // Reset data pointers
    chunkData = nullptr;
    northData = nullptr;
    southData = nullptr;
    upData = nullptr;
    downData = nullptr;
    eastData = nullptr;
    westData = nullptr;
    
    // Reset flags
    ready = false;
    generated = false;
    occlusionVisible = true;
    queryIssued = false;
    occlusionCounter = 0;
    occlusionScore = 1.0f;
    cachedDistSq = 0.0f;
    mergedWorldTriangles = 0;
    mergedBillboardTriangles = 0;
    mergedWaterTriangles = 0;
    listIndex = 0;
    
    // Clear vertex buffers
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        worldVertices[i].clear();
        worldIndices[i].clear();
        billboardVertices[i].clear();
        billboardIndices[i].clear();
        liquidVertices[i].clear();
        liquidIndices[i].clear();
    }
}

// Helper struct to cache neighbor block data
struct NeighborData {
    int block;
    int topBlock;
    const Block *blockType;
    const Block *topBlockType;
    bool isLiquid;

    inline void init(int blk, int topBlk) {
        block = blk;
        topBlock = topBlk;
        blockType = &Blocks::blocks[block];
        topBlockType = &Blocks::blocks[topBlock];
        isLiquid = (blockType->blockType == Block::LIQUID);
    }
};

// Inline helper to check if face should render
inline bool shouldRenderFace(const NeighborData &neighbor, bool isCurrentLiquid) {
    bool shouldRender = (neighbor.blockType->blockType == Block::LEAVES) ||
           (neighbor.blockType->blockType == Block::TRANSPARENT) ||
           (neighbor.blockType->blockType == Block::BILLBOARD) ||
           (neighbor.isLiquid && !isCurrentLiquid);

    return shouldRender;
}

// Extract neighbor fetching into separate functions for clarity
inline void fetchNorthNeighbor(NeighborData &neighbor, int x, int y, int z,
                               const std::shared_ptr<ChunkData>& chunkData, const std::shared_ptr<ChunkData>& northData, const std::shared_ptr<ChunkData>& upData) {
    if (z > 0) {
        int blk = chunkData->getBlock(x, y, z - 1);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? chunkData->getBlock(x, y + 1, z - 1)
                         : upData->getBlock(x, 0, z - 1);
        neighbor.init(blk, topBlk);
    } else if (northData) {
        int blk = northData->getBlock(x, y, CHUNK_WIDTH - 1);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? northData->getBlock(x, y + 1, CHUNK_WIDTH - 1)
                         : Blocks::AIR;
        neighbor.init(blk, topBlk);
        
        static bool logN = false;
        if (blk == 0 && topBlk == 0) { // Suspicious AIR
           // Probe if the chunk has ANY data?
           int sum = 0;
           for(int i=0; i<64; i++) sum += northData->getBlock(x, i, CHUNK_WIDTH - 1);
           
           if (sum == 0 && !logN) {
                std::cout << "[DEBUG] CRITICAL: NorthNeighbor appears to be EMPTY (AIR) below water level! Ptr=" << northData.get() << std::endl;
                logN = true;
           }
        }

    } else {
        static bool logN = false;
        if (!logN) { std::cout << "[DEBUG] North Neighbor PTR is NULL!" << std::endl; logN = true; }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchSouthNeighbor(NeighborData &neighbor, int x, int y, int z,
                               const std::shared_ptr<ChunkData>& chunkData, const std::shared_ptr<ChunkData>& southData, const std::shared_ptr<ChunkData>& upData) {
    if (z < CHUNK_WIDTH - 1) {
        int blk = chunkData->getBlock(x, y, z + 1);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? chunkData->getBlock(x, y + 1, z + 1)
                         : upData->getBlock(x, 0, z + 1);
        neighbor.init(blk, topBlk);
    } else if (southData) {
        int blk = southData->getBlock(x, y, 0);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? southData->getBlock(x, y + 1, 0)
                         : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static bool logS = false;
        if (!logS) { std::cout << "[DEBUG] South Neighbor PTR is NULL!" << std::endl; logS = true; }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchWestNeighbor(NeighborData &neighbor, int x, int y, int z,
                              const std::shared_ptr<ChunkData>& chunkData, const std::shared_ptr<ChunkData>& westData, const std::shared_ptr<ChunkData>& upData) {
    if (x > 0) {
        int blk = chunkData->getBlock(x - 1, y, z);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? chunkData->getBlock(x - 1, y + 1, z)
                         : upData->getBlock(x - 1, 0, z);
        neighbor.init(blk, topBlk);
    } else if (westData) {
        int blk = westData->getBlock(CHUNK_WIDTH - 1, y, z);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? westData->getBlock(CHUNK_WIDTH - 1, y + 1, z)
                         : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static bool logW = false;
        if (!logW) { std::cout << "[DEBUG] West Neighbor PTR is NULL!" << std::endl; logW = true; }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchEastNeighbor(NeighborData &neighbor, int x, int y, int z,
                              const std::shared_ptr<ChunkData>& chunkData, const std::shared_ptr<ChunkData>& eastData, const std::shared_ptr<ChunkData>& upData) {
    if (x < CHUNK_WIDTH - 1) {
        int blk = chunkData->getBlock(x + 1, y, z);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? chunkData->getBlock(x + 1, y + 1, z)
                         : upData->getBlock(x + 1, 0, z);
        neighbor.init(blk, topBlk);
    } else if (eastData) {
        int blk = eastData->getBlock(0, y, z);
        int topBlk = (y + 1 < CHUNK_HEIGHT)
                         ? eastData->getBlock(0, y + 1, z)
                         : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static bool logE = false;
        if (!logE) { std::cout << "[DEBUG] East Neighbor PTR is NULL!" << std::endl; logE = true; }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

// ============================================================================
// GREEDY MESHING - Merge adjacent same-block faces into larger quads
// ============================================================================

// Structure to hold greedy quad information
struct GreedyQuad {
    int x, y, z;           // Position
    int width, height;     // Merged dimensions  
    uint16_t blockId;      // Block type for texture
    FACE_DIRECTION dir;    // Face direction
};

// Generate a merged quad with proper texture tiling (Array Texture version)
inline void emitGreedyQuad(const GreedyQuad& quad, const glm::vec3& worldPos,
                           std::vector<WorldVertex>& vertices, std::vector<unsigned int>& indices,
                           unsigned int& currentVertex) {
    const Block* block = &Blocks::blocks[quad.blockId];
    
    // Get texture grid coordinates based on face direction
    char gridX, gridY;
    bool isSide = (quad.dir != TOP && quad.dir != BOTTOM);
    bool isBottom = (quad.dir == BOTTOM);
    
    if (isBottom) {
        gridX = block->bottomMinX; gridY = block->bottomMinY;
    } else if (isSide) {
        gridX = block->sideMinX; gridY = block->sideMinY;
    } else {
        gridX = block->topMinX; gridY = block->topMinY;
    }
    
    // Calculate Array Layer Index (assuming 16x16 atlas)
    uint8_t layerIndex = (uint8_t)((int)gridY * 16 + (int)gridX);
    
    // UVs are now local repeat counts (0 to width/height)
    char uMax = quad.width;
    char vMax = quad.height;
    
    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;
    
    // Generate 4 vertices based on face direction
    switch (quad.dir) {
        case TOP:
            // Top face: UVs typically (x, z)
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + quad.height + wz, 0, 0, TOP, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + quad.height + wz, uMax, 0, TOP, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + wz, 0, vMax, TOP, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + wz, uMax, vMax, TOP, layerIndex);
            break;
        case BOTTOM:
             vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + quad.height + wz, uMax, vMax, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.height + wz, 0, vMax, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, BOTTOM, layerIndex);
             break;
        case NORTH: // -Z face
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, NORTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, NORTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + wz, uMax, vMax, NORTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, NORTH, layerIndex);
            break;
        case SOUTH: // +Z face
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + 1 + wz, 0, 0, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + 1 + wz, uMax, 0, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + 1 + wz, 0, vMax, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + 1 + wz, uMax, vMax, SOUTH, layerIndex);
            break;
        case WEST: // -X face
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax, WEST, layerIndex);
            break;
        case EAST: // +X face
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + wz, 0, 0, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, EAST, layerIndex);
            break;
        default:
            break;
    }
    
    // Standard quad indices
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);
    currentVertex += 4;
}

void Chunk::generateChunkMesh() {
    // Clear and reserve per-subchunk buffers
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        worldVertices[i].clear();
        worldIndices[i].clear();
        liquidVertices[i].clear();
        liquidIndices[i].clear();
        billboardVertices[i].clear();
        billboardIndices[i].clear();

        // Reserve estimates per sub-chunk (reduced due to greedy merging)
        constexpr size_t estimatedFacesPerSub = CHUNK_WIDTH * SUBCHUNK_HEIGHT;
        worldVertices[i].reserve(estimatedFacesPerSub * 4);
        worldIndices[i].reserve(estimatedFacesPerSub * 6);
        liquidVertices[i].reserve(estimatedFacesPerSub);
        liquidIndices[i].reserve(estimatedFacesPerSub * 2);
        billboardVertices[i].reserve(CHUNK_WIDTH * SUBCHUNK_HEIGHT / 4);
        billboardIndices[i].reserve(CHUNK_WIDTH * SUBCHUNK_HEIGHT / 2);
    }

    // Per-subchunk vertex counters
    unsigned int currentVertex[NUM_SUBCHUNKS] = {};
    unsigned int currentLiquidVertex[NUM_SUBCHUNKS] = {};
    unsigned int currentBillboardVertex[NUM_SUBCHUNKS] = {};

    // Reusable neighbor data
    NeighborData neighbor{};
    
    // ========================================================================
    // GREEDY MESHING for TOP and BOTTOM faces (biggest gains on flat terrain)
    // Process slice-by-slice for horizontal faces
    // ========================================================================
    
    // Mask arrays for greedy meshing (reused per slice)
    static thread_local uint16_t topMask[CHUNK_WIDTH][CHUNK_WIDTH];
    static thread_local uint16_t bottomMask[CHUNK_WIDTH][CHUNK_WIDTH];
    static thread_local bool processed[CHUNK_WIDTH][CHUNK_WIDTH];
    
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        int subChunkIndex = y / SUBCHUNK_HEIGHT;
        
        // Clear masks for this height slice
        memset(topMask, 0, sizeof(topMask));
        memset(bottomMask, 0, sizeof(bottomMask));
        memset(processed, 0, sizeof(processed));
        
        // Build masks: which blocks have visible TOP/BOTTOM faces at this Y level?
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_WIDTH; z++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;
                
                const Block* block = &Blocks::blocks[blockId];
                
                // Skip non-solid blocks for greedy meshing
                if (block->blockType != Block::SOLID && block->blockType != Block::LEAVES) {
                    continue;
                }
                
                // Check TOP face visibility
                int topBlock = (y < CHUNK_HEIGHT - 1) 
                    ? chunkData->getBlock(x, y + 1, z) 
                    : (upData ? upData->getBlock(x, 0, z) : Blocks::AIR);
                const Block* topBlockType = &Blocks::blocks[topBlock];
                
                if (topBlockType->blockType == Block::LEAVES ||
                    topBlockType->blockType == Block::TRANSPARENT ||
                    topBlockType->blockType == Block::BILLBOARD ||
                    topBlockType->blockType == Block::LIQUID) {
                    topMask[x][z] = blockId;
                }
                
                // Check BOTTOM face visibility
                int bottomBlock = (y > 0) 
                    ? chunkData->getBlock(x, y - 1, z) 
                    : (downData ? downData->getBlock(x, CHUNK_HEIGHT - 1, z) : Blocks::AIR);
                const Block* bottomBlockType = &Blocks::blocks[bottomBlock];
                
                if (bottomBlockType->blockType == Block::LEAVES ||
                    bottomBlockType->blockType == Block::TRANSPARENT ||
                    bottomBlockType->blockType == Block::BILLBOARD ||
                    bottomBlockType->blockType == Block::LIQUID) {
                    bottomMask[x][z] = blockId;
                }
            }
        }
        
        // Greedy merge TOP faces
        memset(processed, 0, sizeof(processed));
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_WIDTH; z++) {
                if (topMask[x][z] == 0 || processed[x][z]) continue;
                
                uint16_t blockId = topMask[x][z];
                int width = 1, height = 1;
                
                // Extend in X direction
                while (x + width < CHUNK_WIDTH && 
                       topMask[x + width][z] == blockId &&
                       !processed[x + width][z]) {
                    width++;
                }
                
                // Extend in Z direction
                bool canExtend = true;
                while (z + height < CHUNK_WIDTH && canExtend) {
                    for (int dx = 0; dx < width; dx++) {
                        if (topMask[x + dx][z + height] != blockId ||
                            processed[x + dx][z + height]) {
                            canExtend = false;
                            break;
                        }
                    }
                    if (canExtend) height++;
                }
                
                // Mark as processed
                for (int dx = 0; dx < width; dx++) {
                    for (int dz = 0; dz < height; dz++) {
                        processed[x + dx][z + dz] = true;
                    }
                }
                
                // Emit merged quad
                GreedyQuad quad{x, y, z, width, height, blockId, TOP};
                emitGreedyQuad(quad, worldPos, worldVertices[subChunkIndex], 
                              worldIndices[subChunkIndex], currentVertex[subChunkIndex]);
            }
        }
        
        // Greedy merge BOTTOM faces
        memset(processed, 0, sizeof(processed));
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_WIDTH; z++) {
                if (bottomMask[x][z] == 0 || processed[x][z]) continue;
                
                uint16_t blockId = bottomMask[x][z];
                int width = 1, height = 1;
                
                while (x + width < CHUNK_WIDTH && 
                       bottomMask[x + width][z] == blockId &&
                       !processed[x + width][z]) {
                    width++;
                }
                
                bool canExtend = true;
                while (z + height < CHUNK_WIDTH && canExtend) {
                    for (int dx = 0; dx < width; dx++) {
                        if (bottomMask[x + dx][z + height] != blockId ||
                            processed[x + dx][z + height]) {
                            canExtend = false;
                            break;
                        }
                    }
                    if (canExtend) height++;
                }
                
                for (int dx = 0; dx < width; dx++) {
                    for (int dz = 0; dz < height; dz++) {
                        processed[x + dx][z + dz] = true;
                    }
                }
                
                GreedyQuad quad{x, y, z, width, height, blockId, BOTTOM};
                emitGreedyQuad(quad, worldPos, worldVertices[subChunkIndex], 
                              worldIndices[subChunkIndex], currentVertex[subChunkIndex]);
            }
        }
    }
    
    // ========================================================================
    // GREEDY MESHING SIDE FACES (X/Z Axis)
    // ========================================================================
    
    static thread_local uint16_t maskA[CHUNK_WIDTH][CHUNK_HEIGHT];
    static thread_local uint16_t maskB[CHUNK_WIDTH][CHUNK_HEIGHT];
    static thread_local bool processedSide[CHUNK_WIDTH][CHUNK_HEIGHT];
    
    // ------------------------------------------------------------------------
    // Z-AXIS PASS (North/South Faces) - Slice by Z
    // ------------------------------------------------------------------------
    for (int z = 0; z < CHUNK_WIDTH; z++) {
        memset(maskA, 0, sizeof(maskA)); // North Mask
        memset(maskB, 0, sizeof(maskB)); // South Mask
        memset(processedSide, 0, sizeof(processedSide));
        
        // Build masks
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;
                const Block* block = &Blocks::blocks[blockId];
                if (block->blockType != Block::SOLID && block->blockType != Block::LEAVES) continue;
                
                // Check North Neighbor (-Z)
                neighbor.init(0, 0); // Clear
                fetchNorthNeighbor(neighbor, x, y, z, chunkData, northData, upData);
                if (shouldRenderFace(neighbor, false)) maskA[x][y] = blockId;
                
                // Check South Neighbor (+Z)
                neighbor.init(0, 0);
                fetchSouthNeighbor(neighbor, x, y, z, chunkData, southData, upData);
                if (shouldRenderFace(neighbor, false)) maskB[x][y] = blockId;
            }
        }
        
        // Greedy Mesh North (maskA)
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                if (maskA[x][y] == 0 || processedSide[x][y]) continue;
                
                uint16_t id = maskA[x][y];
                int width = 1, height = 1;
                
                // Extend X
                while (x + width < CHUNK_WIDTH && maskA[x + width][y] == id && !processedSide[x + width][y]) width++;
                
                // Extend Y
                bool canExtend = true;
                while (y + height < CHUNK_HEIGHT && canExtend) {
                    for (int dx = 0; dx < width; dx++) {
                        if (maskA[x + dx][y + height] != id || processedSide[x + dx][y + height]) {
                            canExtend = false; break;
                        }
                    }
                    if (canExtend) height++;
                }
                
                // Mark processed
                for (int dx = 0; dx < width; dx++)
                    for (int dy = 0; dy < height; dy++)
                        processedSide[x + dx][y + dy] = true;
                        
                // Emit
                int subIdx = y / SUBCHUNK_HEIGHT;
                if (subIdx >= NUM_SUBCHUNKS) subIdx = NUM_SUBCHUNKS - 1;
                GreedyQuad quad{x, y, z, width, height, id, NORTH};
                emitGreedyQuad(quad, worldPos, worldVertices[subIdx], worldIndices[subIdx], currentVertex[subIdx]);
            }
        }
        
        // Greedy Mesh South (maskB) - reuse processedSide? NO, need clear or separate loop/mask.
        // Actually, reusing processedSide is WRONG if we do both passes in one loop without reset.
        // So we clear processedSide between North and South.
        memset(processedSide, 0, sizeof(processedSide));
        
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                if (maskB[x][y] == 0 || processedSide[x][y]) continue;
                
                uint16_t id = maskB[x][y];
                int width = 1, height = 1;
                while (x + width < CHUNK_WIDTH && maskB[x + width][y] == id && !processedSide[x + width][y]) width++;
                bool canExtend = true;
                while (y + height < CHUNK_HEIGHT && canExtend) {
                    for (int dx = 0; dx < width; dx++) {
                        if (maskB[x + dx][y + height] != id || processedSide[x + dx][y + height]) {
                            canExtend = false; break;
                        }
                    }
                    if (canExtend) height++;
                }
                for (int dx = 0; dx < width; dx++)
                    for (int dy = 0; dy < height; dy++)
                        processedSide[x + dx][y + dy] = true;

                int subIdx = y / SUBCHUNK_HEIGHT;
                if (subIdx >= NUM_SUBCHUNKS) subIdx = NUM_SUBCHUNKS - 1;
                GreedyQuad quad{x, y, z, width, height, id, SOUTH};
                emitGreedyQuad(quad, worldPos, worldVertices[subIdx], worldIndices[subIdx], currentVertex[subIdx]);
            }
        }
    }

    // ------------------------------------------------------------------------
    // X-AXIS PASS (West/East Faces) - Slice by X
    // ------------------------------------------------------------------------
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        memset(maskA, 0, sizeof(maskA)); // West Mask
        memset(maskB, 0, sizeof(maskB)); // East Mask
        memset(processedSide, 0, sizeof(processedSide));
        
        for (int z = 0; z < CHUNK_WIDTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;
                const Block* block = &Blocks::blocks[blockId];
                if (block->blockType != Block::SOLID && block->blockType != Block::LEAVES) continue;
                
                // West (-X)
                neighbor.init(0, 0);
                fetchWestNeighbor(neighbor, x, y, z, chunkData, westData, upData);
                if (shouldRenderFace(neighbor, false)) maskA[z][y] = blockId;
                
                // East (+X)
                neighbor.init(0, 0);
                fetchEastNeighbor(neighbor, x, y, z, chunkData, eastData, upData);
                if (shouldRenderFace(neighbor, false)) maskB[z][y] = blockId;
            }
        }
        
        // Greedy Mesh West
        for (int z = 0; z < CHUNK_WIDTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                if (maskA[z][y] == 0 || processedSide[z][y]) continue;
                uint16_t id = maskA[z][y];
                int width = 1, height = 1;
                while (z + width < CHUNK_WIDTH && maskA[z + width][y] == id && !processedSide[z + width][y]) width++;
                bool canExtend = true;
                while (y + height < CHUNK_HEIGHT && canExtend) {
                    for (int dz = 0; dz < width; dz++) {
                        if (maskA[z + dz][y + height] != id || processedSide[z + dz][y + height]) {
                            canExtend = false; break;
                        }
                    }
                    if (canExtend) height++;
                }
                for (int dz = 0; dz < width; dz++)
                    for (int dy = 0; dy < height; dy++)
                        processedSide[z + dz][y + dy] = true;

                int subIdx = y / SUBCHUNK_HEIGHT;
                if (subIdx >= NUM_SUBCHUNKS) subIdx = NUM_SUBCHUNKS - 1;
                GreedyQuad quad{x, y, z, width, height, id, WEST};
                emitGreedyQuad(quad, worldPos, worldVertices[subIdx], worldIndices[subIdx], currentVertex[subIdx]);
            }
        }
        
        memset(processedSide, 0, sizeof(processedSide));
        
        // Greedy Mesh East
        for (int z = 0; z < CHUNK_WIDTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                if (maskB[z][y] == 0 || processedSide[z][y]) continue;
                uint16_t id = maskB[z][y];
                int width = 1, height = 1;
                while (z + width < CHUNK_WIDTH && maskB[z + width][y] == id && !processedSide[z + width][y]) width++;
                bool canExtend = true;
                while (y + height < CHUNK_HEIGHT && canExtend) {
                    for (int dz = 0; dz < width; dz++) {
                        if (maskB[z + dz][y + height] != id || processedSide[z + dz][y + height]) {
                            canExtend = false; break;
                        }
                    }
                    if (canExtend) height++;
                }
                for (int dz = 0; dz < width; dz++)
                    for (int dy = 0; dy < height; dy++)
                        processedSide[z + dz][y + dy] = true;
                        
                int subIdx = y / SUBCHUNK_HEIGHT;
                if (subIdx >= NUM_SUBCHUNKS) subIdx = NUM_SUBCHUNKS - 1;
                GreedyQuad quad{x, y, z, width, height, id, EAST};
                emitGreedyQuad(quad, worldPos, worldVertices[subIdx], worldIndices[subIdx], currentVertex[subIdx]);
            }
        }
    }
    
    // ========================================================================
    // NON-SOLID / SPECIAL BLOCKS LOOP (Fluids, Billboards, Transparent)
    // ========================================================================
    
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_WIDTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;

                int subChunkIndex = y / SUBCHUNK_HEIGHT;
                const Block *block = &Blocks::blocks[blockId];

                // Cache top block data
                int topBlock = (y < CHUNK_HEIGHT - 1)
                                   ? chunkData->getBlock(x, y + 1, z)
                                   : (upData ? upData->getBlock(x, 0, z) : Blocks::AIR);
                const Block *topBlockType = &Blocks::blocks[topBlock];
                char waterTopValue = (topBlockType->blockType == Block::TRANSPARENT ||
                                      topBlockType->blockType == Block::SOLID)
                                         ? 1
                                         : 0;

                // Handle billboards
                if (block->blockType == Block::BILLBOARD) {
                    generateBillboardFaces(x, y, z, block, currentBillboardVertex[subChunkIndex], subChunkIndex);
                    continue;
                }

                // Skip Solids/Leaves (handled by greedy meshing)
                if (block->blockType == Block::SOLID || block->blockType == Block::LEAVES) {
                    continue;
                }

                bool isBlockLiquid = (block->blockType == Block::LIQUID);

                // Side faces - not greedy meshed (yet)
                // North face
                fetchNorthNeighbor(neighbor, x, y, z, chunkData, northData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, NORTH, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, NORTH, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                // South face
                fetchSouthNeighbor(neighbor, x, y, z, chunkData, southData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, SOUTH, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, SOUTH, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                // West face
                fetchWestNeighbor(neighbor, x, y, z, chunkData, westData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, WEST, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, WEST, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                // East face
                fetchEastNeighbor(neighbor, x, y, z, chunkData, eastData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, EAST, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, EAST, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                // Liquid TOP faces (not greedy meshed)
                if (isBlockLiquid && topBlockType->blockType != Block::LIQUID) {
                    generateLiquidFaces(x, y, z, TOP, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                }
                
                // Liquid BOTTOM faces
                if (isBlockLiquid) {
                    int bottomBlock = (y > 0)
                                          ? chunkData->getBlock(x, y - 1, z)
                                          : (downData ? downData->getBlock(x, CHUNK_HEIGHT - 1, z) : Blocks::AIR);
                    const Block *bottomBlockType = &Blocks::blocks[bottomBlock];
                    if (bottomBlockType->blockType != Block::LIQUID && bottomBlockType->blockType != Block::SOLID) {
                        generateLiquidFaces(x, y, z, BOTTOM, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    }
                }
            }
        }
    }
    generated = true;
}

// Generate helper for world block faces (Array Texture Version)
void Chunk::generateWorldFaces(const int x, const int y, const int z, FACE_DIRECTION faceDirection, const Block *block,
                               unsigned int &currentVertex, const int subChunkIndex) {
    auto& vertices = worldVertices[subChunkIndex];
    auto& indices = worldIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    char gridX, gridY;
    if (faceDirection == TOP) {
        gridX = block->topMinX; gridY = block->topMinY;
    } else if (faceDirection == BOTTOM) {
        gridX = block->bottomMinX; gridY = block->bottomMinY;
    } else {
        gridX = block->sideMinX; gridY = block->sideMinY;
    }
    
    uint8_t layerIndex = (uint8_t)((int)gridY * 16 + (int)gridX);

    switch (faceDirection) {
    case NORTH:
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, NORTH, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, NORTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, NORTH, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, NORTH, layerIndex);
        break;
    case SOUTH:
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, SOUTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, SOUTH, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 1, SOUTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, SOUTH, layerIndex);
        break;
    case WEST:
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 1, 0, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 1, 1, WEST, layerIndex);
        break;
    case EAST:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 0, 0, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 0, 1, EAST, layerIndex);
        break;
    case TOP:
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 0, TOP, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 0, TOP, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, TOP, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, TOP, layerIndex);
        break;
    case BOTTOM:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 1, BOTTOM, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 1, BOTTOM, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, BOTTOM, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, BOTTOM, layerIndex);
        break;
    default:
        break;
    }

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);
    currentVertex += 4;
}

// Billboard generator
void Chunk::generateBillboardFaces(const int x, const int y, const int z, const Block *block,
                                   unsigned int &currentVertex, const int subChunkIndex) {
    auto& vertices = billboardVertices[subChunkIndex];
    auto& indices = billboardIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    uint8_t layerIndex = (uint8_t)((int)block->sideMinY * 16 + (int)block->sideMinX);

    // Billboards use 2 crossed quads
    // Quad 1: (0,0,0) to (1,1,1)
    vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, layerIndex);
    vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, layerIndex);

    // Quad 2: (0,0,1) to (1,1,0)
    vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, layerIndex);
    vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 1, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, layerIndex);

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);

    indices.push_back(currentVertex + 4);
    indices.push_back(currentVertex + 7);
    indices.push_back(currentVertex + 5);
    indices.push_back(currentVertex + 4);
    indices.push_back(currentVertex + 6);
    indices.push_back(currentVertex + 7);

    currentVertex += 8;
}

// Liquid generator
void Chunk::generateLiquidFaces(const int x, const int y, const int z, const FACE_DIRECTION faceDirection,
                                const Block *block, unsigned int &currentVertex, char liquidTopValue, const int subChunkIndex) {
    auto& vertices = liquidVertices[subChunkIndex];
    auto& indices = liquidIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    uint8_t layerIndex = (uint8_t)((int)block->sideMinY * 16 + (int)block->sideMinX); 
    
    // Liquid height adjustment
    float topY = (faceDirection == TOP) ? 0.875f : 1.0f; // Top face slightly lower? 
    if (liquidTopValue == 1) topY = 1.0f;

    switch (faceDirection) {
    case NORTH:
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 1, 1, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, NORTH, layerIndex, liquidTopValue);
        break;
    case SOUTH:
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 1, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 0, 1, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, SOUTH, layerIndex, liquidTopValue);
        break;
    case WEST:
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 1, 1, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 1, 0, WEST, layerIndex, liquidTopValue);
        break;
    case EAST:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 0, 0, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 1, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 0, 1, EAST, layerIndex, liquidTopValue);
        break;
    case TOP:
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 0, 0, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 0, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 1, 1, TOP, layerIndex, liquidTopValue);
        break;
    case BOTTOM:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 1, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 1, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, BOTTOM, layerIndex, liquidTopValue);
        break;
    default:
        break;
    }

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);

    currentVertex += 4;
}

// Helper function to setup VAO attributes
// Helper function to setup VAO attributes
inline void setupWorldVAO() {
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, posX)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, texU))); // texU/texV as local UVs
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, direction)));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, layerIndex)));
    glEnableVertexAttribArray(3);
}

inline void setupWaterVAO() {
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(FluidVertex),
                          reinterpret_cast<void *>(offsetof(FluidVertex, posX)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(FluidVertex),
                          reinterpret_cast<void *>(offsetof(FluidVertex, texU)));
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(FluidVertex),
                           reinterpret_cast<void *>(offsetof(FluidVertex, direction)));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(FluidVertex), reinterpret_cast<void *>(offsetof(FluidVertex, layerIndex)));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(4, 1, GL_BYTE, sizeof(FluidVertex), reinterpret_cast<void *>(offsetof(FluidVertex, top)));
    glEnableVertexAttribArray(4);
}

inline void setupBillboardVAO() {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex),
                          reinterpret_cast<void *>(offsetof(BillboardVertex, posX)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(BillboardVertex),
                              reinterpret_cast<void *>(offsetof(BillboardVertex, texU)));
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(BillboardVertex), 
                          reinterpret_cast<void *>(offsetof(BillboardVertex, layerIndex)));
    glEnableVertexAttribArray(3);
}

void Chunk::uploadMesh() {
    if (ready || !generated) return;
    
    // Generate occlusion query ID on main thread (OpenGL context required)
    if (queryID == 0) {
        glGenQueries(1, &queryID);
    }

    // Calculate total sizes across all sub-chunks
    size_t totalWorldVerts = 0, totalWorldInds = 0;
    size_t totalBillboardVerts = 0, totalBillboardInds = 0;
    size_t totalWaterVerts = 0, totalWaterInds = 0;
    
    // Track min/max Y for tight bounding box
    int16_t minY = INT16_MAX, maxY = INT16_MIN;
    
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        totalWorldVerts += worldVertices[i].size();
        totalWorldInds += worldIndices[i].size();
        totalBillboardVerts += billboardVertices[i].size();
        totalBillboardInds += billboardIndices[i].size();
        totalWaterVerts += liquidVertices[i].size();
        totalWaterInds += liquidIndices[i].size();
        
        // Find actual Y bounds from world vertices
        for (const auto& v : worldVertices[i]) {
            if (v.posY < minY) minY = v.posY;
            if (v.posY > maxY) maxY = v.posY;
        }
        for (const auto& v : billboardVertices[i]) {
            if (v.posY < minY) minY = v.posY;
            if (v.posY > maxY) maxY = v.posY;
        }
        for (const auto& v : liquidVertices[i]) {
            if (v.posY < minY) minY = v.posY;
            if (v.posY > maxY) maxY = v.posY;
        }
    }
    
    // Update culling bounds to match actual geometry (tight bounding box)
    if (minY != INT16_MAX && maxY != INT16_MIN) {
        float geometryMinY = static_cast<float>(minY);
        float geometryMaxY = static_cast<float>(maxY);
        float geometryHeight = geometryMaxY - geometryMinY;
        
        cullingCenter = glm::vec3(
            worldPos.x + CHUNK_WIDTH * 0.5f,
            (geometryMinY + geometryMaxY) * 0.5f,
            worldPos.z + CHUNK_WIDTH * 0.5f
        );
        cullingExtents = glm::vec3(
            CHUNK_WIDTH * 0.5f,
            geometryHeight * 0.5f + 1.0f, // +1 for safety margin
            CHUNK_WIDTH * 0.5f
        );
    } else {
        // Fallback for empty/weird chunks
        cullingCenter = worldPos + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
        cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    }

    // Merge and upload WORLD geometry
    mergedWorldTriangles = totalWorldInds;
    if (totalWorldVerts > 0) {
        // Merge all sub-chunk vertices and indices
        std::vector<WorldVertex> mergedVerts;
        std::vector<unsigned int> mergedInds;
        mergedVerts.reserve(totalWorldVerts);
        mergedInds.reserve(totalWorldInds);
        
        unsigned int vertexOffset = 0;
        for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
            // Append vertices
            mergedVerts.insert(mergedVerts.end(), worldVertices[i].begin(), worldVertices[i].end());
            // Append indices with offset
            for (unsigned int idx : worldIndices[i]) {
                mergedInds.push_back(idx + vertexOffset);
            }
            vertexOffset += worldVertices[i].size();
            
            // Track per-sub-chunk counts for potential future use
            subChunks[i].numTrianglesWorld = worldIndices[i].size();
        }
        
        glGenVertexArrays(1, &mergedWorldVAO);
        glGenBuffers(1, &mergedWorldVBO);
        glGenBuffers(1, &mergedWorldEBO);
        glBindVertexArray(mergedWorldVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mergedWorldVBO);
        glBufferData(GL_ARRAY_BUFFER, mergedVerts.size() * sizeof(WorldVertex), mergedVerts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mergedWorldEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mergedInds.size() * sizeof(unsigned int), mergedInds.data(), GL_STATIC_DRAW);
        setupWorldVAO();
    }

    // Merge and upload BILLBOARD geometry
    mergedBillboardTriangles = totalBillboardInds;
    if (totalBillboardVerts > 0) {
        std::vector<BillboardVertex> mergedVerts;
        std::vector<unsigned int> mergedInds;
        mergedVerts.reserve(totalBillboardVerts);
        mergedInds.reserve(totalBillboardInds);
        
        unsigned int vertexOffset = 0;
        for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
            mergedVerts.insert(mergedVerts.end(), billboardVertices[i].begin(), billboardVertices[i].end());
            for (unsigned int idx : billboardIndices[i]) {
                mergedInds.push_back(idx + vertexOffset);
            }
            vertexOffset += billboardVertices[i].size();
            subChunks[i].numTrianglesBillboard = billboardIndices[i].size();
        }
        
        glGenVertexArrays(1, &mergedBillboardVAO);
        glGenBuffers(1, &mergedBillboardVBO);
        glGenBuffers(1, &mergedBillboardEBO);
        glBindVertexArray(mergedBillboardVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mergedBillboardVBO);
        glBufferData(GL_ARRAY_BUFFER, mergedVerts.size() * sizeof(BillboardVertex), mergedVerts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mergedBillboardEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mergedInds.size() * sizeof(unsigned int), mergedInds.data(), GL_STATIC_DRAW);
        setupBillboardVAO();
    }

    // Merge and upload WATER geometry
    mergedWaterTriangles = totalWaterInds;
    if (totalWaterVerts > 0) {
        std::vector<FluidVertex> mergedVerts;
        std::vector<unsigned int> mergedInds;
        mergedVerts.reserve(totalWaterVerts);
        mergedInds.reserve(totalWaterInds);
        
        unsigned int vertexOffset = 0;
        for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
            mergedVerts.insert(mergedVerts.end(), liquidVertices[i].begin(), liquidVertices[i].end());
            for (unsigned int idx : liquidIndices[i]) {
                mergedInds.push_back(idx + vertexOffset);
            }
            vertexOffset += liquidVertices[i].size();
            subChunks[i].numTrianglesLiquid = liquidIndices[i].size();
        }
        
        glGenVertexArrays(1, &mergedWaterVAO);
        glGenBuffers(1, &mergedWaterVBO);
        glGenBuffers(1, &mergedWaterEBO);
        glBindVertexArray(mergedWaterVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mergedWaterVBO);
        glBufferData(GL_ARRAY_BUFFER, mergedVerts.size() * sizeof(FluidVertex), mergedVerts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mergedWaterEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mergedInds.size() * sizeof(unsigned int), mergedInds.data(), GL_STATIC_DRAW);
        setupWaterVAO();
    }

    // Clear ALL CPU-side mesh data
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        worldVertices[i].clear(); 
        worldIndices[i].clear(); 
        billboardVertices[i].clear(); 
        billboardIndices[i].clear(); 
        liquidVertices[i].clear(); 
        liquidIndices[i].clear(); 
        subChunks[i].ready = true;
    }

    ready = true;
}

void Chunk::renderSolid(const int subChunkIndex) {
    if (!ready) {
        if (generated) {
            uploadMesh();
        } else {
            return;
        }
    }

    const SubChunk& sc = subChunks[subChunkIndex];
    if (sc.numTrianglesWorld == 0 || sc.worldVAO == 0) return;
    
    glBindVertexArray(sc.worldVAO);
    glDrawElements(GL_TRIANGLES, sc.numTrianglesWorld, GL_UNSIGNED_INT, 0);
}

void Chunk::renderBillboard(const int subChunkIndex) const {
    if (!ready) return;
    
    const SubChunk& sc = subChunks[subChunkIndex];
    if (sc.numTrianglesBillboard == 0 || sc.billboardVAO == 0) return;

    glBindVertexArray(sc.billboardVAO);
    glDrawElements(GL_TRIANGLES, sc.numTrianglesBillboard, GL_UNSIGNED_INT, 0);
}

void Chunk::renderWater(const int subChunkIndex) const {
    if (!ready) return;
    
    const SubChunk& sc = subChunks[subChunkIndex];
    if (sc.numTrianglesLiquid == 0 || sc.waterVAO == 0) return;

    glBindVertexArray(sc.waterVAO);
    glDrawElements(GL_TRIANGLES, sc.numTrianglesLiquid, GL_UNSIGNED_INT, 0);
}

// Render all solid geometry in ONE draw call using merged VAO
void Chunk::renderAllSolid() {
    if (!ready) {
        if (generated) {
            uploadMesh();
        } else {
            return;
        }
    }

    if (mergedWorldTriangles > 0 && mergedWorldVAO != 0) {
        glBindVertexArray(mergedWorldVAO);
        glDrawElements(GL_TRIANGLES, mergedWorldTriangles, GL_UNSIGNED_INT, 0);
    }
}

// Render all billboard geometry in ONE draw call using merged VAO
void Chunk::renderAllBillboard() const {
    if (!ready) return;

    if (mergedBillboardTriangles > 0 && mergedBillboardVAO != 0) {
        glBindVertexArray(mergedBillboardVAO);
        glDrawElements(GL_TRIANGLES, mergedBillboardTriangles, GL_UNSIGNED_INT, 0);
    }
}

// Render all water geometry in ONE draw call using merged VAO
void Chunk::renderAllWater() const {
    if (!ready) return;

    if (mergedWaterTriangles > 0 && mergedWaterVAO != 0) {
        glBindVertexArray(mergedWaterVAO);
        glDrawElements(GL_TRIANGLES, mergedWaterTriangles, GL_UNSIGNED_INT, 0);
    }
}

uint16_t Chunk::getBlockAtPos(int x, int y, int z) const {
    return ready ? chunkData->getBlock(x, y, z) : 0;
}

// Unified buffer update function to reduce code duplication
inline void updateBuffers(GLuint vao, GLuint vbo, GLuint ebo,
                          const void *vertexData, size_t vertexSize,
                          const unsigned int *indexData, size_t indexSize) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertexData);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexSize, indexData);
}

void Chunk::updateBlock(int x, int y, int z, uint8_t newBlock) {
    chunkData->setBlock(x, y, z, newBlock);
    
    // Regenerate mesh data
    generateChunkMesh();

    // Use uploadMesh() to regenerate the merged buffers correctly.
    ready = false; 
    generated = true;
    uploadMesh();

    // Update neighboring chunks if at boundaries
    if (x == 0) {
        Chunk *westChunk = Planet::planet->getChunk({chunkPos.x - 1, chunkPos.y, chunkPos.z});
        if (westChunk) westChunk->updateChunk();
    } else if (x == CHUNK_WIDTH - 1) {
        Chunk *eastChunk = Planet::planet->getChunk({chunkPos.x + 1, chunkPos.y, chunkPos.z});
        if (eastChunk) eastChunk->updateChunk();
    }

    if (y == 0) {
        Chunk *downChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y - 1, chunkPos.z});
        if (downChunk) downChunk->updateChunk();
    } else if (y == CHUNK_HEIGHT - 1) {
        Chunk *upChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y + 1, chunkPos.z});
        if (upChunk) upChunk->updateChunk();
    }

    if (z == 0) {
        Chunk *northChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z - 1});
        if (northChunk) northChunk->updateChunk();
    } else if (z == CHUNK_WIDTH - 1) {
        Chunk *southChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z + 1});
        if (southChunk) southChunk->updateChunk();
    }
}

void Chunk::updateChunk() {
    generateChunkMesh();
    ready = false; 
    generated = true;
    uploadMesh();
}

