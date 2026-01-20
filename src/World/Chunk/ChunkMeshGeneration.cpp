#include "Chunk.h"
#include "ChunkUtils.h"
#include "ChunkGreedyMeshing.h"
#include "Blocks.h"

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

    // Generate greedy-meshed horizontal faces
    generateHorizontalFaces(currentVertex);

    // Generate greedy-meshed vertical faces
    generateVerticalFaces(currentVertex);

    // Generate non-solid blocks (fluids, billboards, transparent)
    generateSpecialBlocks(currentVertex, currentLiquidVertex, currentBillboardVertex);

    generated = true;
}

void Chunk::generateHorizontalFaces(unsigned int* currentVertex) {
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
        buildHorizontalMasks(y, topMask, bottomMask);

        // Greedy merge TOP faces
        greedyMergeTopFaces(y, subChunkIndex, topMask, processed, currentVertex);

        // Greedy merge BOTTOM faces
        greedyMergeBottomFaces(y, subChunkIndex, bottomMask, processed, currentVertex);
    }
}

void Chunk::buildHorizontalMasks(const int y, uint16_t topMask[][CHUNK_WIDTH], uint16_t bottomMask[][CHUNK_WIDTH]) const {
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
}

void Chunk::greedyMergeTopFaces(int y, int subChunkIndex, uint16_t topMask[][CHUNK_WIDTH],
                                bool processed[][CHUNK_WIDTH], unsigned int* currentVertex) {
    memset(processed, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_WIDTH);

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
}

void Chunk::greedyMergeBottomFaces(int y, int subChunkIndex, uint16_t bottomMask[][CHUNK_WIDTH],
                                   bool processed[][CHUNK_WIDTH], unsigned int* currentVertex) {
    memset(processed, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_WIDTH);

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

void Chunk::generateVerticalFaces(unsigned int* currentVertex) {
    static thread_local uint16_t maskA[CHUNK_WIDTH][CHUNK_HEIGHT];
    static thread_local uint16_t maskB[CHUNK_WIDTH][CHUNK_HEIGHT];
    static thread_local bool processedSide[CHUNK_WIDTH][CHUNK_HEIGHT];

    // Z-AXIS PASS (North/South Faces)
    generateZAxisFaces(currentVertex, maskA, maskB, processedSide);

    // X-AXIS PASS (West/East Faces)
    generateXAxisFaces(currentVertex, maskA, maskB, processedSide);
}

void Chunk::generateZAxisFaces(unsigned int* currentVertex, uint16_t maskA[][CHUNK_HEIGHT],
                               uint16_t maskB[][CHUNK_HEIGHT], bool processedSide[][CHUNK_HEIGHT]) {
    NeighborData neighbor{};

    for (int z = 0; z < CHUNK_WIDTH; z++) {
        memset(maskA, 0, sizeof(uint16_t) * CHUNK_WIDTH * CHUNK_HEIGHT);
        memset(maskB, 0, sizeof(uint16_t) * CHUNK_WIDTH * CHUNK_HEIGHT);
        memset(processedSide, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_HEIGHT);

        // Build masks
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;
                const Block* block = &Blocks::blocks[blockId];
                if (block->blockType != Block::SOLID && block->blockType != Block::LEAVES) continue;

                // Check North Neighbor (-Z)
                fetchNorthNeighbor(neighbor, x, y, z, chunkData, northData, upData);
                if (shouldRenderFace(neighbor, false)) maskA[x][y] = blockId;

                // Check South Neighbor (+Z)
                fetchSouthNeighbor(neighbor, x, y, z, chunkData, southData, upData);
                if (shouldRenderFace(neighbor, false)) maskB[x][y] = blockId;
            }
        }

        // Greedy Mesh North
        greedyMergeNorthFaces(z, maskA, processedSide, currentVertex);

        // Greedy Mesh South
        memset(processedSide, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_HEIGHT);
        greedyMergeSouthFaces(z, maskB, processedSide, currentVertex);
    }
}

void Chunk::generateXAxisFaces(unsigned int* currentVertex, uint16_t maskA[][CHUNK_HEIGHT],
                               uint16_t maskB[][CHUNK_HEIGHT], bool processedSide[][CHUNK_HEIGHT]) {
    NeighborData neighbor{};

    for (int x = 0; x < CHUNK_WIDTH; x++) {
        memset(maskA, 0, sizeof(uint16_t) * CHUNK_WIDTH * CHUNK_HEIGHT);
        memset(maskB, 0, sizeof(uint16_t) * CHUNK_WIDTH * CHUNK_HEIGHT);
        memset(processedSide, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_HEIGHT);

        for (int z = 0; z < CHUNK_WIDTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                uint16_t blockId = chunkData->getBlock(x, y, z);
                if (blockId == 0) continue;
                const Block* block = &Blocks::blocks[blockId];
                if (block->blockType != Block::SOLID && block->blockType != Block::LEAVES) continue;

                // West (-X)
                fetchWestNeighbor(neighbor, x, y, z, chunkData, westData, upData);
                if (shouldRenderFace(neighbor, false)) maskA[z][y] = blockId;

                // East (+X)
                fetchEastNeighbor(neighbor, x, y, z, chunkData, eastData, upData);
                if (shouldRenderFace(neighbor, false)) maskB[z][y] = blockId;
            }
        }

        // Greedy Mesh West
        greedyMergeWestFaces(x, maskA, processedSide, currentVertex);

        // Greedy Mesh East
        memset(processedSide, 0, sizeof(bool) * CHUNK_WIDTH * CHUNK_HEIGHT);
        greedyMergeEastFaces(x, maskB, processedSide, currentVertex);
    }
}

void Chunk::greedyMergeNorthFaces(int z, uint16_t maskA[][CHUNK_HEIGHT],
                                  bool processedSide[][CHUNK_HEIGHT], unsigned int* currentVertex) {
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            if (maskA[x][y] == 0 || processedSide[x][y]) continue;

            uint16_t id = maskA[x][y];
            int width = 1, height = 1;

            while (x + width < CHUNK_WIDTH && maskA[x + width][y] == id && !processedSide[x + width][y]) width++;

            bool canExtend = true;
            while (y + height < CHUNK_HEIGHT && canExtend) {
                for (int dx = 0; dx < width; dx++) {
                    if (maskA[x + dx][y + height] != id || processedSide[x + dx][y + height]) {
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
            GreedyQuad quad{x, y, z, width, height, id, NORTH};
            emitGreedyQuad(quad, worldPos, worldVertices[subIdx], worldIndices[subIdx], currentVertex[subIdx]);
        }
    }
}

void Chunk::greedyMergeSouthFaces(int z, uint16_t maskB[][CHUNK_HEIGHT],
                                  bool processedSide[][CHUNK_HEIGHT], unsigned int* currentVertex) {
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

void Chunk::greedyMergeWestFaces(int x, uint16_t maskA[][CHUNK_HEIGHT],
                                 bool processedSide[][CHUNK_HEIGHT], unsigned int* currentVertex) {
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
}

void Chunk::greedyMergeEastFaces(int x, uint16_t maskB[][CHUNK_HEIGHT],
                                 bool processedSide[][CHUNK_HEIGHT], unsigned int* currentVertex) {
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

void Chunk::generateSpecialBlocks(unsigned int* currentVertex, unsigned int* currentLiquidVertex,
                                  unsigned int* currentBillboardVertex) {
    NeighborData neighbor{};

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
                fetchNorthNeighbor(neighbor, x, y, z, chunkData, northData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, NORTH, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, NORTH, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                fetchSouthNeighbor(neighbor, x, y, z, chunkData, southData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, SOUTH, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, SOUTH, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

                fetchWestNeighbor(neighbor, x, y, z, chunkData, westData, upData);
                if (shouldRenderFace(neighbor, isBlockLiquid)) {
                    if (isBlockLiquid) {
                        generateLiquidFaces(x, y, z, WEST, block, currentLiquidVertex[subChunkIndex], waterTopValue, subChunkIndex);
                    } else {
                        generateWorldFaces(x, y, z, WEST, block, currentVertex[subChunkIndex], subChunkIndex);
                    }
                }

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
}