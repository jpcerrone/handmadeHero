#include "main.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#ifdef DEV_BUILD
#define Assert(expression) if(!(expression)){std::cout << "Assertion failure at " << __FUNCTION__ << "-" << __FILE__ << ":" << __LINE__ << std::endl; *(int*) 0 = 0;}
#else
#define Assert(expression)
#endif

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

void updateAndRender(GameMemory *gameMemory, uint32_t framesToWrite, void *bufferLocation, int samplesPerSec,
                     void *memory, int width, int height, GameInputState inputState)
{
    GameState *gameState = (GameState*)gameMemory->permanentStorage;
    Assert(sizeof(GameState) <= gameMemory->permanentStorageSize);
    if (!gameMemory->isinitialized){
        gameState->frequency = 440;
        gameMemory->isinitialized = true;
    }

    // Disabling for now as it's only for debugging.
/*     FileReadResult result;
    result = readFile("main.h");
    writeFile("out.txt", result.memory, result.size);
    freeFileMemory(result.memory); */
    if (inputState.A_Button.isDown){
        gameState->xOffset++;
    }
    gameState->frequency += inputState.Left_Stick.xPosition;
    loadSineWave(framesToWrite, bufferLocation, samplesPerSec, gameState->frequency, &gameState->waveOffset);
    #if 1
        renderGradient(memory, width, height, gameState->xOffset);
    #else
        renderArgFlag(memory, width, height);
    #endif
}