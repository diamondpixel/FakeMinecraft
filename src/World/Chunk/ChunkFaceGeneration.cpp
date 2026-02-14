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

    uint16_t layerIndex;
    if (faceDirection == TOP) {
        layerIndex = block->topLayer;
    } else if (faceDirection == BOTTOM) {
        layerIndex = block->bottomLayer;
    } else {
        layerIndex = block->sideLayer;
    }

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

    uint16_t layerIndex = block->sideLayer;

    // Random rotation based on position to break the "X" grid pattern
    float randomAngle = (float)((x * 12345 + z * 67890 + y * 444) % 360) * (3.14159f / 180.0f);
    
    float cosA = cos(randomAngle);
    float sinA = sin(randomAngle);

    auto rotate = [&](float dx, float dz) {
        return glm::vec2(dx * cosA - dz * sinA, dx * sinA + dz * cosA);
    };

    // Calculate rotated corners relative to center
    // We normally go from 0 to 1. Center is 0.5, 0.5.
    // P00 (0,0) -> dl = -0.5, -0.5 (Bottom Left relative to center)
    // P11 (1,1) -> dl = 0.5, 0.5   (Top Right relative to center)
    // P01 (0,1) -> dl = -0.5, 0.5  (Top Left relative to center)
    // P10 (1,0) -> dl = 0.5, -0.5  (Bottom Right relative to center)
    
    glm::vec2 p00 = rotate(-0.5f, -0.5f);
    glm::vec2 p11 = rotate(0.5f, 0.5f);
    glm::vec2 p01 = rotate(-0.5f, 0.5f);
    glm::vec2 p10 = rotate(0.5f, -0.5f);

    float cx = x + wx + 0.5f;
    float cz = z + wz + 0.5f;

    // Quad 1: P00 to P11
    // Bottom Left (P00)
    vertices.emplace_back(cx + p00.x, y + wy, cz + p00.y, 0, 0, layerIndex);
    // Bottom Right (P11)
    vertices.emplace_back(cx + p11.x, y + wy, cz + p11.y, 1, 0, layerIndex);
    // Top Left (P00)
    vertices.emplace_back(cx + p00.x, y + 1 + wy, cz + p00.y, 0, 1, layerIndex);
    // Top Right (P11)
    vertices.emplace_back(cx + p11.x, y + 1 + wy, cz + p11.y, 1, 1, layerIndex);

    // Quad 2: P01 to P10
    // Bottom Left (P01)
    vertices.emplace_back(cx + p01.x, y + wy, cz + p01.y, 0, 0, layerIndex);
    // Bottom Right (P10)
    vertices.emplace_back(cx + p10.x, y + wy, cz + p10.y, 1, 0, layerIndex);
    // Top Left (P01)
    vertices.emplace_back(cx + p01.x, y + 1 + wy, cz + p01.y, 0, 1, layerIndex);
    // Top Right (P10)
    vertices.emplace_back(cx + p10.x, y + 1 + wy, cz + p10.y, 1, 1, layerIndex);

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

    uint16_t layerIndex = block->sideLayer; 
    
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