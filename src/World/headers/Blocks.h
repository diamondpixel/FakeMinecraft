/**
 * @file Blocks.h
 * @brief Utilities for finding blocks and adding them to the game.
 */

#pragma once

#include "BlockRegistry.h"
#include "BlockBuilder.h"
#include "../../Renderer/headers/TextureManager.h"

/**
 * @namespace Blocks
 * @brief Provides a simple way to get block IDs and register new blocks.
 */
namespace Blocks {
    /** @name Global Block IDs
     * Cached numeric identifiers for high-speed voxel access. These are 
     * populated during the init() call.
     * @{
     */
    inline uint8_t AIR = 0;
    inline uint8_t DIRT = 0;
    inline uint8_t GRASS = 0; ///< The short grass plant entity.
    inline uint8_t GRASS_BLOCK = 0; ///< The standard dirt block with grass on top.
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
    /** @} */

    /**
     * @brief Adds all the blocks to the game system.
     * 
     * 1. Teaches the registry about every block type.
     * 2. Saves the block numbers for later use.
     * 3. Finds the right textures for each block.
     */
    inline void init() {
        BlockRegistry& registry = BlockRegistry::getInstance();
        
        // --- Registration Phase ---
        
        AIR = registry.registerBlock(
            BlockBuilder("AIR").setTransparent().setTexture("air").build()
        );

        DIRT = registry.registerBlock(
            BlockBuilder("DIRT").setSolid().setTexture("dirt").build()
        );

        GRASS_BLOCK = registry.registerBlock(
            BlockBuilder("GRASS_BLOCK")
                .setGrassPattern("grass_block_top", "dirt", "grass_block_side")
                .setSolid()
                .build()
        );

        STONE = registry.registerBlock(
            BlockBuilder("STONE").setSolid().setTexture("stone").build()
        );

        OAK_LOG = registry.registerBlock(
            BlockBuilder("LOG")
                .setLogPattern("oak_log_top", "oak_log")
                .setSolid()
                .build()
        );

        OAK_LEAVES = registry.registerBlock(
            BlockBuilder("LEAVES").setLeaves().setTexture("oak_leaves").build()
        );

        GRASS = registry.registerBlock(
            BlockBuilder("GRASS").setBillboard().setTexture("short_grass").build()
        );

        TALL_GRASS_BOTTOM = registry.registerBlock(
            BlockBuilder("TALL_GRASS_BOTTOM").setBillboard().setTexture("tall_grass_bottom").build()
        );

        TALL_GRASS_TOP = registry.registerBlock(
            BlockBuilder("TALL_GRASS_TOP").setBillboard().setTexture("tall_grass_top").build()
        );

        POPPY = registry.registerBlock(
            BlockBuilder("POPPY").setBillboard().setTexture("poppy").build()
        );

        WHITE_TULIP = registry.registerBlock(
            BlockBuilder("WHITE_TULIP").setBillboard().setTexture("white_tulip").build()
        );

        PINK_TULIP = registry.registerBlock(
            BlockBuilder("PINK_TULIP").setBillboard().setTexture("pink_tulip").build()
        );

        ORANGE_TULIP = registry.registerBlock(
            BlockBuilder("ORANGE_TULIP").setBillboard().setTexture("orange_tulip").build()
        );

        WATER = registry.registerBlock(
            BlockBuilder("WATER").setLiquid().setTexture("water_still").build()
        );

        LAVA = registry.registerBlock(
            BlockBuilder("LAVA").setLiquid().setTexture("lava_still").build()
        );

        SAND = registry.registerBlock(
            BlockBuilder("SAND").setSolid().setTexture("sand").build()
        );
        
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

        GRAVEL = registry.registerBlock(
            BlockBuilder("GRAVEL").setSolid().setTexture("gravel").build()
        );

        BEDROCK = registry.registerBlock(
            BlockBuilder("BEDROCK").setSolid().setTexture("bedrock").build()
        );

        // --- Resolution Phase ---
        const TextureManager& tm = TextureManager::getInstance();
        for (const auto& block : registry.getAllBlocks()) {
             const Block& b = registry.getBlockMutable(block.id);
             if (b.blockName == "AIR") continue;
             
             // Map string names to numeric texture indices
             b.topLayer = tm.getLayerIndex(b.topTexName);
             b.bottomLayer = tm.getLayerIndex(b.bottomTexName);
             b.sideLayer = tm.getLayerIndex(b.sideTexName);
        }
    }
}
