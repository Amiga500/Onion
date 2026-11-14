/* Minimal stand-in for SDL 1.2, for testing on a PC without SDL installed. */
#ifndef SHIM_SDL_H
#define SHIM_SDL_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t Uint8; typedef uint16_t Uint16; typedef uint32_t Uint32;
typedef int16_t Sint16; typedef int32_t Sint32;
typedef struct { Uint8 BitsPerPixel, BytesPerPixel; Uint32 Rmask, Gmask, Bmask, Amask; } SDL_PixelFormat;
typedef struct { Sint16 x, y; Uint16 w, h; } SDL_Rect;
typedef struct { Uint8 r, g, b, unused; } SDL_Color;
typedef struct SDL_Surface { Uint32 flags; SDL_PixelFormat *format; int w, h; Uint16 pitch; void *pixels; SDL_PixelFormat fmt; } SDL_Surface;
typedef enum { SDLK_ESCAPE = 27, SDLK_SPACE = 32, SDLK_UP = 273, SDLK_DOWN, SDLK_RIGHT, SDLK_LEFT,
               SDLK_LSHIFT = 304, SDLK_LCTRL = 306, SDLK_LAST = 323 } SDLKey;
typedef struct { SDLKey sym; } SDL_keysym;
typedef struct { Uint8 type; SDL_keysym keysym; } SDL_KeyboardEvent;
typedef union { Uint8 type; SDL_KeyboardEvent key; } SDL_Event;
typedef struct SDL_mutex SDL_mutex; typedef struct SDL_Thread SDL_Thread;
typedef struct { int freq; Uint16 format; Uint8 channels, silence; Uint16 samples; Uint32 size;
                 void (*callback)(void *, Uint8 *, int); void *userdata; } SDL_AudioSpec;
#define SDL_SWSURFACE 0u
#define SDL_HWSURFACE 1u
#define SDL_SRCALPHA 0x10000u
#define SDL_DOUBLEBUF 0x40000000u
#define SDL_ALPHA_OPAQUE 255
#define SDL_INIT_AUDIO 0x10u
#define SDL_INIT_VIDEO 0x20u
#define SDL_KEYDOWN 2
#define SDL_QUIT 12
#define SDL_DISABLE 0
#define AUDIO_S16SYS 0x8010
#define SDL_MUSTLOCK(s) 0
int SDL_Init(Uint32); void SDL_Quit(void); int SDL_InitSubSystem(Uint32); void SDL_QuitSubSystem(Uint32);
const char *SDL_GetError(void);
SDL_Surface *SDL_SetVideoMode(int, int, int, Uint32);
SDL_Surface *SDL_CreateRGBSurface(Uint32, int, int, int, Uint32, Uint32, Uint32, Uint32);
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *);
void SDL_FreeSurface(SDL_Surface *);
int SDL_FillRect(SDL_Surface *, SDL_Rect *, Uint32);
int SDL_BlitSurface(SDL_Surface *, SDL_Rect *, SDL_Surface *, SDL_Rect *);
int SDL_SetAlpha(SDL_Surface *, Uint32, Uint8);
Uint32 SDL_MapRGB(const SDL_PixelFormat *, Uint8, Uint8, Uint8);
int SDL_Flip(SDL_Surface *); int SDL_LockSurface(SDL_Surface *); void SDL_UnlockSurface(SDL_Surface *);
int SDL_SaveBMP(SDL_Surface *, const char *);
int SDL_ShowCursor(int); int SDL_EnableKeyRepeat(int, int);
int SDL_PollEvent(SDL_Event *); Uint8 *SDL_GetKeyState(int *);
Uint32 SDL_GetTicks(void); void SDL_Delay(Uint32);
SDL_mutex *SDL_CreateMutex(void); void SDL_DestroyMutex(SDL_mutex *);
int SDL_LockMutex(SDL_mutex *); int SDL_UnlockMutex(SDL_mutex *);
SDL_Thread *SDL_CreateThread(int (*)(void *), void *); void SDL_WaitThread(SDL_Thread *, int *);
void SDL_LockAudio(void); void SDL_UnlockAudio(void); void SDL_PauseAudio(int);
#endif
