/**
 * @file BlockBuilder.h
 * @brief Fluent interface for constructing Block definitions.
 */

#pragma once

#include "Block.h"

/**
 * @class BlockBuilder
 * @brief Helper class to simplify the registration of new block types.
 * 
 * Uses a fluent "Builder" pattern to configure block properties like textures 
 * and physical types before returning a finalized Block structure.
 */
class BlockBuilder {
public:
    /**
     * @brief Starts a new block definition.
     * @param name The unique name of the block.
     */
    BlockBuilder(const char* name) {
        strncpy_s(block.blockName, name, 31); block.blockName[31] = '\0';
        block.blockType = Block::SOLID; // Default
        
        // Default texture name = block name
        strncpy_s(block.topTexName, name, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, name, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, name, 31); block.sideTexName[31] = '\0';
    }

    /**
     * @brief Sets all 6 faces to use the same texture.
     */
    BlockBuilder& setTexture(const char* texName) {
        strncpy_s(block.topTexName, texName, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, texName, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, texName, 31); block.sideTexName[31] = '\0';
        return *this;
    }

    /**
     * @brief Manually sets different textures for Top, Bottom, and Sides.
     */
    BlockBuilder& setTextures(const char* top, const char* bottom, const char* side) {
        strncpy_s(block.topTexName, top, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, bottom, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, side, 31); block.sideTexName[31] = '\0';
        return *this;
    }

    /**
     * @brief Higher-level helper for grass-like blocks.
     */
    BlockBuilder& setGrassPattern(const char* top, const char* dirt, const char* side) {
        return setTextures(top, dirt, side);
    }
    
    /**
     * @brief Higher-level helper for cylindrical log blocks.
     */
    BlockBuilder& setLogPattern(const char* topBottom, const char* side) {
        return setTextures(topBottom, topBottom, side);
    }

    /// @brief Marks the block as a standard solid cube.
    BlockBuilder& setSolid() {
        block.blockType = Block::SOLID;
        return *this;
    }

    /// @brief Marks the block as transparent (e.g., Glass).
    BlockBuilder& setTransparent() {
        block.blockType = Block::TRANSPARENT;
        return *this;
    }

    /// @brief Marks the block as leaves (optimized alpha-testing).
    BlockBuilder& setLeaves() {
        block.blockType = Block::LEAVES;
        return *this;
    }

    /// @brief Marks the block as a plant/billboard (cross-quad).
    BlockBuilder& setBillboard() {
        block.blockType = Block::BILLBOARD;
        return *this;
    }

    /// @brief Marks the block as a liquid (lava/water).
    BlockBuilder& setLiquid() {
        block.blockType = Block::LIQUID;
        return *this;
    }
    
    /**
     * @brief Finalizes the construction.
     * @return The configured Block object.
     */
    Block build() const {
        return block;
    }

private:
    Block block{"temp", Block::SOLID}; ///< Internal workspace for the builder.
};
