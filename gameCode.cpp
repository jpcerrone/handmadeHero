#include "gameCode.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include "world.cpp"
#include "memory_arena.cpp"
#include "instrinsics.h"

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

#pragma pack(push, 1)
struct BitmapHeader {
    uint16_t FileType;
    uint32_t FileSize;
    uint16_t Reserved1;
    uint16_t Reserved2;
    uint32_t BitmapOffset;
    uint32_t Size;            /* Size of this header in bytes */
    int32_t  Width;           /* Image width in pixels */
    int32_t  Height;          /* Image height in pixels */
    uint16_t  Planes;          /* Number of color planes */
    uint16_t  BitsPerPixel;    /* Number of bits per pixel */
    uint32_t Compression;     /* Compression methods used */
    uint32_t SizeOfBitmap;    /* Size of bitmap in bytes */
    int32_t  HorzResolution;  /* Horizontal resolution in pixels per meter */
    int32_t  VertResolution;  /* Vertical resolution in pixels per meter */
    uint32_t ColorsUsed;      /* Number of colors in the image */
    uint32_t ColorsImportant; /* Minimum number of important colors */
    /* Fields added for Windows 4.x follow this line */

    uint32_t RedMask;       /* Mask identifying bits of red component */
    uint32_t GreenMask;     /* Mask identifying bits of green component */
    uint32_t BlueMask;      /* Mask identifying bits of blue component */
    uint32_t AlphaMask;     /* Mask identifying bits of alpha component */
};
#pragma pack(pop)

Bitmap loadBMP(char* path, readFile_t* readFunction, ThreadContext *thread) {
    FileReadResult result = readFunction(thread, path);
    Bitmap retBitmap = {};
    BitmapHeader *header = (BitmapHeader*)(result.memory);

    Assert(header->Compression == 3);

    retBitmap.startPixelPointer = (uint32_t*)((uint8_t*)result.memory + header->BitmapOffset);
    retBitmap.width = header->Width;
    retBitmap.height = header->Height;

    // Modify loaded bmp to set its pixels in the right order. Our pixel format is AARRGGBB, but bmps may vary because of their masks.
    int redOffset = findFirstSignificantBit(header->RedMask);
    int greenOffset = findFirstSignificantBit(header->GreenMask);
    int blueOffset = findFirstSignificantBit(header->BlueMask);
    int alphaOffset = findFirstSignificantBit(header->AlphaMask);

    uint32_t *modifyingPixelPointer = retBitmap.startPixelPointer;
    for (int j = 0; j < header->Height; j++) {
        for (int i = 0; i < header->Width; i++) {
            int newRedValue = ((*modifyingPixelPointer & header->RedMask) >> redOffset) << 16;
            int newGreenValue = ((*modifyingPixelPointer & header->GreenMask) >> greenOffset) << 8;
            int newBlueValue = ((*modifyingPixelPointer & header->BlueMask) >> blueOffset) << 0;
            int newAlphaValue = ((*modifyingPixelPointer & header->AlphaMask) >> alphaOffset) << 24;

            *modifyingPixelPointer = newAlphaValue | newRedValue | newGreenValue | newBlueValue; //OG RRGGBBAA
            modifyingPixelPointer++;
        }
    }

    return retBitmap;
}

float clamp(float value) {
    if (value < 0) {
        return 0;
    }
    else {
        return value;
    }
}

void displayBMP(uint32_t *bufferMemory, const Bitmap *bitmap, float x, float y, int screenWidth, int screenHeight) {

    y += bitmap->offsetY;
    x += bitmap->offsetX;

    //x = clamp(x);
    //y = clamp(y);

    int drawHeight = bitmap->height;
    int drawWidth = bitmap->width;
    if (drawHeight + y > screenHeight) {
        drawHeight = (int)(screenHeight - y);
    }    
    if (drawWidth + x > screenWidth) {
        drawWidth = (int)(screenWidth - x);
    }
    // Go to upper left corner.
    bufferMemory += (int)clamp((float)screenWidth * (screenHeight - (bitmap->height + roundFloat(clamp(y)))));
    bufferMemory += roundFloat(clamp(x));

    uint32_t* pixelPointer = bitmap->startPixelPointer;
    pixelPointer += (drawHeight-1) * bitmap->width;

    if (x < 0) {
        drawWidth += roundFloat(x);
        pixelPointer -= roundFloat(x);
    }
    if (y < 0) {
        drawHeight += roundFloat(y);
        bufferMemory -= screenWidth * (roundFloat(y));
    }

    int strideToNextRow = screenWidth - drawWidth;
    if (strideToNextRow < 0) {
        strideToNextRow = 0;
    }

    for (int j = 0; j < drawHeight; j++) {
        for (int i = 0; i < drawWidth; i++) {
            float alphaValue = (*pixelPointer >> 24) / 255.0f;
            uint32_t redValueS = (*pixelPointer & 0xFF0000) >> 16;
            uint32_t greenValueS = (*pixelPointer & 0x00FF00) >> 8;
            uint32_t blueValueS = (*pixelPointer & 0x0000FF);

            uint32_t redValueD = (*bufferMemory & 0xFF0000) >> 16;
            uint32_t greenValueD = (*bufferMemory & 0x00FF00) >> 8;
            uint32_t blueValueD = *bufferMemory & 0x0000FF;

            uint32_t interpolatedPixel = (uint32_t)(alphaValue * redValueS + (1 - alphaValue) * redValueD) << 16
                | (uint32_t)(alphaValue * greenValueS + (1 - alphaValue) * greenValueD) << 8
                | (uint32_t)(alphaValue * blueValueS + (1 - alphaValue) * blueValueD);

            *bufferMemory = interpolatedPixel;
            bufferMemory++;
            pixelPointer++; // left to right
        }
        pixelPointer += bitmap->width - drawWidth; // Remainder
        pixelPointer -= 2* bitmap->width; // start at the top, go down
        bufferMemory += strideToNextRow;
    }
}

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized) {
        gameState->background = loadBMP("../data/test/test_background.bmp", gameMemory->readFile, thread);

        gameState->guyHead[0] = loadBMP("../data/test/test_hero_back_head.bmp", gameMemory->readFile, thread);
        gameState->guyHead[1] = loadBMP("../data/test/test_hero_front_head.bmp", gameMemory->readFile, thread);
        gameState->guyHead[2] = loadBMP("../data/test/test_hero_left_head.bmp", gameMemory->readFile, thread);
        gameState->guyHead[3] = loadBMP("../data/test/test_hero_right_head.bmp", gameMemory->readFile, thread);

        gameState->guyCape[0] = loadBMP("../data/test/test_hero_back_cape.bmp", gameMemory->readFile, thread);
        gameState->guyCape[1] = loadBMP("../data/test/test_hero_front_cape.bmp", gameMemory->readFile, thread);
        gameState->guyCape[2] = loadBMP("../data/test/test_hero_left_cape.bmp", gameMemory->readFile, thread);
        gameState->guyCape[3] = loadBMP("../data/test/test_hero_right_cape.bmp", gameMemory->readFile, thread);

        gameState->guyTorso[0] = loadBMP("../data/test/test_hero_back_torso.bmp", gameMemory->readFile, thread);
        gameState->guyTorso[1] = loadBMP("../data/test/test_hero_front_torso.bmp", gameMemory->readFile, thread);
        gameState->guyTorso[2] = loadBMP("../data/test/test_hero_left_torso.bmp", gameMemory->readFile, thread);
        gameState->guyTorso[3] = loadBMP("../data/test/test_hero_right_torso.bmp", gameMemory->readFile, thread);

        for (int i = 0; i < 4; i++) {
            gameState->guyHead[i].offsetY = -33;
            gameState->guyCape[i].offsetY = -33;
            gameState->guyTorso[i].offsetY = -33;
            gameState->guyHead[i].offsetX = -52;
            gameState->guyCape[i].offsetX = -52;
            gameState->guyTorso[i].offsetX = -52;
        }

        // Construct world
        initWorldArena(&gameState->worldArena, (uint8_t*)gameMemory->permanentStorage + sizeof(GameState),
            (size_t)(gameMemory->permanentStorageSize - sizeof(GameState)));
        
        gameState->world = pushStruct(&gameState->worldArena, World);

        gameState->world->numChunksX = 256;
        gameState->world->numChunksY = 256;
        gameState->world->numChunksZ = 8;
        gameState->world->bitsForTiles = 2; // 2**4 = 16 ||| bitsForTiles**tilesPerChunk = desired num of tiles per chunk
        gameState->world->tilesPerChunk = 4;
        gameState->world->tileSize = 1.0f;

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
        gameState->orientation = 1;
        gameMemory->isinitialized = true;

    }
    World *overworld = gameState->world;

    float playerSpeed = 6.0f;
    float playerWidth = 0.8f;
    float playerHeight = 1;
    float newOffsetX = gameState->offsetinTileX;
    float newOffsetY = gameState->offsetinTileY;
    if (inputState.Left_Stick.xPosition < 0) {
        newOffsetX -= pixelsToUnits(unitsToPixels(playerSpeed) *inputState.deltaTime);
        gameState->orientation = 2;
    }
    if (inputState.Left_Stick.xPosition > 0) {
        newOffsetX += pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
        gameState->orientation = 3;
    }
    if (inputState.Left_Stick.yPosition < 0) {
        newOffsetY += pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
        gameState->orientation = 0;
    }
    if (inputState.Left_Stick.yPosition > 0) {
        newOffsetY -= pixelsToUnits(unitsToPixels(playerSpeed) * inputState.deltaTime);
        gameState->orientation = 1;
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

    drawRectangle(bitmapMemory, width, height, 0, 0, (float)width, (float)height, 0.0f, 0.0f, 0.0f); // Clear screen to black


    int playerScreenX = getTileX(overworld, gameState->playerCoord) + getChunkX(overworld, gameState->playerCoord)*overworld->tilesPerChunk;
    int playerScreenY = getTileY(overworld, gameState->playerCoord) + getChunkY(overworld, gameState->playerCoord) * overworld->tilesPerChunk;

    // Draw TileMap
    displayBMP((uint32_t*)bitmapMemory, &gameState->background, 0, 0, width, height);
    float grayShadeForTile = 0.5;

    int currentScreenX = playerScreenX/ SCREEN_TILE_WIDTH;
    int currentScreenY = playerScreenY / SCREEN_TILE_HEIGHT;

    for (int j = 0; j < SCREEN_TILE_HEIGHT; j++) {
        for (int i = 0; i < SCREEN_TILE_WIDTH; i++) {
            AbsoluteCoordinate tileCoord;
            tileCoord = constructCoordinate(overworld, 0, 0, gameState->playerCoord.z, i + currentScreenX* SCREEN_TILE_WIDTH, j + currentScreenY* SCREEN_TILE_HEIGHT);
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
                continue;
            }

            if ((playerScreenX == (i + currentScreenX * SCREEN_TILE_WIDTH)) && (playerScreenY == (j+currentScreenY * SCREEN_TILE_HEIGHT))) {
                grayShadeForTile = 0.2f;
            }

            float minX = unitsToPixels((float)(overworld->tileSize * i));
            float maxX = unitsToPixels((float)(overworld->tileSize * (i+1)));

            float minY = unitsToPixels((float)(overworld->tileSize * j));
            float maxY = unitsToPixels((float)(overworld->tileSize * (j +1)));
            drawRectangle(bitmapMemory, width, height, minX, minY, maxX, maxY, grayShadeForTile, grayShadeForTile, grayShadeForTile);
        }
    }
    // Draw Player
    drawRectangle(bitmapMemory, width, height, unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH) ,
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT),
        unitsToPixels(playerScreenX + playerWidth + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH) ,
        unitsToPixels(playerScreenY + playerHeight + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT)  , 1.0f, 1.0f, 0.0f);

    // Load data
    //displayBMP((uint32_t*)bitmapMemory, &gameState->guyTorso[gameState->orientation], unitsToPixels(playerScreenX), unitsToPixels(playerScreenY), width, height);
    //displayBMP((uint32_t*)bitmapMemory, &gameState->guyCape[gameState->orientation], unitsToPixels(playerScreenX), unitsToPixels(playerScreenY), width, height);
   /* drawRectangle(bitmapMemory, width, height, unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH),
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT),
        unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH) + gameState->guyHead[gameState->orientation].width,
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT) + gameState->guyHead[gameState->orientation].height, 1.0f, 0.0f, 0.0f);*/
    displayBMP((uint32_t*)bitmapMemory, &gameState->guyTorso[gameState->orientation],
        unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH),
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT), width, height);
    displayBMP((uint32_t*)bitmapMemory, &gameState->guyCape[gameState->orientation],
        unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH),
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT), width, height);
    displayBMP((uint32_t*)bitmapMemory, &gameState->guyHead[gameState->orientation], 
        unitsToPixels(playerScreenX + gameState->offsetinTileX - currentScreenX * SCREEN_TILE_WIDTH), 
        unitsToPixels(playerScreenY + gameState->offsetinTileY - currentScreenY * SCREEN_TILE_HEIGHT), width, height);
    //displayBMP((uint32_t*)bitmapMemory, &gameState->guyHead[gameState->orientation],0, 0, width, height);
}