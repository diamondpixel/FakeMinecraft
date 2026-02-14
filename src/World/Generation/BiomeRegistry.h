#pragma once

#include <vector>
#include <cstring>
#include "Biome.h"
#include "TreeFeature.h"
#include "BigTreeFeature.h"
#include "OreFeature.h"
#include "VegetationFeature.h"
#include "LakeFeature.h"
#include "../headers/Blocks.h"

/**
 * BiomeRegistry - Singleton that manages biome definitions and lookup.
 */
class BiomeRegistry {
public:
    static BiomeRegistry& getInstance() {
        static BiomeRegistry instance;
        return instance;
    }

    // Get biome based on climate and continental noise
    [[nodiscard]] const Biome& getBiome(float temp, float humid, float cont) const {
        // Continental Priority:
        if (cont < -0.2f) return biomes[5]; // Ocean (ID: 5)
        if (cont < 0.05f) return biomes[4];  // Beach (ID: 4)

        // Inland Biomes:
        if (temp > 0.8f && humid < 0.2f) return biomes[1]; // Desert

        for (const auto& biome : biomes) {
            // Biomes list only includes IDs 0, 1, 2, 3 internally for the loop
            if (biome.id >= 4) continue; 
            if (temp >= biome.minTemperature && temp <= biome.maxTemperature &&
                humid >= biome.minHumidity && humid <= biome.maxHumidity) {
                return biome;
            }
        }
        return biomes[0];
    }

    [[nodiscard]] const Biome& getBiome(const uint8_t id) const {
        if (id < biomes.size()) {
            return biomes[id];
        }
        return biomes[0];
    }

    [[nodiscard]] const std::vector<Biome>& getAllBiomes() const { return biomes; }

    BiomeRegistry(const BiomeRegistry&) = delete;
    BiomeRegistry& operator=(const BiomeRegistry&) = delete;

    // Initialize standard biomes (called once at startup)
    void init() {
        if (initialized) return;
        initialized = true;

        // =====================================================================
        // PLAINS (ID: 0)
        // =====================================================================
        Biome plains;
        plains.id = 0;
        strncpy_s(plains.name, "Plains", 31);
        plains.surfaceBlock = Blocks::GRASS_BLOCK;
        plains.underBlock = Blocks::DIRT;
        plains.deepBlock = Blocks::STONE;
        plains.baseHeight = 66.0f;
        plains.heightMultiplier = 0.6f;
        plains.minTemperature = 0.3f;
        plains.maxTemperature = 0.7f;
        plains.minHumidity = 0.3f;
        plains.maxHumidity = 0.7f;
        
        plainsTreeFeature = std::make_unique<TreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 4, 6, 2, 0.005f);
        plainsGrassFeature = std::make_unique<VegetationFeature>(Blocks::GRASS, Blocks::GRASS_BLOCK, 0.5f);
        plainsPoppyFeature = std::make_unique<VegetationFeature>(Blocks::POPPY, Blocks::GRASS_BLOCK, 0.08f);
        plainsWhiteTulipFeature = std::make_unique<VegetationFeature>(Blocks::WHITE_TULIP, Blocks::GRASS_BLOCK, 0.04f);
        plainsPinkTulipFeature = std::make_unique<VegetationFeature>(Blocks::PINK_TULIP, Blocks::GRASS_BLOCK, 0.04f);
        plainsOrangeTulipFeature = std::make_unique<VegetationFeature>(Blocks::ORANGE_TULIP, Blocks::GRASS_BLOCK, 0.04f);
        
        plains.features.push_back(plainsPoppyFeature.get());
        plains.features.push_back(plainsWhiteTulipFeature.get());
        plains.features.push_back(plainsPinkTulipFeature.get());
        plains.features.push_back(plainsOrangeTulipFeature.get());
        plains.features.push_back(plainsTreeFeature.get());
        plains.features.push_back(plainsGrassFeature.get());
        biomes.push_back(plains);

        // =====================================================================
        // DESERT (ID: 1)
        // =====================================================================
        Biome desert;
        desert.id = 1;
        strncpy_s(desert.name, "Desert", 31);
        desert.surfaceBlock = Blocks::SAND;
        desert.underBlock = Blocks::SAND;
        desert.deepBlock = Blocks::STONE;
        desert.baseHeight = 65.0f;
        desert.heightMultiplier = 0.4f;
        desert.minTemperature = 0.8f;
        desert.maxTemperature = 1.0f;
        desert.minHumidity = 0.0f;
        desert.maxHumidity = 0.2f;

        desertLavaPool = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::SAND, 5, 0.002f);
        desert.features.push_back(desertLavaPool.get());
        biomes.push_back(desert);

        // =====================================================================
        // FOREST (ID: 2)
        // =====================================================================
        Biome forest;
        forest.id = 2;
        strncpy_s(forest.name, "Forest", 31);
        forest.surfaceBlock = Blocks::GRASS_BLOCK;
        forest.underBlock = Blocks::DIRT;
        forest.deepBlock = Blocks::STONE;
        forest.baseHeight = 68.0f;
        forest.heightMultiplier = 1.2f;
        forest.minTemperature = 0.4f;
        forest.maxTemperature = 0.8f;
        forest.minHumidity = 0.6f;
        forest.maxHumidity = 1.0f;

        forestTreeFeature = std::make_unique<TreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 5, 8, 3, 0.15f);
        bigTreeFeature = std::make_unique<BigTreeFeature>(Blocks::OAK_LOG, Blocks::OAK_LEAVES, 0.05f); // Boosted
        forestGrassFeature = std::make_unique<VegetationFeature>(Blocks::GRASS, Blocks::GRASS_BLOCK, 0.25f);
        forestPondFeature = std::make_unique<LakeFeature>(Blocks::WATER, Blocks::GRASS_BLOCK, 4, 0.001f);
        forestWhiteTulipFeature = std::make_unique<VegetationFeature>(Blocks::WHITE_TULIP, Blocks::GRASS_BLOCK, 0.08f);
        forestPoppyFeature = std::make_unique<VegetationFeature>(Blocks::POPPY, Blocks::GRASS_BLOCK, 0.06f);

        // Rare/Complex items FIRST so they aren't blocked by Grass/Small Trees
        forest.features.push_back(forestPondFeature.get());
        forest.features.push_back(bigTreeFeature.get());
        forest.features.push_back(forestWhiteTulipFeature.get());
        forest.features.push_back(forestPoppyFeature.get());
        forest.features.push_back(forestTreeFeature.get());
        forest.features.push_back(forestGrassFeature.get());
        biomes.push_back(forest);

        // =====================================================================
        // MOUNTAINS (ID: 3)
        // =====================================================================
        Biome mountains;
        mountains.id = 3;
        strncpy_s(mountains.name, "Mountains", 31);
        mountains.surfaceBlock = Blocks::STONE;
        mountains.underBlock = Blocks::STONE;
        mountains.deepBlock = Blocks::STONE;
        mountains.baseHeight = 70.0f;
        mountains.heightMultiplier = 3.5f;
        mountains.minTemperature = 0.0f;
        mountains.maxTemperature = 0.3f;
        mountains.minHumidity = 0.0f;
        mountains.maxHumidity = 1.0f;

        mountainsLavaPool = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 6, 0.003f);
        mountains.features.push_back(mountainsLavaPool.get());
        biomes.push_back(mountains);

        // =====================================================================
        // BEACH (ID: 4)
        // =====================================================================
        Biome beach;
        beach.id = 4;
        strncpy_s(beach.name, "Beach", 31);
        beach.surfaceBlock = Blocks::SAND;
        beach.underBlock = Blocks::SAND;
        beach.deepBlock = Blocks::STONE;
        beach.baseHeight = 63.0f;
        beach.heightMultiplier = 0.1f;
        beach.minTemperature = 0.6f;
        beach.maxTemperature = 0.8f;
        beach.minHumidity = 0.2f;
        beach.maxHumidity = 0.5f;
        biomes.push_back(beach);

        // =====================================================================
        // OCEAN (ID: 5)
        // =====================================================================
        Biome ocean;
        ocean.id = 5;
        strncpy_s(ocean.name, "Ocean", 31);
        ocean.surfaceBlock = Blocks::GRAVEL;
        ocean.underBlock = Blocks::SAND;
        ocean.deepBlock = Blocks::STONE;
        ocean.baseHeight = 45.0f;
        ocean.heightMultiplier = 0.5f;
        biomes.push_back(ocean);

        // =====================================================================
        // UNIVERSAL ORE FEATURES
        // =====================================================================
        coalOreFeature = std::make_unique<OreFeature>(Blocks::COAL_ORE, Blocks::STONE, 5, 128, 12, 0.15f);
        ironOreFeature = std::make_unique<OreFeature>(Blocks::IRON_ORE, Blocks::STONE, 5, 64, 8, 0.08f);
        goldOreFeature = std::make_unique<OreFeature>(Blocks::GOLD_ORE, Blocks::STONE, 1, 32, 6, 0.02f);
        diamondOreFeature = std::make_unique<OreFeature>(Blocks::DIAMOND_ORE, Blocks::STONE, 1, 15, 4, 0.005f);
        emeraldOreFeature = std::make_unique<OreFeature>(Blocks::EMERALD_ORE, Blocks::STONE, 1, 48, 2, 0.002f);
        
        waterPocketFeature = std::make_unique<LakeFeature>(Blocks::WATER, Blocks::STONE, 3, 0.002f);
        lavaPocketFeature = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 4, 0.0002f);
        lavaLakeFeature = std::make_unique<LakeFeature>(Blocks::LAVA, Blocks::STONE, 7, 0.00005f); // Smaller, rarer lava lakes
    }

    [[nodiscard]] OreFeature* getCoalOreFeature() const { return coalOreFeature.get(); }
    [[nodiscard]] OreFeature* getIronOreFeature() const { return ironOreFeature.get(); }
    [[nodiscard]] OreFeature* getGoldOreFeature() const { return goldOreFeature.get(); }
    [[nodiscard]] OreFeature* getDiamondOreFeature() const { return diamondOreFeature.get(); }
    [[nodiscard]] OreFeature* getEmeraldOreFeature() const { return emeraldOreFeature.get(); }
    [[nodiscard]] LakeFeature* getWaterPocketFeature() const { return waterPocketFeature.get(); }
    [[nodiscard]] LakeFeature* getLavaPocketFeature() const { return lavaPocketFeature.get(); }
    [[nodiscard]] LakeFeature* getLavaLakeFeature() const { return lavaLakeFeature.get(); }

private:
    BiomeRegistry() : initialized(false) {}

    bool initialized;
    std::vector<Biome> biomes;

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

    std::unique_ptr<VegetationFeature> beachPoppyFeature;
    std::unique_ptr<VegetationFeature> beachWhiteTulipFeature;

    std::unique_ptr<OreFeature> coalOreFeature;
    std::unique_ptr<OreFeature> ironOreFeature;
    std::unique_ptr<OreFeature> goldOreFeature;
    std::unique_ptr<OreFeature> diamondOreFeature;
    std::unique_ptr<OreFeature> emeraldOreFeature;

    std::unique_ptr<LakeFeature> waterPocketFeature;
    std::unique_ptr<LakeFeature> lavaPocketFeature;
    std::unique_ptr<LakeFeature> lavaLakeFeature;
};
