#pragma once

#include "BlockRegistry.h"
#include "BlockBuilder.h"
#include "../../Renderer/headers/TextureManager.h"

namespace Blocks {
    // Cached IDs for fast access
    inline uint8_t AIR = 0;
    inline uint8_t DIRT = 0;
    inline uint8_t GRASS = 0; // "Grass" typically refers to the short grass plant
    inline uint8_t GRASS_BLOCK = 0;
    inline uint8_t STONE = 0;
    inline uint8_t OAK_LOG = 0;
    inline uint8_t OAK_LEAVES = 0;
    inline uint8_t TALL_GRASS_BOTTOM = 0;
    inline uint8_t TALL_GRASS_TOP = 0;
    inline uint8_t POPPY = 0;
    inline uint8_t WHITE_TULIP = 0;
    inline uint8_t PINK_TULIP = 0;
    inline uint8_t ORANGE_TULIP = 0;
    inline uint8_t WATER = 0;
    inline uint8_t LAVA = 0;
    inline uint8_t SAND = 0;
    inline uint8_t COAL_ORE = 0;
    inline uint8_t IRON_ORE = 0;
    inline uint8_t GOLD_ORE = 0;
    inline uint8_t DIAMOND_ORE = 0;
    inline uint8_t EMERALD_ORE = 0;
    inline uint8_t GRAVEL = 0;
    inline uint8_t BEDROCK = 0;

    inline void init() {
        BlockRegistry& registry = BlockRegistry::getInstance();
        
        // 0. AIR
        AIR = registry.registerBlock(
            BlockBuilder("AIR").setTransparent().setTexture("air").build()
        );

        // 1. DIRT
        DIRT = registry.registerBlock(
            BlockBuilder("DIRT").setSolid().setTexture("dirt").build()
        );

        // 2. GRASS_BLOCK
        GRASS_BLOCK = registry.registerBlock(
            BlockBuilder("GRASS_BLOCK")
                .setGrassPattern("grass_block_top", "dirt", "grass_block_side")
                .setSolid()
                .build()
        );

        // 3. STONE
        STONE = registry.registerBlock(
            BlockBuilder("STONE").setSolid().setTexture("stone").build()
        );

        // 4. OAK_LOG
        OAK_LOG = registry.registerBlock(
            BlockBuilder("LOG")
                .setLogPattern("oak_log_top", "oak_log")
                .setSolid()
                .build()
        );

        // 5. OAK_LEAVES
        OAK_LEAVES = registry.registerBlock(
            BlockBuilder("LEAVES").setLeaves().setTexture("oak_leaves").build()
        );

        // 6. SHORT_GRASS (Plant)
        GRASS = registry.registerBlock(
            BlockBuilder("GRASS").setBillboard().setTexture("short_grass").build()
        );

        // 7. TALL_GRASS_BOTTOM
        TALL_GRASS_BOTTOM = registry.registerBlock(
            BlockBuilder("TALL_GRASS_BOTTOM").setBillboard().setTexture("tall_grass_bottom").build()
        );

        // 8. TALL_GRASS_TOP
        TALL_GRASS_TOP = registry.registerBlock(
            BlockBuilder("TALL_GRASS_TOP").setBillboard().setTexture("tall_grass_top").build()
        );

        // 9. POPPY
        POPPY = registry.registerBlock(
            BlockBuilder("POPPY").setBillboard().setTexture("poppy").build()
        );

        // 10. WHITE_TULIP
        WHITE_TULIP = registry.registerBlock(
            BlockBuilder("WHITE_TULIP").setBillboard().setTexture("white_tulip").build()
        );

        // 11. PINK_TULIP
        PINK_TULIP = registry.registerBlock(
            BlockBuilder("PINK_TULIP").setBillboard().setTexture("pink_tulip").build()
        );

        // 12. ORANGE_TULIP
        ORANGE_TULIP = registry.registerBlock(
            BlockBuilder("ORANGE_TULIP").setBillboard().setTexture("orange_tulip").build()
        );

        // 13. WATER
        WATER = registry.registerBlock(
            BlockBuilder("WATER").setLiquid().setTexture("water_still").build()
        );

        // 14. LAVA
        LAVA = registry.registerBlock(
            BlockBuilder("LAVA").setLiquid().setTexture("lava_still").build()
        );

        // 15. SAND
        SAND = registry.registerBlock(
            BlockBuilder("SAND").setSolid().setTexture("sand").build()
        );
        
        // 16. ORES
        COAL_ORE = registry.registerBlock(
            BlockBuilder("COAL_ORE").setSolid().setTexture("coal_ore").build()
        );
        IRON_ORE = registry.registerBlock(
            BlockBuilder("IRON_ORE").setSolid().setTexture("iron_ore").build()
        );
        GOLD_ORE = registry.registerBlock(
            BlockBuilder("GOLD_ORE").setSolid().setTexture("gold_ore").build()
        );
        DIAMOND_ORE = registry.registerBlock(
            BlockBuilder("DIAMOND_ORE").setSolid().setTexture("diamond_ore").build()
        );
        EMERALD_ORE = registry.registerBlock(
            BlockBuilder("EMERALD_ORE").setSolid().setTexture("emerald_ore").build()
        );

        // 17. GRAVEL
        GRAVEL = registry.registerBlock(
            BlockBuilder("GRAVEL").setSolid().setTexture("gravel").build()
        );

        // 18. BEDROCK
        BEDROCK = registry.registerBlock(
            BlockBuilder("BEDROCK").setSolid().setTexture("bedrock").build()
        );

        // After registration, resolve texture layers
        TextureManager& tm = TextureManager::getInstance();
        for (const auto& block : registry.getAllBlocks()) {
             // Access mutable block ref to set layers
             Block& b = registry.getBlockMutable(block.id);
             if (b.blockName == "AIR") continue;
             b.topLayer = tm.getLayerIndex(b.topTexName);
             b.bottomLayer = tm.getLayerIndex(b.bottomTexName);
             b.sideLayer = tm.getLayerIndex(b.sideTexName);
        }
    }
}
