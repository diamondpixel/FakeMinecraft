#include "ChunkPos.h"
#include <unordered_map>

struct ChunkPosHash
{
    std::size_t operator()(const ChunkPos& key) const noexcept
    {
        // FNV-1a style hash for minimal collisions on spatial data
        std::size_t h = 2166136261u;
        h ^= static_cast<std::size_t>(key.x);
        h *= 16777619u;
        h ^= static_cast<std::size_t>(key.y);
        h *= 16777619u;
        h ^= static_cast<std::size_t>(key.z);
        h *= 16777619u;
        return h;
    }
};