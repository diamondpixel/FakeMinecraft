#pragma once

#include "_Vertex.h"

struct FluidVertex : Vertex {
    char top;

    FluidVertex(int16_t _posX, int16_t _posY, int16_t _posZ, char _texU, char _texV, char _direction, uint16_t _layerIndex, char _top, uint8_t _lightLevel = 15)
        : Vertex(_posX, _posY, _posZ, _texU, _texV, _direction, _layerIndex, _lightLevel), top(_top) {}
};