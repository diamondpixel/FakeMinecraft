#pragma once

#include "_Vertex.h"

struct FluidVertex : Vertex {
    char top;

    FluidVertex(int16_t _posX, int16_t _posY, int16_t _posZ, char _texU, char _texV, char _direction, char _layerIndex, char _top)
        : Vertex(_posX, _posY, _posZ, _texU, _texV, _direction, _layerIndex), top(_top) {}
};