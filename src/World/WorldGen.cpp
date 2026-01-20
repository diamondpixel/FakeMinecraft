#include "WorldGen.h"

#include <OpenSimplexNoise.hh>
#include <random>
#include "Blocks.h"
#include "Planet.h"
#include "ChunkPos.h"

namespace {
    // Cache line optimized for modern CPUs
    struct alignas(64) NoiseCache {
        int16_t surfaceHeight[CHUNK_WIDTH][CHUNK_WIDTH];
    };

    struct NoiseSettings {
        float frequency{};
        float amplitude{};
        float offset{};
        float chance = 0.0f;
        int minHeight = 0;
        int maxHeight = MAX_HEIGHT;
        uint8_t block = 0;
    };

    struct SurfaceFeature {
        NoiseSettings noiseSettings;
        uint8_t blocks[343]{}; // Max size: 7x7x7
        bool replaceBlock[343]{};
        int sizeX{};
        int sizeY{};
        int sizeZ{};
        int offsetX{};
        int offsetY{};
        int offsetZ{};
    };

    // Forward declarations
    void generateSurfaceFeatures(
        uint8_t *chunkData,
        const OSN::Noise<2> &noise2D,
        const OSN::Noise<3> &noise3D,
        const NoiseCache &cache,
        int startX, int startY, int startZ,
        const NoiseSettings *surfaceSettings
    );

    void placeFeatureBlocks(
        uint8_t *chunkData,
        const SurfaceFeature &feature,
        int worldX, int surfaceY, int worldZ,
        int startX, int startY, int startZ
    );

    template<int STEP>
    class TrilinearNoise {
        static constexpr int POINTS_X = CHUNK_WIDTH / STEP + 1;
        static constexpr int POINTS_Y = CHUNK_HEIGHT / STEP + 1;
        static constexpr int POINTS_Z = CHUNK_WIDTH / STEP + 1;
        float storage[POINTS_X][POINTS_Z][POINTS_Y] = {};

    public:
        void fill(const OSN::Noise<3> &noise, const int startX, const int startY, const int startZ,
                  const NoiseSettings &settings) {
            const float freq = settings.frequency;
            const float offset = settings.offset;
            const float amp = settings.amplitude;
            const int maxH = settings.maxHeight;
            const int minH = MIN_HEIGHT;

            // Loop reordering for better cache access (Y innermost)
            for (int x = 0; x < POINTS_X; ++x) {
                const int worldX = startX + x * STEP;
                const float nx = worldX * freq + offset;

                for (int z = 0; z < POINTS_Z; ++z) {
                    const int worldZ = startZ + z * STEP;
                    const float nz = worldZ * freq + offset;

                    for (int y = 0; y < POINTS_Y; ++y) {
                        const int worldY = startY + y * STEP;

                        // Early out for heights outside range
                        if (worldY > maxH + STEP || worldY < minH - STEP) {
                            storage[x][z][y] = -1.0f;
                            continue;
                        }

                        const float ny = worldY * freq + offset;
                        storage[x][z][y] = noise.eval(nx, ny, nz) * amp;
                    }
                }
            }
        }

        [[nodiscard]] float get(const int localX, const int localY, const int localZ) const {
            const int x0 = localX / STEP;
            const int y0 = localY / STEP;
            const int z0 = localZ / STEP;

            const float tx = static_cast<float>(localX % STEP) / STEP;
            const float ty = static_cast<float>(localY % STEP) / STEP;
            const float tz = static_cast<float>(localZ % STEP) / STEP;

            // Prefetch corners for better cache utilization
            const float c000 = storage[x0][z0][y0];
            const float c100 = storage[x0 + 1][z0][y0];
            const float c010 = storage[x0][z0 + 1][y0];
            const float c110 = storage[x0 + 1][z0 + 1][y0];
            const float c001 = storage[x0][z0][y0 + 1];
            const float c101 = storage[x0 + 1][z0][y0 + 1];
            const float c011 = storage[x0][z0 + 1][y0 + 1];
            const float c111 = storage[x0 + 1][z0 + 1][y0 + 1];

            // Optimized interpolation using FMA-friendly operations
            const float c00 = c000 + (c100 - c000) * tx;
            const float c01 = c001 + (c101 - c001) * tx;
            const float c10 = c010 + (c110 - c010) * tx;
            const float c11 = c011 + (c111 - c011) * tx;

            const float c0 = c00 + (c10 - c00) * tz;
            const float c1 = c01 + (c11 - c01) * tz;

            return c0 + (c1 - c0) * ty;
        }
    };

    [[nodiscard]] constexpr int getIndex3D(const int x, const int z, const int y) noexcept {
        return x * CHUNK_WIDTH * CHUNK_HEIGHT + z * CHUNK_HEIGHT + y;
    }

    // Optimized surface height computation
    [[nodiscard]] int computeSurfaceHeight(const OSN::Noise<2> &noise2D,
                                           const float worldX, const float worldZ,
                                           const NoiseSettings *settings
    ) {
        int height = 70;

        // Manual unroll for 2 settings (most common case)
        height += static_cast<int>(
            noise2D.eval(worldX * settings[0].frequency + settings[0].offset,
                         worldZ * settings[0].frequency + settings[0].offset) *
            settings[0].amplitude +
            noise2D.eval(worldX * settings[1].frequency + settings[1].offset,
                         worldZ * settings[1].frequency + settings[1].offset) *
            settings[1].amplitude
        );

        return height;
    }

    // Lookup table for terrain blocks (branchless)
    [[nodiscard]] uint8_t getTerrainBlock(const int worldY, const int surfaceY) noexcept {
        if (worldY == surfaceY) {
            return (surfaceY > WATER_LEVEL + 1) ? Blocks::GRASS_BLOCK : Blocks::SAND;
        }
        if (worldY > WATER_LEVEL - 5) {
            return (surfaceY > WATER_LEVEL + 1) ? Blocks::DIRT_BLOCK : Blocks::SAND;
        }
        if (worldY > MIN_HEIGHT) {
            return Blocks::STONE_BLOCK;
        }
        return (worldY == MIN_HEIGHT) ? Blocks::BEDROCK : Blocks::AIR;
    }

    // Optimized bounds checking
    [[nodiscard]] bool inChunkBounds(const int x, const int y, const int z) noexcept {
        return static_cast<unsigned>(x) < CHUNK_WIDTH &
               static_cast<unsigned>(y) < CHUNK_HEIGHT &
               static_cast<unsigned>(z) < CHUNK_WIDTH;
    }
}

// ============================================================================
// MAIN GENERATION FUNCTION - Optimized hot path
// ============================================================================
void WorldGen::generateChunkData(const ChunkPos chunkPos, uint8_t *chunkData, const long seed) {
    // Thread-local persistent noise objects (avoid reinitialization)
    thread_local struct {
        OSN::Noise<2> noise2D;
        OSN::Noise<3> noise3D;
        long cachedSeed = -1;
    } noiseState;

    // Only reinitialize on seed change
    if (noiseState.cachedSeed != seed) {
        noiseState.noise2D = OSN::Noise<2>(seed);
        noiseState.noise3D = OSN::Noise<3>(seed);
        noiseState.cachedSeed = seed;
    }

    // Const settings for compiler optimization
    static NoiseSettings surfaceSettings[] = {
        {0.01f, 20.0f, 0},
        {0.05f, 3.0f, 0}
    };

    static NoiseSettings caveSettings[] = {
        {0.05f, 1.0f, 0, 0.5f, 0, 50}
    };

    static NoiseSettings oreSettings[] = {
        {0.1f, 1.0f, 8.54f, 0.5f, 0, 16, Blocks::DIAMOND_ORE}
    };

    static NoiseSettings lavaPocketSettings[] =
    {
        0.075f, 1.5f, 5.54f, 0.79f, 0, 20, Blocks::LAVA
    };

    // World coordinates
    const int startX = chunkPos.x * CHUNK_WIDTH;
    const int startY = chunkPos.y * CHUNK_HEIGHT;
    const int startZ = chunkPos.z * CHUNK_WIDTH;
    const int endY = startY + CHUNK_HEIGHT - 1;

    // -------------------------------------------------------------------------
    // OPTIMIZATION: Early exit for entire chunks
    // -------------------------------------------------------------------------
    if (startY > MAX_HEIGHT + 50) {
        std::memset(chunkData, 0, sizeof(uint8_t) * CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);
        return;
    }

    if (endY < MIN_HEIGHT) {
        std::memset(chunkData, 0, sizeof(uint8_t) * CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);
        return;
    }

    // -------------------------------------------------------------------------
    // STEP 1: Compute surface heights (cache-optimized)
    // -------------------------------------------------------------------------
    thread_local NoiseCache cache;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        const auto worldX = static_cast<float>(x + startX);
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const auto worldZ = static_cast<float>(z + startZ);
            cache.surfaceHeight[x][z] = static_cast<int16_t>(
                computeSurfaceHeight(noiseState.noise2D, worldX, worldZ,
                                     surfaceSettings)
            );
        }
    }

    // -------------------------------------------------------------------------
    // STEP 2: Generate 3D noise grids (STEP = 4 for better detail)
    // -------------------------------------------------------------------------
    constexpr int STEP = 4;
    thread_local TrilinearNoise<STEP> caveNoise;
    thread_local TrilinearNoise<STEP> oreNoise;
    thread_local TrilinearNoise<STEP> lavaNoise;

    caveNoise.fill(noiseState.noise3D, startX, startY, startZ, caveSettings[0]);
    oreNoise.fill(noiseState.noise3D, startX, startY, startZ, oreSettings[0]);
    lavaNoise.fill(noiseState.noise3D, startX, startY, startZ, lavaPocketSettings[0]);

    // -------------------------------------------------------------------------
    // STEP 3: Generate base terrain (optimized loop order)
    // -------------------------------------------------------------------------
    constexpr uint8_t airBlock = Blocks::AIR;
    constexpr uint8_t waterBlock = Blocks::WATER;
    constexpr uint8_t bedrockBlock = Blocks::BEDROCK;
    const uint8_t oreBlock = oreSettings[0].block;
    const uint8_t lavaBlock = lavaPocketSettings[0].block;

    const float caveThreshold = caveSettings[0].chance;
    const float oreThreshold = oreSettings[0].chance;
    const float lavaThreshold = lavaPocketSettings[0].chance;
    const int oreMaxHeight = oreSettings[0].maxHeight;
    const int lavaMaxHeight = lavaPocketSettings[0].maxHeight;

    // Y-innermost for better cache locality
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const int surfaceY = cache.surfaceHeight[x][z];

            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                const int worldY = y + startY;
                const int index = getIndex3D(x, z, y);

                // Void below world
                if (worldY < MIN_HEIGHT) {
                    chunkData[index] = airBlock;
                    continue;
                }

                // Bedrock floor
                if (worldY == MIN_HEIGHT) {
                    chunkData[index] = bedrockBlock;
                    continue;
                }

                // Above surface
                if (worldY > surfaceY) {
                    chunkData[index] = (worldY <= WATER_LEVEL) ? waterBlock : airBlock;
                    continue;
                }

                // Cave check
                const float caveValue = caveNoise.get(x, y, z);
                if (caveValue > caveThreshold) {
                    chunkData[index] = (worldY == MIN_HEIGHT) ? bedrockBlock : airBlock;
                    continue;
                }

                // Ore check
                if (worldY <= oreMaxHeight && worldY >= MIN_HEIGHT + 2) {
                    const float oreValue = oreNoise.get(x, y, z);
                    if (oreValue > oreThreshold) {
                        chunkData[index] = oreBlock;
                        continue;
                    }
                }

                // Lava check
                if (worldY <= lavaMaxHeight && worldY >= MIN_HEIGHT + 2) {
                    const float lavaValue = lavaNoise.get(x, y, z);
                    if (lavaValue > lavaThreshold) {
                        chunkData[index] = lavaBlock;
                        continue;
                    }
                }

                // Default terrain
                chunkData[index] = getTerrainBlock(worldY, surfaceY);
            }
        }
    }

    // -------------------------------------------------------------------------
    // STEP 4: Surface Features (extracted to separate function)
    // -------------------------------------------------------------------------
    generateSurfaceFeatures(chunkData, noiseState.noise2D, noiseState.noise3D,
                            cache, startX, startY, startZ, surfaceSettings);
}

// ============================================================================
// ============================================================================
// SURFACE FEATURES - Separated for better code organization
// ============================================================================
namespace {
    void generateSurfaceFeatures(
        uint8_t *chunkData,
        const OSN::Noise<2> &noise2D,
        const OSN::Noise<3> &noise3D,
        const NoiseCache &cache,
        const int startX, const int startY, const int startZ,
        const NoiseSettings *surfaceSettings) {
        static const SurfaceFeature surfaceFeatures[] = {
            // Pond
            {
                {0.43f, 1.0f, 2.35f, 0.85f, 1, 0},
                {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 13, 0, 0,
                    0, 0, 13, 13, 13, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 2, 13, 13, 2, 0, 0, 0, 2, 13, 13, 13, 2, 0,
                    2, 13, 13, 13, 13, 13, 2, 2, 13, 13, 13, 13, 13, 2, 2, 13, 13, 13, 13, 13, 2,
                    0, 2, 13, 13, 13, 2, 0, 0, 0, 2, 13, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                },
                {
                    false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false,
                    false, false, false, true, true, false, false,
                    false, false, true, true, true, false, false,
                    false, false, false, true, false, false, false,
                    false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false,

                    false, false, true, true, false, false, false,
                    false, false, true, true, true, false, false,
                    false, true, true, true, true, true, false,
                    false, true, true, true, true, true, false,
                    false, true, true, true, true, true, false,
                    false, false, true, true, true, false, false,
                    false, false, false, true, false, false, false,

                    false, false, true, true, false, false, false,
                    false, false, true, true, true, true, false,
                    false, true, true, true, true, true, false,
                    false, true, true, true, true, true, false,
                    false, true, true, true, true, true, false,
                    false, false, true, true, true, false, false,
                    false, false, false, true, false, false, false,
                },
                7, 3, 7, -3, -2, -3
            },
            // Big Tree
            {
                {5.23f, 1.0f, 0.54f, 0.8f, 1, 0},
                {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
                    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0,
                    0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0,
                    0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0,
                    0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0,
                    0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 5, 5,
                    5, 5, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                    0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 0, 0, 5, 4, 4, 5, 0,
                    0, 5, 4, 4, 5, 0, 0, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 0, 0,
                    0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                },
                {
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    true, true, true, true, true, true, true, true, true, true, true, true,
                    true, true, true, true, true, true, true, true, true, true, true, true,
                    true, true, true, true, true, true, true, true, true, true, true, true,
                    false, false, false, false, false, false, false, true, true, true, true, false,
                    false, true, true, true, true, false, false, true, true, true, true, false,
                    false, true, true, true, true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                    false, false, true, true, false, false, false, false, true, true, false, false,
                    false, false, false, false, false, false, false, false, false, false, false, false,
                },
                6, 8, 6, -2, 0, -2
            },
            // Tree
            {
                {17.23f, 1.0f, 8.54f, 0.75f, 1, 0},
                {
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                    0, 5, 5, 5, 0, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 0, 5, 5, 5, 0,
                    0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 5, 5, 5, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 5, 5, 5, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
                },
                {
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, true, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, true, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false,
                },
                5, 7, 5, -2, 0, -2
            },
            // Tall Grass
            {{1.23f, 1.0f, 4.34f, 0.4f, 1, 0}, {2, 7, 8}, {false, false, false}, 1, 3, 1, 0, 0, 0},
            // Grass
            {{2.65f, 1.0f, 8.54f, 0.3f, 1, 0}, {2, 6}, {false, false}, 1, 2, 1, 0, 0, 0},
            // Poppy
            {{5.32f, 1.0f, 3.67f, 0.2f, 1, 0}, {2, 9}, {false, false}, 1, 2, 1, 0, 0, 0},
            // White Tulip
            {{5.57f, 1.0f, 7.654f, 0.2f, 1, 0}, {2, 10}, {false, false}, 1, 2, 1, 0, 0, 0},
            // Pink Tulip
            {{4.94f, 1.0f, 2.23f, 0.2f, 1, 0}, {2, 11}, {false, false}, 1, 2, 1, 0, 0, 0},
            // Orange Tulip
            {{6.32f, 1.0f, 8.2f, 0.2f, 1, 0}, {2, 12}, {false, false}, 1, 2, 1, 0, 0, 0},
        };

        static const NoiseSettings caveSettings[] = {
            {0.05f, 1.0f, 0, 0.5f, 0, MAX_HEIGHT - 10}
        };

        for (const auto &feature: surfaceFeatures) {
            // Precompute iteration bounds
            const int xStart = -feature.sizeX - feature.offsetX;
            const int xEnd = CHUNK_WIDTH - feature.offsetX;
            const int zStart = -feature.sizeZ - feature.offsetZ;
            const int zEnd = CHUNK_WIDTH - feature.offsetZ;

            for (int x = xStart; x < xEnd; ++x) {
                const int worldX = x + startX;
                const auto fWorldX = static_cast<float>(worldX);

                for (int z = zStart; z < zEnd; ++z) {
                    const int worldZ = z + startZ;
                    const auto fWorldZ = static_cast<float>(worldZ);

                    // Get surface height from cache or recompute
                    int surfaceY;
                    if (x >= 0 && x < CHUNK_WIDTH && z >= 0 && z < CHUNK_WIDTH) {
                        surfaceY = cache.surfaceHeight[x][z];
                    } else {
                        surfaceY = computeSurfaceHeight(noise2D, fWorldX, fWorldZ, surfaceSettings);
                    }

                    // Early exits
                    if (surfaceY + feature.offsetY > startY + CHUNK_HEIGHT ||
                        surfaceY + feature.sizeY + feature.offsetY < startY ||
                        surfaceY < WATER_LEVEL + 2) {
                        continue;
                    }

                    // Cave check
                    const float caveCheck = noise3D.eval(
                                                fWorldX * caveSettings[0].frequency + caveSettings[0].offset,
                                                static_cast<float>(surfaceY) * caveSettings[0].frequency + caveSettings[
                                                    0].
                                                offset,
                                                fWorldZ * caveSettings[0].frequency + caveSettings[0].offset
                                            ) * caveSettings[0].amplitude;

                    if (caveCheck > caveSettings[0].chance) {
                        continue;
                    }

                    // Feature noise threshold
                    const float featureNoise = noise2D.eval(
                        fWorldX * feature.noiseSettings.frequency + feature.noiseSettings.offset,
                        fWorldZ * feature.noiseSettings.frequency + feature.noiseSettings.offset
                    );

                    if (featureNoise <= feature.noiseSettings.chance) {
                        continue;
                    }

                    // Place feature blocks
                    placeFeatureBlocks(chunkData, feature, worldX, surfaceY, worldZ,
                                       startX, startY, startZ);
                }
            }
        }
    }

    // ============================================================================
    // FEATURE BLOCK PLACEMENT - Extracted for better optimization
    // ============================================================================
    void placeFeatureBlocks(
        uint8_t *chunkData,
        const SurfaceFeature &feature,
        const int worldX, const int surfaceY, const int worldZ,
        const int startX, const int startY, const int startZ) {
        for (int fX = 0; fX < feature.sizeX; ++fX) {
            for (int fY = 0; fY < feature.sizeY; ++fY) {
                for (int fZ = 0; fZ < feature.sizeZ; ++fZ) {
                    const int localX = worldX + fX + feature.offsetX - startX;
                    const int localY = surfaceY + fY + feature.offsetY - startY;
                    const int localZ = worldZ + fZ + feature.offsetZ - startZ;

                    // Optimized bounds check
                    if (!inChunkBounds(localX, localY, localZ)) {
                        continue;
                    }

                    const int featureIndex = fY * feature.sizeX * feature.sizeZ +
                                             fX * feature.sizeZ + fZ;
                    const int localIndex = getIndex3D(localX, localZ, localY);

                    // Place block if replacement allowed or position is air
                    if (feature.replaceBlock[featureIndex] || chunkData[localIndex] == 0) {
                        chunkData[localIndex] = feature.blocks[featureIndex];
                    }
                }
            }
        }
    }
}
