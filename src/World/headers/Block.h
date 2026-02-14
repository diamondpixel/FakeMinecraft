/**
 * @file Block.h
 * @brief Definition of the Block structure, the fundamental data unit of the world.
 */

#pragma once

#include <string>
#include <cstring>

/**
 * @struct Block
 * @brief Represents a single block type definition with its physical properties and textures.
 * 
 * This structure is designed for high performance and memory locality. It uses 
 * fixed-size character arrays instead of std::string to avoid heap allocations 
 * and ensure that all block data remains within the same CPU cache line when 
 * accessed from the BlockRegistry.
 */
struct Block
{
public:
    /**
     * @enum BLOCK_TYPE
     * @brief Categorizes blocks by their physical and rendering behavior.
     */
	enum BLOCK_TYPE
	{
		SOLID,       ///< Fully opaque cubes (e.g., Stone, Dirt).
		TRANSPARENT, ///< Blocks that allow light but aren't liquid (e.g., Glass).
		LEAVES,      ///< Semi-transparent blocks with custom culling logic.
		BILLBOARD,   ///< X-quads used for plants and flowers.
		LIQUID       ///< Fluids with animated textures and no collision.
	};

    /** @name Texture Layer Cache
     * Cached indices for the TextureArray. These are resolved at runtime 
     * during initialization.
     * @{
     */
    mutable uint16_t topLayer = 0;    ///< Index for the top face texture.
    mutable uint16_t bottomLayer = 0; ///< Index for the bottom face texture.
    mutable uint16_t sideLayer = 0;   ///< Index for the side face textures.
    /** @} */

    BLOCK_TYPE blockType; ///< The physical behavior of this block.
    
    /** @name Fixed-Size Identifiers
     * Fixed arrays ensure strict structure locality and zero heap fragmentation.
     * @{
     */
    char blockName[32];     ///< Descriptive name (e.g., "GRASS_BLOCK").
    char topTexName[32];    ///< Unique name of the top texture file.
    char bottomTexName[32]; ///< Unique name of the bottom texture file.
    char sideTexName[32];   ///< Unique name of the side texture file.
    /** @} */

    uint8_t id = 0; ///< Internal runtime ID used for voxel mapping.

    /**
     * @brief Default Constructor. Initializes an empty block template.
     */
    Block() : blockType(SOLID), id(0) {
        blockName[0] = '\0';
        topTexName[0] = '\0';
        bottomTexName[0] = '\0';
        sideTexName[0] = '\0';
    }

    /**
     * @brief Builder-compatible Constructor.
     * @param name The unique identifier for this block.
     * @param type The physical and render category.
     */
    Block(const char* name, BLOCK_TYPE type) : blockType(type) {
         strncpy(blockName, name, 31); blockName[31] = '\0';
         strncpy(topTexName, name, 31); topTexName[31] = '\0';
         strncpy(bottomTexName, name, 31); bottomTexName[31] = '\0';
         strncpy(sideTexName, name, 31); sideTexName[31] = '\0';
    }
};