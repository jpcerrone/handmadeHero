#include "gameCode.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>

void renderArgFlag(void *memory, int width, int height)
{
    const uint32_t lightBlueColor = 0xadd8e6; // RGB
    const uint32_t whiteColor = 0xFFFFFF;     // RGB
    uint32_t *pixel = (uint32_t *)memory;
    for (int y = 0; y < height / 3; y++)
    {
        for (int x = 0; x < width; x++)
        {
            *pixel = lightBlueColor;
            pixel++;
        }
    }
    for (int y = height / 3; y < height * 2 / 3; y++)
    {
        for (int x = 0; x < width; x++)
        {
            *pixel = whiteColor;
            pixel++;
        }
    }
    for (int y = height * 2 / 3; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            *pixel = lightBlueColor;
            pixel++;
        }
    }

    int radius = 100;
    const uint32_t yellowColor = 0xFFBF00; // RGB
    pixel = (uint32_t *)memory;
    pixel += (height / 2 - radius) * (width) + (width / 2 - radius);
    for (int y = height / 2 - radius; y < height / 2 + radius; y++)
    {
        for (int x = width / 2 - radius; x < width / 2 + radius; x++)
        {
            *pixel = yellowColor;
            pixel++;
        }
        pixel += width - 2 * radius;
    }
}

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

void renderGradient(void *memory, int width, int height, int xOffset)
{
    // pixel = 4B = 32b
    uint32_t *pixel = (uint32_t *)memory;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8_t green = 0;
            uint8_t blue = (uint8_t)(x + xOffset);
            //*pixel = 0x000000FF; //xx RR GG BB -> little endian, most significant bits get loaded last (xx)
            *pixel = (green << 16) | blue;
            pixel++;
        }
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

#define tileMapWidth 17
#define tileMapHeight 9
struct TileMap {
    int* map;
    int getTileValue(int y, int x) {
        return map[y* tileMapWidth + x];
    }
    bool canMove(int y, int x) {
        if ((x < 0) || (x >= tileMapWidth) || (y < 0) || (y >= tileMapHeight)){
            return false;
        }
        return getTileValue(y, x) == 0;
    }
};

#define WORLD_WIDTH 2
#define WORLD_HEIGHT 2
struct World {
    TileMap* tilemaps;
    TileMap getTileMap(int y, int x) {
        return tilemaps[y * WORLD_WIDTH + x];
    }
};

int playerPosToTileIdx(float playerPos, int tileSize) {
    if (playerPos < 0) {
        return -1;
    }
    return (int)((playerPos / (float)tileSize));
};

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized){
        gameState->playerX = 60.0f;
        gameState->playerY = 240.0f;
        gameMemory->isinitialized = true;
    }

    World overworld;

    TileMap t0;
    int t0Map[tileMapHeight][tileMapWidth] = {
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t0.map = (int*)t0Map;
    TileMap t1;
    int t1Map[tileMapHeight][tileMapWidth] = {
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,0,1,0, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t1.map = (int*)t1Map;
    TileMap t2;
    int t2Map[tileMapHeight][tileMapWidth] = {
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,0,1,0, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0},
        {0,0,0,0, 1,0,0,0, 0,0,0,0, 0,1,0,0, 0},
        {0,0,0,0, 0,1,0,0, 0,0,0,0, 0,0,1,0, 0},
        {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t2.map = (int*)t2Map;
    TileMap t3;
    int t3Map[tileMapHeight][tileMapWidth] = {
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,1,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t3.map = (int*)t3Map;

    TileMap tileArray[WORLD_HEIGHT][WORLD_WIDTH] = {
        {t0, t1},
        {t2, t3},
    };
    overworld.tilemaps = (TileMap*)tileArray;
    TileMap* currentTileMap = &t0;

    int tileSize = 48;

    float playerSpeed = 120.0f;
    float playerWidth = 32;
    float playerHeight = 48;
    float newPlayerX = gameState->playerX;
    float newPlayerY = gameState->playerY;
    if (inputState.Left_Stick.xPosition < 0) {
        newPlayerX -= playerSpeed *inputState.deltaTime;
    }
    if (inputState.Left_Stick.xPosition > 0) {
        newPlayerX += playerSpeed * inputState.deltaTime;
    }
    if (inputState.Left_Stick.yPosition < 0) {
        newPlayerY -= playerSpeed * inputState.deltaTime;
    }
    if (inputState.Left_Stick.yPosition > 0) {
        newPlayerY += playerSpeed * inputState.deltaTime;
    }

    int tileForNewY = playerPosToTileIdx(newPlayerY + (playerHeight / 2.0f), tileSize);
    int tileForLeftX = playerPosToTileIdx(newPlayerX - (playerWidth / 2.0f), tileSize);
    int tileForRightX = playerPosToTileIdx(newPlayerX + (playerWidth / 2.0f), tileSize);


    if ((currentTileMap->canMove(tileForNewY, tileForLeftX)) && (currentTileMap->canMove(tileForNewY, tileForRightX))) {
        gameState->playerX = newPlayerX;
        gameState->playerY = newPlayerY;
    }

    drawRectangle(memory, width, height, 0, 0, (float)width, (float)height, 0.0f, 0.0f, 0.0f); // Clear screen to black


    // Draw TileMap
    float grayShadeForTile = 0.5;
    for (int j = 0; j < tileMapHeight; j++) {
        for (int i = 0; i < tileMapWidth; i++) {
            int minX = tileSize * i;
            int maxX = tileSize * (i + 1);
            int minY = tileSize * j;
            int maxY = tileSize * (j + 1);
            if (currentTileMap->getTileValue(j,i) == 0) {
                grayShadeForTile = 0.5;
            }
            else {
                grayShadeForTile = 1.0;
            }
            drawRectangle(memory, width, height, (float)minX, (float)minY, (float)maxX, (float)maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }

    // Draw Player
    drawRectangle(memory, width, height, (float)gameState->playerX - playerWidth / 2.0f, (float)gameState->playerY - playerHeight / 2.0f, gameState->playerX + playerWidth / 2.0f, gameState->playerY + playerHeight / 2.0f, 1.0f, 1.0f, 0.0f);
}