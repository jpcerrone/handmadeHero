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

#define tileMapWidth 17
#define tileMapHeight 9
struct TileMap {
    int* map;
};

#define WORLD_WIDTH 2
#define WORLD_HEIGHT 2
struct World {
    TileMap* tilemaps;
    float tileSize = 1.0;
    TileMap* getTileMap(int y, int x) {
        if ((y >= 0) && (x >= 0) && (y < WORLD_HEIGHT) && (x < WORLD_WIDTH)) {
            return &tilemaps[y * WORLD_WIDTH + x];
        }
        else {
            return nullptr;
        }
    }
};

int getTileValue(World world, int tileMapY, int tileMapX, int y, int x) {
    return world.getTileMap(tileMapY, tileMapX)->map[y * tileMapWidth + x];
}

bool canMove(World world, int tileSetY, int tileSetX, int y, int x) { // Todo pointer instead?
    if ((tileSetY < 0) || (tileSetX < 0) || (tileSetY >= WORLD_HEIGHT) || (tileSetX >= WORLD_WIDTH)) { // Out of bounds
        return false;
    }
    if (x >= tileMapWidth) {
        if (world.getTileMap(tileSetY, tileSetX + 1)) {
            return getTileValue(world, tileSetY, tileSetX+1, y, 0) == 0;
        }
        else {
            return false;
        }
    }
    if ((x < 0)) {
        if (world.getTileMap(tileSetY, tileSetX - 1)) {
            return getTileValue(world, tileSetY, tileSetX - 1, y, tileMapWidth - 1) == 0;
        }
        else {
            return false;
        }
    }
    if (y >= tileMapHeight) {
        if (world.getTileMap(tileSetY + 1, tileSetX)) {
            return getTileValue(world, tileSetY + 1, tileSetX, 0, x) == 0;
        }
        else {
            return false;
        }
    }
    if ((y < 0)) {
        if (world.getTileMap(tileSetY - 1, tileSetX)) {
            return getTileValue(world, tileSetY - 1, tileSetX, tileMapHeight - 1, x) == 0;
        }
        else {
            return false;
        }
    }
    return getTileValue(world, tileSetY, tileSetX, y, x) == 0;
}

Coordinate getCanonCoordinate(World *world, int mapX, int mapY, int tileX, int tileY, float offsetX, float offsetY) {
    Coordinate retCoord;
    retCoord.mapX = mapX;
    retCoord.mapY = mapY;
    retCoord.tileX = tileX;
    retCoord.tileY = tileY;
    retCoord.offsetX = offsetX;
    retCoord.offsetY = offsetY;
    // Offset
    if (retCoord.offsetX > world->tileSize) {
        retCoord.offsetX -= world->tileSize;
        retCoord.tileX += 1;
    }
    if (retCoord.offsetX < 0) {
        retCoord.offsetX += world->tileSize;
        retCoord.tileX -= 1;
    }
    if (retCoord.offsetY > world->tileSize) {
        retCoord.offsetY -= world->tileSize;
        retCoord.tileY += 1;
    }
    if (retCoord.offsetY < 0) {
        retCoord.offsetY += world->tileSize;
        retCoord.tileY -= 1;
    }

    // Tile
    if (retCoord.tileX >= tileMapWidth) {
        retCoord.tileX = 0;
        retCoord.mapX += 1;
    }
    if (retCoord.tileX < 0) {
        retCoord.tileX = tileMapWidth - 1;
        retCoord.mapX -= 1;
    }
    if (retCoord.tileY >= tileMapHeight) {
        retCoord.tileY = 0;
        retCoord.mapY += 1;
    }
    if (retCoord.tileY < 0) {
        retCoord.tileY = tileMapHeight - 1;
        retCoord.mapY -= 1;
    }
    return retCoord;
}

const float pixelsPerUnit = 48.0f;

float pixelsToUnits(float pixelMagnitude) {
    return (float)pixelMagnitude / pixelsPerUnit;
}
float unitsToPixels(float unitMagnitude) {
    return unitMagnitude * pixelsPerUnit;
}

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized){
        gameState->playerCoord.mapX = 0;
        gameState->playerCoord.mapY = 0;
        gameState->playerCoord.tileX = 5;
        gameState->playerCoord.tileY = 5;
        gameState->playerCoord.offsetX = 0.0f;
        gameState->playerCoord.offsetY = 0.0f;
        gameMemory->isinitialized = true;
    }

    World overworld;

    TileMap t0;
    int t0Map[tileMapHeight][tileMapWidth] = { // This is from bottom to top
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
        {0,0,0,1, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,0,1,0, 0,0,0,0, 0,1,1,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t1.map = (int*)t1Map;
    TileMap t2;
    int t2Map[tileMapHeight][tileMapWidth] = {
        {0,0,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 1,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,1,0,0, 0,0,1,1, 1,1,1,0, 0},
        {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,0,1, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1},
    };
    t2.map = (int*)t2Map;
    TileMap t3;
    int t3Map[tileMapHeight][tileMapWidth] = {
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,1,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0},
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

    float playerSpeed = 5.0f;
    float playerWidth = 0.8f;
    float playerHeight = 1;
    float newOffsetX = gameState->playerCoord.offsetX;
    float newOffsetY = gameState->playerCoord.offsetY;
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

    int newTileCenterX = gameState->playerCoord.tileX;
    int newTileCenterY = gameState->playerCoord.tileY;
    int newMapX = gameState->playerCoord.mapX;
    int newMapY = gameState->playerCoord.mapY;
    Coordinate middle = getCanonCoordinate(&overworld, newMapX, newMapY, newTileCenterX, newTileCenterY, newOffsetX, newOffsetY);
    Coordinate left = getCanonCoordinate(&overworld, newMapX, newMapY, newTileCenterX, newTileCenterY, newOffsetX - playerWidth/2.0f, newOffsetY + playerHeight / 2.0f);
    Coordinate right = getCanonCoordinate(&overworld, newMapX, newMapY, newTileCenterX, newTileCenterY, newOffsetX + playerWidth / 2.0f, newOffsetY + playerHeight/2.0f);
    if (canMove(overworld, right.mapY, right.mapX, right.tileY, right.tileX) 
        && canMove(overworld, left.mapY, left.mapX, left.tileY, left.tileX)
        && canMove(overworld, middle.mapY, middle.mapX, middle.tileY, middle.tileX)) {
        gameState->playerCoord.mapX = middle.mapX;
        gameState->playerCoord.mapY = middle.mapY;
        gameState->playerCoord.tileX = middle.tileX;
        gameState->playerCoord.tileY = middle.tileY;
        gameState->playerCoord.offsetX = middle.offsetX;
        gameState->playerCoord.offsetY = middle.offsetY;
    }

    drawRectangle(memory, width, height, 0, 0, (float)width, (float)height, 0.0f, 0.0f, 0.0f); // Clear screen to black

    // Draw TileMap
    float grayShadeForTile = 0.5;
    for (int j = 0; j < tileMapHeight; j++) {
        for (int i = 0; i < tileMapWidth; i++) {
            if (getTileValue(overworld, gameState->playerCoord.mapY, gameState->playerCoord.mapX, j, i) == 0) {
                grayShadeForTile = 0.5;
            }
            else {
                grayShadeForTile = 1.0;
            }
            if ((gameState->playerCoord.tileX == i) && (gameState->playerCoord.tileY == (j))) {
                grayShadeForTile = 0.2f;
            }
            float minX = unitsToPixels(overworld.tileSize * i);
            float maxX = unitsToPixels(overworld.tileSize * (i + 1));

            float minY = unitsToPixels(overworld.tileSize *j);
            float maxY = unitsToPixels(overworld.tileSize * (j + 1));
            drawRectangle(memory, width, height, minX, minY, maxX, maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }
    // Units from cannonical:
    float playerX = (gameState->playerCoord.tileX * overworld.tileSize) + gameState->playerCoord.offsetX;
    float playerY = (gameState->playerCoord.tileY * overworld.tileSize) + gameState->playerCoord.offsetY;
    //playerY = overworld.tileSize * tileMapHeight - playerY + playerHeight / 2.0f; // Invert y to draw 
    // Draw Player
    drawRectangle(memory, width, height, unitsToPixels(playerX - playerWidth / 2.0f), unitsToPixels(playerY - playerHeight / 2.0f), 
        unitsToPixels(playerX + playerWidth / 2.0f), unitsToPixels(playerY + playerHeight / 2.0f), 1.0f, 1.0f, 0.0f);

    //drawRectangle(memory, width, height, 10.0, 10.0,
    //    20.0, 20.0, 1.0f, 0.0f, 0.0f);
}