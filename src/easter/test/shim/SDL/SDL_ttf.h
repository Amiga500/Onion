#include "SDL.h"
typedef struct TTF_Font TTF_Font;
int TTF_Init(void);
void TTF_Quit(void);
const char *TTF_GetError(void);
TTF_Font *TTF_OpenFont(const char *, int);
void TTF_CloseFont(TTF_Font *);
int TTF_SizeUTF8(TTF_Font *, const char *, int *, int *);
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *, const char *, SDL_Color);
