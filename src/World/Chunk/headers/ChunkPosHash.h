#include "ChunkPos.h"
#include <unordered_map>

struct ChunkPosHash
{
    std::size_t operator()(const ChunkPos& key) const noexcept
    {
        // A simple math trick to mix the X, Y, and Z values into one unique ID.
        // This follows the FNV-1a hash pattern (https://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function).
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