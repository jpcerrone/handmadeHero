#pragma once

static const int SCREEN_TILE_WIDTH = 20;
static const int SCREEN_TILE_HEIGHT = 12;

static const float PIXELS_PER_UNIT = 48.0f;

struct Chunk {
    uint32_t* tiles;
};

struct World {
    uint32_t numChunksX;
    uint32_t numChunksY;
    Chunk* chunks; // Multi demensional array
    int bitsForTiles;
    float tileSize = 1.0;
};

struct AbsoluteCoordinate {
    uint32_t x; // (32-n) bits for chunk index, n for tile index
    uint32_t y; // ...
};