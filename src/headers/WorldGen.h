#pragma once

#include <vector>
#include "NoiseSettings.h"
#include "SurfaceFeature.h"
#include "../Chunk/headers/ChunkPos.h"

namespace WorldGen
{
	void generateChunkData(ChunkPos chunkPos, uint32_t* chunkData, long seed);
}