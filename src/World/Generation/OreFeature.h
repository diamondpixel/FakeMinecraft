#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"
#include <random>

/**
 * Ore feature - places clusters of ore blocks underground.
 */
class OreFeature : public Feature {
public:
    uint8_t oreBlock;
    uint8_t replaceBlock;  // Block type this ore can replace (usually stone)
    int minHeight;
    int maxHeight;
    int veinSize;
    float spawnChance;

    OreFeature(uint8_t ore, uint8_t replace,
               int minH, int maxH, int vein = 8, float chance = 0.1f)
        : oreBlock(ore), replaceBlock(replace),
          minHeight(minH), maxHeight(maxH),
          veinSize(vein), spawnChance(chance) {}

    float getSpawnChance() const override { return spawnChance; }

    bool canPlace(const uint8_t* chunkData,
                  int localX, int localY, int localZ) const override {
        // Only place in valid height range
        if (localY < minHeight || localY > maxHeight) return false;
        
        // Check if we're in stone
        int idx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + localY;
        return chunkData[idx] == replaceBlock;
    }

    bool place(uint8_t* chunkData, 
               int localX, int localY, int localZ,
               int worldX, int worldZ, 
               uint64_t seed) override {
        
        // Simple seeded random
        std::mt19937 rng(seed ^ (worldX * 73856093) ^ (worldZ * 19349663) ^ (localY * 83492791));
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        int placed = 0;
        
        // Place vein as a random blob
        for (int i = 0; i < veinSize * 2 && placed < veinSize; ++i) {
            int ox = static_cast<int>(dist(rng) * 2);
            int oy = static_cast<int>(dist(rng) * 2);
            int oz = static_cast<int>(dist(rng) * 2);
            
            int px = localX + ox;
            int py = localY + oy;
            int pz = localZ + oz;

            if (px < 0 || px >= CHUNK_WIDTH) continue;
            if (pz < 0 || pz >= CHUNK_WIDTH) continue;
            if (py < 0 || py >= CHUNK_HEIGHT) continue;

            int idx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + py;
            
            if (chunkData[idx] == replaceBlock) {
                chunkData[idx] = oreBlock;
                placed++;
            }
        }

        return placed > 0;
    }
};
