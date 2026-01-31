#ifndef COLOR_H__
#define COLOR_H__

#include <SDL/SDL.h>
#include <stdlib.h>

SDL_Color hex2sdl(char *input)
{
    // Skip '#' if present - optimize pointer arithmetic
    const char *hex_str = (input[0] == '#') ? input + 1 : input;
    
    // Direct conversion without intermediate ptr
    unsigned long value = strtoul(hex_str, NULL, 16);
    
    SDL_Color color = {
        (value >> 16) & 0xff,  // R
        (value >> 8) & 0xff,   // G
        value & 0xff           // B
    };
    return color;
}

Uint32 colorToUint(SDL_Color color)
{
    return (Uint32)((color.r << 16) + (color.g << 8) + (color.b << 0));
}

SDL_Color uintToColor(Uint32 color)
{
    SDL_Color sdl_color;
    sdl_color.unused = 255;
    sdl_color.r = (color >> 16) & 0xFF;
    sdl_color.g = (color >> 8) & 0xFF;
    sdl_color.b = color & 0xFF;
    return sdl_color;
}

#endif // COLOR_H__
