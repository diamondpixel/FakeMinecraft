#include "Physics.h"
#include <iostream>
#include <cmath>
#include "Blocks.h"
#include "../World/headers/Planet.h"

/**
 * @file Physics.cpp
 * @brief This file handles physics and raycasting for our voxel world.
 */

namespace {
    /**
     * @brief A fast floor helper is used to avoid repeated calls to std::floor.
     */
    [[gnu::always_inline]]
    int fastFloor(float x) {
        return static_cast<int>(std::floor(x));
    }
    
    /**
     * @brief This helps us get the chunk index from world coordinates.
     * It handles negative coordinates correctly by using flooring.
     */
    [[gnu::always_inline, msvc::forceinline]]
    int worldToChunkCoord(float worldCoord, int chunkSize) {
        return fastFloor(worldCoord / static_cast<float>(chunkSize));
    }

    /**
     * @brief Extracts the fractional part of a float (always in range [0, 1)).
     */
    [[gnu::always_inline, msvc::forceinline]]
    float fract(float x) {
        return x - std::floor(x);
    }
}

/**
 * @brief The DDA algorithm is used for raycasting due to its efficiency with voxels.
 * 
 * This version is adjusted to handle:
 * 1. **Negative Coordinates**: floor() is used for consistency across coordinates.
 * 2. **Boundary Precision**: Distance to the next block boundary is calculated.
 * 3. **Chunk Caching**: Chunks are only re-queried when crossing into a new one.
 */
Physics::RaycastResult Physics::raycast(const glm::vec3 &startPos, const glm::vec3 &direction, const float maxDistance)
{
    // Ensure direction is unit length for predictable distance accumulation
    const glm::vec3 dir = glm::normalize(direction);

    // Initial voxel coordinate (integer grid address)
    glm::ivec3 mapPos(fastFloor(startPos.x), fastFloor(startPos.y), fastFloor(startPos.z));

    // Step direction along each axis: +1 or -1
    const glm::ivec3 step(
        dir.x >= 0 ? 1 : -1,
        dir.y >= 0 ? 1 : -1,
        dir.z >= 0 ? 1 : -1
    );

    /**
     * Distance to the next block boundary is calculated here.
     * The 0.0f case is handled to prevent infinite loops.
     */
    auto calcBoundaryDist = [](float pos, float dir_component) -> float {
        if (dir_component >= 0) {
            const float frac = fract(pos);
            return (frac == 0.0f) ? 1.0f : (1.0f - frac);
        } else {
            const float frac = fract(pos);
            return (frac == 0.0f) ? 1.0f : frac;
        }
    };

    // Distances to the first voxel boundaries
    const float boundaryX = calcBoundaryDist(startPos.x, dir.x);
    const float boundaryY = calcBoundaryDist(startPos.y, dir.y);
    const float boundaryZ = calcBoundaryDist(startPos.z, dir.z);

    // Inverse directions are pre-calculated to save time inside the loop.
    const float invDirX = (std::abs(dir.x) > 1e-6f) ? (1.0f / std::abs(dir.x)) : 1e6f;
    const float invDirY = (std::abs(dir.y) > 1e-6f) ? (1.0f / std::abs(dir.y)) : 1e6f;
    const float invDirZ = (std::abs(dir.z) > 1e-6f) ? (1.0f / std::abs(dir.z)) : 1e6f;

    // tMax represents the ray distance (t) to the next boundary for each axis
    glm::vec3 tMax(
        boundaryX * invDirX,
        boundaryY * invDirY,
        boundaryZ * invDirZ
    );

    // tDelta represents the constant distance (t) required to move one full voxel along an axis
    const glm::vec3 tDelta(invDirX, invDirY, invDirZ);

    // Initial chunk coordinate lookup
    int currentChunkX = worldToChunkCoord(static_cast<float>(mapPos.x), CHUNK_WIDTH);
    int currentChunkY = worldToChunkCoord(static_cast<float>(mapPos.y), CHUNK_HEIGHT);
    int currentChunkZ = worldToChunkCoord(static_cast<float>(mapPos.z), CHUNK_WIDTH);

    Chunk* currentChunk = Planet::planet->getChunk(ChunkPos(currentChunkX, currentChunkY, currentChunkZ));
    const BlockRegistry& blockRegistry = BlockRegistry::getInstance();

    // Iterate until maxDistance is exceeded or a collision is found
    const int maxIterations = static_cast<int>(maxDistance * 2.0f) + 16; // Heuristic safety limit
    float currentDistance = 0.0f;

    for (int iteration = 0; iteration < maxIterations && currentDistance < maxDistance; ++iteration) {
        
        // Checking if the ray moved into a new chunk since the last step.
        const int chunkX = worldToChunkCoord(static_cast<float>(mapPos.x), CHUNK_WIDTH);
        const int chunkY = worldToChunkCoord(static_cast<float>(mapPos.y), CHUNK_HEIGHT);
        const int chunkZ = worldToChunkCoord(static_cast<float>(mapPos.z), CHUNK_WIDTH);

        if (chunkX != currentChunkX || chunkY != currentChunkY || chunkZ != currentChunkZ) [[unlikely]] {
            currentChunkX = chunkX;
            currentChunkY = chunkY;
            currentChunkZ = chunkZ;
            currentChunk = Planet::planet->getChunk(ChunkPos(currentChunkX, currentChunkY, currentChunkZ));
        }

        // Only query voxels if the chunk is loaded/exists
        if (currentChunk != nullptr) [[likely]] {
            // Transform global mapPos to local chunk relative coordinates
            const int localBlockX = mapPos.x - currentChunkX * CHUNK_WIDTH;
            const int localBlockY = mapPos.y - currentChunkY * CHUNK_HEIGHT;
            const int localBlockZ = mapPos.z - currentChunkZ * CHUNK_WIDTH;

            // Safety check against CHUNK bounds
            if (localBlockX >= 0 && localBlockX < CHUNK_WIDTH &&
                localBlockY >= 0 && localBlockY < CHUNK_HEIGHT &&
                localBlockZ >= 0 && localBlockZ < CHUNK_WIDTH) [[likely]] {

                const unsigned int block = currentChunk->getBlockAtPos(localBlockX, localBlockY, localBlockZ);

                // Check for block solidity (ignore air and liquids)
                if (block != 0) [[unlikely]] {
                    const Block& blockData = blockRegistry.getBlock(block);
                    if (blockData.blockType != Block::LIQUID) {
                        
                        // Precise world-space hit position
                        const glm::vec3 resultPos = startPos + dir * currentDistance;

                        return {
                            true, resultPos, currentChunk,
                            mapPos.x, mapPos.y, mapPos.z,
                            localBlockX, localBlockY, localBlockZ
                        };
                    }
                }
            }
        }

        /**
         * @brief DDA Step logic.
         * The closest axis is selected, and the position moves one block in that direction.
         */
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                currentDistance = tMax.x;
                mapPos.x += step.x;
                tMax.x += tDelta.x;
            } else {
                currentDistance = tMax.z;
                mapPos.z += step.z;
                tMax.z += tDelta.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                currentDistance = tMax.y;
                mapPos.y += step.y;
                tMax.y += tDelta.y;
            } else {
                currentDistance = tMax.z;
                mapPos.z += step.z;
                tMax.z += tDelta.z;
            }
        }
    }

    // Default return: Ray exceeded range without hit
    return {
        false, glm::vec3(0), nullptr,
        0, 0, 0,
        0, 0, 0
    };
}
