#pragma once
#include "math.h"
static const int SCREEN_TILE_WIDTH = 20;
static const int SCREEN_TILE_HEIGHT = 12;

static const float PIXELS_PER_UNIT = 48.0f;

struct Chunk {
    uint32_t* tiles;
};

struct World {
    uint32_t numChunksX;
    uint32_t numChunksY;
    uint32_t numChunksZ;
    Chunk* chunks; // Multi demensional array
    int tilesPerChunk;
    int bitsForTiles;
    float tileSize;
};

struct AbsoluteCoordinate {
    uint32_t x; // (32-n) bits for chunk index, n for tile index
    uint32_t y; // ...
    uint32_t z; // all 32 bits for chunk index, theres no tile separation in z

    // TODO check if this impacts anything, added on day 50 more or less
    Vector2 offset; // Offset from tile center

    bool operator ==(AbsoluteCoordinate other) {
        return other.x == x && other.y == y && other.z == z && offset == other.offset;
    }
    bool operator !=(AbsoluteCoordinate other) { // TODO rewrite basing on the other one
        return !(other.x == x && other.y == y && other.z == z && offset == other.offset);
    }
    AbsoluteCoordinate operator -(AbsoluteCoordinate other) {
        return { x - other.x, y - other.y, z - other.z, offset - other.offset };
    }
};