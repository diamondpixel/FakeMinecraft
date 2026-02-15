 /**
 * @file ChunkPos.h
 * @brief Simple structure for storing a chunk's position.
 */
#pragma once

struct ChunkPos
{
    int x;
    int y;
    int z;

    ChunkPos(): x(0), y(0), z(0){}
    ChunkPos(const int x, const int y, const int z): x(x), y(y), z(z) {}

    bool operator==(const ChunkPos& other) const {
        return other.x == x && other.y == y && other.z == z;
    }
};