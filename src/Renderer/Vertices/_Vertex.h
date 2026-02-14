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

#pragma pack(push, 1)
struct Vertex {
    int16_t posX, posY, posZ;
    char texU, texV; // Local UV tiling factor (repeat count)
    char direction;
    uint16_t layerIndex; // Texture Layer Index (0-65535)
    uint8_t lightLevel;  // Block light level (0-15)

    Vertex(int16_t _posX, int16_t _posY, int16_t _posZ, char _texU, char _texV, char _direction, uint16_t _layerIndex, uint8_t _lightLevel = 15)
        : posX(_posX), posY(_posY), posZ(_posZ), texU(_texU), texV(_texV), direction(_direction), layerIndex(_layerIndex), lightLevel(_lightLevel) {}
};
#pragma pack(pop)