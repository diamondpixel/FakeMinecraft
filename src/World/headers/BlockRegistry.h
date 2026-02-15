/**
 * @file BlockRegistry.h
 * @brief Registry for keeping track of all the block types in the game.
 */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include "Block.h"

/**
 * @class BlockRegistry
 * @brief This class stores all the different blocks we've created.
 * 
 * It assigns a number to each block name so we can find them 
 * very quickly when building the world.
 */
class BlockRegistry {
public:
    /**
     * @brief Access the singleton registry instance.
     */
    static BlockRegistry& getInstance() {
        static BlockRegistry instance;
        return instance;
    }

    /**
     * @brief Registers a new block type into the game.
     * 
     * @param block The block template to register.
     * @return The unique numeric ID assigned to this block (0-255).
     */
    uint8_t registerBlock(Block block) {
        if (blocks.size() >= 255) {
            std::cerr << "Error: Max block limit reached (255)!" << std::endl;
            return 0;
        }

        uint8_t id = static_cast<uint8_t>(blocks.size());
        block.id = id; // Assign internal ID
        
        blocks.push_back(block);
        nameMap[block.blockName] = id;

        return id;
    }

    /**
     * @brief Retrieves a block definition by its numeric ID.
     * @param id The internal ID.
     * @return Reference to the Block data (returns AIR if ID is invalid).
     */
    const Block& getBlock(uint8_t id) const {
        if (id >= blocks.size()) {
            return blocks[0]; // Return Air/Default if Out of Bounds
        }
        return blocks[id];
    }

    /**
     * @brief Retrieves a block definition by its string name.
     * @param name The descriptive name (e.g., "STONE").
     * @return Reference to the Block data.
     */
    const Block& getBlock(const std::string& name) const {
        auto it = nameMap.find(name);
        if (it != nameMap.end()) {
            return blocks[it->second];
        }
        std::cerr << "Warning: Block not found by name: " << name << std::endl;
        return blocks[0];
    }
    
    /**
     * @brief Provides mutable access to a block definition.
     * Used primarily during initialization to resolve texture layers.
     */
    Block& getBlockMutable(uint8_t id) {
        if (id >= blocks.size()) {
            return blocks[0];
        }
        return blocks[id];
    }
    
    /**
     * @brief Returns a read-only list of all registered blocks.
     */
    const std::vector<Block>& getAllBlocks() const {
        return blocks;
    }

private:
    /**
     * @brief Private constructor to enforce Singleton pattern.
     */
    BlockRegistry() {
        // Typically AIR should be the first block registered (ID 0).
    }
    
    std::vector<Block> blocks; ///< Linear storage for O(1) ID-based access.
    std::unordered_map<std::string, uint8_t> nameMap; ///< Hash map for name-based search.
};
