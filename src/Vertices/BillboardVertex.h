#pragma once

#include "_Vertex.h"

struct BillboardVertex : public Vertex {
    float posX, posY, posZ; // Override base char position with float

    BillboardVertex(float _posX, float _posY, float _posZ, char _texGridX, char _texGridY)
        : Vertex(0, 0, 0, _texGridX, _texGridY, 0), posX(_posX), posY(_posY), posZ(_posZ) {}
};