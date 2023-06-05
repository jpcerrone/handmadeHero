#include "main.h"
void renderArgFlag(void* memory, int width, int height)
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

void renderGradient(void* memory, int width, int height, int xOffset)
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

void renderGraphics(void* memory, int width, int height, int xOffset){
    # if 0
    return renderGradient(memory, width, height, xOffset);
    # else
    return renderArgFlag(memory, width, height);
    # endif
}