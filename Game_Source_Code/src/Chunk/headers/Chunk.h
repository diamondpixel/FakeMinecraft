#pragma once

#include <Shader.h>
#include <thread>
#include <vector>
#include <glm/glm.hpp>
#include "../Vertices/WorldVertex.h"
#include "../Vertices/FluidVertex.h"
#include "../Vertices/BillboardVertex.h""
#include "../headers/Block.h"
#include "ChunkPos.h"
#include "ChunkData.h"

struct LiquidFace {
    char x, y, z;
    FACE_DIRECTION dir;
    const Block* block;
    char waterTopValue;
};

class Chunk
{
public:
    Chunk(ChunkPos chunkPos, Shader* shader, Shader* waterShader);
    ~Chunk();

    void generateChunkMesh();
    void generateWorldFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block, unsigned int &currentVertex);
    void generateBillboardFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block, unsigned int &currentVertex);
    void generateLiquidFaces(int x, int y, int z, FACE_DIRECTION faceDirection, const Block *block, unsigned int &currentVertex, char liquidTopValue);
    void render(Shader* mainShader, Shader* billboardShader);
    void renderWater(Shader* shader);
    uint16_t getBlockAtPos(int x, int y, int z);
    void updateBlock(int x, int y, int z, uint32_t newBlock);
    void updateChunk();

public:
    ChunkData* chunkData;
    ChunkData* northData;
    ChunkData* southData;
    ChunkData* upData;
    ChunkData* downData;
    ChunkData* eastData;
    ChunkData* westData;
    ChunkPos chunkPos;
    bool ready;
    bool generated;

private:
    glm::vec3 worldPos;
    std::thread chunkThread;

    std::vector<WorldVertex> worldVertices;
    std::vector<unsigned int> worldIndices;
    std::vector<FluidVertex> liquidVertices;
    std::vector<unsigned int> liquidIndices;
    std::vector<BillboardVertex> billboardVertices;
    std::vector<unsigned int> billboardIndices;

    unsigned int worldVAO, waterVAO, billboardVAO;
    unsigned int worldVBO, worldEBO, liquidVBO, liquidEBO, billboardVBO, billboardEBO;
    unsigned int numTrianglesWorld, numTrianglesLiquid, numTrianglesBillboard;
    unsigned int modelLoc;
};
