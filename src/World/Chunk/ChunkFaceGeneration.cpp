#include "Chunk.h"
#include "Blocks.h"

// Generate helper for world block faces (Array Texture Version)
void Chunk::generateWorldFaces(const int x, const int y, const int z, FACE_DIRECTION faceDirection, const Block *block,
                               unsigned int &currentVertex, const int subChunkIndex) {
    auto& vertices = worldVertices[subChunkIndex];
    auto& indices = worldIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    char gridX, gridY;
    if (faceDirection == TOP) {
        gridX = block->topMinX; gridY = block->topMinY;
    } else if (faceDirection == BOTTOM) {
        gridX = block->bottomMinX; gridY = block->bottomMinY;
    } else {
        gridX = block->sideMinX; gridY = block->sideMinY;
    }
    
    uint8_t layerIndex = (uint8_t)((int)gridY * 16 + (int)gridX);

    switch (faceDirection) {
    case NORTH:
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, NORTH, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, NORTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, NORTH, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, NORTH, layerIndex);
        break;
    case SOUTH:
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, SOUTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, SOUTH, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 1, SOUTH, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, SOUTH, layerIndex);
        break;
    case WEST:
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 1, 0, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, WEST, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 1, 1, WEST, layerIndex);
        break;
    case EAST:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 0, 0, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, EAST, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 0, 1, EAST, layerIndex);
        break;
    case TOP:
        vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 0, TOP, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 0, TOP, layerIndex);
        vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, TOP, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, TOP, layerIndex);
        break;
    case BOTTOM:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 1, BOTTOM, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 1, BOTTOM, layerIndex);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, BOTTOM, layerIndex);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, BOTTOM, layerIndex);
        break;
    default:
        break;
    }

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);
    currentVertex += 4;
}

// Billboard generator
void Chunk::generateBillboardFaces(const int x, const int y, const int z, const Block *block,
                                   unsigned int &currentVertex, const int subChunkIndex) {
    auto& vertices = billboardVertices[subChunkIndex];
    auto& indices = billboardIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    uint8_t layerIndex = (uint8_t)((int)block->sideMinY * 16 + (int)block->sideMinX);

    // Billboards use 2 crossed quads
    // Quad 1: (0,0,0) to (1,1,1)
    vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, layerIndex);
    vertices.emplace_back(x + wx, y + 1 + wy, z + wz, 0, 1, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + 1 + wz, 1, 1, layerIndex);

    // Quad 2: (0,0,1) to (1,1,0)
    vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, layerIndex);
    vertices.emplace_back(x + wx, y + 1 + wy, z + 1 + wz, 0, 1, layerIndex);
    vertices.emplace_back(x + 1 + wx, y + 1 + wy, z + wz, 1, 1, layerIndex);

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);

    indices.push_back(currentVertex + 4);
    indices.push_back(currentVertex + 7);
    indices.push_back(currentVertex + 5);
    indices.push_back(currentVertex + 4);
    indices.push_back(currentVertex + 6);
    indices.push_back(currentVertex + 7);

    currentVertex += 8;
}

// Liquid generator
void Chunk::generateLiquidFaces(const int x, const int y, const int z, const FACE_DIRECTION faceDirection,
                                const Block *block, unsigned int &currentVertex, char liquidTopValue, const int subChunkIndex) {
    auto& vertices = liquidVertices[subChunkIndex];
    auto& indices = liquidIndices[subChunkIndex];

    float wx = worldPos.x;
    float wy = worldPos.y;
    float wz = worldPos.z;

    uint8_t layerIndex = (uint8_t)((int)block->sideMinY * 16 + (int)block->sideMinX); 
    
    // Liquid height adjustment
    float topY = (faceDirection == TOP) ? 0.875f : 1.0f;
    if (liquidTopValue == 1) topY = 1.0f;

    switch (faceDirection) {
    case NORTH:
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 1, 1, NORTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, NORTH, layerIndex, liquidTopValue);
        break;
    case SOUTH:
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 0, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 1, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 0, 1, SOUTH, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, SOUTH, layerIndex, liquidTopValue);
        break;
    case WEST:
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 1, 1, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, WEST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 1, 0, WEST, layerIndex, liquidTopValue);
        break;
    case EAST:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 0, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 0, 0, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 1, EAST, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 0, 1, EAST, layerIndex, liquidTopValue);
        break;
    case TOP:
        vertices.emplace_back(x + wx, y + topY + wy, z + 1 + wz, 0, 0, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + 1 + wz, 1, 0, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + topY + wy, z + wz, 0, 1, TOP, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + topY + wy, z + wz, 1, 1, TOP, layerIndex, liquidTopValue);
        break;
    case BOTTOM:
        vertices.emplace_back(x + 1 + wx, y + wy, z + 1 + wz, 1, 1, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + 1 + wz, 0, 1, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + 1 + wx, y + wy, z + wz, 1, 0, BOTTOM, layerIndex, liquidTopValue);
        vertices.emplace_back(x + wx, y + wy, z + wz, 0, 0, BOTTOM, layerIndex, liquidTopValue);
        break;
    default:
        break;
    }

    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 3);
    indices.push_back(currentVertex + 1);
    indices.push_back(currentVertex + 0);
    indices.push_back(currentVertex + 2);
    indices.push_back(currentVertex + 3);

    currentVertex += 4;
}