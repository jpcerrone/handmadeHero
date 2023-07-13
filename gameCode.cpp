#include "gameCode.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

int roundFloat(float value) {
    return (int)(value + 0.5f);
}

uint32_t roundUFloat(float value) {
    return (uint32_t)(value + 0.5f);
}

void drawRectangle(void* bitmap, int bmWidth, int bmHeight, float minX, float minY, float maxX, float maxY, float r, float g, float b) {
    uint32_t* pixel = (uint32_t*)bitmap;

    if (minX < 0.0f)
        minX = 0.0f;
    if (minY < 0.0f)
        minY = 0.0f;
    if (maxX > bmWidth)
        maxX = (float)bmWidth;
    if (maxY > bmHeight)
        maxY = (float)bmHeight;

    float tmp = maxY;
    maxY = bmHeight - minY;
    minY = bmHeight - tmp;

    uint32_t color = (0xFF << 24) | (roundUFloat(r*255.0f) << 16) | (roundUFloat(g * 255.0f) << 8) | (roundUFloat(b * 255.0f) << 0); //0xAA RR GG BB 0b1111
    
    // Go to upper left corner.
    pixel += bmWidth*(roundFloat(minY));
    pixel += roundFloat(minX);

    int recWidth = roundFloat(maxX) - roundFloat(minX);
    int recHeight = roundFloat(maxY) - roundFloat(minY);

    for (int y = 0; y < recHeight; y++) {
        for (int x = 0; x < recWidth; x++) {
            *pixel = color;
            pixel++;
        }
        pixel += bmWidth - recWidth;
    }
}

void loadSineWave(uint32_t framesToWrite, void *bufferLocation, int samplesPerSec, float frequency, float *waveOffset)
{
    int samplesPerWave = (int)(samplesPerSec / frequency);

    int volume = 1200;
    int16_t *sample = (int16_t *)bufferLocation;
    for (uint32_t i = 0; i < framesToWrite; i++)
    { // The size of an audio frame is the number of channels in the stream multiplied by the sample size
        {
            *waveOffset += ((float)1 / (float)samplesPerWave);
            float sinValue = sinf(2.0f * (float)M_PI * *waveOffset);
            *sample = (int16_t)(sinValue * volume);
            *(sample + 1) = (int16_t)(sinValue * volume);
        }
        sample += 2;
    }
    *waveOffset -= (int)*waveOffset; // Keep it between 0 and 1 to avoid overflow.
}


const float pixelsPerUnit = 48.0f;

float pixelsToUnits(float pixelMagnitude) {
    return (float)pixelMagnitude / pixelsPerUnit;
}
float unitsToPixels(float unitMagnitude) {
    return unitMagnitude * pixelsPerUnit;
}

int getTileValue(World *world, AbsoluteCoordinate coord) {
    int chunkX = coord.x >> 24;
    int chunkY = coord.y >> 24;
    int tileX = coord.x & 0xFF;
    int tileY = coord.y & 0xFF;
    Assert(chunkX >= 0);
    Assert(chunkY >= 0);
    Assert(tileX >= 0);
    Assert(tileY >= 0);
    return world->chunks[chunkY*world->numChunksY + chunkX].tiles[tileY*CHUNK_SIZE + tileX];
}

int getTileX(AbsoluteCoordinate coord) {
    int tileX = coord.x & 0xFF;
    return tileX;
}

int getTileY(AbsoluteCoordinate coord) {
    int tileY = coord.y & 0xFF;
    return tileY;
}

int getChunkX(AbsoluteCoordinate coord) {
    int chunkX = coord.x >> 24;    
    return chunkX;
}

int getChunkY(AbsoluteCoordinate coord) {
    int chunkY = coord.y >> 24;    
    return chunkY;
}

AbsoluteCoordinate constructCoordinate(int chunkX, int chunkY, int tileX, int tileY) {
    Assert(chunkX <= 0xFFFFFF00); // TODO: Topology do that thing
    Assert(chunkY <= 0xFFFFFF00);
    Assert(tileX <= 0xFF);
    Assert(tileY <= 0xFF);

    AbsoluteCoordinate ret;

    ret.x = (chunkX << 24) | (tileX);
    ret.y = (chunkY << 24) | (tileY);

    return ret;
}

bool canMove(World *world, AbsoluteCoordinate coord) {
    return getTileValue(world, coord) == 0;
}


AbsoluteCoordinate canonicalize(World *world, AbsoluteCoordinate *coord, float *offsetX, float *offsetY) {
    int tileX = getTileX(*coord);
    int tileY = getTileY(*coord);
    int chunkX = getChunkX(*coord);
    int chunkY = getChunkY(*coord);

    // Offset
    if (*offsetX > world->tileSize) {
        *offsetX -= world->tileSize;
        tileX += 1;
    }
    if (*offsetX < 0) {
        *offsetX += world->tileSize;
        tileX -= 1;
    }
    if (*offsetY > world->tileSize) {
        *offsetY -= world->tileSize;
        tileY += 1;
    }
    if (*offsetY < 0) {
        *offsetY += world->tileSize;
        tileY -= 1;
    }

    // Chunk
    if (tileX >= CHUNK_SIZE) {
        tileX = 0;
        chunkX += 1;
    }
    if (tileX < 0) {
        tileX = CHUNK_SIZE - 1;
        chunkX -= 1;
    }
    if (tileY >= CHUNK_SIZE) {
        tileY = 0;
        chunkY += 1;
    }
    if (tileY < 0) {
        tileY = CHUNK_SIZE - 1;
        chunkY -= 1;
    }
    return constructCoordinate(chunkX, chunkY, tileX, tileY);
}


extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized){
        int playerX = 24;
        int playerY = 16;
        gameState->playerCoord = constructCoordinate(0, 0, playerX, playerY);
        gameState->offsetinTileX = 0.0;
        gameState->offsetinTileY = 0.0;
        gameMemory->isinitialized = true;
    }

    //World overworld;

    uint32_t tempTiles[CHUNK_SIZE][CHUNK_SIZE] = { // 1 Chunk world
        {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,1},
        {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,1},
    };
    Chunk testChunk;
    testChunk.tiles = (uint32_t*) tempTiles;

    World overworld;
    overworld.chunks = &testChunk;
    overworld.numChunksX = 1;
    overworld.numChunksY = 1;

    float playerSpeed = 5.0f;
    float playerWidth = 0.8f;
    float playerHeight = 1;
    float newOffsetX = gameState->offsetinTileX;
    float newOffsetY = gameState->offsetinTileY;
    if (inputState.Left_Stick.xPosition < 0) {
        newOffsetX -= pixelsToUnits(unitsToPixels(playerSpeed) *inputState.deltaTime);
    }
    if (inputState.Left_Stick.xPosition > 0) {
        newOffsetX += pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
    }
    if (inputState.Left_Stick.yPosition < 0) {
        newOffsetY += pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
    }
    if (inputState.Left_Stick.yPosition > 0) {
        newOffsetY -= pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
    }

    float offsetLx = newOffsetX - playerWidth / 2.0f;
    float offsetRx = newOffsetX + playerWidth / 2.0f;
    float offsetHeight = newOffsetY - playerHeight / 2.0f;
    float offsetHeight2 = newOffsetY - playerHeight / 2.0f;
    AbsoluteCoordinate middle = canonicalize(&overworld, &gameState->playerCoord, &newOffsetX, &newOffsetY);
    AbsoluteCoordinate left = canonicalize(&overworld, &gameState->playerCoord, &offsetLx, &offsetHeight);
    AbsoluteCoordinate right = canonicalize(&overworld, &gameState->playerCoord, &offsetRx, &offsetHeight2);
    if (canMove(&overworld, middle) && canMove(&overworld, left) && canMove(&overworld, right)) {
        gameState->playerCoord = middle;
        gameState->offsetinTileX = newOffsetX;
        gameState->offsetinTileY = newOffsetY;
    }

    drawRectangle(memory, width, height, 0, 0, (float)width, (float)height, 0.0f, 0.0f, 0.0f); // Clear screen to black

    float playerX = overworld.tileSize * SCREEN_TILE_WIDTH / 2.0f;
    float playerY = overworld.tileSize * SCREEN_TILE_HEIGHT / 2.0f;
    int playerTileX = getTileX(gameState->playerCoord);
    int playerTileY = getTileY(gameState->playerCoord);

    // Draw TileMap
    float grayShadeForTile = 0.5;
    for (int j = playerTileY - SCREEN_TILE_HEIGHT/2; j <= playerTileY + SCREEN_TILE_HEIGHT/2; j++) {
        for (int i = playerTileX - SCREEN_TILE_WIDTH / 2; i <= playerTileX + SCREEN_TILE_WIDTH / 2; i++) {
            AbsoluteCoordinate tileCoord;
            tileCoord.x = i;
            tileCoord.y = j;
            if (getTileValue(&overworld, tileCoord) == 0) {
                grayShadeForTile = 0.5;
            }
            else {
                grayShadeForTile = 1.0;
            }

            if ((playerTileX == i) && (playerTileY == j)) {
                grayShadeForTile = 0.2f;
            }

            float minX = unitsToPixels((float)(overworld.tileSize * (i - gameState->offsetinTileX - (playerTileX - SCREEN_TILE_WIDTH / 2.0))));
            float maxX = unitsToPixels((float)(overworld.tileSize * ((i+1 - gameState->offsetinTileX) - (playerTileX - SCREEN_TILE_WIDTH / 2.0))));

            float minY = unitsToPixels((float)(overworld.tileSize * (j - gameState->offsetinTileY  - (playerTileY - SCREEN_TILE_HEIGHT / 2.0))));
            float maxY = unitsToPixels((float)(overworld.tileSize * ((j + 1 - gameState->offsetinTileY ) - (playerTileY - SCREEN_TILE_HEIGHT / 2.0))));
            drawRectangle(memory, width, height, minX, minY, maxX, maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }

    // Draw Player
    drawRectangle(memory, width, height, unitsToPixels(playerX - playerWidth / 2.0f), unitsToPixels(playerY - playerHeight / 2.0f), 
        unitsToPixels(playerX + playerWidth / 2.0f), unitsToPixels(playerY + playerHeight / 2.0f), 1.0f, 1.0f, 0.0f);
}