#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include "Block.h"

class BlockRegistry {
public:
    static BlockRegistry& getInstance() {
        static BlockRegistry instance;
        return instance;
    }

    uint8_t registerBlock(Block block) {
        if (blocks.size() >= 255) {
            std::cerr << "Error: Max block limit reached (255)!" << std::endl;
            return 0;
        }

        uint8_t id = static_cast<uint8_t>(blocks.size());
        block.id = id; // Assign internal ID
        
        blocks.push_back(block);
        nameMap[block.blockName] = id;

        // std::cout << "Registered Block: " << block.blockName << " [ID: " << (int)id << "]" << std::endl;
        return id;
    }

    const Block& getBlock(uint8_t id) const {
        if (id >= blocks.size()) {
            return blocks[0]; // Return Air/Default if Out of Bounds
        }
        return blocks[id];
    }

    const Block& getBlock(const std::string& name) const {
        auto it = nameMap.find(name);
        if (it != nameMap.end()) {
            return blocks[it->second];
        }
        std::cerr << "Warning: Block not found by name: " << name << std::endl;
        return blocks[0];
    }
    
    // Mutable access for texture resolution if needed
    Block& getBlockMutable(uint8_t id) {
        if (id >= blocks.size()) {
            return blocks[0];
        }
        return blocks[id];
    }
    
    const std::vector<Block>& getAllBlocks() const {
        return blocks;
    }

private:
    BlockRegistry() {
        // Reserve index 0 for AIR if not explicitly registered, 
        // but typically AIR is registered first.
    }
    
    std::vector<Block> blocks;
    std::unordered_map<std::string, uint8_t> nameMap;
};
