#include "headers/ChunkData.h"
#include "../headers/Planet.h"

ChunkData::ChunkData(uint16_t* data)
    : data(data)
{

}

ChunkData::~ChunkData()
{
    delete[] data;
}

inline int ChunkData::getIndex(int x, int y, int z) const
{
    return x * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + y;
}

inline int ChunkData::getIndex(ChunkPos localBlockPos) const
{
    return localBlockPos.x * CHUNK_SIZE * CHUNK_SIZE + localBlockPos.z * CHUNK_SIZE + localBlockPos.y;
}

uint16_t ChunkData::getBlock(ChunkPos blockPos)
{
    return data[getIndex(blockPos)];
}

uint16_t ChunkData::getBlock(int x, int y, int z)
{
    return data[getIndex(x, y, z)];
}

void ChunkData::setBlock(int x, int y, int z, uint16_t block)
{
    data[getIndex(x, y, z)] = block;
}