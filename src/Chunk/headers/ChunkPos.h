#pragma once

struct ChunkPos
{
    int x;
    int y;
    int z;

    ChunkPos(): x(0), y(0), z(0){}
    ChunkPos(int x, int y, int z): x(x), y(y), z(z) {}

    bool operator==(const ChunkPos& other) const {
        return other.x == x && other.y == y && other.z == z;
    }
};