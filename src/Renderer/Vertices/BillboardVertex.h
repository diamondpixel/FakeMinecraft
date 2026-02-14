#pragma once

#include "_Vertex.h"

struct BillboardVertex : Vertex {
    float posX, posY, posZ; // Override base char position with float

    BillboardVertex(float _posX, float _posY, float _posZ, char _texU, char _texV, uint16_t _layerIndex)
        : Vertex(0, 0, 0, _texU, _texV, 0, _layerIndex), posX(_posX), posY(_posY), posZ(_posZ) {}
};