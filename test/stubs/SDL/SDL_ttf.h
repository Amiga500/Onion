#ifndef ONION_TEST_STUB_SDL_TTF_H
#define ONION_TEST_STUB_SDL_TTF_H

/* SDL_ttf stub for the production theme headers (see SDL_image.h). */
#include "SDL.h"

typedef struct _TTF_Font TTF_Font;

TTF_Font *TTF_OpenFont(const char *file, int ptsize);

#endif /* ONION_TEST_STUB_SDL_TTF_H */
