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

    Vector2 chunk = { (float)(coord.x >> world->bitsForTiles), (float)(coord.y >> world->bitsForTiles) };
    Vector2 tile = { (float)(coord.x & ((1 << world->bitsForTiles) - 1)), (float)(coord.y & ((1 << world->bitsForTiles) - 1)) };// creates mask with 'bitsForTiles' 1's. ie (4bft): 0x 0000 0000 0000 1111

    Assert(chunk.x >= 0);
    Assert(chunk.y >= 0);
    Assert(coord.z >= 0);
    Assert(coord.z < (int)world->numChunksZ);
    Assert(tile.x >= 0);
    Assert(tile.y >= 0);
    if (world->chunks[coord.z * world->numChunksY * world->numChunksX + (int)chunk.y * world->numChunksY + (int)chunk.x].tiles != nullptr) {
        return world->chunks[coord.z * world->numChunksY * world->numChunksX + (int)chunk.y * world->numChunksY + (int)chunk.x].tiles[(int)tile.y * getChunkSize(world) + (int)tile.x];
    }
    else {
        return 0;
    }
}

void setTileValue(MemoryArena* worldArena, World* world, int absoluteX, int absoluteY, int chunkZ, int value) {
    Vector2 chunk = { 0,0 };
    while (absoluteX >= world->tilesPerChunk) {
        absoluteX -= world->tilesPerChunk;
        chunk.x += 1;
    }
    while (absoluteY >= world->tilesPerChunk) {
        absoluteY -= world->tilesPerChunk;
        chunk.y += 1;
    }
    while (chunk.x >= world->numChunksX) {
        chunk.x -= world->numChunksX;
    }
    while (chunk.y >= world->numChunksY) {
        chunk.y -= world->numChunksY;
    }
    Assert(chunkZ < (int)world->numChunksZ);

    Chunk* currentChunk = &world->chunks[chunkZ * world->numChunksY * world->numChunksX + (int)chunk.y * world->numChunksY + (int)chunk.x];

    if (currentChunk->tiles == nullptr) {
        uint32_t* tiles = pushArray(worldArena, world->tilesPerChunk * world->tilesPerChunk, uint32_t);
        currentChunk->tiles = tiles;
    }

    currentChunk->tiles[absoluteY * world->tilesPerChunk + absoluteX] = value;
}

Vector2 subract(AbsoluteCoordinate A, AbsoluteCoordinate B) {
    Vector2 difference = { (float)A.x - B.x, (float)A.y - B.y };
    Vector2 offsetDifference = { (float)A.offset.x - B.offset.x, (float)A.offset.y - B.offset.y };
    return difference + offsetDifference;
}

Vector2 getTile(World* world, AbsoluteCoordinate coord) {
    return { (float)(coord.x & ((1 << world->bitsForTiles) - 1)), (float)(coord.y & ((1 << world->bitsForTiles) - 1) )};
}

Vector2 getChunk(World* world, AbsoluteCoordinate coord) {
    return { (float)(coord.x >> world->bitsForTiles), (float)(coord.y >> world->bitsForTiles) };
}

AbsoluteCoordinate constructCoordinate(World* world, Vector2 chunk, int chunkZ, Vector2 tile, Vector2 offset) {
    while (tile.x >= getChunkSize(world)) {
        tile.x -= getChunkSize(world);
        chunk.x += 1;
    }
    while (tile.x < 0) {
        tile.x += getChunkSize(world);
        chunk.x -= 1;
    }
    while (tile.y >= getChunkSize(world)) {
        tile.y -= getChunkSize(world);
        chunk.y += 1;
    }
    while (tile.y < 0) {
        tile.y += getChunkSize(world);
        chunk.y -= 1;
    }
    while (chunk.x < 0) {
        chunk.x += world->numChunksX;
    }
    while (chunk.y < 0) {
        chunk.y += world->numChunksY;
    }
    while (chunk.x >= (int)world->numChunksX) {
        chunk.x -= world->numChunksX;
    }
    while (chunk.y >= (int)world->numChunksY) {
        chunk.y -= world->numChunksY;
    }
    AbsoluteCoordinate ret;

    ret.x = ((int)(chunk.x) << (world->bitsForTiles)) | ((int)tile.x);
    ret.y = ((int)(chunk.y) << (world->bitsForTiles)) | ((int)tile.y);
    ret.z = chunkZ;
    ret.offset = offset;
    return ret;
}

bool canMove(World* world, AbsoluteCoordinate coord) {
    return getTileValue(world, coord) != 0 && getTileValue(world, coord) != 2;
}


AbsoluteCoordinate canonicalize(World* world, AbsoluteCoordinate* coord, Vector2* offset) {
    Vector2 tile = getTile(world, *coord);
    Vector2 chunk = getChunk(world, *coord);
    int chunkZ = coord->z;
    // Offset
    // TODO: this prob wont work for very large values, will need to do a while loop
    if (offset->x > world->tileSize / 2.0f) {
        offset->x -= world->tileSize;
        tile.x += 1;
    }
    if (offset->x < -world->tileSize / 2.0f) {
        offset->x += world->tileSize;
        tile.x -= 1;
    }
    if (offset->y > world->tileSize / 2.0f) {
        offset->y -= world->tileSize;
        tile.y += 1;
    }
    if (offset->y < -world->tileSize / 2.0f) {
        offset->y += world->tileSize;
        tile.y -= 1;
    }

    // Chunk
    if (tile.x >= getChunkSize(world)) {
        tile.x -= getChunkSize(world);
        chunk.x += 1;
    }
    if (tile.x < 0) {
        tile.x = (float)getChunkSize(world) - 1;
        chunk.x -= 1;
    }
    if (tile.y >= getChunkSize(world)) {
        tile.y -= (float)getChunkSize(world);
        chunk.y += 1;
    }
    if (tile.y < 0) {
        tile.y = (float)getChunkSize(world) - 1;
        chunk.y -= 1;
    }
    return constructCoordinate(world, chunk, chunkZ, tile, *offset);
}