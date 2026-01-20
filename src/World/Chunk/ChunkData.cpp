#include "ChunkData.h"
#include "Planet.h"

ChunkData::ChunkData(uint8_t* data)
    : data(data)
{

}

ChunkData::~ChunkData()
{
    delete[] data;
}

inline int ChunkData::getIndex(int x, int y, int z) const
{
    return x * CHUNK_WIDTH * CHUNK_HEIGHT + z * CHUNK_HEIGHT + y;
}

inline int ChunkData::getIndex(ChunkPos localBlockPos) const
{
    return localBlockPos.x * CHUNK_WIDTH * CHUNK_HEIGHT + localBlockPos.z * CHUNK_HEIGHT + localBlockPos.y;
}

uint16_t ChunkData::getBlock(ChunkPos blockPos)
{
    return data[getIndex(blockPos)];
}

uint16_t ChunkData::getBlock(int x, int y, int z)
{
    return data[getIndex(x, y, z)];
}

void ChunkData::setBlock(int x, int y, int z, uint8_t block)
{
    data[getIndex(x, y, z)] = block;
}