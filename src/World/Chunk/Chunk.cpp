#include "Chunk.h"
#include "ChunkUtils.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "Planet.h"
#include "WorldGen.h"

Chunk::Chunk(const ChunkPos chunkPos)
    : chunkPos(chunkPos) {
    worldPos = glm::vec3(static_cast<float>(chunkPos.x) * CHUNK_WIDTH,
                         static_cast<float>(chunkPos.y) * CHUNK_HEIGHT,
                         static_cast<float>(chunkPos.z) * CHUNK_WIDTH);

    // Initialize chunk-level culling data (for pre-cull)
    cullingCenter = worldPos + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);

    // Initialize sub-chunk culling data
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        const float subChunkY = worldPos.y + i * SUBCHUNK_HEIGHT;
        subChunks[i].cullingCenter = glm::vec3(
            worldPos.x + CHUNK_WIDTH * 0.5f,
            subChunkY + SUBCHUNK_HEIGHT * 0.5f,
            worldPos.z + CHUNK_WIDTH * 0.5f
        );
        subChunks[i].cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, SUBCHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    }

    ready = false;
    generated = false;
}

Chunk::~Chunk() {
    if (chunkThread.joinable())
        chunkThread.join();

    // Clean up merged VAOs
    if (mergedWorldVAO) {
        glDeleteBuffers(1, &mergedWorldVBO);
        glDeleteBuffers(1, &mergedWorldEBO);
        glDeleteVertexArrays(1, &mergedWorldVAO);
    }
    if (mergedBillboardVAO) {
        glDeleteBuffers(1, &mergedBillboardVBO);
        glDeleteBuffers(1, &mergedBillboardEBO);
        glDeleteVertexArrays(1, &mergedBillboardVAO);
    }
    if (mergedWaterVAO) {
        glDeleteBuffers(1, &mergedWaterVBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteVertexArrays(1, &mergedWaterVAO);
    }

    if (queryID != 0) {
        glDeleteQueries(1, &queryID);
    }
}

void Chunk::reset(ChunkPos newPos) {
    // Join any existing thread
    if (chunkThread.joinable()) {
        chunkThread.join();
    }

    // Clean up existing GL resources
    if (mergedWorldVAO) {
        glDeleteBuffers(1, &mergedWorldVBO);
        glDeleteBuffers(1, &mergedWorldEBO);
        glDeleteVertexArrays(1, &mergedWorldVAO);
        mergedWorldVAO = 0;
        mergedWorldVBO = 0;
        mergedWorldEBO = 0;
    }
    if (mergedBillboardVAO) {
        glDeleteBuffers(1, &mergedBillboardVBO);
        glDeleteBuffers(1, &mergedBillboardEBO);
        glDeleteVertexArrays(1, &mergedBillboardVAO);
        mergedBillboardVAO = 0;
        mergedBillboardVBO = 0;
        mergedBillboardEBO = 0;
    }
    if (mergedWaterVAO) {
        glDeleteBuffers(1, &mergedWaterVBO);
        glDeleteBuffers(1, &mergedWaterEBO);
        glDeleteVertexArrays(1, &mergedWaterVAO);
        mergedWaterVAO = 0;
        mergedWaterVBO = 0;
        mergedWaterEBO = 0;
    }

    // Reset position and state
    chunkPos = newPos;
    worldPos = glm::vec3(
        static_cast<float>(newPos.x) * CHUNK_WIDTH,
        static_cast<float>(newPos.y) * CHUNK_HEIGHT,
        static_cast<float>(newPos.z) * CHUNK_WIDTH
    );

    cullingCenter = worldPos + glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
    cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, CHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);

    // Reset sub-chunk culling
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        float subChunkY = worldPos.y + i * SUBCHUNK_HEIGHT;
        subChunks[i].cullingCenter = glm::vec3(
            worldPos.x + CHUNK_WIDTH * 0.5f,
            subChunkY + SUBCHUNK_HEIGHT * 0.5f,
            worldPos.z + CHUNK_WIDTH * 0.5f
        );
        subChunks[i].cullingExtents = glm::vec3(CHUNK_WIDTH * 0.5f, SUBCHUNK_HEIGHT * 0.5f, CHUNK_WIDTH * 0.5f);
        subChunks[i].ready = false;
    }

    // Reset data pointers
    chunkData = nullptr;
    northData = nullptr;
    southData = nullptr;
    upData = nullptr;
    downData = nullptr;
    eastData = nullptr;
    westData = nullptr;

    // Reset flags
    ready = false;
    generated = false;
    occlusionVisible = true;
    queryIssued = false;
    occlusionCounter = 0;
    occlusionScore = 1.0f;
    cachedDistSq = 0.0f;
    mergedWorldTriangles = 0;
    mergedBillboardTriangles = 0;
    mergedWaterTriangles = 0;
    listIndex = 0;

    // Clear vertex buffers
    for (int i = 0; i < NUM_SUBCHUNKS; ++i) {
        worldVertices[i].clear();
        worldIndices[i].clear();
        billboardVertices[i].clear();
        billboardIndices[i].clear();
        liquidVertices[i].clear();
        liquidIndices[i].clear();
    }
}

uint16_t Chunk::getBlockAtPos(int x, int y, int z) const {
    return ready ? chunkData->getBlock(x, y, z) : 0;
}

void Chunk::updateBlock(int x, int y, int z, uint8_t newBlock) {
    chunkData->setBlock(x, y, z, newBlock);

    // Regenerate mesh data
    generateChunkMesh();

    // Use uploadMesh() to regenerate the merged buffers correctly.
    ready = false;
    generated = true;
    uploadMesh();

    // Update neighboring chunks if at boundaries
    if (x == 0) {
        Chunk *westChunk = Planet::planet->getChunk({chunkPos.x - 1, chunkPos.y, chunkPos.z});
        if (westChunk) westChunk->updateChunk();
    } else if (x == CHUNK_WIDTH - 1) {
        Chunk *eastChunk = Planet::planet->getChunk({chunkPos.x + 1, chunkPos.y, chunkPos.z});
        if (eastChunk) eastChunk->updateChunk();
    }

    if (y == 0) {
        Chunk *downChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y - 1, chunkPos.z});
        if (downChunk) downChunk->updateChunk();
    } else if (y == CHUNK_HEIGHT - 1) {
        Chunk *upChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y + 1, chunkPos.z});
        if (upChunk) upChunk->updateChunk();
    }

    if (z == 0) {
        Chunk *northChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z - 1});
        if (northChunk) northChunk->updateChunk();
    } else if (z == CHUNK_WIDTH - 1) {
        Chunk *southChunk = Planet::planet->getChunk({chunkPos.x, chunkPos.y, chunkPos.z + 1});
        if (southChunk) southChunk->updateChunk();
    }
}

void Chunk::updateChunk() {
    generateChunkMesh();
    ready = false;
    generated = true;
    uploadMesh();
}
