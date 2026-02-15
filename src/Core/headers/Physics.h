#pragma once

#include <glm/glm.hpp>
#include "Planet.h"

/**
 * @namespace Physics
 * @brief Provides gravity, collision detection, and raycasting utilities for the voxel world.
 * 
 * This namespace contains the logic for how the player interacts with 
 * the blocks, including the math used to detect which block is being looked at.
 */
namespace Physics
{
	/**
	 * @struct RaycastResult
	 * @brief Comprehensive data structure returned by a raycast operation.
	 * 
	 * Contains both high-level state (hit/miss) and low-level coordinates required 
	 * for block modification (global vs local chunk coordinates).
	 */
	struct RaycastResult
	{
		/**
		 * @brief Indicates if the ray intersected a solid voxel.
		 * Surfaces like air and liquids are typically ignored by the core raycaster.
		 */
		bool hit;

		/**
		 * @brief The exact world-space position where the intersection occurred.
		 * Useful for placing effects, particles, or determining the hit face.
		 */
		glm::vec3 hitPos;

		/**
		 * @brief A raw pointer to the Chunk containing the hit voxel.
		 * Allows for direct block updates without re-traversing the Planet.
		 */
		Chunk* chunk;

		/**
		 * @name Global Coordinates
		 * Integer coordinates indicating the block's absolute position in the world.
		 * @{
		 */
		int blockX;
		int blockY;
		int blockZ;
		/** @} */

		/**
		 * @name Local Coordinates
		 * Integer coordinates within the specific chunk (range: 0 to CHUNK_WIDTH-1).
		 * @{
		 */
		int localBlockX;
		int localBlockY;
		int localBlockZ;
		/** @} */
			
		/**
		 * @brief Constructs a new RaycastResult.
		 */
		RaycastResult(bool hit, glm::vec3 hitPos, Chunk* chunk, int blockX, int blockY, int blockZ, int localBlockX, int localBlockY, int localBlockZ)
			: hit(hit), hitPos(hitPos), chunk(chunk), blockX(blockX), blockY(blockY), blockZ(blockZ), localBlockX(localBlockX), localBlockY(localBlockY), localBlockZ(localBlockZ)
		{}
	};

	/**
	 * The DDA algorithm is used because it checks every single block in the 
	 * ray's path, ensuring that small blocks aren't skipped over. 
	 * This makes it work better than simpler methods.
	 * 
	 * Logic Flow:
	 * 1. Normalize the direction and initialize the current voxel (mapPos).
	 * 2. Calculate the "tMax" parameters: the distance to the next X, Y, or Z voxel boundary.
	 * 3. Calculate "tDelta": the distance required to move one full voxel along each axis.
	 * 4. Step through the grid by incrementing the smallest tMax.
	 * 
	 * @param startPos The world-space origin of the ray.
	 * @param direction The look direction (automatically normalized).
	 * @param maxDistance The maximum search range in world units.
	 * @return A RaycastResult object containing hit details or miss state.
	 */
	RaycastResult raycast(const glm::vec3 &startPos, const glm::vec3 &direction, float maxDistance);

	/**
	 * @brief Obsolete constant for legacy raymarching.
	 * @deprecated Use Physics::raycast for pixel-perfect voxel intersection.
	 */
	static constexpr float RAY_STEP = 0.01f;
}
