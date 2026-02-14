#pragma once

#include <array>
#include <memory>
#include <string_view>
#include "Biome.h"
#include "TreeFeature.h"
#include "BigTreeFeature.h"
#include "OreFeature.h"
#include "VegetationFeature.h"
#include "LakeFeature.h"
#include "../headers/Blocks.h"

/**
 * BiomeRegistry - Singleton that manages biome definitions and lookup.
 * Optimized for fast biome lookup and minimal allocation overhead.
 */
class BiomeRegistry {
public:
    // Total number of biomes (compile-time constant)
    static constexpr size_t BIOME_COUNT = 6;

    // Biome IDs (for clarity and type safety)
    enum BiomeID : uint8_t {
        PLAINS = 0,
        DESERT = 1,
        FOREST = 2,
        MOUNTAINS = 3,
        BEACH = 4,
        OCEAN = 5
    };

    static BiomeRegistry& getInstance() noexcept {
        static BiomeRegistry instance;
        return instance;
    }

    // Get biome based on climate and continental noise
    // Optimized for branch prediction and minimal comparisons
    [[nodiscard]] const Biome& getBiome(const float temp, const float humid, const float cont) const noexcept {
        // Continental Priority (most predictable branches first):
        // Ocean check - most common for large worlds
        if (cont < -0.2f) {
            return biomes[OCEAN];
        }

        // Beach check - transition zone
        if (cont < 0.05f) {
            return biomes[BEACH];
        }

        // Desert check - specific high temp + low humid condition
        // Check this before general ranges as it's a specific override
        if (temp > 0.8f && humid < 0.2f) {
            return biomes[DESERT];
        }

        // Inline range checks for remaining biomes (0-3) to avoid loop overhead
        // Ordered by likelihood for better branch prediction

        // Plains (most common fallback) - check last for best average case
        if (temp >= 0.3f && temp <= 0.7f && humid >= 0.3f && humid <= 0.7f) {
            return biomes[PLAINS];
        }

        // Forest (high humidity)
        if (temp >= 0.4f && temp <= 0.8f && humid >= 0.6f && humid <= 1.0f) {
            return biomes[FOREST];
        }

        // Mountains (cold)
        if (temp >= 0.0f && temp <= 0.3f) {
            return biomes[MOUNTAINS];
        }

        // Default fallback to plains
        return biomes[PLAINS];
    }

    [[nodiscard]] const Biome& getBiome(const uint8_t id) const noexcept {
        // Bounds check with early return (avoid branch misprediction)
        if (id >= BIOME_COUNT) [[unlikely]] {
            return biomes[PLAINS];
        }
        return biomes[id];
    }

    [[nodiscard]] const std::array<Biome, BIOME_COUNT>& getAllBiomes() const noexcept {
        return biomes;
    }

    BiomeRegistry(const BiomeRegistry&) = delete;
    BiomeRegistry& operator=(const BiomeRegistry&) = delete;
    BiomeRegistry(BiomeRegistry&&) = delete;
    BiomeRegistry& operator=(BiomeRegistry&&) = delete;

    // Initialize standard biomes (called once at startup)
    void init() {
        if (initialized) [[unlikely]] return;
        initialized = true;

        // Pre-allocate all features upfront to avoid repeated allocations
        initializeFeatures();

        // Initialize biomes in-place (no copying)
        initializePlains();
        initializeDesert();
        initializeForest();
        initializeMountains();
        initializeBeach();
        initializeOcean();
    }

    // Universal ore feature getters
    [[nodiscard]] OreFeature* getCoalOreFeature() const noexcept { return coalOreFeature.get(); }
    [[nodiscard]] OreFeature* getIronOreFeature() const noexcept { return ironOreFeature.get(); }
    [[nodiscard]] OreFeature* getGoldOreFeature() const noexcept { return goldOreFeature.get(); }
    [[nodiscard]] OreFeature* getDiamondOreFeature() const noexcept { return diamondOreFeature.get(); }
    [[nodiscard]] OreFeature* getEmeraldOreFeature() const noexcept { return emeraldOreFeature.get(); }
    [[nodiscard]] LakeFeature* getWaterPocketFeature() const noexcept { return waterPocketFeature.get(); }
    [[nodiscard]] LakeFeature* getLavaPocketFeature() const noexcept { return lavaPocketFeature.get(); }
    [[nodiscard]] LakeFeature* getLavaLakeFeature() const noexcept { return lavaLakeFeature.get(); }

private:
    BiomeRegistry() noexcept : initialized(false), biomes{} {}

    bool initialized;

    // Use std::array instead of vector - no dynamic allocation, better cache locality
    std::array<Biome, BIOME_COUNT> biomes;

    // Feature storage - grouped by type for better cache locality
    std::unique_ptr<TreeFeature> plainsTreeFeature;
    std::unique_ptr<VegetationFeature> plainsGrassFeature;
    std::unique_ptr<VegetationFeature> plainsPoppyFeature;
    std::unique_ptr<VegetationFeature> plainsWhiteTulipFeature;
    std::unique_ptr<VegetationFeature> plainsPinkTulipFeature;
    std::unique_ptr<VegetationFeature> plainsOrangeTulipFeature;

    std::unique_ptr<TreeFeature> forestTreeFeature;
    std::unique_ptr<BigTreeFeature> bigTreeFeature;
    std::unique_ptr<VegetationFeature> forestGrassFeature;
    std::unique_ptr<LakeFeature> forestPondFeature;
    std::unique_ptr<VegetationFeature> forestWhiteTulipFeature;
    std::unique_ptr<VegetationFeature> forestPoppyFeature;

    std::unique_ptr<LakeFeature> desertLavaPool;
    std::unique_ptr<LakeFeature> mountainsLavaPool;

    std::unique_ptr<OreFeature> coalOreFeature;
    std::unique_ptr<OreFeature> ironOreFeature;
    std::unique_ptr<OreFeature> goldOreFeature;
    std::unique_ptr<OreFeature> diamondOreFeature;
    std::unique_ptr<OreFeature> emeraldOreFeature;

    std::unique_ptr<LakeFeature> waterPocketFeature;
    std::unique_ptr<LakeFeature> lavaPocketFeature;
    std::unique_ptr<LakeFeature> lavaLakeFeature;

    // Helper to set biome name efficiently (avoids strncpy_s overhead)
    static inline void setBiomeName(char* dest, const char* src) noexcept {
        size_t i = 0;
        while (i < 31 && src[i] != '\0') {
            dest[i] = src[i];
            ++i;
        }
        dest[i] = '\0';
    }

    // Initialize all features (called once)
    void initializeFeatures() {
        // Universal ore features
        coalOreFeature = std::make_unique<OreFeature>(Blocks::COAL_ORE, Blocks::STONE, 5, 128, 12, 0.15f);
        ironOreFeature = std::make_unique<OreFeature>(Blocks::IRON_ORE, Blocks::STONE, 5, 64, 8, 0.08f);
        goldOreFeature = std::make_unique<OreFeature>(Blocks::GOLD_ORE, Blocks::STONE, 1, 32, 6, 0.02f);
        diamondOreFeature = std::make_unique<OreFeature>(Blocks::DIAMOND_ORE, Blocks::STONE, 1, 15, 4, 0.005f);
        emeraldOreFeature = std::make_unique<OreFeature>(Blocks::EMERALD_ORE, Blocks::STONE, 1, 48, 2, 0.002f);

        waterPocketFeature = std::make_unique<LakeFeature>(Blocks::WATER, Blocks::STONE, 3, 0.002f);
        lavaPocketFeature = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 4, 0.0002f);
        lavaLakeFeature = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 7, 0.00005f);

        // Plains features
        plainsTreeFeature = std::make_unique<TreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 4, 6, 2, 0.005f);
        plainsGrassFeature = std::make_unique<VegetationFeature>(Blocks::GRASS, Blocks::GRASS_BLOCK, 0.5f);
        plainsPoppyFeature = std::make_unique<VegetationFeature>(Blocks::POPPY, Blocks::GRASS_BLOCK, 0.08f);
        plainsWhiteTulipFeature = std::make_unique<VegetationFeature>(Blocks::WHITE_TULIP, Blocks::GRASS_BLOCK, 0.04f);
        plainsPinkTulipFeature = std::make_unique<VegetationFeature>(Blocks::PINK_TULIP, Blocks::GRASS_BLOCK, 0.04f);
        plainsOrangeTulipFeature = std::make_unique<VegetationFeature>(Blocks::ORANGE_TULIP, Blocks::GRASS_BLOCK, 0.04f);

        // Forest features
        forestTreeFeature = std::make_unique<TreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 5, 8, 3, 0.15f);
        bigTreeFeature = std::make_unique<BigTreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 0.05f);
        forestGrassFeature = std::make_unique<VegetationFeature>(Blocks::GRASS, Blocks::GRASS_BLOCK, 0.25f);
        forestPondFeature = std::make_unique<LakeFeature>(Blocks::WATER, Blocks::GRASS_BLOCK, 4, 0.001f);
        forestWhiteTulipFeature = std::make_unique<VegetationFeature>(Blocks::WHITE_TULIP, Blocks::GRASS_BLOCK, 0.08f);
        forestPoppyFeature = std::make_unique<VegetationFeature>(Blocks::POPPY, Blocks::GRASS_BLOCK, 0.06f);

        // Desert features
        desertLavaPool = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::SAND, 5, 0.002f);

        // Mountain features
        mountainsLavaPool = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 6, 0.003f);
    }

    void initializePlains() {
        Biome& plains = biomes[PLAINS];
        plains.id = PLAINS;
        setBiomeName(plains.name, "Plains");
        plains.surfaceBlock = Blocks::GRASS_BLOCK;
        plains.underBlock = Blocks::DIRT;
        plains.deepBlock = Blocks::STONE;
        plains.baseHeight = 66.0f;
        plains.heightMultiplier = 0.6f;
        plains.minTemperature = 0.3f;
        plains.maxTemperature = 0.7f;
        plains.minHumidity = 0.3f;
        plains.maxHumidity = 0.7f;

        // Reserve capacity to avoid reallocation
        plains.features.reserve(6);
        plains.features.push_back(plainsPoppyFeature.get());
        plains.features.push_back(plainsWhiteTulipFeature.get());
        plains.features.push_back(plainsPinkTulipFeature.get());
        plains.features.push_back(plainsOrangeTulipFeature.get());
        plains.features.push_back(plainsTreeFeature.get());
        plains.features.push_back(plainsGrassFeature.get());
    }

    void initializeDesert() {
        Biome& desert = biomes[DESERT];
        desert.id = DESERT;
        setBiomeName(desert.name, "Desert");
        desert.surfaceBlock = Blocks::SAND;
        desert.underBlock = Blocks::SAND;
        desert.deepBlock = Blocks::STONE;
        desert.baseHeight = 65.0f;
        desert.heightMultiplier = 0.4f;
        desert.minTemperature = 0.8f;
        desert.maxTemperature = 1.0f;
        desert.minHumidity = 0.0f;
        desert.maxHumidity = 0.2f;

        desert.features.reserve(1);
        desert.features.push_back(desertLavaPool.get());
    }

    void initializeForest() {
        Biome& forest = biomes[FOREST];
        forest.id = FOREST;
        setBiomeName(forest.name, "Forest");
        forest.surfaceBlock = Blocks::GRASS_BLOCK;
        forest.underBlock = Blocks::DIRT;
        forest.deepBlock = Blocks::STONE;
        forest.baseHeight = 68.0f;
        forest.heightMultiplier = 1.2f;
        forest.minTemperature = 0.4f;
        forest.maxTemperature = 0.8f;
        forest.minHumidity = 0.6f;
        forest.maxHumidity = 1.0f;

        // Rare/Complex items FIRST so they aren't blocked by Grass/Small Trees
        forest.features.reserve(6);
        forest.features.push_back(forestPondFeature.get());
        forest.features.push_back(bigTreeFeature.get());
        forest.features.push_back(forestWhiteTulipFeature.get());
        forest.features.push_back(forestPoppyFeature.get());
        forest.features.push_back(forestTreeFeature.get());
        forest.features.push_back(forestGrassFeature.get());
    }

    void initializeMountains() {
        Biome& mountains = biomes[MOUNTAINS];
        mountains.id = MOUNTAINS;
        setBiomeName(mountains.name, "Mountains");
        mountains.surfaceBlock = Blocks::STONE;
        mountains.underBlock = Blocks::STONE;
        mountains.deepBlock = Blocks::STONE;
        mountains.baseHeight = 70.0f;
        mountains.heightMultiplier = 3.5f;
        mountains.minTemperature = 0.0f;
        mountains.maxTemperature = 0.3f;
        mountains.minHumidity = 0.0f;
        mountains.maxHumidity = 1.0f;

        mountains.features.reserve(1);
        mountains.features.push_back(mountainsLavaPool.get());
    }

    void initializeBeach() {
        Biome& beach = biomes[BEACH];
        beach.id = BEACH;
        setBiomeName(beach.name, "Beach");
        beach.surfaceBlock = Blocks::SAND;
        beach.underBlock = Blocks::SAND;
        beach.deepBlock = Blocks::STONE;
        beach.baseHeight = 63.0f;
        beach.heightMultiplier = 0.1f;
        beach.minTemperature = 0.6f;
        beach.maxTemperature = 0.8f;
        beach.minHumidity = 0.2f;
        beach.maxHumidity = 0.5f;
        // No features - reserve(0) not needed
    }

    void initializeOcean() {
        Biome& ocean = biomes[OCEAN];
        ocean.id = OCEAN;
        setBiomeName(ocean.name, "Ocean");
        ocean.surfaceBlock = Blocks::GRAVEL;
        ocean.underBlock = Blocks::SAND;
        ocean.deepBlock = Blocks::STONE;
        ocean.baseHeight = 45.0f;
        ocean.heightMultiplier = 0.5f;
        // No temperature/humidity ranges for ocean
        // No features - reserve(0) not needed
    }
};