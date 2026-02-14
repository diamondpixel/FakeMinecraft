#pragma once

#include <cstdint>
#include <vector>
#include "Blocks.h"

// Forward declaration
class Chunk;

/**
 * Abstract base class for world generation features (trees, ores, flowers, etc).
 * Features are placed during the "Feature Pass" of world generation.
 */
class Feature {
public:
    virtual ~Feature() = default;

    /**
     * Attempts to place this feature at the given world position.
     * The feature may choose not to place if conditions are not met.
     * 
     * @param chunkData Raw block data array for the chunk
     * @param localX Local X coordinate within chunk (0-31)
     * @param localY Local Y coordinate within chunk (0-63)
     * @param localZ Local Z coordinate within chunk (0-31)
     * @param worldX World X coordinate
     * @param worldZ World Z coordinate
     * @param seed Seed for randomization
     * @return true if feature was placed, false otherwise
     */
    virtual bool place(uint8_t* chunkData, 
                       int localX, int localY, int localZ,
                       int worldX, int worldZ, 
                       uint64_t seed) = 0;

    /**
     * Check if this feature can be placed at the given position.
     * Override to add custom placement rules (e.g., only on grass).
     */
    virtual bool canPlace(const uint8_t* chunkData,
                          int localX, int localY, int localZ) const {
        return true; // Default: can always place
    }

    /**
     * Get the probability (0.0 to 1.0) of this feature attempting to spawn per column.
     */
    virtual float getSpawnChance() const = 0;
};
