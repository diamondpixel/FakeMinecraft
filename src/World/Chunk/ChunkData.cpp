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

