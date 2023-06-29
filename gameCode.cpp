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

void drawRectangle(void* bitmap, int bmWidth, int bmHeight, int recX, int recY, int recWidth, int recHeight) {
    uint32_t* pixel = (uint32_t*)bitmap;
    static uint32_t color = 0xFFFFFFFF;

    // TODO: Add bounds checking.

    // Go to upper left corner.
    pixel += bmWidth*(recY - recHeight/2);
    pixel += recX - recWidth / 2;

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
            *pixel = (green << 8) | blue;
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

extern "C" GAMECODE_API UPDATE_AND_RENDER(updateAndRender)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized){
        gameState->frequency = 440;
        gameState->playerX = 60;
        gameState->playerY = 240;
        gameState->jumping = false;
        gameState->jumpProgress = 0.0f;
        gameMemory->isinitialized = true;
    }

    // Disabling for now as it's only for debugging.
    //FileReadResult result;
    //result = gameMemory->readFile("main.h");
    //gameMemory->writeFile("out.txt", result.memory, result.size);
    //gameMemory->freeFileMemory(result.memory);
    if (inputState.A_Button.isDown){
        gameState->xOffset++;
    }

    // Jump
    if (inputState.B_Button.isDown && !gameState->jumping) {
        gameState->jumping = true;
        gameState->jumpProgress = 0.0f;
    }
    if (gameState->jumping) {
        gameState->playerY = 240 - (int)(150.0f*sin(gameState->jumpProgress*M_PI));
        gameState->jumpProgress += 0.05f; // TODO: Calculate this based on the expected frames it should take.
        if (gameState->jumpProgress >= 1.0f) {
            gameState->jumping = false;
        }
    }

    gameState->frequency += inputState.Left_Stick.xPosition;
    if (inputState.Left_Stick.xPosition < 0) {
        gameState->playerX-=4;
    }
    if (inputState.Left_Stick.xPosition > 0) {
        gameState->playerX+=4;
    }
    loadSineWave(framesToWrite, bufferLocation, samplesPerSec, gameState->frequency, &gameState->waveOffset);
    #if 1
        renderGradient(memory, width, height, gameState->xOffset);
    #else
        renderArgFlag(memory, width, height);
    #endif
    drawRectangle(memory, width, height, gameState->playerX, gameState->playerY, 20, 20);
}