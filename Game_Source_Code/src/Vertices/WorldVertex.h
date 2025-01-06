#pragma once

#include "_Vertex.h"

struct WorldVertex : public Vertex {

    WorldVertex(char _posX, char _posY, char _posZ, char _texGridX, char _texGridY, char _direction)
        : Vertex(_posX, _posY, _posZ, _texGridX, _texGridY, _direction) {}
};