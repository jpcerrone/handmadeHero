#include "main.h"

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
            uint8_t red = 0x00;
            uint8_t green = 0;
            uint8_t blue = x + xOffset;
            //*pixel = 0x000000FF; //xx RR GG BB -> little endian, most significant bits get loaded last (xx)
            *pixel = (green << 8) | blue;
            pixel++;
        }
    }
}

void loadSineWave(uint32_t framesToWrite, void *bufferLocation, int samplesPerSec)
{
    int samplesPerWave = samplesPerSec / frequency;

    int16_t volume = 1200;
    int16_t *sample = (int16_t *)bufferLocation;
    for (int i = 0; i < framesToWrite; i++)
    { // The size of an audio frame is the number of channels in the stream multiplied by the sample size
        {
            waveOffset += ((float)1 / (float)samplesPerWave);
            float sinValue = sinf(2.0f * (float)M_PI * waveOffset);
            *sample = sinValue * volume;
            *(sample + 1) = sinValue * volume;
        }
        sample += 2;
    }
    waveOffset -= (int)waveOffset; // Keep it between 0 and 1 to avoid overflow.
}

void increaseSoundFrequency(int ammount)
{
    frequency += ammount;
}

void decreaseSoundFrequency(int ammount)
{
    frequency -= ammount;
}

void updateAndRender(uint32_t framesToWrite, void *bufferLocation, int samplesPerSec,
                     void *memory, int width, int height, GameInputState newState)
{
    static int xOffset = 0;
    if (newState.A_Button.isDown){
        xOffset++;
    }
    loadSineWave(framesToWrite, bufferLocation, samplesPerSec);
    #if 1
        return renderGradient(memory, width, height, xOffset);
    #else
        return renderArgFlag(memory, width, height);
    #endif
}