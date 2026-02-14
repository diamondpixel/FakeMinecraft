#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "WorldVertex.h"
#include "Block.h"
#include "BlockRegistry.h"

// Structure to hold greedy quad information
struct GreedyQuad {
    int x, y, z; // Position
    int width, height; // Merged dimensions
    uint16_t blockId; // Block type for texture
    FACE_DIRECTION dir; // Face direction
    uint8_t lightLevel; // Block light level for this face
};

// Generate a merged quad with proper texture tiling (Array Texture version)
inline void emitGreedyQuad(const GreedyQuad &quad, const glm::vec3 &worldPos,
                           std::vector<WorldVertex> &vertices, std::vector<unsigned int> &indices,
                           unsigned int &currentVertex) {
    const Block &block = BlockRegistry::getInstance().getBlock(quad.blockId);

    // Get texture layer index based on face direction
    uint16_t layerIndex;
    if (quad.dir == BOTTOM) {
        layerIndex = block.bottomLayer;
    } else if (quad.dir == TOP) {
        layerIndex = block.topLayer;
    } else {
        layerIndex = block.sideLayer;
    }

    const char uMax = quad.width;
    const char vMax = quad.height;

    const float wx = worldPos.x;
    const float wy = worldPos.y;
    const float wz = worldPos.z;

    const uint8_t light = quad.lightLevel;

    // Generate 4 vertices based on face direction
    switch (quad.dir) {
        case TOP:
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + quad.height + wz, 0, 0, TOP, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + quad.height + wz, uMax, 0, TOP,
                                  layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + 1 + wy, quad.z + wz, 0, vMax, TOP, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + 1 + wy, quad.z + wz, uMax, vMax, TOP, layerIndex, light);
            break;
        case BOTTOM:
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + quad.height + wz, uMax, vMax, BOTTOM,
                                  layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.height + wz, 0, vMax, BOTTOM, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, BOTTOM, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, BOTTOM, layerIndex, light);
            break;
        case NORTH:
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + wz, uMax, 0, NORTH, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, NORTH, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + wz, uMax, vMax, NORTH,
                                  layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, NORTH, layerIndex, light);
            break;
        case SOUTH:
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + 1 + wz, 0, 0, SOUTH, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + wy, quad.z + 1 + wz, uMax, 0, SOUTH, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + 1 + wz, 0, vMax, SOUTH, layerIndex, light);
            vertices.emplace_back(quad.x + quad.width + wx, quad.y + quad.height + wy, quad.z + 1 + wz, uMax, vMax,
                                  SOUTH, layerIndex, light);
            break;
        case WEST:
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + wz, 0, 0, WEST, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, WEST, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, WEST, layerIndex, light);
            vertices.emplace_back(quad.x + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax, WEST,
                                  layerIndex, light);
            break;
        case EAST:
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + quad.width + wz, uMax, 0, EAST, layerIndex, light);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + wy, quad.z + wz, 0, 0, EAST, layerIndex, light);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + quad.width + wz, uMax, vMax,
                                  EAST, layerIndex, light);
            vertices.emplace_back(quad.x + 1 + wx, quad.y + quad.height + wy, quad.z + wz, 0, vMax, EAST, layerIndex, light);
            break;
        default:
            break;
    }

    // Standard quad indices
    indices.push_back(currentVertex);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);
    currentVertex += 4;
}