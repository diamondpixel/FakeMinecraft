#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"

/**
 * Ore feature - places clusters of ore blocks underground.
 * Optimized for fast vein generation with minimal overhead.
 */
class OreFeature final : public Feature {
public:
    const uint8_t oreBlock;      ///< The block type to be placed in the cluster.
    const uint8_t replaceBlock;  ///< The block type this ore is allowed to replace (e.g., stone).
    const int minHeight;         ///< Minimum world-space Y-level for generation.
    const int maxHeight;         ///< Maximum world-space Y-level for generation.
    const int veinSize;          ///< Target number of blocks in a single cluster/vein.
    const float spawnChance;     ///< Probability of a cluster spawning at a potential location.

    constexpr OreFeature(uint8_t ore, uint8_t replace,
                         int minH, int maxH, int vein = 8, float chance = 0.1f) noexcept
        : oreBlock(ore), replaceBlock(replace),
          minHeight(minH), maxHeight(maxH),
          veinSize(vein), spawnChance(chance) {}

    [[nodiscard]] float getSpawnChance() const noexcept override {
        return spawnChance;
    }

    [[nodiscard]] bool canPlace(const uint8_t* chunkData,
                                 int localX, int localY, int localZ) const noexcept override {
        // Validate height range
        if (localY < minHeight || localY > maxHeight) {
            return false;
        }

        // Check if center position is replaceable
        const int idx = calculateIndex(localX, localZ, localY);
        return chunkData[idx] == replaceBlock;
    }

    [[nodiscard]] bool place(uint8_t* chunkData,
                              int localX, int localY, int localZ,
                              int worldX, int worldZ,
                              uint64_t seed) noexcept override {

        // Initialize fast RNG with hashed seed
        FastRNG rng(hashSeed(seed, worldX, worldZ, localY));

        int placed = 0;

        // Place vein as a random blob with optimized loop
        // Use a reasonable attempt limit to avoid infinite loops on edge cases
        const int maxAttempts = veinSize * 3;

        for (int attempt = 0; attempt < maxAttempts && placed < veinSize; ++attempt) {
            // Generate random offsets in range [-2, 2]
            const int ox = rng.nextOffset(2);
            const int oy = rng.nextOffset(2);
            const int oz = rng.nextOffset(2);

            const int px = localX + ox;
            const int py = localY + oy;
            const int pz = localZ + oz;

            // Bounds validation - early continue for out-of-bounds
            if (px < 0 || px >= CHUNK_WIDTH) continue;
            if (pz < 0 || pz >= CHUNK_WIDTH) continue;
            if (py < 0 || py >= CHUNK_HEIGHT) continue;

            const int idx = calculateIndex(px, pz, py);

            // Only replace target block type
            if (chunkData[idx] == replaceBlock) {
                chunkData[idx] = oreBlock;
                ++placed;
            }
        }

        return placed > 0;
    }

private:
    // Fast inline index calculation
    [[nodiscard]] static constexpr inline int calculateIndex(int x, int z, int y) noexcept {
        return (x * CHUNK_WIDTH + z) * CHUNK_HEIGHT + y;
    }

    // Lightweight random number generator for ore placement
    // Uses xorshift algorithm for speed and good distribution
    struct FastRNG {
        uint64_t state;

        explicit FastRNG(uint64_t seed) noexcept : state(seed) {
            // Ensure non-zero state
            if (state == 0) state = 1;
        }

        // Generate next random value using xorshift64*
        [[nodiscard]] inline uint64_t next() noexcept {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            return state * 0x2545F4914F6CDD1DULL;
        }

        // Generate random integer in range [-range, range]
        [[nodiscard]] inline int nextOffset(int range) noexcept {
            const uint64_t val = next();
            // Map to [0, 2*range] then shift to [-range, range]
            return static_cast<int>(val % (2 * range + 1)) - range;
        }
    };

    // Hash seed with world coordinates for unique vein generation
    [[nodiscard]] static inline uint64_t hashSeed(uint64_t seed, int worldX, int worldZ, int localY) noexcept {
        uint64_t hash = seed;
        hash ^= static_cast<uint64_t>(worldX) * 73856093ULL;
        hash ^= static_cast<uint64_t>(worldZ) * 19349663ULL;
        hash ^= static_cast<uint64_t>(localY) * 83492791ULL;
        hash ^= hash >> 33;
        hash *= 0xff51afd7ed558ccdULL;
        hash ^= hash >> 33;
        return hash;
    }
};