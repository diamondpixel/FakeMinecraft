#pragma once

#include "Block.h"
#include <string>

class BlockBuilder {
public:
    BlockBuilder(const char* name) {
        strncpy_s(block.blockName, name, 31); block.blockName[31] = '\0';
        block.blockType = Block::SOLID; // Default
        
        // Default texture name = block name
        strncpy_s(block.topTexName, name, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, name, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, name, 31); block.sideTexName[31] = '\0';
    }

    BlockBuilder& setTexture(const char* texName) {
        strncpy_s(block.topTexName, texName, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, texName, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, texName, 31); block.sideTexName[31] = '\0';
        return *this;
    }

    BlockBuilder& setTextures(const char* top, const char* bottom, const char* side) {
        strncpy_s(block.topTexName, top, 31); block.topTexName[31] = '\0';
        strncpy_s(block.bottomTexName, bottom, 31); block.bottomTexName[31] = '\0';
        strncpy_s(block.sideTexName, side, 31); block.sideTexName[31] = '\0';
        return *this;
    }

    // Convenience for standard "Grass" pattern (Top, Bottom, Side)
    BlockBuilder& setGrassPattern(const char* top, const char* dirt, const char* side) {
        return setTextures(top, dirt, side);
    }
    
    // Convenience for standard "Log" pattern (Top/Bottom same, Side different)
    BlockBuilder& setLogPattern(const char* topBottom, const char* side) {
        return setTextures(topBottom, topBottom, side);
    }

    BlockBuilder& setSolid() {
        block.blockType = Block::SOLID;
        return *this;
    }

    BlockBuilder& setTransparent() {
        block.blockType = Block::TRANSPARENT;
        return *this;
    }

    BlockBuilder& setLeaves() {
        block.blockType = Block::LEAVES;
        return *this;
    }

    BlockBuilder& setBillboard() {
        block.blockType = Block::BILLBOARD;
        return *this;
    }

    BlockBuilder& setLiquid() {
        block.blockType = Block::LIQUID;
        return *this;
    }
    
    // Sounds temporarily removed for optimization
    /*
    BlockBuilder& setSounds(std::string breakSound, std::string placeSound, std::string stepSound) {
        block.breakSound = breakSound;
        block.placeSound = placeSound;
        block.stepSound = stepSound;
        return *this;
    }
    */

    Block build() const {
        return block;
    }

private:
    Block block{"temp", Block::SOLID};
};
