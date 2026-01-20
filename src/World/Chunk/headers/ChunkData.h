#pragma once

#include <cstdint>
#include "ChunkPos.h"


struct ChunkData
{
    uint8_t* data;


    explicit ChunkData(uint8_t* data);
    ~ChunkData();

    [[nodiscard]] inline int getIndex(int x, int y, int z) const;
    [[nodiscard]] inline int getIndex(ChunkPos localBlockPos) const;

    uint16_t getBlock(ChunkPos blockPos);
    uint16_t getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, uint8_t block);
};