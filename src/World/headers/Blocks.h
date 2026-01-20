#pragma once

#include <vector>
#include <array>

#include "Block.h"

namespace Blocks {
    const std::vector blocks{
        Block(0, 0, 0, 0, Block::TRANSPARENT, "AIR"),
        Block(0, 0, 1, 1, Block::SOLID, "DIRT"),
        Block(1, 1, 2, 2,
              0, 0, 1, 1,
              1, 0, 2, 1, Block::SOLID, "GRASS_BLOCK"),
        Block(0, 1, 1, 2, Block::SOLID, "STONE"),
        Block(2, 1, 3, 2,
              2, 1, 3, 2,
              2, 0, 3, 1, Block::SOLID, "LOG"),
        Block(0, 2, 1, 3, Block::LEAVES, "LEAVES"),
        Block(1, 2, 2, 3, Block::BILLBOARD, "GRASS"),
        Block(3, 0, 4, 1, Block::BILLBOARD, "TALL_GRASS_BOTTOM"),
        Block(3, 1, 4, 2, Block::BILLBOARD, "TALL_GRASS_TOP"),
        Block(0, 3, 1, 4, Block::BILLBOARD, "POPPY"),
        Block(2, 2, 3, 3, Block::BILLBOARD, "WHITE_TULIP"),
        Block(3, 2, 4, 3, Block::BILLBOARD, "PINK_TULIP"),
        Block(1, 3, 2, 4, Block::BILLBOARD, "ORANGE_TULIP"),
        Block(0, 4, 1, 5, Block::LIQUID, "WATER"),
        Block(0, 6, 1, 7, Block::LIQUID, "LAVA"),
        Block(4, 0, 5, 1, Block::SOLID, "SAND"),
        Block(4, 1, 5, 2, Block::SOLID, "DIAMOND_ORE"),
        Block(4, 2, 5, 3, Block::SOLID, "BEDROCK"),
    };

    enum BLOCKS {
        AIR = 0,
        DIRT_BLOCK = 1,
        GRASS_BLOCK = 2,
        STONE_BLOCK = 3,
        LOG = 4,
        LEAVES = 5,
        GRASS = 6,
        TALL_GRASS_BOTTOM = 7,
        TALL_GRASS_TOP = 8,
        POPPY = 9,
        WHITE_TULIP = 10,
        PINK_TULIP = 11,
        ORANGE_TULIP = 12,
        WATER = 13,
        LAVA = 14,
        SAND = 15,
        DIAMOND_ORE = 16,
        BEDROCK = 17,
    };
}
