#pragma once

#include <memory>
#include <iostream>
#include "Block.h"
#include "ChunkData.h"
#include "Blocks.h"

// Helper struct to cache neighbor block data
struct NeighborData {
    int block;
    int topBlock;
    const Block *blockType;
    const Block *topBlockType;
    bool isLiquid;

    void init(int const blk, int const topBlk) {
        block = blk;
        topBlock = topBlk;
        blockType = &Blocks::blocks[block];
        topBlockType = &Blocks::blocks[topBlock];
        isLiquid = (blockType->blockType == Block::LIQUID);
    }
};

// Inline helper to check if face should render
inline bool shouldRenderFace(const NeighborData &neighbor, bool const isCurrentLiquid) {
    const auto shouldRender = (neighbor.blockType->blockType == Block::LEAVES) ||
                              (neighbor.blockType->blockType == Block::TRANSPARENT) ||
                              (neighbor.blockType->blockType == Block::BILLBOARD) ||
                              (neighbor.isLiquid && !isCurrentLiquid);

    return shouldRender;
}

// Extract neighbor fetching into separate functions for clarity
inline void fetchNorthNeighbor(NeighborData &neighbor, const int x, const int y, const int z,
                               const std::shared_ptr<ChunkData> &chunkData,
                               const std::shared_ptr<ChunkData> &northData,
                               const std::shared_ptr<ChunkData> &upData) {
    if (z > 0) {
        const auto blk = chunkData->getBlock(x, y, z - 1);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? chunkData->getBlock(x, y + 1, z - 1)
                                : upData->getBlock(x, 0, z - 1);
        neighbor.init(blk, topBlk);
    } else if (northData) {
        const auto blk = northData->getBlock(x, y, CHUNK_WIDTH - 1);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? northData->getBlock(x, y + 1, CHUNK_WIDTH - 1)
                                : Blocks::AIR;
        neighbor.init(blk, topBlk);

        static auto logN = false;
        if (blk == 0 && topBlk == 0) {
            int sum = 0;
            for (int i = 0; i < 64; i++) sum += northData->getBlock(x, i, CHUNK_WIDTH - 1);

            if (sum == 0 && !logN) {
                std::cout << "[DEBUG] CRITICAL: NorthNeighbor appears to be EMPTY (AIR) below water level! Ptr=" <<
                        northData.get() << std::endl;
                logN = true;
            }
        }
    } else {
        static auto logN = false;
        if (!logN) {
            std::cout << "[DEBUG] North Neighbor PTR is NULL!" << std::endl;
            logN = true;
        }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchSouthNeighbor(NeighborData &neighbor, const int x, const int y, const int z,
                               const std::shared_ptr<ChunkData> &chunkData,
                               const std::shared_ptr<ChunkData> &southData,
                               const std::shared_ptr<ChunkData> &upData) {
    if (z < CHUNK_WIDTH - 1) {
        const auto blk = chunkData->getBlock(x, y, z + 1);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? chunkData->getBlock(x, y + 1, z + 1)
                                : upData->getBlock(x, 0, z + 1);
        neighbor.init(blk, topBlk);
    } else if (southData) {
        const auto blk = southData->getBlock(x, y, 0);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? southData->getBlock(x, y + 1, 0)
                                : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static auto logS = false;
        if (!logS) {
            std::cout << "[DEBUG] South Neighbor PTR is NULL!" << std::endl;
            logS = true;
        }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchWestNeighbor(NeighborData &neighbor, const int x, const int y, const int z,
                              const std::shared_ptr<ChunkData> &chunkData,
                              const std::shared_ptr<ChunkData> &westData,
                              const std::shared_ptr<ChunkData> &upData) {
    if (x > 0) {
        const auto blk = chunkData->getBlock(x - 1, y, z);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? chunkData->getBlock(x - 1, y + 1, z)
                                : upData->getBlock(x - 1, 0, z);
        neighbor.init(blk, topBlk);
    } else if (westData) {
        const auto blk = westData->getBlock(CHUNK_WIDTH - 1, y, z);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? westData->getBlock(CHUNK_WIDTH - 1, y + 1, z)
                                : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static auto logW = false;
        if (!logW) {
            std::cout << "[DEBUG] West Neighbor PTR is NULL!" << std::endl;
            logW = true;
        }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}

inline void fetchEastNeighbor(NeighborData &neighbor, int x, int y, int z,
                              const std::shared_ptr<ChunkData> &chunkData,
                              const std::shared_ptr<ChunkData> &eastData,
                              const std::shared_ptr<ChunkData> &upData) {
    if (x < CHUNK_WIDTH - 1) {
        const auto blk = chunkData->getBlock(x + 1, y, z);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? chunkData->getBlock(x + 1, y + 1, z)
                                : upData->getBlock(x + 1, 0, z);
        neighbor.init(blk, topBlk);
    } else if (eastData) {
        const auto blk = eastData->getBlock(0, y, z);
        const auto topBlk = (y + 1 < CHUNK_HEIGHT)
                                ? eastData->getBlock(0, y + 1, z)
                                : Blocks::AIR;
        neighbor.init(blk, topBlk);
    } else {
        static auto logE = false;
        if (!logE) {
            std::cout << "[DEBUG] East Neighbor PTR is NULL!" << std::endl;
            logE = true;
        }
        neighbor.init(Blocks::AIR, Blocks::AIR);
    }
}
