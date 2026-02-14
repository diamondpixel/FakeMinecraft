#pragma once

#include <cstdint>
#include <vector>
#include "Blocks.h"

// Forward declaration
class Chunk;

/**
 * Abstract base class for world generation features (trees, ores, flowers, etc).
 * Features are placed during the "Feature Pass" of world generation.
 *
 * PERFORMANCE NOTES:
 * - All virtual methods are marked noexcept for zero-cost exception handling
 * - Derived classes should mark overrides as final where possible for devirtualization
 * - canPlace() and getSpawnChance() are hot path - keep implementations lightweight
 */
class Feature {
public:
    virtual ~Feature() noexcept = default;

    // Prevent copying (features are typically managed by unique_ptr/shared_ptr)
    Feature(const Feature&) = delete;
    Feature& operator=(const Feature&) = delete;

    // Allow moving
    Feature(Feature&&) noexcept = default;
    Feature& operator=(Feature&&) noexcept = default;

protected:
    // Protected default constructor - only derived classes can construct
    Feature() noexcept = default;

public:
    /**
     * Attempts to place this feature at the given world position.
     * The feature may choose not to place if conditions are not met.
     *
     * PERFORMANCE: This is a hot path method called frequently during generation.
     * Implementations should:
     * - Minimize allocations
     * - Use efficient index calculations
     * - Early-exit on failed conditions
     * - Mark override as 'noexcept' and consider 'final' if not further derived
     *
     * @param chunkData Raw block data array for the chunk (non-null, size >= CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT)
     * @param localX Local X coordinate within chunk [0, CHUNK_WIDTH-1]
     * @param localY Local Y coordinate within chunk [0, CHUNK_HEIGHT-1]
     * @param localZ Local Z coordinate within chunk [0, CHUNK_WIDTH-1]
     * @param worldX World X coordinate (used for seeding/randomization)
     * @param worldZ World Z coordinate (used for seeding/randomization)
     * @param seed Base seed for randomization (combine with world coords for uniqueness)
     * @return true if feature was successfully placed, false otherwise
     */
    [[nodiscard]] virtual bool place(uint8_t* chunkData,
                                      int localX, int localY, int localZ,
                                      int worldX, int worldZ,
                                      uint64_t seed) noexcept = 0;

    /**
     * Check if this feature can be placed at the given position.
     * Called before place() to validate placement conditions.
     *
     * PERFORMANCE: Called very frequently during feature generation.
     * Implementations should:
     * - Be as fast as possible (simple checks only)
     * - Avoid complex calculations
     * - Early-exit on first failed condition
     * - Mark override as 'noexcept' and 'final' where possible
     *
     * @param chunkData Raw block data array for the chunk (non-null, read-only)
     * @param localX Local X coordinate within chunk [0, CHUNK_WIDTH-1]
     * @param localY Local Y coordinate within chunk [0, CHUNK_HEIGHT-1]
     * @param localZ Local Z coordinate within chunk [0, CHUNK_WIDTH-1]
     * @return true if feature can be placed at this position, false otherwise
     */
    [[nodiscard]] virtual bool canPlace(const uint8_t* chunkData,
                                         int localX, int localY, int localZ) const noexcept {
        // Default: always allow placement
        // Suppress unused parameter warnings without runtime cost
        (void)chunkData;
        (void)localX;
        (void)localY;
        (void)localZ;
        return true;
    }

    /**
     * Get the probability (0.0 to 1.0) of this feature attempting to spawn per column.
     *
     * PERFORMANCE: Called once per feature per chunk column during generation.
     * Should be a simple getter returning a cached value.
     * Mark override as 'noexcept', 'const', and 'final' where possible.
     *
     * @return Spawn probability in range [0.0, 1.0]
     *         0.0 = never spawns
     *         1.0 = always attempts to spawn (still subject to canPlace checks)
     */
    [[nodiscard]] virtual float getSpawnChance() const noexcept = 0;
};

/**
 * Helper macro for defining final feature classes with optimal devirtualization.
 * Use when a feature class will not be further derived from.
 *
 * Example usage:
 * class MyFeature final : public Feature {
 *     bool place(...) noexcept override final { ... }
 *     bool canPlace(...) const noexcept override final { ... }
 *     float getSpawnChance() const noexcept override final { ... }
 * };
 */
#define FEATURE_FINAL_OVERRIDES \
    [[nodiscard]] bool place(uint8_t* chunkData, \
                             int localX, int localY, int localZ, \
                             int worldX, int worldZ, \
                             uint64_t seed) noexcept override final; \
    [[nodiscard]] bool canPlace(const uint8_t* chunkData, \
                                 int localX, int localY, int localZ) const noexcept override final; \
    [[nodiscard]] float getSpawnChance() const noexcept override final