#pragma once


enum FACE_DIRECTION {
    NORTH,
    SOUTH,
    WEST,
    EAST,
    BOTTOM,
    TOP,
    PLACEHOLDER_VALUE
};

struct Vertex {
    char posX, posY, posZ;
    char texGridX, texGridY;
    char direction;

    Vertex(char _posX, char _posY, char _posZ, char _texGridX, char _texGridY, char _direction)
        : posX(_posX), posY(_posY), posZ(_posZ), texGridX(_texGridX), texGridY(_texGridY), direction(_direction) {}
};