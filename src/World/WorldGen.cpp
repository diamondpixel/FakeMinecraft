#include "WorldGen.h"

#include <OpenSimplexNoise.hh>
#include <random>
#include "Blocks.h"
#include "Planet.h"
#include "ChunkPos.h"
#include "Generation/BiomeRegistry.h"
#include "Generation/Feature.h"

namespace {
    // Cache line optimized for modern CPUs
    struct alignas(64) NoiseCache {
        int16_t surfaceHeight[CHUNK_WIDTH][CHUNK_WIDTH];
        uint8_t biomeID[CHUNK_WIDTH][CHUNK_WIDTH];
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

            for (int x = 0; x < POINTS_X; ++x) {
                const int worldX = startX + x * STEP;
                const float nx = worldX * freq + offset;

                for (int z = 0; z < POINTS_Z; ++z) {
                    const int worldZ = startZ + z * STEP;
                    const float nz = worldZ * freq + offset;

                    for (int y = 0; y < POINTS_Y; ++y) {
                        const int worldY = startY + y * STEP;

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

            const float c000 = storage[x0][z0][y0];
            const float c100 = storage[x0 + 1][z0][y0];
            const float c010 = storage[x0][z0 + 1][y0];
            const float c110 = storage[x0 + 1][z0 + 1][y0];
            const float c001 = storage[x0][z0][y0 + 1];
            const float c101 = storage[x0 + 1][z0][y0 + 1];
            const float c011 = storage[x0][z0 + 1][y0 + 1];
            const float c111 = storage[x0 + 1][z0 + 1][y0 + 1];

            const float c00 = c000 + (c100 - c000) * tx;
            const float c01 = c001 + (c101 - c001) * tx;
            const float c10 = c010 + (c110 - c010) * tx;
            const float c11 = c011 + (c111 - c011) * tx;

            const float c0 = c00 + (c10 - c00) * tz;
            const float c1 = c01 + (c11 - c01) * tz;

            return c0 + (c1 - c0) * ty;
        }
    };
}

// ============================================================================
// MAIN GENERATION FUNCTION - Biome-Driven 4-Pass Pipeline
// ============================================================================
void WorldGen::generateChunkData(const ChunkPos chunkPos, uint8_t *chunkData, const long seed) {
    // Thread-local persistent noise objects
    thread_local struct {
        OSN::Noise<2> terrainNoise;
        OSN::Noise<2> tempNoise;
        OSN::Noise<2> humidityNoise;
        OSN::Noise<2> continentalNoise; // New pass for land/sea
        OSN::Noise<3> noise3D;
        long cachedSeed = -1;
    } noise;

    // Only reinitialize on seed change
    if (noise.cachedSeed != seed) {
        noise.terrainNoise = OSN::Noise<2>(seed);
        noise.tempNoise = OSN::Noise<2>(seed + 1);
        noise.humidityNoise = OSN::Noise<2>(seed + 2);
        noise.continentalNoise = OSN::Noise<2>(seed + 4);
        noise.noise3D = OSN::Noise<3>(seed + 3);
        noise.cachedSeed = seed;
    }

    // World coordinates
    const int startX = chunkPos.x * CHUNK_WIDTH;
    const int startY = chunkPos.y * CHUNK_HEIGHT;
    const int startZ = chunkPos.z * CHUNK_WIDTH;
    const int endY = startY + CHUNK_HEIGHT - 1;

    // Early exit for chunks completely outside vertical bounds
    if (startY > MAX_HEIGHT + 64 || endY < 0) {
        std::memset(chunkData, 0, CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);
        return;
    }

    // -------------------------------------------------------------------------
    // PASS 1: Biome & Height Pass (Column-based)
    // -------------------------------------------------------------------------
    thread_local NoiseCache cache;
    auto& registry = BiomeRegistry::getInstance();

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        const float wx = static_cast<float>(startX + x);
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const float wz = static_cast<float>(startZ + z);

            // 1.1 Calculate climate noise (Temperature, Humidity, Continentalness)
            float temp = (noise.tempNoise.eval(wx * 0.001f, wz * 0.001f) + 1.0f) * 0.5f;
            float humid = (noise.humidityNoise.eval(wx * 0.001f, wz * 0.001f) + 1.0f) * 0.5f;
            float cont = noise.continentalNoise.eval(wx * 0.0005f, wz * 0.0005f); // Large scale

            const Biome& biome = registry.getBiome(temp, humid, cont);
            cache.biomeID[x][z] = biome.id;

            // 1.2 Calculate Terrain Height with Ultra-Smooth Blending (3x3 grid)
            float totalBaseHeight = 0.0f;
            float totalMultiplier = 0.0f;
            int samples = 0;
            
            const int blendRadius = 12; 
            for (int dx = -blendRadius; dx <= blendRadius; dx += blendRadius) {
                for (int dz = -blendRadius; dz <= blendRadius; dz += blendRadius) {
                    float sampleWX = wx + dx;
                    float sampleWZ = wz + dz;
                    
                    float sTemp = (noise.tempNoise.eval(sampleWX * 0.001f, sampleWZ * 0.001f) + 1.0f) * 0.5f;
                    float sHumid = (noise.humidityNoise.eval(sampleWX * 0.001f, sampleWZ * 0.001f) + 1.0f) * 0.5f;
                    float sCont = noise.continentalNoise.eval(sampleWX * 0.0005f, sampleWZ * 0.0005f);
                    
                    const Biome& b = registry.getBiome(sTemp, sHumid, sCont);
                    totalBaseHeight += b.baseHeight;
                    totalMultiplier += b.heightMultiplier;
                    samples++;
                }
            }

            float blendedBaseHeight = totalBaseHeight / samples;
            float blendedMultiplier = totalMultiplier / samples;

            // Base noise for terrain (Lower frequency = Broader mountains)
            float n = noise.terrainNoise.eval(wx * 0.005f, wz * 0.005f);
            
            // "Ridged" noise for peaks
            float ridgeN = 1.0f - std::abs(n);
            float blendedN = (ridgeN * 0.7f) + ((n + 1.0f) * 0.5f * 0.3f);
            float detailN = noise.terrainNoise.eval(wx * 0.05f, wz * 0.05f) * 0.1f;
            
            float height = blendedBaseHeight + (blendedN + detailN) * 50.0f * blendedMultiplier;
            
            // Ensure oceans stay submerged
            if (biome.id == 5 && height > WATER_LEVEL - 4) {
                height = WATER_LEVEL - 4 - (WATER_LEVEL - height) * 0.1f;
            }

            cache.surfaceHeight[x][z] = static_cast<int16_t>(height);
        }
    }

    // -------------------------------------------------------------------------
    // PASS 2: 3D Noise Preparation (Caves)
    // -------------------------------------------------------------------------
    static NoiseSettings caveSettings = {0.05f, 1.0f, 0, 0.5f, 0, MAX_HEIGHT - 10};
    constexpr int STEP = 4;
    thread_local TrilinearNoise<STEP> caveNoise;
    caveNoise.fill(noise.noise3D, startX, startY, startZ, caveSettings);

    // -------------------------------------------------------------------------
    // PASS 3: Volume Fill (Base terrain)
    // -------------------------------------------------------------------------
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const int surfaceY = cache.surfaceHeight[x][z];
            const Biome& biome = registry.getBiome(cache.biomeID[x][z]);
            const int baseIndex = x * CHUNK_WIDTH * CHUNK_HEIGHT + z * CHUNK_HEIGHT;

            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                const int worldY = y + startY;
                const int index = baseIndex + y;

                if (worldY <= 0) {
                    chunkData[index] = Blocks::BEDROCK;
                    continue;
                }

                if (worldY > surfaceY) {
                    chunkData[index] = (worldY <= WATER_LEVEL) ? Blocks::WATER : Blocks::AIR;
                    continue;
                }

                const float cv = caveNoise.get(x, y, z);
                if (cv > caveSettings.chance) {
                    chunkData[index] = Blocks::AIR;
                    continue;
                }

                if (worldY == surfaceY) {
                    chunkData[index] = (worldY >= WATER_LEVEL) ? biome.surfaceBlock : Blocks::SAND;
                } else if (worldY > surfaceY - 4) {
                    chunkData[index] = biome.underBlock;
                } else {
                    chunkData[index] = biome.deepBlock;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // PASS 4: Feature Pass (Decorations & Ores)
    // -------------------------------------------------------------------------
    std::mt19937 columnRng(seed ^ (static_cast<uint64_t>(chunkPos.x) * 341873128712) ^ 
                                (static_cast<uint64_t>(chunkPos.z) * 1328979838));

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        const int worldX = startX + x;
        for (int z = 0; z < CHUNK_WIDTH; ++z) {
            const int worldZ = startZ + z;
            const int surfaceY = cache.surfaceHeight[x][z];
            const Biome& biome = registry.getBiome(cache.biomeID[x][z]);

            // 4.1 Surface Decorations
            if (surfaceY >= startY && surfaceY < endY && surfaceY >= WATER_LEVEL) {
                const int localY = surfaceY - startY + 1;
                for (size_t i = 0; i < biome.features.size(); ++i) {
                    Feature* feature = biome.features[i];
                    uint64_t posHash = static_cast<uint64_t>(worldX) * 73856093 ^ 
                                       static_cast<uint64_t>(worldZ) * 19349663 ^ 
                                       seed ^ (static_cast<uint64_t>(i) * 1234567);
                    float prob = static_cast<float>(posHash % 10000) / 10000.0f;
                    if (prob < feature->getSpawnChance()) {
                        if (feature->canPlace(chunkData, x, localY, z)) {
                            feature->place(chunkData, x, localY, z, worldX, worldZ, seed);
                        }
                    }
                }
            }
        }
    }

    // 4.2 Distributed Underground Features (Ores & Pockets)
    auto tryPlaceOre = [&](OreFeature* ore) {
        if (!ore) return;
        int attempts = static_cast<int>(ore->getSpawnChance() * 1000); 
        for (int i = 0; i < attempts; ++i) {
            int rx = columnRng() % CHUNK_WIDTH;
            int rz = columnRng() % CHUNK_WIDTH;
            int ry_world = ore->minHeight + (columnRng() % (ore->maxHeight - ore->minHeight + 1));
            
            if (ry_world >= startY && ry_world < endY) {
                int ry = ry_world - startY;
                if (ore->canPlace(chunkData, rx, ry, rz)) {
                    ore->place(chunkData, rx, ry, rz, startX + rx, startZ + rz, seed);
                }
            }
        }
    };

    auto tryPlacePocket = [&](LakeFeature* pocket) {
        if (!pocket) return;
        int attempts = static_cast<int>(pocket->getSpawnChance() * 1000);
        for (int i = 0; i < attempts; ++i) {
            int rx = columnRng() % CHUNK_WIDTH;
            int rz = columnRng() % CHUNK_WIDTH;
            int ry_world = 5 + (columnRng() % 60); // Underground depth range
            
            if (ry_world >= startY && ry_world < endY) {
                int ry = ry_world - startY;
                if (pocket->canPlace(chunkData, rx, ry, rz)) {
                    pocket->place(chunkData, rx, ry, rz, startX + rx, startZ + rz, seed);
                }
            }
        }
    };

    tryPlaceOre(registry.getCoalOreFeature());
    tryPlaceOre(registry.getIronOreFeature());
    tryPlaceOre(registry.getGoldOreFeature());
    tryPlaceOre(registry.getDiamondOreFeature());
    tryPlaceOre(registry.getEmeraldOreFeature());
    
    // Liquid Pockets & Lakes
    tryPlacePocket(registry.getWaterPocketFeature());
    tryPlacePocket(registry.getLavaPocketFeature()); // Small pockets everywhere
    
    // Massive Underground Lava Lakes (Deep and Rare)
    auto tryPlaceLavaLake = [&](LakeFeature* lake) {
        if (!lake) return;
        int attempts = 2; // Reduced attempts
        for (int i = 0; i < attempts; ++i) {
            int rx = columnRng() % CHUNK_WIDTH;
            int rz = columnRng() % CHUNK_WIDTH;
            int ry_world = 5 + (columnRng() % 15); // Deep only (Y=5 to Y=20)
            
            if (ry_world >= startY && ry_world < endY) {
                int ry = ry_world - startY;
                if (lake->canPlace(chunkData, rx, ry, rz)) {
                    lake->place(chunkData, rx, ry, rz, startX + rx, startZ + rz, seed);
                }
            }
        }
    };
    tryPlaceLavaLake(registry.getLavaLakeFeature());
}
