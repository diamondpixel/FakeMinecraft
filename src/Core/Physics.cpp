#include "Physics.h"
#include <iostream>
#include "Blocks.h"
#include "../World/headers/Planet.h"

Physics::RaycastResult Physics::raycast(const glm::vec3 startPos, const glm::vec3 direction, const float maxDistance)
{
	float currentDistance = 0;

	while (currentDistance < maxDistance)
	{
		currentDistance += RAY_STEP;
		if (currentDistance > maxDistance)
			currentDistance = maxDistance;

		// Get chunk
		glm::vec3 resultPos = startPos + direction * currentDistance;
		int chunkX = resultPos.x >= 0 ? resultPos.x / CHUNK_WIDTH : resultPos.x / CHUNK_WIDTH - 1;
		int chunkY = resultPos.y >= 0 ? resultPos.y / CHUNK_HEIGHT : resultPos.y / CHUNK_HEIGHT - 1;
		int chunkZ = resultPos.z >= 0 ? resultPos.z / CHUNK_WIDTH : resultPos.z / CHUNK_WIDTH - 1;
		Chunk* chunk = Planet::planet->getChunk(ChunkPos(chunkX, chunkY, chunkZ));
		if (chunk == nullptr)
			continue;

		// Get block pos
		int localBlockX = static_cast<int>(floor(resultPos.x)) - chunkX * CHUNK_WIDTH;
		int localBlockZ = static_cast<int>(floor(resultPos.z)) - chunkZ * CHUNK_WIDTH;
		int localBlockY = static_cast<int>(floor(resultPos.y)) - chunkY * CHUNK_HEIGHT;

		// Get block from chunk
		unsigned int block = chunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);

		// Get result pos
		int blockX = resultPos.x >= 0 ? static_cast<int>(resultPos.x) : static_cast<int>(resultPos.x) - 1;
		int blockY = resultPos.y >= 0 ? static_cast<int>(resultPos.y) : static_cast<int>(resultPos.y) - 1;
		int blockZ = resultPos.z >= 0 ? static_cast<int>(resultPos.z) : static_cast<int>(resultPos.z) - 1;

		// Return true if it hit a block
		if (block != 0 && Blocks::blocks[block].blockType != Block::LIQUID)
			return { true, resultPos, chunk,
			blockX, blockY, blockZ,
			localBlockX, localBlockY, localBlockZ};
	}

	return { false, glm::vec3(0), nullptr,
		0, 0, 0,
		0, 0, 0};
}