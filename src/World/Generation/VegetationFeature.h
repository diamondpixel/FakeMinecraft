#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"

/**
 * Vegetation feature - places simple 1-2 block tall plants (grass, flowers).
 */
class VegetationFeature : public Feature {
public:
    uint8_t plantBlock;
    uint8_t topBlock;         // Optional second block for tall plants (0 = none)
    uint8_t surfaceBlock;     // Block this plant grows on
    float spawnChance;

    VegetationFeature(uint8_t plant, uint8_t surface, float chance, uint8_t top = 0)
        : plantBlock(plant), topBlock(top), surfaceBlock(surface), spawnChance(chance) {}

    float getSpawnChance() const override { return spawnChance; }

    bool canPlace(const uint8_t* chunkData,
                  int localX, int localY, int localZ) const override {
        if (localY <= 0 || localY >= CHUNK_HEIGHT - 2) return false;

        // Check the block below
        int belowIdx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY - 1);
        if (chunkData[belowIdx] != surfaceBlock) return false;

        // Check current position is air
        int currentIdx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + localY;
        return chunkData[currentIdx] == Blocks::AIR;
    }

    bool place(uint8_t* chunkData, 
               int localX, int localY, int localZ,
               int worldX, int worldZ, 
               uint64_t seed) override {
        
        int idx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + localY;
        chunkData[idx] = plantBlock;

        // Place top block for tall plants
        if (topBlock != 0 && localY + 1 < CHUNK_HEIGHT) {
            int topIdx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY + 1);
            if (chunkData[topIdx] == Blocks::AIR) {
                chunkData[topIdx] = topBlock;
            }
        }

        return true;
    }
};
