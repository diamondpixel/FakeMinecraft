#pragma once

#include <string>

struct Block
{
public:
	enum BLOCK_TYPE
	{
		SOLID,
		TRANSPARENT,
		LEAVES,
		BILLBOARD,
		LIQUID
	};

    // Resolved Texture Layer Indices (Runtime)
    mutable uint16_t topLayer = 0;
    mutable uint16_t bottomLayer = 0;
    mutable uint16_t sideLayer = 0;

    BLOCK_TYPE blockType;
    
    // Memory Optimization: Fixed size char arrays ensuring structure locality
    // Strings are slow (heap allocation).
    char blockName[32];
    char topTexName[32];
    char bottomTexName[32];
    char sideTexName[32];

    uint8_t id = 0; // Internal runtime ID

    // Sound Paths (Optional, can be indices later)
    // std::string breakSound; 
    
    // Default Constructor
    Block() : blockType(SOLID), id(0) {
        blockName[0] = '\0';
        topTexName[0] = '\0';
        bottomTexName[0] = '\0';
        sideTexName[0] = '\0';
    }

    // Builder-compatible Constructor
    Block(const char* name, BLOCK_TYPE type) : blockType(type) {
         strncpy(blockName, name, 31); blockName[31] = '\0';
         strncpy(topTexName, name, 31); topTexName[31] = '\0';
         strncpy(bottomTexName, name, 31); bottomTexName[31] = '\0';
         strncpy(sideTexName, name, 31); sideTexName[31] = '\0';
    }
};