#pragma once

#include <cstdint>
#include "ChunkPos.h"


struct ChunkData
{
    uint32_t* data;


    ChunkData(uint32_t* data);
    ~ChunkData();

    inline int getIndex(int x, int y, int z) const;
    inline int getIndex(ChunkPos localBlockPos) const;

    uint16_t getBlock(ChunkPos blockPos);
    uint16_t getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, uint32_t block);
};