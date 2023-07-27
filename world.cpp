#include "world.h"
#include "memory_arena.cpp"

float pixelsToUnits(float pixelMagnitude) {
    return (float)pixelMagnitude / PIXELS_PER_UNIT;
}
float unitsToPixels(float unitMagnitude) {
    return unitMagnitude * PIXELS_PER_UNIT;
}

int getChunkSize(World* world) {
    // for 16x16 chunks
    // 2**4 -> 4 bits
    // chunk shift = 32 - 4 = 28
    // tile mask = 4
    return (int)std::pow(2.0, world->bitsForTiles);
}

int getTileValue(World* world, AbsoluteCoordinate coord) {

    int chunkX = coord.x >> world->bitsForTiles;
    int chunkY = coord.y >> world->bitsForTiles;
    int tileX = coord.x & ((1 << world->bitsForTiles) - 1); // creates mask with 'bitsForTiles' 1's. ie (4bft): 0x 0000 0000 0000 1111
    int tileY = coord.y & ((1 << world->bitsForTiles) - 1);
    Assert(chunkX >= 0);
    Assert(chunkY >= 0);
    Assert(coord.z >= 0);
    Assert(coord.z < (int)world->numChunksZ);
    Assert(tileX >= 0);
    Assert(tileY >= 0);
    if (world->chunks[coord.z * world->numChunksY * world->numChunksX + chunkY * world->numChunksY + chunkX].tiles != nullptr) {
        return world->chunks[coord.z * world->numChunksY * world->numChunksX + chunkY * world->numChunksY + chunkX].tiles[tileY * getChunkSize(world) + tileX];
    }
    else {
        return 0;
    }
}

void setTileValue(MemoryArena* worldArena, World* world, int absoluteX, int absoluteY, int chunkZ, int value) {
    uint32_t chunkX = 0;
    uint32_t chunkY = 0;
    while (absoluteX >= world->tilesPerChunk) {
        absoluteX -= world->tilesPerChunk;
        chunkX += 1;
    }
    while (absoluteY >= world->tilesPerChunk) {
        absoluteY -= world->tilesPerChunk;
        chunkY += 1;
    }
    while (chunkX >= world->numChunksX) {
        chunkX -= world->numChunksX;
    }
    while (chunkY >= world->numChunksY) {
        chunkY -= world->numChunksY;
    }
    Assert(chunkZ < (int)world->numChunksZ);

    Chunk* currentChunk = &world->chunks[chunkZ * world->numChunksY * world->numChunksX + chunkY * world->numChunksY + chunkX];

    if (currentChunk->tiles == nullptr) {
        uint32_t* tiles = pushArray(worldArena, world->tilesPerChunk * world->tilesPerChunk, uint32_t);
        currentChunk->tiles = tiles;
    }

    currentChunk->tiles[absoluteY * world->tilesPerChunk + absoluteX] = value;
}

int getTileX(World* world, AbsoluteCoordinate coord) {
    int tileX = coord.x & ((1 << world->bitsForTiles) - 1);
    return tileX;
}

int getTileY(World* world, AbsoluteCoordinate coord) {
    int tileY = coord.y & ((1 << world->bitsForTiles) - 1);
    return tileY;
}

int getChunkX(World* world, AbsoluteCoordinate coord) {
    int chunkX = coord.x >> world->bitsForTiles;
    return chunkX;
}

int getChunkY(World* world, AbsoluteCoordinate coord) {
    int chunkY = coord.y >> world->bitsForTiles;
    return chunkY;
}

AbsoluteCoordinate constructCoordinate(World* world, int chunkX, int chunkY, int chunkZ, int tileX, int tileY) {
    while (tileX >= getChunkSize(world)) {
        tileX -= getChunkSize(world);
        chunkX += 1;
    }
    while (tileX < 0) {
        tileX += getChunkSize(world);
        chunkX -= 1;
    }
    while (tileY >= getChunkSize(world)) {
        tileY -= getChunkSize(world);
        chunkY += 1;
    }
    while (tileY < 0) {
        tileY += getChunkSize(world);
        chunkY -= 1;
    }
    while (chunkX < 0) {
        chunkX += world->numChunksX;
    }
    while (chunkY < 0) {
        chunkY += world->numChunksY;
    }
    while (chunkX >= (int)world->numChunksX) {
        chunkX -= world->numChunksX;
    }
    while (chunkY >= (int)world->numChunksY) {
        chunkY -= world->numChunksY;
    }
    AbsoluteCoordinate ret;

    ret.x = (chunkX << (world->bitsForTiles)) | (tileX);
    ret.y = (chunkY << (world->bitsForTiles)) | (tileY);
    ret.z = chunkZ;
    return ret;
}

bool canMove(World* world, AbsoluteCoordinate coord) {
    return getTileValue(world, coord) != 0 && getTileValue(world, coord) != 2;
}


AbsoluteCoordinate canonicalize(World* world, AbsoluteCoordinate* coord, float* offsetX, float* offsetY) {
    int tileX = getTileX(world, *coord);
    int tileY = getTileY(world, *coord);
    int chunkX = getChunkX(world, *coord);
    int chunkY = getChunkY(world, *coord);
    int chunkZ = coord->z;
    // Offset
    // TODO: this prob wont work for very large values, will need to do a while loop
    if (*offsetX > world->tileSize / 2.0f) {
        *offsetX -= world->tileSize;
        tileX += 1;
    }
    if (*offsetX < -world->tileSize / 2.0f) {
        *offsetX += world->tileSize;
        tileX -= 1;
    }
    if (*offsetY > world->tileSize / 2.0f) {
        *offsetY -= world->tileSize;
        tileY += 1;
    }
    if (*offsetY < -world->tileSize / 2.0f) {
        *offsetY += world->tileSize;
        tileY -= 1;
    }

    // Chunk
    if (tileX >= getChunkSize(world)) {
        tileX -= getChunkSize(world);
        chunkX += 1;
    }
    if (tileX < 0) {
        tileX = getChunkSize(world) - 1;
        chunkX -= 1;
    }
    if (tileY >= getChunkSize(world)) {
        tileY -= getChunkSize(world);
        chunkY += 1;
    }
    if (tileY < 0) {
        tileY = getChunkSize(world) - 1;
        chunkY -= 1;
    }
    return constructCoordinate(world, chunkX, chunkY, chunkZ, tileX, tileY);
}