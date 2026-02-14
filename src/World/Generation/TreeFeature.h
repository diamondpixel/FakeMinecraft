#pragma once

#include "Feature.h"
#include "../../Chunk/headers/Chunk.h"
#include "../headers/WorldConstants.h"
#include <random>

/**
 * Tree feature - places a tree at the given position.
 * Configurable trunk and leaf blocks, height, and canopy size.
 */
class TreeFeature : public Feature {
public:
    uint8_t logBlock;
    uint8_t leavesBlock;
    int minHeight;
    int maxHeight;
    int canopyRadius;
    float spawnChance;

    TreeFeature(uint8_t log, uint8_t leaves, 
                int minH = 4, int maxH = 7, 
                int canopy = 2, float chance = 0.02f)
        : logBlock(log), leavesBlock(leaves),
          minHeight(minH), maxHeight(maxH),
          canopyRadius(canopy), spawnChance(chance) {}

    float getSpawnChance() const override { return spawnChance; }

    bool canPlace(const uint8_t* chunkData,
                  int localX, int localY, int localZ) const override {
        // Check if we're on a valid surface block
        if (localY <= 0 || localY >= CHUNK_HEIGHT - maxHeight - 3) return false;
        
        // Get the block below (where tree will be planted)
        int belowIdx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY - 1);
        uint8_t belowBlock = chunkData[belowIdx];
        
        // Only place on grass or dirt
        return belowBlock == Blocks::GRASS_BLOCK || belowBlock == Blocks::DIRT;
    }

    bool place(uint8_t* chunkData, 
               int localX, int localY, int localZ,
               int worldX, int worldZ, 
               uint64_t seed) override {
        
        // Simple seeded random for height variation
        std::mt19937 rng(seed ^ (worldX * 73856093) ^ (worldZ * 19349663));
        std::uniform_int_distribution<int> heightDist(minHeight, maxHeight);
        int treeHeight = heightDist(rng);

        // Check bounds - make sure we can fit the tree
        if (localY + treeHeight + 2 >= CHUNK_HEIGHT) return false;
        if (localX < canopyRadius || localX >= CHUNK_WIDTH - canopyRadius) return false;
        if (localZ < canopyRadius || localZ >= CHUNK_WIDTH - canopyRadius) return false;

        // Place trunk
        for (int y = 0; y < treeHeight; ++y) {
            int idx = localX * CHUNK_WIDTH * CHUNK_HEIGHT + localZ * CHUNK_HEIGHT + (localY + y);
            chunkData[idx] = logBlock;
        }

        // Place leaves (simple sphere-ish pattern)
        int leafStartY = localY + treeHeight - 2;
        int leafEndY = localY + treeHeight + 1;

        for (int ly = leafStartY; ly <= leafEndY; ++ly) {
            int yOffset = ly - (localY + treeHeight);
            int radius = (yOffset < 0) ? canopyRadius : (canopyRadius - 1);
            if (radius < 1) radius = 1;

            for (int lx = -radius; lx <= radius; ++lx) {
                for (int lz = -radius; lz <= radius; ++lz) {
                    // Skip corners for rounder shape
                    if (abs(lx) == radius && abs(lz) == radius) continue;

                    int px = localX + lx;
                    int pz = localZ + lz;
                    int py = ly;

                    if (px < 0 || px >= CHUNK_WIDTH) continue;
                    if (pz < 0 || pz >= CHUNK_WIDTH) continue;
                    if (py < 0 || py >= CHUNK_HEIGHT) continue;

                    int idx = px * CHUNK_WIDTH * CHUNK_HEIGHT + pz * CHUNK_HEIGHT + py;
                    
                    // Only place leaves if air or existing leaves
                    if (chunkData[idx] == Blocks::AIR || chunkData[idx] == leavesBlock) {
                        chunkData[idx] = leavesBlock;
                    }
                }
            }
        }

        return true;
    }
};
