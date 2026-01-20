#pragma once

#include <cstdint>
#include "ChunkPos.h"
#include "WorldConstants.h"


struct ChunkData {
    uint8_t *data;

    explicit ChunkData(uint8_t *data);

    ~ChunkData();

    [[nodiscard]] static int getIndex(const int x, const int y, const int z) {
        return x * CHUNK_WIDTH * CHUNK_HEIGHT + z * CHUNK_HEIGHT + y;
    }

    [[nodiscard]] static int getIndex(const ChunkPos localBlockPos) {
        return localBlockPos.x * CHUNK_WIDTH * CHUNK_HEIGHT + localBlockPos.z * CHUNK_HEIGHT + localBlockPos.y;
    }

    uint16_t getBlock(const ChunkPos blockPos) const {
        return data[getIndex(blockPos)];
    }

    uint16_t getBlock(const int x, const int y, const int z) const {
        // Safety check for debugging (can be removed for release)
        // if (x < 0 || x >= CHUNK_WIDTH || z < 0 || z >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT) return 0;
        return data[getIndex(x, y, z)];
    }

    void setBlock(const int x, const int y, const int z, const uint8_t block) const {
        data[getIndex(x, y, z)] = block;
    }
};
