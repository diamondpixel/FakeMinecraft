#pragma once

#include "_Vertex.h"

struct FluidVertex : public Vertex {
    char top;

    FluidVertex(char _posX, char _posY, char _posZ, char _texGridX, char _texGridY, char _direction, char _top)
        : Vertex(_posX, _posY, _posZ, _texGridX, _texGridY, _direction), top(_top) {}
};