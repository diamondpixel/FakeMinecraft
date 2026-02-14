#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"
#include "../headers/Blocks.h"
#include <random>

/**
 * BigTreeFeature - Generates large oak-like trees with branches.
 */
class BigTreeFeature : public Feature {
public:
    uint8_t logBlock;
    uint8_t leafBlock;
    float spawnChance;

    BigTreeFeature(uint8_t log, uint8_t leaf, float chance = 0.001f)
        : logBlock(log), leafBlock(leaf), spawnChance(chance) {}

    float getSpawnChance() const override { return spawnChance; }

    bool canPlace(const uint8_t* chunkData, int localX, int localY, int localZ) const override {
        if (localY >= CHUNK_HEIGHT - 12) return false;
        if (localX < 2 || localX > CHUNK_WIDTH - 3 || localZ < 2 || localZ > CHUNK_WIDTH - 3) return false;

        int idx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY - 1);
        return (chunkData[idx] == Blocks::GRASS_BLOCK || chunkData[idx] == Blocks::DIRT);
    }

    bool place(uint8_t* chunkData, 
               int localX, int localY, int localZ,
               int worldX, int worldZ, 
               uint64_t seed) override {
        
        std::mt19937 rng(seed ^ (worldX * 73856093) ^ (worldZ * 19349663));
        std::uniform_int_distribution<int> heightDist(8, 12);
        int height = heightDist(rng);

        // Place main trunk
        for (int h = 0; h < height; ++h) {
            int idx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY + h);
            chunkData[idx] = logBlock;
        }

        // Place canopy (large sphere-ish)
        int canopyY = localY + height - 2;
        int radius = 3;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dz = -radius; dz <= radius; ++dz) {
                    if (dx * dx + dy * dy + dz * dz <= radius * radius + 1) {
                        int px = localX + dx;
                        int py = canopyY + dy;
                        int pz = localZ + dz;

                        if (px >= 0 && px < CHUNK_WIDTH && pz >= 0 && pz < CHUNK_WIDTH && py >= 0 && py < CHUNK_HEIGHT) {
                            int idx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + py;
                            if (chunkData[idx] == Blocks::AIR) {
                                chunkData[idx] = leafBlock;
                            }
                        }
                    }
                }
            }
        }

        // Add some branches/structure
        for (int i = 0; i < 3; ++i) {
            int bh = height / 2 + (rng() % 3);
            int dirX = (rng() % 3) - 1;
            int dirZ = (rng() % 3) - 1;
            if (dirX == 0 && dirZ == 0) continue;

            for (int len = 1; len <= 2; ++len) {
                int px = localX + dirX * len;
                int py = localY + bh + len;
                int pz = localZ + dirZ * len;
                if (px >= 0 && px < CHUNK_WIDTH && pz >= 0 && pz < CHUNK_WIDTH && py >= 0 && py < CHUNK_HEIGHT) {
                    int idx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + py;
                    chunkData[idx] = logBlock;
                }
            }
        }

        return true;
    }
};
