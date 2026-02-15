/**
 * @file WorldGen.h
 * @brief Header for the terrain generation logic.
 */
#pragma once

#include <cstdint>
#include "Planet.h"

struct ChunkPos;

/**
 * @class WorldGen
 * @brief Class that handles creating the terrain layout based on a seed.
 */
class WorldGen {
public:
    // Main generation function
    static void generateChunkData(ChunkPos chunkPos, uint8_t *chunkData, long seed);
};
