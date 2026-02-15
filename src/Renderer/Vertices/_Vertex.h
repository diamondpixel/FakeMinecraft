/**
 * @file _Vertex.h
 * @brief Basic vertex structure and face direction definitions.
 */
#pragma once


/**
 * @enum FACE_DIRECTION
 * @brief Directions for block faces.
 */
enum FACE_DIRECTION {
    NORTH,
    SOUTH,
    WEST,
    EAST,
    BOTTOM,
    TOP,
    PLACEHOLDER_VALUE
};

/**
 * @struct Vertex
 * @brief Standard vertex layout for rendering.
 * 
 * The data is packed tightly to save memory.
 */
#pragma pack(push, 1)
struct Vertex {
    int16_t posX, posY, posZ;
    char texU, texV; // Texture coordinates
    char direction;
    uint16_t layerIndex; // Which texture to use from the array
    uint8_t lightLevel;  // How bright the block should be (0-15)

    Vertex(int16_t _posX, int16_t _posY, int16_t _posZ, char _texU, char _texV, char _direction, uint16_t _layerIndex, uint8_t _lightLevel = 15)
        : posX(_posX), posY(_posY), posZ(_posZ), texU(_texU), texV(_texV), direction(_direction), layerIndex(_layerIndex), lightLevel(_lightLevel) {}
};
#pragma pack(pop)