#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "WorldVertex.h"
#include "Block.h"
#include "Blocks.h"

// Structure to hold greedy quad information
struct GreedyQuad {
    int x, y, z;           // Position
    int width, height;     // Merged dimensions  
    uint16_t blockId;      // Block type for texture
    FACE_DIRECTION dir;    // Face direction
};

// Generate a merged quad with proper texture tiling (Array Texture version)
inline void emitGreedyQuad(const GreedyQuad& quad, const glm::vec3& worldPos,
                           std::vector<WorldVertex>& vertices, std::vector<unsigned int>& indices,
                           unsigned int& currentVertex) {
    const Block* block = &Blocks::blocks[quad.blockId];
    
    // Get texture grid coordinates based on face direction
    char gridX, gridY;
    bool isSide = (quad.dir != TOP && quad.dir != BOTTOM);
    bool isBottom = (quad.dir == BOTTOM);
    
    if (isBottom) {
        gridX = block->bottomMinX; gridY = block->bottomMinY;
    } else if (isSide) {
        gridX = block->sideMinX; gridY = block->sideMinY;
    } else {
        gridX = block->topMinX; gridY = block->topMinY;
    }
    
    // Calculate Array Layer Index (assuming 16x16 atlas)
    uint8_t layerIndex = (uint8_t)((int)gridY * 16 + (int)gridX);
    
    // UVs are now local repeat counts (0 to width/height)
    char uMax = quad.width;
    char vMax = quad.height;
    
    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;
    
    // Generate 4 vertices based on face direction
    switch (quad.dir) {
        case TOP:
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + quad.height + wz, 0, 0, TOP, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + quad.height + wz, uMax, 0, TOP, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + wz, 0, vMax, TOP, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + wz, uMax, vMax, TOP, layerIndex);
            break;
        case BOTTOM:
             vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + quad.height + wz, uMax, vMax, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.height + wz, 0, vMax, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, BOTTOM, layerIndex);
             vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, BOTTOM, layerIndex);
             break;
        case NORTH:
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, NORTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, NORTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + wz, uMax, vMax, NORTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, NORTH, layerIndex);
            break;
        case SOUTH:
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + 1 + wz, 0, 0, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + 1 + wz, uMax, 0, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + 1 + wz, 0, vMax, SOUTH, layerIndex);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + 1 + wz, uMax, vMax, SOUTH, layerIndex);
            break;
        case WEST:
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, WEST, layerIndex);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax, WEST, layerIndex);
            break;
        case EAST:
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + wz, 0, 0, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax, EAST, layerIndex);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, EAST, layerIndex);
            break;
        default:
            break;
    }
    
    // Standard quad indices
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);
    currentVertex += 4;
}