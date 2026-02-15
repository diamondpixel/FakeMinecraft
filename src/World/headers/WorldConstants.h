/**
 * @file WorldConstants.h
 * @brief Global settings for the project (like world height and chunk size).
 */
#pragma once

constexpr int MAX_HEIGHT = 256;
constexpr int MIN_HEIGHT = 0;
constexpr unsigned int CHUNK_WIDTH = 32;
constexpr unsigned int CHUNK_HEIGHT = MAX_HEIGHT - (MIN_HEIGHT > 0 ? MIN_HEIGHT : -MIN_HEIGHT); // Manual abs for constexpr safety
static int WATER_LEVEL = 64;
inline long SEED = 0;
