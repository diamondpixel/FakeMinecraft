#include "WorldGen.h"

#include <OpenSimplexNoise.hh>
#include <random>
#include <cstring>
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

    // OPTIMIZATION: Precompute noise evaluations inline for surface height
    [[nodiscard]] int computeSurfaceHeight(const OSN::Noise<2> &noise2D,
                                           const float worldX, const float worldZ,
                                           const NoiseSettings *settings
    ) {
        // Direct computation with fused operations
        const float n0 = noise2D.eval(worldX * settings[0].frequency + settings[0].offset,
                                      worldZ * settings[0].frequency + settings[0].offset) * settings[0].amplitude;
        const float n1 = noise2D.eval(worldX * settings[1].frequency + settings[1].offset,
                                      worldZ * settings[1].frequency + settings[1].offset) * settings[1].amplitude;
        
        return 70 + static_cast<int>(n0 + n1);
    }

    // OPTIMIZATION: Branchless terrain block selection using lookup
    [[nodiscard]] uint8_t getTerrainBlock(const int worldY, const int surfaceY) noexcept {
        const bool isAboveSurface = worldY > surfaceY;
        const bool isSurfaceLayer = worldY == surfaceY;
        const bool isUpperLayer = worldY > WATER_LEVEL - 5;
        const bool isAboveWater = surfaceY > WATER_LEVEL + 1;
        const bool isMinHeight = worldY == MIN_HEIGHT;
        
        // Branchless selection
        const uint8_t surfaceBlock = isAboveWater ? Blocks::GRASS_BLOCK : Blocks::SAND;
        const uint8_t upperBlock = isAboveWater ? Blocks::DIRT_BLOCK : Blocks::SAND;
        const uint8_t deepBlock = isMinHeight ? Blocks::BEDROCK : Blocks::STONE_BLOCK;
        
        return isSurfaceLayer ? surfaceBlock : (isUpperLayer ? upperBlock : deepBlock);
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
    static constexpr NoiseSettings surfaceSettings[] = {
        {0.01f, 20.0f, 0},
        {0.05f, 3.0f, 0}
    };

    static constexpr NoiseSettings caveSettings[] = {
        {0.05f, 1.0f, 0, 0.5f, 0, 50}
    };

    static constexpr NoiseSettings oreSettings[] = {
        {0.1f, 1.0f, 8.54f, 0.5f, 0, 16, Blocks::DIAMOND_ORE}
    };

    static constexpr NoiseSettings lavaPocketSettings[] = {
        {0.075f, 1.5f, 5.54f, 0.79f, 0, 20, Blocks::LAVA}
    };

    // World coordinates
    const int startX = chunkPos.x * CHUNK_WIDTH;
    const int startY = chunkPos.y * CHUNK_HEIGHT;
    const int startZ = chunkPos.z * CHUNK_WIDTH;
    const int endY = startY + CHUNK_HEIGHT - 1;

    // -------------------------------------------------------------------------
    // OPTIMIZATION: Early exit for entire chunks
    // -------------------------------------------------------------------------
    if (startY > MAX_HEIGHT + 50 || endY < MIN_HEIGHT) {
        std::memset(chunkData, 0, CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);
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
                computeSurfaceHeight(noiseState.noise2D, worldX, worldZ, surfaceSettings)
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
    const int oreMinHeight = MIN_HEIGHT + 2;

    // OPTIMIZATION: Y-innermost for better cache locality + reduced branches
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const int surfaceY = cache.surfaceHeight[x][z];
            const int baseIndex = x * CHUNK_WIDTH * CHUNK_HEIGHT + z * CHUNK_HEIGHT;

            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                const int worldY = y + startY;
                const int index = baseIndex + y;

                // OPTIMIZATION: Combined early exit conditions
                if (worldY < MIN_HEIGHT) {
                    chunkData[index] = airBlock;
                    continue;
                }

                if (worldY == MIN_HEIGHT) {
                    chunkData[index] = bedrockBlock;
                    continue;
                }

                // Above surface
                if (worldY > surfaceY) {
                    chunkData[index] = (worldY <= WATER_LEVEL) ? waterBlock : airBlock;
                    continue;
                }

                // OPTIMIZATION: Single cave noise fetch
                const float caveValue = caveNoise.get(x, y, z);
                if (caveValue > caveThreshold) {
                    chunkData[index] = airBlock;
                    continue;
                }

                // OPTIMIZATION: Check ore/lava ranges once
                uint8_t blockType = 0;
                
                if (worldY <= oreMaxHeight && worldY >= oreMinHeight) {
                    const float oreValue = oreNoise.get(x, y, z);
                    if (oreValue > oreThreshold) {
                        blockType = oreBlock;
                    }
                }
                
                // OPTIMIZATION: Only check lava if ore wasn't placed
                if (blockType == 0 && worldY <= lavaMaxHeight && worldY >= oreMinHeight) {
                    const float lavaValue = lavaNoise.get(x, y, z);
                    if (lavaValue > lavaThreshold) {
                        blockType = lavaBlock;
                    }
                }

                // Set final block
                chunkData[index] = (blockType != 0) ? blockType : getTerrainBlock(worldY, surfaceY);
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

        static constexpr NoiseSettings caveSettings[] = {
            {0.05f, 1.0f, 0, 0.5f, 0, MAX_HEIGHT - 10}
        };

        // OPTIMIZATION: Precompute bounds to reduce redundant calculations
        const int chunkEndX = startX + CHUNK_WIDTH;
        const int chunkEndZ = startZ + CHUNK_WIDTH;
        const int chunkEndY = startY + CHUNK_HEIGHT;

        for (const auto &feature: surfaceFeatures) {
            // Precompute iteration bounds
            const int xStart = -feature.sizeX - feature.offsetX;
            const int xEnd = CHUNK_WIDTH - feature.offsetX;
            const int zStart = -feature.sizeZ - feature.offsetZ;
            const int zEnd = CHUNK_WIDTH - feature.offsetZ;
            
            // OPTIMIZATION: Precompute noise parameters
            const float freq = feature.noiseSettings.frequency;
            const float offset = feature.noiseSettings.offset;
            const float threshold = feature.noiseSettings.chance;
            const float caveFreq = caveSettings[0].frequency;
            const float caveOffset = caveSettings[0].offset;
            const float caveThreshold = caveSettings[0].chance;

            for (int x = xStart; x < xEnd; ++x) {
                const int worldX = x + startX;
                const auto fWorldX = static_cast<float>(worldX);
                const float noisePosX = fWorldX * freq + offset;
                const float cavePosX = fWorldX * caveFreq + caveOffset;

                for (int z = zStart; z < zEnd; ++z) {
                    const int worldZ = z + startZ;
                    const auto fWorldZ = static_cast<float>(worldZ);
                    const float noisePosZ = fWorldZ * freq + offset;
                    const float cavePosZ = fWorldZ * caveFreq + caveOffset;

                    // Get surface height from cache or recompute
                    int surfaceY;
                    if (x >= 0 && x < CHUNK_WIDTH && z >= 0 && z < CHUNK_WIDTH) {
                        surfaceY = cache.surfaceHeight[x][z];
                    } else {
                        surfaceY = computeSurfaceHeight(noise2D, fWorldX, fWorldZ, surfaceSettings);
                    }

                    // OPTIMIZATION: Combined early exit checks
                    const int featureTopY = surfaceY + feature.sizeY + feature.offsetY;
                    const int featureBottomY = surfaceY + feature.offsetY;
                    
                    if (featureTopY < startY || featureBottomY > chunkEndY || surfaceY < WATER_LEVEL + 2) {
                        continue;
                    }

                    // OPTIMIZATION: Single cave check with precomputed positions
                    const float caveCheck = noise3D.eval(
                        cavePosX,
                        static_cast<float>(surfaceY) * caveFreq + caveOffset,
                        cavePosZ
                    ) * caveSettings[0].amplitude;

                    if (caveCheck > caveThreshold) {
                        continue;
                    }

                    // OPTIMIZATION: Single feature noise check with precomputed positions
                    const float featureNoise = noise2D.eval(noisePosX, noisePosZ);

                    if (featureNoise <= threshold) {
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
        
        // OPTIMIZATION: Precompute common offsets
        const int baseX = worldX + feature.offsetX - startX;
        const int baseY = surfaceY + feature.offsetY - startY;
        const int baseZ = worldZ + feature.offsetZ - startZ;
        
        // Store for tight loop usage
        const int strideZ = feature.sizeZ;
        const int strideXZ = feature.sizeX * strideZ;

        // LOOP ORDER: X -> Z -> Y (Matches chunkData layout: Y-innermost)
        for (int fX = 0; fX < feature.sizeX; ++fX) {
            const int localX = baseX + fX;
            
            // Early X bounds check
            if (static_cast<unsigned>(localX) >= CHUNK_WIDTH) {
                continue;
            }
            
            const int chunkXOffset = localX * CHUNK_WIDTH * CHUNK_HEIGHT;
            const int fXOffset = fX * strideZ; // Offset in feature.blocks for X

            for (int fZ = 0; fZ < feature.sizeZ; ++fZ) {
                const int localZ = baseZ + fZ;

                // Early Z bounds check
                if (static_cast<unsigned>(localZ) >= CHUNK_WIDTH) {
                    continue;
                }
                
                // Base index for this X,Z column in chunkData
                const int colBaseIndex = chunkXOffset + localZ * CHUNK_HEIGHT;
                
                // Base index for this X,Z column in feature blocks
                const int featureColBase = fXOffset + fZ; // This is actually an index into 'X*StrideZ + Z'. 
                // Wait, feature array is [Y][X][Z] flattend? 
                // Access in original was: fY * sizeX * sizeZ + fX * sizeZ + fZ
                // So featureColBase needs to be just fX * sizeZ + fZ, but we need to add Y offset in inner loop.
                
                for (int fY = 0; fY < feature.sizeY; ++fY) {
                    const int localY = baseY + fY;

                    // Early Y bounds check
                    if (static_cast<unsigned>(localY) >= CHUNK_HEIGHT) {
                        continue;
                    }

                    const int localIndex = colBaseIndex + localY; // Sequential access!
                    const int featureIndex = fY * strideXZ + featureColBase;

                    // OPTIMIZATION: Branchless block placement
                    const uint8_t featureBlock = feature.blocks[featureIndex];
                    // Skip 0 blocks early to avoid read from chunkData if not needed (optional, depends on density of feature)
                    if (featureBlock == 0) continue;

                    const bool shouldPlace = feature.replaceBlock[featureIndex] || (chunkData[localIndex] == 0);
                    
                    if (shouldPlace) {
                        chunkData[localIndex] = featureBlock;
                    }
                }
            }
        }
    }
}