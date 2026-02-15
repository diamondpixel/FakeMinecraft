/**
 * @file Biome.h
 * @brief Definition of different biomes in the game.
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "Feature.h"

/**
 * @struct Biome
 * @brief Stores information about how a specific biome (like a Forest) should look.
 */
struct Biome {
    // Identification
    uint8_t id;
    char name[32];

    // Block Configuration
    uint8_t surfaceBlock;    // Top layer (Grass, Sand)
    uint8_t underBlock;      // Sub-surface (Dirt, Sand)
    uint8_t deepBlock;       // Deep underground (Stone)

    // Terrain Shaping
    float baseHeight;        // Base terrain height offset
    float heightMultiplier;  // Amplifies/flattens noise (1.0 = normal)
    
    // Climate (for lookup)
    float minTemperature;
    float maxTemperature;
    float minHumidity;
    float maxHumidity;

    // Features/Decorations (stored as raw pointers, Registry owns lifetime)
    std::vector<Feature*> features;

    Biome() 
        : id(0), surfaceBlock(0), underBlock(0), deepBlock(0),
          baseHeight(64.0f), heightMultiplier(1.0f),
          minTemperature(0.0f), maxTemperature(1.0f),
          minHumidity(0.0f), maxHumidity(1.0f) {
        name[0] = '\0';
    }
};
