#pragma once

#include "Feature.h"
#include "../headers/WorldConstants.h"
#include "../headers/Blocks.h"
#include <algorithm>

/**
 * LakeFeature - Creates small pools of water or lava.
 */
class LakeFeature final : public Feature {
public:
    const uint8_t liquidBlock;
    const uint8_t surfaceBlock; ///< Usually grass or sand
    const int radius;
    const float spawnChance;

    // Help speed up calculations by pre-calculating some values.
    const int radiusSquared;
    const int halfRadius;

    constexpr LakeFeature(uint8_t liquid, uint8_t surface, int rad = 4, float chance = 0.001f) noexcept
        : liquidBlock(liquid), surfaceBlock(surface), radius(rad), spawnChance(chance),
          radiusSquared(rad * rad), halfRadius(rad / 2) {}

    [[nodiscard]] float getSpawnChance() const noexcept override {
        return spawnChance;
    }

    [[nodiscard]] bool canPlace(const uint8_t* chunkData,
                                 int localX, int localY, int localZ) const noexcept override {
        // Range check to prevent out of bounds
        if (localX <= 1 || localX >= CHUNK_WIDTH - 2 ||
            localZ <= 1 || localZ >= CHUNK_WIDTH - 2 ||
            localY <= 5 || localY >= CHUNK_HEIGHT - radius - 2) {
            return false;
        }

        // Make sure there is solid ground below before placing water.
        const int baseY = localY - 1;

        for (int dx = -1; dx <= 1; ++dx) {
            const int x = localX + dx;
            const int xBase = x * CHUNK_WIDTH * CHUNK_HEIGHT + baseY;

            for (int dz = -1; dz <= 1; ++dz) {
                const int idx = xBase + (localZ + dz) * CHUNK_HEIGHT;
                if (chunkData[idx] == Blocks::AIR) {
                    return false;
                }
            }
        }

        // Check center block is appropriate
        const int centerIdx = calculateIndex(localX, localZ, baseY);
        const uint8_t centerBlock = chunkData[centerIdx];
        return centerBlock == surfaceBlock || centerBlock == Blocks::STONE;
    }

    [[nodiscard]] bool place(uint8_t* chunkData,
                              int localX, int localY, int localZ,
                              int worldX, int worldZ,
                              uint64_t seed) noexcept override {
        // Suppress unused parameter warnings
        (void)worldX;
        (void)worldZ;
        (void)seed;

        const int minX = std::max(0, localX - radius);
        const int minZ = std::max(0, localZ - radius);
        const int minY = std::max(0, localY - halfRadius);
        const int maxX = std::min<int>(CHUNK_WIDTH - 1, localX + radius);
        const int maxZ = std::min<int>(CHUNK_WIDTH - 1, localZ + radius);
        const int maxY = std::min<int>(CHUNK_HEIGHT - 1, localY + 2);

        const uint8_t fillBlock = (liquidBlock == Blocks::LAVA) ? Blocks::STONE : Blocks::DIRT;

        for (int px = minX; px <= maxX; ++px) {
            const int dx = px - localX;
            const int dxSq = dx * dx;
            const int xBase = px * CHUNK_WIDTH * CHUNK_HEIGHT;

            for (int pz = minZ; pz <= maxZ; ++pz) {
                const int dz = pz - localZ;
                const int dzSq = dz * dz;
                const int dxzSq = dxSq + dzSq;
                const int xzBase = xBase + pz * CHUNK_HEIGHT;

                for (int py = minY; py <= maxY; ++py) {
                    const int dy = py - localY;
                    const int distSq = dxzSq + (dy * dy * 4);

                    if (distSq <= radiusSquared) {
                        const int idx = xzBase + py;

                        if (dy <= 0) {
                            chunkData[idx] = liquidBlock;
                            const int fillStartY = py - 1;
                            const int fillEndY = std::max(0, py - 5);

                            for (int fY = fillStartY; fY >= fillEndY; --fY) {
                                const int fIdx = xzBase + fY;
                                if (chunkData[fIdx] == Blocks::AIR) {
                                    chunkData[fIdx] = fillBlock;
                                } else {
                                    break;
                                }
                            }
                        } else {
                            chunkData[idx] = Blocks::AIR;
                        }
                    }
                }
            }
        }

        return true;
    }

private:
    [[nodiscard]] static constexpr inline int calculateIndex(int x, int z, int y) noexcept {
        return (x * CHUNK_WIDTH + z) * CHUNK_HEIGHT + y;
    }
};