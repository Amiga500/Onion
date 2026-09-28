#ifndef ONION_TEST_STUB_SDL_H
#define ONION_TEST_STUB_SDL_H

/* Minimal SDL 1.2 stub so production headers (list.h, surfaceSetAlpha.h,
 * theme/color.h) can be included from host unit tests without libSDL. */
#include <stdint.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

#define SDL_SRCALPHA 0x00010000

typedef struct SDL_Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 unused;
} SDL_Color;

typedef int16_t Sint16;

typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

typedef struct SDL_PixelFormat {
    Uint32 Amask;
    Uint8 Ashift;
    Uint8 BytesPerPixel;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    int w;
    int h;
    int pitch;
    void *pixels;
    SDL_PixelFormat *format;
} SDL_Surface;

/* Keyboard events, for utils/keystate.h (tests provide the functions). */
typedef int SDLKey;
#define SDLK_UNKNOWN 0
#define SDLK_SPACE 32
enum { SDL_KEYDOWN = 2, SDL_KEYUP = 3, SDL_QUIT = 12 };
typedef struct SDL_keysym {
    SDLKey sym;
} SDL_keysym;
typedef struct SDL_KeyboardEvent {
    Uint8 type;
    SDL_keysym keysym;
} SDL_KeyboardEvent;
typedef union SDL_Event {
    Uint8 type;
    SDL_KeyboardEvent key;
} SDL_Event;
int SDL_PollEvent(SDL_Event *event);
Uint8 *SDL_GetKeyState(int *numkeys);

void SDL_FreeSurface(SDL_Surface *surface);
int SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);
int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha);

#endif /* ONION_TEST_STUB_SDL_H */
