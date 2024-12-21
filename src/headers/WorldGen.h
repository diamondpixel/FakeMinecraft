#pragma once

#include <vector>
#include "NoiseSettings.h"
#include "SurfaceFeature.h"

namespace WorldGen
{
	void generateChunkData(int chunkX, int chunkY, int chunkZ, int chunkSize, std::vector<unsigned int>* chunkData);
}