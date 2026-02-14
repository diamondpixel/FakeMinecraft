#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"
#include "../headers/Blocks.h"
#include <random>
#include <cmath>

/**
 * LakeFeature - Carves out a depression and fills it with a liquid.
 */
class LakeFeature : public Feature {
public:
    uint8_t liquidBlock;
    uint8_t surfaceBlock; // Usually grass or sand
    float spawnChance;
    int radius;

    LakeFeature(uint8_t liquid, uint8_t surface, int rad = 4, float chance = 0.001f)
        : liquidBlock(liquid), surfaceBlock(surface), radius(rad), spawnChance(chance) {}

    float getSpawnChance() const override { return spawnChance; }

    bool canPlace(const uint8_t* chunkData, int localX, int localY, int localZ) const override {
        // Range check to prevent out of bounds
        if (localX <= 1 || localX >= CHUNK_WIDTH - 2 || localZ <= 1 || localZ >= CHUNK_WIDTH - 2 ||
            localY <= 5 || localY >= CHUNK_HEIGHT - radius - 2) return false;
        
        // Foundation Check: Ensure we have a solid 3x3 base of something solid
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                int idx = (localX + dx) * CHUNK_WIDTH * CHUNK_HEIGHT + (localZ + dz) * CHUNK_HEIGHT + (localY - 1);
                if (chunkData[idx] == Blocks::AIR) return false;
            }
        }
        
        int centerIdx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY - 1);
        return chunkData[centerIdx] == surfaceBlock || chunkData[centerIdx] == Blocks::STONE;
    }

    bool place(uint8_t* chunkData, 
               int localX, int localY, int localZ,
               int worldX, int worldZ, 
               uint64_t seed) override {
        
        // Carve out a bowl shape
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dy = -radius / 2; dy <= 2; ++dy) {
                    float distSq = static_cast<float>(dx * dx + dz * dz + dy * dy * 4);
                    if (distSq <= static_cast<float>(radius * radius)) {
                        int px = localX + dx;
                        int py = localY + dy;
                        int pz = localZ + dz;

                        if (px >= 0 && px < CHUNK_WIDTH && pz >= 0 && pz < CHUNK_WIDTH && py >= 0 && py < CHUNK_HEIGHT) {
                            int idx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + py;
                            
                            if (dy <= 0) {
                                chunkData[idx] = liquidBlock;
                                // Place stone/dirt underneath to avoid floating lakes
                                for (int fY = py - 1; fY >= std::max(0, py - 5); --fY) {
                                    int fIdx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + fY;
                                    if (chunkData[fIdx] == Blocks::AIR) {
                                        chunkData[fIdx] = (liquidBlock == Blocks::LAVA) ? Blocks::STONE : Blocks::DIRT;
                                    } else {
                                        break; // Hit something solid
                                    }
                                }
                            } else {
                                chunkData[idx] = Blocks::AIR;
                            }
                        }
                    }
                }
            }
        }
        return true;
    }
};
