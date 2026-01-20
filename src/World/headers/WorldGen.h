#pragma once

#include <cstdint>
#include "Planet.h"

struct ChunkPos;

class WorldGen {
public:
    // Main generation function
    static void generateChunkData(ChunkPos chunkPos, uint8_t *chunkData, long seed);
};
