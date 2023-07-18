#include "gameCode.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include "world.cpp"
#include "memory_arena.cpp"

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

void initWorldArena(MemoryArena* worldArena, uint8_t *basePointer, size_t totalSize) {
    worldArena->base = basePointer;
    worldArena->total = totalSize;
    worldArena->used = 0;
}

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);

    if (!gameMemory->isinitialized) {
        // Construct world
        initWorldArena(&gameState->worldArena, (uint8_t*)gameMemory->permanentStorage + sizeof(GameState),
            (size_t)(gameMemory->permanentStorageSize - sizeof(GameState)));
        
        gameState->world = pushStruct(&gameState->worldArena, World);

        gameState->world->numChunksX = 256;
        gameState->world->numChunksY = 256;
        gameState->world->numChunksZ = 8;
        gameState->world->bitsForTiles = 2; // 2**4 = 16
        gameState->world->tileSize = 1.0f;
        gameState->world->tilesPerChunk = 4;

        gameState->world->chunks = pushArray(&gameState->worldArena, 
            gameState->world->numChunksX * gameState->world->numChunksY * gameState->world->numChunksZ, Chunk);

        int randomNumbers[10] = { 5, 6, 15, 18, 20, 3, 6, 8, 16, 19 };

        int randomNumberIdx = 0;
        int screenOffsetX = 0;
        int screenOffsetY = 0;

        bool doorLeft = false;
        bool doorRight = false;
        bool doorUp = false;
        bool doorDown = false;
        const int SCREENS_TO_GENERATE = 15;

        for (int s = 0; s < SCREENS_TO_GENERATE; s++) {
            // Get random next room and stairs
            int randomNumber = randomNumbers[randomNumberIdx];
            int nextOffsetX = screenOffsetX;
            int nextOffsetY = screenOffsetY;
            if ((randomNumber % 2) == 1) {
                doorRight = true;
                nextOffsetX += 1;
            }
            else {
                doorUp = true;
                nextOffsetY += 1;
            }
            randomNumberIdx = (randomNumberIdx + 1) % 10;
            int randomStairs = randomNumbers[(randomNumberIdx + 1) % 10] % 2;
            int stairsPosX = 5;
            int stairsPosY = 7;
            int zScreensToWrite = 1;
            if (randomStairs) {
                zScreensToWrite = 2;
            }
            for (int zScreen = 0; zScreen < zScreensToWrite; zScreen++) {
                for (int j = 0; j < SCREEN_TILE_HEIGHT; j++) {
                    for (int i = 0; i < SCREEN_TILE_WIDTH; i++) {
                        int valueToWrite = 1;
                        if (j == 0) {
                            if (!(i == SCREEN_TILE_WIDTH / 2 && doorDown)) {
                                valueToWrite = 2;
                            }
                        }
                        else if (j == SCREEN_TILE_HEIGHT - 1) {
                            if (!(i == SCREEN_TILE_WIDTH / 2 && doorUp)) {
                                valueToWrite = 2;
                            }
                        }
                        else if (i == 0) {
                            if (!(j == SCREEN_TILE_HEIGHT / 2 && doorLeft)) {
                                valueToWrite = 2;
                            }
                        }
                        else if (i == SCREEN_TILE_WIDTH - 1) {
                            if (!(j == SCREEN_TILE_HEIGHT / 2 && doorRight)) {
                                valueToWrite = 2;
                            }
                        }
                        setTileValue(&gameState->worldArena, gameState->world, i + screenOffsetX * SCREEN_TILE_WIDTH, j + screenOffsetY * SCREEN_TILE_HEIGHT, zScreen, valueToWrite);

                    }
                }
                if (randomStairs){
                    if (zScreen == 0) {
                        setTileValue(&gameState->worldArena, gameState->world, stairsPosX + screenOffsetX * SCREEN_TILE_WIDTH, stairsPosY + screenOffsetY * SCREEN_TILE_HEIGHT, zScreen, 3); //Up
                    }
                    else {
                        setTileValue(&gameState->worldArena, gameState->world, stairsPosX + screenOffsetX * SCREEN_TILE_WIDTH, stairsPosY + screenOffsetY * SCREEN_TILE_HEIGHT, zScreen, 4); //Down
                    }
                }
            }


            doorLeft = doorRight;
            doorDown = doorUp;
            doorRight = false;
            doorUp = false;
            screenOffsetX = nextOffsetX;
            screenOffsetY = nextOffsetY;
        }

        
        // Player
        int playerX = 1;
        int playerY = 2;
        int playerZ = 0;
        gameState->playerCoord = constructCoordinate(gameState->world, 0, 0, playerZ, playerX, playerY);
        gameState->offsetinTileX = 0.0;
        gameState->offsetinTileY = 0.0;
        gameMemory->isinitialized = true;

    }
    World *overworld = gameState->world;

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
    AbsoluteCoordinate middle = canonicalize(overworld, &gameState->playerCoord, &newOffsetX, &newOffsetY);
    AbsoluteCoordinate left = canonicalize(overworld, &gameState->playerCoord, &offsetLx, &offsetHeight);
    AbsoluteCoordinate right = canonicalize(overworld, &gameState->playerCoord, &offsetRx, &offsetHeight2);
    if (canMove(overworld, middle) && canMove(overworld, left) && canMove(overworld, right)) {
        if (gameState->playerCoord != middle) {
            if (getTileValue(overworld, middle) == 3) {
                middle.z = 1;
            }
            else if (getTileValue(overworld, middle) == 4) {
                middle.z = 0;
            }
        }
        gameState->playerCoord = middle;
        gameState->offsetinTileX = newOffsetX;
        gameState->offsetinTileY = newOffsetY;
    }

    drawRectangle(memory, width, height, 0, 0, (float)width, (float)height, 0.0f, 0.0f, 0.0f); // Clear screen to black

    float playerX = overworld->tileSize * SCREEN_TILE_WIDTH / 2.0f;
    float playerY = overworld->tileSize * SCREEN_TILE_HEIGHT / 2.0f;
    int playerTileX = getTileX(overworld, gameState->playerCoord);
    int playerTileY = getTileY(overworld, gameState->playerCoord);

    // Draw TileMap
    float grayShadeForTile = 0.5;
    const int DEBUG_ZOOMED_X = SCREEN_TILE_WIDTH * 6;
    const int DEBUG_ZOOMED_Y = SCREEN_TILE_HEIGHT * 6;

    for (int j = playerTileY - DEBUG_ZOOMED_Y /2 - 1; j <= playerTileY + DEBUG_ZOOMED_Y /2; j++) { // -1 to account for offsetY
        for (int i = playerTileX - DEBUG_ZOOMED_X / 2 - 1; i <= playerTileX + DEBUG_ZOOMED_X / 2; i++) { // -1 to account for offsetX
            AbsoluteCoordinate tileCoord;
            tileCoord = constructCoordinate(overworld, getChunkX(overworld, gameState->playerCoord), getChunkY(overworld, gameState->playerCoord), gameState->playerCoord.z,i, j);
            if (getTileValue(overworld, tileCoord) == 1) {
                grayShadeForTile = 0.5;
            }
            else if (getTileValue(overworld, tileCoord) == 2) {
                grayShadeForTile = 1.0;
            }
            else if (getTileValue(overworld, tileCoord) == 3) {
                grayShadeForTile = 0.8f;
            }
            else if (getTileValue(overworld, tileCoord) == 4) {
                grayShadeForTile = 0.6f;
            }
            else {
                grayShadeForTile = 0.1f;
            }

            if ((playerTileX == i) && (playerTileY == j)) {
                grayShadeForTile = 0.2f;
            }

            float minX = unitsToPixels((float)(overworld->tileSize * (i - gameState->offsetinTileX - (playerTileX - SCREEN_TILE_WIDTH / 2.0f))));
            float maxX = unitsToPixels((float)(overworld->tileSize * ((i+1 - gameState->offsetinTileX) - (playerTileX - SCREEN_TILE_WIDTH / 2.0f))));

            float minY = unitsToPixels((float)(overworld->tileSize * (j - gameState->offsetinTileY -(playerTileY - SCREEN_TILE_HEIGHT / 2.0f))));
            float maxY = unitsToPixels((float)(overworld->tileSize * ((j +1 - gameState->offsetinTileY ) - (playerTileY - SCREEN_TILE_HEIGHT / 2.0f))));
            drawRectangle(memory, width, height, minX, minY, maxX, maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }
    float tileCenterX = overworld->tileSize - playerWidth;
    // Draw Player
    drawRectangle(memory, width, height, unitsToPixels(playerX + tileCenterX/2.0f), unitsToPixels(playerY),
        unitsToPixels(playerX + tileCenterX/2.0f + playerWidth), unitsToPixels(playerY + playerHeight), 1.0f, 1.0f, 0.0f);
}