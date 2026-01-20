#pragma once

#include "_Vertex.h"

struct WorldVertex : Vertex {

    WorldVertex(int16_t _posX, int16_t _posY, int16_t _posZ, char _texU, char _texV, char _direction, char _layerIndex)
        : Vertex(_posX, _posY, _posZ, _texU, _texV, _direction, _layerIndex) {}
};