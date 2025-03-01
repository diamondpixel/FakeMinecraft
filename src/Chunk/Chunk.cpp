#include "headers/Chunk.h"

#include <iostream>
#include <Shader.h>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../headers/Planet.h"
#include "../headers/WorldGen.h"
#include "../headers/Blocks.h"

Chunk::Chunk(ChunkPos chunkPos, Shader *shader, Shader *waterShader)
    : chunkPos(chunkPos) {
    worldPos = glm::vec3(chunkPos.x * (float) CHUNK_SIZE, chunkPos.y * (float) CHUNK_SIZE,
                         chunkPos.z * (float) CHUNK_SIZE);

    ready = false;
    generated = false;
}

Chunk::~Chunk() {
    if (chunkThread.joinable())
        chunkThread.join();

    glDeleteBuffers(1, &worldVBO);
    glDeleteBuffers(1, &worldEBO);
    glDeleteVertexArrays(1, &worldVAO);

    glDeleteBuffers(1, &liquidVBO);
    glDeleteBuffers(1, &liquidEBO);
    glDeleteVertexArrays(1, &waterVAO);

    glDeleteBuffers(1, &billboardVBO);
    glDeleteBuffers(1, &billboardEBO);
    glDeleteVertexArrays(1, &billboardVAO);
}

void Chunk::generateChunkMesh() {
    worldVertices.clear();
    worldIndices.clear();
    liquidVertices.clear();
    liquidIndices.clear();
    billboardVertices.clear();
    billboardIndices.clear();
    numTrianglesWorld = 0;
    numTrianglesLiquid = 0;
    numTrianglesBillboard = 0;

    unsigned int currentVertex = 0;
    unsigned int currentLiquidVertex = 0;
    unsigned int currentBillboardVertex = 0;
    for (char x = 0; x < CHUNK_SIZE; x++) {
        for (char z = 0; z < CHUNK_SIZE; z++) {
            for (char y = 0; y < CHUNK_SIZE; y++) {
                if (chunkData->getBlock(x, y, z) == 0)
                    continue;

                const Block *block = &Blocks::blocks[chunkData->getBlock(x, y, z)];

                int topBlock;
                if (y < CHUNK_SIZE - 1) {
                    topBlock = chunkData->getBlock(x, y + 1, z);
                } else {
                    topBlock = upData->getBlock(x, 0, z);
                }

                const Block *topBlockType = &Blocks::blocks[topBlock];
                char waterTopValue = (topBlockType->blockType == Block::TRANSPARENT || topBlockType->blockType ==
                                      Block::SOLID)
                                         ? 1
                                         : 0;

                if (block->blockType == Block::BILLBOARD) {
                    generateBillboardFaces(x, y, z, PLACEHOLDER_VALUE, block, currentBillboardVertex);
                } else {

                    // North
                    {
                        // Initialize blocks with AIR by default
                        int northBlock = Blocks::AIR;
                        int northTopBlock = Blocks::AIR;

                        // Determine north block and top block based on position
                        if (z > 0) {
                            // Access north block and top block within the same chunk
                            northBlock = chunkData->getBlock(x, y, z - 1);
                            northTopBlock = (y + 1 < CHUNK_SIZE)
                                                ? chunkData->getBlock(x, y + 1, z - 1)
                                                : upData->getBlock(x, 0, z - 1);
                        } else if (northData) {
                            // Access north block and top block in neighboring chunk
                            northBlock = northData->getBlock(x, y, CHUNK_SIZE - 1);
                            northTopBlock = (y + 1 < CHUNK_SIZE)
                                                ? northData->getBlock(x, y + 1, CHUNK_SIZE - 1)
                                                : Blocks::AIR;
                        }

                        // Retrieve block types once to avoid repeated access
                        const Block *northBlockType = &Blocks::blocks[northBlock];
                        const Block *northTopBlockType = &Blocks::blocks[northTopBlock];

                        // Precompute conditions for rendering
                        bool isNorthBlockLiquid = (northBlockType->blockType == Block::LIQUID);
                        bool isBlockLiquid = (block->blockType == Block::LIQUID);

                        // Combine rendering conditions into a single check
                        bool shouldRender =
                                (northBlockType->blockType == Block::LEAVES) ||
                                (northBlockType->blockType == Block::TRANSPARENT) ||
                                (northBlockType->blockType == Block::BILLBOARD) ||
                                (isNorthBlockLiquid && !isBlockLiquid) ||
                                (isBlockLiquid && isNorthBlockLiquid && waterTopValue == 0 && northTopBlockType->
                                 blockType != Block::LIQUID);

                        // Render the face if conditions are met
                        if (shouldRender) {
                            if (isBlockLiquid) {
                                generateLiquidFaces(x, y, z, NORTH, block, currentLiquidVertex, waterTopValue);
                            } else {
                                generateWorldFaces(x, y, z, NORTH, block, currentVertex);
                            }
                        }
                    }

                    // South
                    {
                        // Initialize blocks with AIR by default
                        int southBlock = Blocks::AIR;
                        int southTopBlock = Blocks::AIR;

                        // Determine south block and top block based on position
                        if (z < CHUNK_SIZE - 1) {
                            // Access south block and top block within the same chunk
                            southBlock = chunkData->getBlock(x, y, z + 1);
                            southTopBlock = (y + 1 < CHUNK_SIZE)
                                                ? chunkData->getBlock(x, y + 1, z + 1)
                                                : upData->getBlock(x, 0, z + 1);
                        } else if (southData) {
                            // Access south block and top block in neighboring chunk
                            southBlock = southData->getBlock(x, y, 0);
                            southTopBlock = (y + 1 < CHUNK_SIZE)
                                                ? southData->getBlock(x, y + 1, 0)
                                                : Blocks::AIR;
                        }
                        // Retrieve block types once to avoid repeated access
                        const Block *southBlockType = &Blocks::blocks[southBlock];
                        const Block *southTopBlockType = &Blocks::blocks[southTopBlock];

                        // Precompute conditions for rendering
                        bool isSouthBlockLiquid = (southBlockType->blockType == Block::LIQUID);
                        bool isBlockLiquid = (block->blockType == Block::LIQUID);

                        // Combine rendering conditions into a single check
                        bool shouldRender =
                                (southBlockType->blockType == Block::LEAVES) ||
                                (southBlockType->blockType == Block::TRANSPARENT) ||
                                (southBlockType->blockType == Block::BILLBOARD) ||
                                (isSouthBlockLiquid && !isBlockLiquid) ||
                                (isBlockLiquid && isSouthBlockLiquid && waterTopValue == 0 && southTopBlockType->
                                 blockType != Block::LIQUID);

                        // Render the face if conditions are met
                        if (shouldRender) {
                            if (isBlockLiquid) {
                                generateLiquidFaces(x, y, z, SOUTH, block, currentLiquidVertex, waterTopValue);
                            } else {
                                generateWorldFaces(x, y, z, SOUTH, block, currentVertex);
                            }
                        }
                    }

                    // West
                    {
                        // Initialize blocks with AIR by default
                        int westBlock = Blocks::AIR;
                        int westTopBlock = Blocks::AIR;

                        // Determine west block and top block based on position
                        if (x > 0) {
                            westBlock = chunkData->getBlock(x - 1, y, z);
                            westTopBlock = (y + 1 < CHUNK_SIZE)
                                               ? chunkData->getBlock(x - 1, y + 1, z)
                                               : upData->getBlock(x - 1, 0, z);
                        } else {
                            // At chunk boundary, use neighboring chunk
                            westBlock = westData->getBlock(CHUNK_SIZE - 1, y, z);
                            westTopBlock = (y + 1 < CHUNK_SIZE)
                                               ? westData->getBlock(CHUNK_SIZE - 1, y + 1, z)
                                               : Blocks::AIR;
                        }

                        // Retrieve block types once to avoid repeated access
                        const Block *westBlockType = &Blocks::blocks[westBlock];
                        const Block *westTopBlockType = &Blocks::blocks[westTopBlock];

                        // Precompute conditions for rendering
                        bool isWestBlockLiquid = (westBlockType->blockType == Block::LIQUID);
                        bool isBlockLiquid = (block->blockType == Block::LIQUID);

                        // Combine rendering conditions into a single check
                        bool shouldRender =
                                (westBlockType->blockType == Block::LEAVES) ||
                                (westBlockType->blockType == Block::TRANSPARENT) ||
                                (westBlockType->blockType == Block::BILLBOARD) ||
                                (isWestBlockLiquid && !isBlockLiquid) ||
                                (isBlockLiquid && isWestBlockLiquid && waterTopValue == 0 && westTopBlockType->blockType
                                 != Block::LIQUID);

                        // Render the face if conditions are met
                        if (shouldRender) {
                            if (isBlockLiquid) {
                                generateLiquidFaces(x, y, z, WEST, block, currentLiquidVertex, waterTopValue);
                            } else {
                                generateWorldFaces(x, y, z, WEST, block, currentVertex);
                            }
                        }
                    }


                    // East
                    {
                        int eastBlock = Blocks::AIR;
                        int eastTopBlock = Blocks::AIR;

                        // Determine east block and top block based on position and neighboring chunks
                        if (x < CHUNK_SIZE - 1) {
                            eastBlock = chunkData->getBlock(x + 1, y, z);
                            eastTopBlock = (y + 1 < CHUNK_SIZE)
                                               ? chunkData->getBlock(x + 1, y + 1, z)
                                               : upData->getBlock(x + 1, 0, z);
                        } else {
                            // At chunk boundary, use neighboring chunk
                            eastBlock = eastData->getBlock(0, y, z);
                            eastTopBlock = (y + 1 < CHUNK_SIZE) ? eastData->getBlock(0, y + 1, z) : Blocks::AIR;
                        }

                        // Retrieve block types
                        const Block *eastBlockType = &Blocks::blocks[eastBlock];
                        const Block *eastTopBlockType = &Blocks::blocks[eastTopBlock];

                        // Precompute reusable flags
                        bool isEastBlockLiquid = (eastBlockType->blockType == Block::LIQUID);
                        bool isBlockLiquid = (block->blockType == Block::LIQUID);
                        bool isEastBlockRenderable =
                                (eastBlockType->blockType == Block::LEAVES) ||
                                (eastBlockType->blockType == Block::TRANSPARENT) ||
                                (eastBlockType->blockType == Block::BILLBOARD) ||
                                (isEastBlockLiquid && !isBlockLiquid) ||
                                (isBlockLiquid && isEastBlockLiquid && waterTopValue == 0 && eastTopBlockType->blockType
                                 != Block::LIQUID);

                        if (isEastBlockRenderable) {
                            if (isBlockLiquid) {
                                generateLiquidFaces(x, y, z, EAST, block, currentLiquidVertex, waterTopValue);
                            } else {
                                generateWorldFaces(x, y, z, EAST, block, currentVertex);
                            }
                        }
                    }
                    // Bottom
                    {
                        int bottomBlock;
                        if (y > 0) {
                            bottomBlock = chunkData->getBlock(x, y - 1, z);
                        } else {
                            bottomBlock = downData->getBlock(x, CHUNK_SIZE - 1, z);
                        }

                        const Block *bottomBlockType = &Blocks::blocks[bottomBlock];

                        if (bottomBlockType->blockType == Block::LEAVES
                            || bottomBlockType->blockType == Block::TRANSPARENT
                            || bottomBlockType->blockType == Block::BILLBOARD
                            || (bottomBlockType->blockType == Block::LIQUID && block->blockType != Block::LIQUID)) {
                            if (block->blockType == Block::LIQUID) {
                                generateLiquidFaces(x, y, z, BOTTOM,
                                                    block,
                                                    currentLiquidVertex,
                                                    waterTopValue);
                            } else {
                                generateWorldFaces(x, y, z, BOTTOM, block, currentVertex);
                            }
                        }
                    }

                    // Top
                    {
                        if (block->blockType == Block::LIQUID) {
                            if (topBlockType->blockType != Block::LIQUID) {
                                generateLiquidFaces(x, y, z, TOP,
                                                    block,
                                                    currentLiquidVertex,
                                                    waterTopValue);
                            }
                        } else if (topBlockType->blockType == Block::LEAVES
                                   || topBlockType->blockType == Block::TRANSPARENT
                                   || topBlockType->blockType == Block::BILLBOARD
                                   || topBlockType->blockType == Block::LIQUID) {
                            generateWorldFaces(x, y, z, TOP, block, currentVertex);
                        }
                    }
                }
            }
        }
    }
    generated = true;
}

void Chunk::generateWorldFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block,
                               unsigned int &currentVertex) {
    switch (faceDirection) {
        case NORTH: // North face
            worldVertices.emplace_back(x + 1, y + 0, z + 0, block->sideMinX, block->sideMinY, 0);
            worldVertices.emplace_back(x + 0, y + 0, z + 0, block->sideMaxX, block->sideMinY, 0);
            worldVertices.emplace_back(x + 1, y + 1, z + 0, block->sideMinX, block->sideMaxY, 0);
            worldVertices.emplace_back(x + 0, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 0);
            break;
        case SOUTH: // South face
            worldVertices.emplace_back(x + 0, y + 0, z + 1, block->sideMinX, block->sideMinY, 1);
            worldVertices.emplace_back(x + 1, y + 0, z + 1, block->sideMaxX, block->sideMinY, 1);
            worldVertices.emplace_back(x + 0, y + 1, z + 1, block->sideMinX, block->sideMaxY, 1);
            worldVertices.emplace_back(x + 1, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 1);
            break;
        case WEST: // West face
            worldVertices.emplace_back(x + 0, y + 0, z + 0, block->sideMinX, block->sideMinY, 2);
            worldVertices.emplace_back(x + 0, y + 0, z + 1, block->sideMaxX, block->sideMinY, 2);
            worldVertices.emplace_back(x + 0, y + 1, z + 0, block->sideMinX, block->sideMaxY, 2);
            worldVertices.emplace_back(x + 0, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 2);
            break;
        case EAST: // East face
            worldVertices.emplace_back(x + 1, y + 0, z + 1, block->sideMinX, block->sideMinY, 3);
            worldVertices.emplace_back(x + 1, y + 0, z + 0, block->sideMaxX, block->sideMinY, 3);
            worldVertices.emplace_back(x + 1, y + 1, z + 1, block->sideMinX, block->sideMaxY, 3);
            worldVertices.emplace_back(x + 1, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 3);
            break;
        case BOTTOM: //Bottom Face
            worldVertices.emplace_back(x + 1, y + 0, z + 1, block->bottomMinX, block->bottomMinY, 4);
            worldVertices.emplace_back(x + 0, y + 0, z + 1, block->bottomMaxX, block->bottomMinY, 4);
            worldVertices.emplace_back(x + 1, y + 0, z + 0, block->bottomMinX, block->bottomMaxY, 4);
            worldVertices.emplace_back(x + 0, y + 0, z + 0, block->bottomMaxX, block->bottomMaxY, 4);
            break;
        case TOP: //Top Face
            worldVertices.emplace_back(x + 0, y + 1, z + 1, block->topMinX, block->topMinY, 5);
            worldVertices.emplace_back(x + 1, y + 1, z + 1, block->topMaxX, block->topMinY, 5);
            worldVertices.emplace_back(x + 0, y + 1, z + 0, block->topMinX, block->topMaxY, 5);
            worldVertices.emplace_back(x + 1, y + 1, z + 0, block->topMaxX, block->topMaxY, 5);
            break;
        default: break;
    }
    // Add indices for the face
    worldIndices.push_back(currentVertex + 0);
    worldIndices.push_back(currentVertex + 3);
    worldIndices.push_back(currentVertex + 1);
    worldIndices.push_back(currentVertex + 0);
    worldIndices.push_back(currentVertex + 2);
    worldIndices.push_back(currentVertex + 3);

    // Update currentVertex count
    currentVertex += 4;
}

void Chunk::generateBillboardFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block,
                                   unsigned int &currentVertex) {
    billboardVertices.emplace_back(x + .85355f, y + 0, z + .85355f, block->sideMinX, block->sideMinY);
    billboardVertices.emplace_back(x + .14645f, y + 0, z + .14645f, block->sideMaxX, block->sideMinY);
    billboardVertices.emplace_back(x + .85355f, y + 1, z + .85355f, block->sideMinX, block->sideMaxY);
    billboardVertices.emplace_back(x + .14645f, y + 1, z + .14645f, block->sideMaxX, block->sideMaxY);

    billboardIndices.push_back(currentVertex + 0);
    billboardIndices.push_back(currentVertex + 3);
    billboardIndices.push_back(currentVertex + 1);
    billboardIndices.push_back(currentVertex + 0);
    billboardIndices.push_back(currentVertex + 2);
    billboardIndices.push_back(currentVertex + 3);
    currentVertex += 4;

    billboardVertices.emplace_back(x + .14645f, y + 0, z + .85355f, block->sideMinX, block->sideMinY);
    billboardVertices.emplace_back(x + .85355f, y + 0, z + .14645f, block->sideMaxX, block->sideMinY);
    billboardVertices.emplace_back(x + .14645f, y + 1, z + .85355f, block->sideMinX, block->sideMaxY);
    billboardVertices.emplace_back(x + .85355f, y + 1, z + .14645f, block->sideMaxX, block->sideMaxY);

    billboardIndices.push_back(currentVertex + 0);
    billboardIndices.push_back(currentVertex + 3);
    billboardIndices.push_back(currentVertex + 1);
    billboardIndices.push_back(currentVertex + 0);
    billboardIndices.push_back(currentVertex + 2);
    billboardIndices.push_back(currentVertex + 3);
    currentVertex += 4;
}

void Chunk::generateLiquidFaces(int x, int y, int z, FACE_DIRECTION faceDirection,
                                const Block *block,
                                unsigned int &currentVertex, char liquidTopValue) {
    switch (faceDirection) {
        case NORTH: // North face
            liquidVertices.emplace_back(x + 1, y + 0, z + 0, block->sideMinX, block->sideMinY, 0, 0);
            liquidVertices.emplace_back(x + 0, y + 0, z + 0, block->sideMaxX, block->sideMinY, 0, 0);
            liquidVertices.emplace_back(x + 1, y + 1, z + 0, block->sideMinX, block->sideMaxY, 0, liquidTopValue);
            liquidVertices.emplace_back(x + 0, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 0, liquidTopValue);
            break;
        case SOUTH: // South face
            liquidVertices.emplace_back(x + 0, y + 0, z + 1, block->sideMinX, block->sideMinY, 1, 0);
            liquidVertices.emplace_back(x + 1, y + 0, z + 1, block->sideMaxX, block->sideMinY, 1, 0);
            liquidVertices.emplace_back(x + 0, y + 1, z + 1, block->sideMinX, block->sideMaxY, 1, liquidTopValue);
            liquidVertices.emplace_back(x + 1, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 1, liquidTopValue);
            break;
        case WEST: // West face
            liquidVertices.emplace_back(x + 0, y + 0, z + 0, block->sideMinX, block->sideMinY, 2, 0);
            liquidVertices.emplace_back(x + 0, y + 0, z + 1, block->sideMaxX, block->sideMinY, 2, 0);
            liquidVertices.emplace_back(x + 0, y + 1, z + 0, block->sideMinX, block->sideMaxY, 2, liquidTopValue);
            liquidVertices.emplace_back(x + 0, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 2, liquidTopValue);
            break;
        case EAST: // East face
            liquidVertices.emplace_back(x + 1, y + 0, z + 1, block->sideMinX, block->sideMinY, 3, 0);
            liquidVertices.emplace_back(x + 1, y + 0, z + 0, block->sideMaxX, block->sideMinY, 3, 0);
            liquidVertices.emplace_back(x + 1, y + 1, z + 1, block->sideMinX, block->sideMaxY, 3, liquidTopValue);
            liquidVertices.emplace_back(x + 1, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 3, liquidTopValue);
            break;
        case BOTTOM: //Bottom Face
            liquidVertices.emplace_back(x + 1, y + 0, z + 1, block->bottomMinX, block->bottomMinY, 4, 0);
            liquidVertices.emplace_back(x + 0, y + 0, z + 1, block->bottomMaxX, block->bottomMinY, 4, 0);
            liquidVertices.emplace_back(x + 1, y + 0, z + 0, block->bottomMinX, block->bottomMaxY, 4, 0);
            liquidVertices.emplace_back(x + 0, y + 0, z + 0, block->bottomMaxX, block->bottomMaxY, 4, 0);
            break;
        case TOP: //Top Face
            liquidVertices.emplace_back(x + 0, y + 1, z + 1, block->topMinX, block->topMinY, 5, 1);
            liquidVertices.emplace_back(x + 1, y + 1, z + 1, block->topMaxX, block->topMinY, 5, 1);
            liquidVertices.emplace_back(x + 0, y + 1, z + 0, block->topMinX, block->topMaxY, 5, 1);
            liquidVertices.emplace_back(x + 1, y + 1, z + 0, block->topMaxX, block->topMaxY, 5, 1);
        //-------------------------------------------------------------------------------------------------------------------------
            liquidVertices.emplace_back(x + 1, y + 1, z + 1, block->topMinX, block->topMinY, 5, 1);
            liquidVertices.emplace_back(x + 0, y + 1, z + 1, block->topMaxX, block->topMinY, 5, 1);
            liquidVertices.emplace_back(x + 1, y + 1, z + 0, block->topMinX, block->topMaxY, 5, 1);
            liquidVertices.emplace_back(x + 0, y + 1, z + 0, block->topMaxX, block->topMaxY, 5, 1);
        //-------------------------------------------------------------------------------------------------------------------------
            liquidIndices.push_back(currentVertex + 0);
            liquidIndices.push_back(currentVertex + 3);
            liquidIndices.push_back(currentVertex + 1);
            liquidIndices.push_back(currentVertex + 0);
            liquidIndices.push_back(currentVertex + 2);
            liquidIndices.push_back(currentVertex + 3);
        //-------------------------------------------------------------------------------------------------------------------------

            currentVertex += 4;
            break;
        default: break;
    }
    // Add indices for the face
    liquidIndices.push_back(currentVertex + 0);
    liquidIndices.push_back(currentVertex + 3);
    liquidIndices.push_back(currentVertex + 1);
    liquidIndices.push_back(currentVertex + 0);
    liquidIndices.push_back(currentVertex + 2);
    liquidIndices.push_back(currentVertex + 3);

    // Update currentVertex count
    currentVertex += 4;
}

void Chunk::render(Shader *mainShader, Shader *billboardShader) {
    if (!ready) {
        if (generated) {
            // Solid
            numTrianglesWorld = worldIndices.size();

            glGenVertexArrays(1, &worldVAO);
            glGenBuffers(1, &worldVBO);
            glGenBuffers(1, &worldEBO);

            glBindVertexArray(worldVAO);

            glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
            glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(Vertex), worldVertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, worldIndices.size() * sizeof(unsigned int), worldIndices.data(),
                         GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_BYTE, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, posX));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, texGridX));
            glEnableVertexAttribArray(1);
            glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(Vertex), (void *) offsetof(Vertex, direction));
            glEnableVertexAttribArray(2);

            // Water
            numTrianglesLiquid = liquidIndices.size();

            glGenVertexArrays(1, &waterVAO);
            glGenBuffers(1, &liquidVBO);
            glGenBuffers(1, &liquidEBO);

            glBindVertexArray(waterVAO);

            glBindBuffer(GL_ARRAY_BUFFER, liquidVBO);
            glBufferData(GL_ARRAY_BUFFER, liquidVertices.size() * sizeof(FluidVertex), liquidVertices.data(),
                         GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, liquidEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, liquidIndices.size() * sizeof(unsigned int), liquidIndices.data(),
                         GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_BYTE, GL_FALSE, sizeof(FluidVertex), (void *) offsetof(FluidVertex, posX));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(FluidVertex),
                                  (void *) offsetof(FluidVertex, texGridX));
            glEnableVertexAttribArray(1);
            glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(FluidVertex), (void *) offsetof(FluidVertex, direction));
            glEnableVertexAttribArray(2);
            glVertexAttribIPointer(3, 1, GL_BYTE, sizeof(FluidVertex), (void *) offsetof(FluidVertex, top));
            glEnableVertexAttribArray(3);
            ready = true;

            // Billboard
            numTrianglesBillboard = billboardIndices.size();

            glGenVertexArrays(1, &billboardVAO);
            glGenBuffers(1, &billboardVBO);
            glGenBuffers(1, &billboardEBO);

            glBindVertexArray(billboardVAO);

            glBindBuffer(GL_ARRAY_BUFFER, billboardVBO);
            glBufferData(GL_ARRAY_BUFFER, billboardVertices.size() * sizeof(BillboardVertex), billboardVertices.data(),
                         GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, billboardEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, billboardIndices.size() * sizeof(unsigned int),
                         billboardIndices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BillboardVertex),
                                  (void *) offsetof(BillboardVertex, posX));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(BillboardVertex),
                                  (void *) offsetof(BillboardVertex, texGridX));
            glEnableVertexAttribArray(1);
            ready = true;
        }

        return;
    }

    //std::cout << "Rendering chunk " << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << '\n'
    //<< " Triangles: " << numTrianglesBillboard + numTrianglesWorld << '\n';


    // Calculate model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, worldPos);

    // Render main mesh
    mainShader->use();

    modelLoc = glGetUniformLocation(mainShader->program, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(worldVAO);
    glDrawElements(GL_TRIANGLES, numTrianglesWorld, GL_UNSIGNED_INT, 0);

    // Render billboard mesh
    billboardShader->use();

    modelLoc = glGetUniformLocation(billboardShader->program, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glDisable(GL_CULL_FACE);
    glBindVertexArray(billboardVAO);
    glDrawElements(GL_TRIANGLES, numTrianglesBillboard, GL_UNSIGNED_INT, 0);
    glEnable(GL_CULL_FACE);
}

void Chunk::renderWater(Shader *shader) {
    if (!ready)
        return;

    modelLoc = glGetUniformLocation(shader->program, "model");
    glBindVertexArray(waterVAO);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, worldPos);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES, numTrianglesLiquid, GL_UNSIGNED_INT, 0);
}

uint16_t Chunk::getBlockAtPos(int x, int y, int z) {
    if (!ready)
        return 0;

    return chunkData->getBlock(x, y, z);
}

void Chunk::updateBlock(int x, int y, int z, uint32_t newBlock) {
    chunkData->setBlock(x, y, z, newBlock);

    generateChunkMesh();

    // Main
    numTrianglesWorld = worldIndices.size();

    glBindVertexArray(worldVAO);

    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(WorldVertex), worldVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, worldIndices.size() * sizeof(unsigned int), worldIndices.data(),
                 GL_STATIC_DRAW);

    // Liquid
    numTrianglesLiquid = liquidIndices.size();

    glBindVertexArray(waterVAO);

    glBindBuffer(GL_ARRAY_BUFFER, liquidVBO);
    glBufferData(GL_ARRAY_BUFFER, liquidVertices.size() * sizeof(FluidVertex), liquidVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, liquidEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, liquidIndices.size() * sizeof(unsigned int), liquidIndices.data(),
                 GL_STATIC_DRAW);

    // Billboard
    numTrianglesBillboard = billboardIndices.size();;

    glBindVertexArray(billboardVAO);

    glBindBuffer(GL_ARRAY_BUFFER, billboardVBO);
    glBufferData(GL_ARRAY_BUFFER, billboardVertices.size() * sizeof(BillboardVertex), billboardVertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, billboardEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, billboardIndices.size() * sizeof(unsigned int), billboardIndices.data(),
                 GL_STATIC_DRAW);


    if (x == 0) {
        Chunk *westChunk = Planet::planet->getChunk({chunkPos.x - 1, chunkPos.y, chunkPos.z});
        if (westChunk != nullptr)
            westChunk->updateChunk();
    } else if (x == CHUNK_SIZE - 1) {
        Chunk *eastChunk = Planet::planet->getChunk({chunkPos.x + 1, chunkPos.y, chunkPos.z});
        if (eastChunk != nullptr)
            eastChunk->updateChunk();
    }

    if (y == 0) {
        Chunk *downChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y - 1, chunkPos.z});
        if (downChunk != nullptr)
            downChunk->updateChunk();
    } else if (y == CHUNK_SIZE - 1) {
        Chunk *upChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y + 1, chunkPos.z});
        if (upChunk != nullptr)
            upChunk->updateChunk();
    }

    if (z == 0) {
        Chunk *northChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z - 1});
        if (northChunk != nullptr)
            northChunk->updateChunk();
    } else if (z == CHUNK_SIZE - 1) {
        Chunk *southChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z + 1});
        if (southChunk != nullptr)
            southChunk->updateChunk();
    }
}

void Chunk::updateChunk() {
    generateChunkMesh();

    // Main
    numTrianglesWorld = worldIndices.size();

    glBindVertexArray(worldVAO);

    glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(WorldVertex), worldVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, worldIndices.size() * sizeof(unsigned int), worldIndices.data(),
                 GL_STATIC_DRAW);

    // Water
    numTrianglesLiquid = liquidIndices.size();

    glBindVertexArray(waterVAO);

    glBindBuffer(GL_ARRAY_BUFFER, liquidVBO);
    glBufferData(GL_ARRAY_BUFFER, liquidVertices.size() * sizeof(FluidVertex), liquidVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, liquidEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, liquidIndices.size() * sizeof(unsigned int), liquidIndices.data(),
                 GL_STATIC_DRAW);
    numTrianglesBillboard = billboardIndices.size();

    glBindVertexArray(billboardVAO);

    glBindBuffer(GL_ARRAY_BUFFER, billboardVBO);
    glBufferData(GL_ARRAY_BUFFER, billboardVertices.size() * sizeof(BillboardVertex), billboardVertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, billboardEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, billboardIndices.size() * sizeof(unsigned int), billboardIndices.data(),
                 GL_STATIC_DRAW);
}
