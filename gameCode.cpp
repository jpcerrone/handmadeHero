#include "gameCode.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include "world.cpp"

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

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);


    const int chunkSize = 4;
    uint32_t tempTiles[chunkSize][chunkSize] = { 
        {0,0,0,0},
        {0,1,0,0},
        {0,0,0,0},
        {0,0,0,0},
    };
    uint32_t tempTiles1[chunkSize][chunkSize] = {
        {0,0,0,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0},
    };
    uint32_t tempTiles2[chunkSize][chunkSize] = {
        {0,0,0,1},
        {0,1,1,0},
        {0,1,0,0},
        {0,0,0,0},
    };
    uint32_t tempTiles3[chunkSize][chunkSize] = {
        {0,0,0,0},
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0},
    };

    // 4 Chunk world
    Chunk testChunk;
    testChunk.tiles = (uint32_t*) tempTiles;
    Chunk testChunk1;
    testChunk1.tiles = (uint32_t*)tempTiles1;
    Chunk testChunk2;
    testChunk2.tiles = (uint32_t*)tempTiles2;
    Chunk testChunk3;
    testChunk3.tiles = (uint32_t*)tempTiles3;

    World overworld;
    overworld.numChunksX = 2;
    overworld.numChunksY = 2;
    overworld.bitsForTiles = 2; // 2**4 = 16
    Chunk chunks[2][2];
    chunks[0][0] = testChunk;
    chunks[0][1] = testChunk1;
    chunks[1][0] = testChunk2;
    chunks[1][1] = testChunk3;
    overworld.chunks = (Chunk*)chunks;

    if (!gameMemory->isinitialized) {
        int playerX = 1;
        int playerY = 2;
        gameState->playerCoord = constructCoordinate(&overworld, 0, 0, playerX, playerY);
        gameState->offsetinTileX = 0.0;
        gameState->offsetinTileY = 0.0;
        gameMemory->isinitialized = true;
    }
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
    int playerTileX = getTileX(&overworld, gameState->playerCoord);
    int playerTileY = getTileY(&overworld, gameState->playerCoord);

    // Draw TileMap
    float grayShadeForTile = 0.5;
    for (int j = playerTileY - SCREEN_TILE_HEIGHT/2 - 1; j <= playerTileY + SCREEN_TILE_HEIGHT/2; j++) { // -1 to account for offsetY
        for (int i = playerTileX - SCREEN_TILE_WIDTH / 2 - 1; i <= playerTileX + SCREEN_TILE_WIDTH / 2; i++) { // -1 to account for offsetX
            AbsoluteCoordinate tileCoord;
            tileCoord = constructCoordinate(&overworld, getChunkX(&overworld, gameState->playerCoord), getChunkY(&overworld, gameState->playerCoord), i, j);
            if (getTileValue(&overworld, tileCoord) == 0) {
                grayShadeForTile = 0.5;
            }
            else {
                grayShadeForTile = 1.0;
            }

            if ((playerTileX == i) && (playerTileY == j)) {
                grayShadeForTile = 0.2f;
            }

            float minX = unitsToPixels((float)(overworld.tileSize * (i - gameState->offsetinTileX - (playerTileX - SCREEN_TILE_WIDTH / 2.0f))));
            float maxX = unitsToPixels((float)(overworld.tileSize * ((i+1 - gameState->offsetinTileX) - (playerTileX - SCREEN_TILE_WIDTH / 2.0f))));

            float minY = unitsToPixels((float)(overworld.tileSize * (j - gameState->offsetinTileY -(playerTileY - SCREEN_TILE_HEIGHT / 2.0f))));
            float maxY = unitsToPixels((float)(overworld.tileSize * ((j +1 - gameState->offsetinTileY ) - (playerTileY - SCREEN_TILE_HEIGHT / 2.0f))));
            drawRectangle(memory, width, height, minX, minY, maxX, maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }
    float tileCenterX = overworld.tileSize - playerWidth;
    // Draw Player
    drawRectangle(memory, width, height, unitsToPixels(playerX + tileCenterX/2.0f), unitsToPixels(playerY),
        unitsToPixels(playerX + tileCenterX/2.0f + playerWidth), unitsToPixels(playerY + playerHeight), 1.0f, 1.0f, 0.0f);
}