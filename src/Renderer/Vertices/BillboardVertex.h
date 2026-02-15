/**
 * @file BillboardVertex.h
 * @brief Vertex structure for billboard rendering (like clouds or smoke).
 */
#pragma once

#include "_Vertex.h"

struct BillboardVertex : Vertex {
    float posX, posY, posZ; // Billboard positions use floats for smoother movement

    BillboardVertex(float _posX, float _posY, float _posZ, char _texU, char _texV, uint16_t _layerIndex, uint8_t _lightLevel = 15)
        : Vertex(0, 0, 0, _texU, _texV, 0, _layerIndex, _lightLevel), posX(_posX), posY(_posY), posZ(_posZ) {}
};