#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"

/**
 * Vegetation feature - places grass and flowers.
 */
class VegetationFeature final : public Feature {
public:
    const uint8_t plantBlock;
    const uint8_t topBlock;         ///< Optional second block for tall plants (0 = none)
    const uint8_t surfaceBlock;     ///< Block this plant grows on
    const float spawnChance;

    constexpr VegetationFeature(uint8_t plant, uint8_t surface, float chance, uint8_t top = 0) noexcept
        : plantBlock(plant), topBlock(top), surfaceBlock(surface), spawnChance(chance) {}

    [[nodiscard]] float getSpawnChance() const noexcept override {
        return spawnChance;
    }

    [[nodiscard]] bool canPlace(const uint8_t* chunkData,
                                 int localX, int localY, int localZ) const noexcept override {
        // Check if the plant's height is within the world bounds.
        if (localY <= 0 || localY >= CHUNK_HEIGHT - 2) {
            return false;
        }

        const int baseIdx = (localX * CHUNK_WIDTH + localZ) * CHUNK_HEIGHT;

        // Check block below matches required surface type
        if (chunkData[baseIdx + localY - 1] != surfaceBlock) {
            return false;
        }

        // Check current position is air
        return chunkData[baseIdx + localY] == Blocks::AIR;
    }

    [[nodiscard]] bool place(uint8_t* chunkData,
                              int localX, int localY, int localZ,
                              int worldX, int worldZ,
                              uint64_t seed) noexcept override {
        // Suppress unused parameter warnings
        (void)worldX;
        (void)worldZ;
        (void)seed;

        const int baseIdx = (localX * CHUNK_WIDTH + localZ) * CHUNK_HEIGHT;

        // Place primary plant block
        chunkData[baseIdx + localY] = plantBlock;

        // Place top block for tall plants
        if (topBlock != 0) {
            const int topY = localY + 1;
            if (topY < CHUNK_HEIGHT) {
                const int topIdx = baseIdx + topY;
                if (chunkData[topIdx] == Blocks::AIR) {
                    chunkData[topIdx] = topBlock;
                }
            }
        }

        return true;
    }
};