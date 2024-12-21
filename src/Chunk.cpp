#include "headers/Chunk.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "headers/Blocks.h"
#include "headers/Planet.h"
#include "headers/WorldGen.h"

Chunk::Chunk(unsigned int chunkSize, glm::vec3 chunkPos) {
    this->chunkSize = chunkSize;
    this->chunkPos = chunkPos;
    worldPos = glm::vec3(chunkPos.x * chunkSize, chunkPos.y * chunkSize, chunkPos.z * chunkSize);

    ready = false;
    generated = false;
    chunkThread = std::thread(&Chunk::generateChunk, this);
}

Chunk::~Chunk() {
    if (chunkThread.joinable())
        chunkThread.join();

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteVertexArrays(1, &vao);
}

void Chunk::generateChunk() {
    WorldGen::generateChunkData(chunkPos.x, chunkPos.y, chunkPos.z, chunkSize, &chunkData);
    std::vector<unsigned int> northData, southData, eastData, westData, upData, downData;

    WorldGen::generateChunkData(chunkPos.x, chunkPos.y, chunkPos.z - 1, chunkSize, &northData);
    WorldGen::generateChunkData(chunkPos.x, chunkPos.y, chunkPos.z + 1, chunkSize, &southData);
    WorldGen::generateChunkData(chunkPos.x + 1, chunkPos.y, chunkPos.z, chunkSize, &eastData);
    WorldGen::generateChunkData(chunkPos.x - 1, chunkPos.y, chunkPos.z, chunkSize, &westData);
    WorldGen::generateChunkData(chunkPos.x, chunkPos.y + 1, chunkPos.z, chunkSize, &upData);
    WorldGen::generateChunkData(chunkPos.x, chunkPos.y - 1, chunkPos.z, chunkSize, &downData);

    unsigned int currentVertex = 0;
    for (char x = 0; x < chunkSize; x++) {
        for (char z = 0; z < chunkSize; z++) {
            for (char y = 0; y < chunkSize; y++) {
                int index = x * chunkSize * chunkSize + z * chunkSize + y;
                if (chunkData[index] == 0)
                    continue;

                const Block *block = &Blocks::blocks[chunkData[index]];

                if (block->billboard) {
                    vertices.emplace_back(x + .85355f, y + 0, z + .85355f, block->sideMinX, block->sideMinY, 6);
                    vertices.emplace_back(x + .14645f, y + 0, z + .14645f, block->sideMaxX, block->sideMinY, 6);
                    vertices.emplace_back(x + .85355f, y + 1, z + .85355f, block->sideMinX, block->sideMaxY, 6);
                    vertices.emplace_back(x + .14645f, y + 1, z + .14645f, block->sideMaxX, block->sideMaxY, 6);

                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 3);
                    indices.push_back(currentVertex + 1);
                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 2);
                    indices.push_back(currentVertex + 3);
                    currentVertex += 4;

                    vertices.emplace_back(x + .14645f, y + 0, z + .14645f, block->sideMinX, block->sideMinY, 6);
                    vertices.emplace_back(x + .85355f, y + 0, z + .85355f, block->sideMaxX, block->sideMinY, 6);
                    vertices.emplace_back(x + .14645f, y + 1, z + .14645f, block->sideMinX, block->sideMaxY, 6);
                    vertices.emplace_back(x + .85355f, y + 1, z + .85355f, block->sideMaxX, block->sideMaxY, 6);

                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 3);
                    indices.push_back(currentVertex + 1);
                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 2);
                    indices.push_back(currentVertex + 3);
                    currentVertex += 4;

                    vertices.emplace_back(x + .14645f, y + 0, z + .85355f, block->sideMinX, block->sideMinY, 6);
                    vertices.emplace_back(x + .85355f, y + 0, z + .14645f, block->sideMaxX, block->sideMinY, 6);
                    vertices.emplace_back(x + .14645f, y + 1, z + .85355f, block->sideMinX, block->sideMaxY, 6);
                    vertices.emplace_back(x + .85355f, y + 1, z + .14645f, block->sideMaxX, block->sideMaxY, 6);

                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 3);
                    indices.push_back(currentVertex + 1);
                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 2);
                    indices.push_back(currentVertex + 3);
                    currentVertex += 4;

                    vertices.emplace_back(x + .85355f, y + 0, z + .14645f, block->sideMinX, block->sideMinY, 6);
                    vertices.emplace_back(x + .14645f, y + 0, z + .85355f, block->sideMaxX, block->sideMinY, 6);
                    vertices.emplace_back(x + .85355f, y + 1, z + .14645f, block->sideMinX, block->sideMaxY, 6);
                    vertices.emplace_back(x + .14645f, y + 1, z + .85355f, block->sideMaxX, block->sideMaxY, 6);

                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 3);
                    indices.push_back(currentVertex + 1);
                    indices.push_back(currentVertex + 0);
                    indices.push_back(currentVertex + 2);
                    indices.push_back(currentVertex + 3);
                    currentVertex += 4;
                } else {
                    {
                        // North
                        int northBlock;
                        if (z > 0) {
                            int northIndex = x * chunkSize * chunkSize + (z - 1) * chunkSize + y;
                            northBlock = chunkData[northIndex];
                        } else {
                            int northIndex = x * chunkSize * chunkSize + (chunkSize - 1) * chunkSize + y;
                            northBlock = northData[northIndex];
                        }

                        const Block *northBlockType = &Blocks::blocks[northBlock];

                        if (northBlockType->transparent) {
                            vertices.emplace_back(x + 1, y + 0, z + 0, block->sideMinX, block->sideMinY, 0);
                            vertices.emplace_back(x + 0, y + 0, z + 0, block->sideMaxX, block->sideMinY, 0);
                            vertices.emplace_back(x + 1, y + 1, z + 0, block->sideMinX, block->sideMaxY, 0);
                            vertices.emplace_back(x + 0, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 0);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    } {
                        // South
                        int southBlock;
                        if (z < chunkSize - 1) {
                            int southIndex = x * chunkSize * chunkSize + (z + 1) * chunkSize + y;
                            southBlock = chunkData[southIndex];
                        } else {
                            int southIndex = x * chunkSize * chunkSize + 0 * chunkSize + y;
                            southBlock = southData[southIndex];
                        }

                        const Block *southBlockType = &Blocks::blocks[southBlock];

                        if (southBlockType->transparent) {
                            vertices.emplace_back(x + 0, y + 0, z + 1, block->sideMinX, block->sideMinY, 1);
                            vertices.emplace_back(x + 1, y + 0, z + 1, block->sideMaxX, block->sideMinY, 1);
                            vertices.emplace_back(x + 0, y + 1, z + 1, block->sideMinX, block->sideMaxY, 1);
                            vertices.emplace_back(x + 1, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 1);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    } {
                        // West
                        int westBlock;
                        if (x > 0) {
                            int blockIndex = (x - 1) * chunkSize * chunkSize + z * chunkSize + y;
                            westBlock = chunkData[blockIndex];
                        } else {
                            int blockIndex = (chunkSize - 1) * chunkSize * chunkSize + z * chunkSize + y;
                            westBlock = westData[blockIndex];
                        }

                        const Block *westBlockType = &Blocks::blocks[westBlock];

                        if (westBlockType->transparent) {
                            vertices.emplace_back(x + 0, y + 0, z + 0, block->sideMinX, block->sideMinY, 2);
                            vertices.emplace_back(x + 0, y + 0, z + 1, block->sideMaxX, block->sideMinY, 2);
                            vertices.emplace_back(x + 0, y + 1, z + 0, block->sideMinX, block->sideMaxY, 2);
                            vertices.emplace_back(x + 0, y + 1, z + 1, block->sideMaxX, block->sideMaxY, 2);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    } {
                        // East
                        int eastBlock;
                        if (x < chunkSize - 1) {
                            int blockIndex = (x + 1) * chunkSize * chunkSize + z * chunkSize + y;
                            eastBlock = chunkData[blockIndex];
                        } else {
                            int blockIndex = 0 * chunkSize * chunkSize + z * chunkSize + y;
                            eastBlock = eastData[blockIndex];
                        }

                        const Block *eastBlockType = &Blocks::blocks[eastBlock];

                        if (eastBlockType->transparent) {
                            vertices.emplace_back(x + 1, y + 0, z + 1, block->sideMinX, block->sideMinY, 3);
                            vertices.emplace_back(x + 1, y + 0, z + 0, block->sideMaxX, block->sideMinY, 3);
                            vertices.emplace_back(x + 1, y + 1, z + 1, block->sideMinX, block->sideMaxY, 3);
                            vertices.emplace_back(x + 1, y + 1, z + 0, block->sideMaxX, block->sideMaxY, 3);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    } {
                        // Bottom
                        int bottomBlock;
                        if (y > 0) {
                            int blockIndex = x * chunkSize * chunkSize + z * chunkSize + (y - 1);
                            bottomBlock = chunkData[blockIndex];
                        } else {
                            int blockIndex = x * chunkSize * chunkSize + z * chunkSize + (chunkSize - 1);
                            bottomBlock = downData[blockIndex];
                        }

                        const Block *bottomBlockType = &Blocks::blocks[bottomBlock];

                        if (bottomBlockType->transparent) {
                            vertices.emplace_back(x + 1, y + 0, z + 1, block->bottomMinX, block->bottomMinY, 4);
                            vertices.emplace_back(x + 0, y + 0, z + 1, block->bottomMaxX, block->bottomMinY, 4);
                            vertices.emplace_back(x + 1, y + 0, z + 0, block->bottomMinX, block->bottomMaxY, 4);
                            vertices.emplace_back(x + 0, y + 0, z + 0, block->bottomMaxX, block->bottomMaxY, 4);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    } {
                        // Top
                        int topBlock;
                        if (y < chunkSize - 1) {
                            int blockIndex = x * chunkSize * chunkSize + z * chunkSize + (y + 1);
                            topBlock = chunkData[blockIndex];
                        } else {
                            int blockIndex = x * chunkSize * chunkSize + z * chunkSize + 0;
                            topBlock = upData[blockIndex];
                        }

                        const Block *topBlockType = &Blocks::blocks[topBlock];

                        if (topBlockType->transparent) {
                            vertices.emplace_back(x + 0, y + 1, z + 1, block->topMinX, block->topMinY, 5);
                            vertices.emplace_back(x + 1, y + 1, z + 1, block->topMaxX, block->topMinY, 5);
                            vertices.emplace_back(x + 0, y + 1, z + 0, block->topMinX, block->topMaxY, 5);
                            vertices.emplace_back(x + 1, y + 1, z + 0, block->topMaxX, block->topMaxY, 5);

                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 3);
                            indices.push_back(currentVertex + 1);
                            indices.push_back(currentVertex + 0);
                            indices.push_back(currentVertex + 2);
                            indices.push_back(currentVertex + 3);
                            currentVertex += 4;
                        }
                    }
                }
            }
        }
    }

    generated = true;
}

void Chunk::render(unsigned int modelLoc) {
    if (!ready) {
        if (generated) {
            numTriangles = indices.size();

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, posX));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_BYTE, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, texGridX));
            glEnableVertexAttribArray(1);
            glVertexAttribIPointer(2, 1, GL_BYTE, sizeof(Vertex), (void *) offsetof(Vertex, direction));
            glEnableVertexAttribArray(2);

            glGenBuffers(1, &ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(),
                         GL_STATIC_DRAW);
            ready = true;
        }

        return;
    }

    //std::cout << "Rendering chunk " << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << '\n'
    //	<< "Chunk VAO: " << vertexArrayObject << '\n' << "Triangles: " << numTriangles << '\n';

    glBindVertexArray(vao);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, worldPos);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &(model[0][0]));
    glDrawElements(GL_TRIANGLES, numTriangles, GL_UNSIGNED_INT, 0);
}
