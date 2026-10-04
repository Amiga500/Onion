/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DRAWING_H
#define MAINUI_DRAWING_H
#include <SDL.h>
#include <SDL_ttf.h>

/* Internal drawing helpers borrow destination/image/font. The text helper owns
 * and releases only its temporary raster; neither helper changes the clip. */
static inline void mainui_blit(SDL_Surface *screen, SDL_Surface *surface, int x, int y)
{
    SDL_Rect destination = {(Sint16)x, (Sint16)y, 0, 0};
    if (surface) {
        SDL_BlitSurface(surface, NULL, screen, &destination);
    }
}

static inline void mainui_label(SDL_Surface *screen, TTF_Font *font, SDL_Color color,
                                const char *text, int x, int y)
{
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    mainui_blit(screen, surface, x, y);
    if (surface) {
        SDL_FreeSurface(surface);
    }
}

/* Rotate the final framebuffer for Onion's physically inverted panel. Pitch and
 * pixel width are respected; logical drawing and snapshots stay upright. */
static inline void mainui_rotate_frame(SDL_Surface *screen)
{
    if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
        return;
    }
    int bytes = screen->format->BytesPerPixel;
    /* Device frames are 32-bit: reverse whole pixels without a division for
     * every byte of every frame. Keep the generic path for other formats. */
    if (bytes == 4) {
        for (int y = 0; y < (screen->h + 1) / 2; ++y) {
            Uint32 *top = (Uint32 *)((Uint8 *)screen->pixels + y * screen->pitch);
            Uint32 *bottom =
                (Uint32 *)((Uint8 *)screen->pixels + (screen->h - 1 - y) * screen->pitch);
            int limit = y == screen->h - 1 - y ? screen->w / 2 : screen->w;
            for (int x = 0; x < limit; ++x) {
                Uint32 pixel = top[x];
                top[x] = bottom[screen->w - 1 - x];
                bottom[screen->w - 1 - x] = pixel;
            }
        }
        if (SDL_MUSTLOCK(screen)) {
            SDL_UnlockSurface(screen);
        }
        return;
    }
    int count = screen->w * screen->h;
    for (int i = 0; i < count / 2; ++i) {
        int j = count - 1 - i;
        Uint8 *a =
            (Uint8 *)screen->pixels + (i / screen->w) * screen->pitch + (i % screen->w) * bytes;
        Uint8 *b =
            (Uint8 *)screen->pixels + (j / screen->w) * screen->pitch + (j % screen->w) * bytes;
        for (int k = 0; k < bytes; ++k) {
            Uint8 value = a[k];
            a[k] = b[k];
            b[k] = value;
        }
    }
    if (SDL_MUSTLOCK(screen)) {
        SDL_UnlockSurface(screen);
    }
}

/* The display area that shows frame area `area` after the 180-degree copy. */
static inline SDL_Rect mainui_rotated_rect(const SDL_Surface *frame, SDL_Rect area)
{
    SDL_Rect rotated = {(Sint16)(frame->w - area.x - area.w), (Sint16)(frame->h - area.y - area.h),
                        area.w, area.h};
    return rotated;
}

/* The device uses matching RGB masks with an opaque frame. Reverse-copy into the
 * display, respecting both pitches and its clip, without modifying the frame.
 * Keep SDL's conversion/blending semantics for other surface configurations. */
static inline int mainui_blit_rotated(SDL_Surface *frame, SDL_Surface *display)
{
    SDL_PixelFormat *src = frame->format, *dst = display->format;
    int bytes = src->BytesPerPixel;
    if (frame == display || bytes < 2 || bytes != dst->BytesPerPixel ||
        src->BitsPerPixel != dst->BitsPerPixel || src->Rmask != dst->Rmask ||
        src->Gmask != dst->Gmask || src->Bmask != dst->Bmask ||
        /* An opaque frame may fill an ARGB display: SDL writes alpha 255. */
        (src->Amask != dst->Amask && (src->Amask || bytes != 4)) ||
        (frame->flags & (SDL_SRCALPHA | SDL_SRCCOLORKEY))) {
        mainui_rotate_frame(frame);
        int result = SDL_BlitSurface(frame, NULL, display, NULL);
        mainui_rotate_frame(frame);
        return result;
    }
    if (SDL_MUSTLOCK(frame) && SDL_LockSurface(frame) < 0) {
        return -1;
    }
    if (SDL_MUSTLOCK(display) && SDL_LockSurface(display) < 0) {
        if (SDL_MUSTLOCK(frame)) {
            SDL_UnlockSurface(frame);
        }
        return -1;
    }
    Uint32 opaque = src->Amask ? 0 : dst->Amask;
    SDL_Rect clip = display->clip_rect;
    int right = clip.x + clip.w < frame->w ? clip.x + clip.w : frame->w;
    int bottom = clip.y + clip.h < frame->h ? clip.y + clip.h : frame->h;
    for (int y = clip.y; y < bottom; ++y) {
        const Uint8 *source = (const Uint8 *)frame->pixels + (frame->h - 1 - y) * frame->pitch;
        Uint8 *target = (Uint8 *)display->pixels + y * display->pitch;
        if (bytes == 4) {
            const Uint32 *pixels = (const Uint32 *)source;
            Uint32 *output = (Uint32 *)target;
            for (int x = clip.x; x < right; ++x) {
                output[x] = pixels[frame->w - 1 - x] | opaque;
            }
        }
        else {
            for (int x = clip.x; x < right; ++x) {
                for (int k = 0; k < bytes; ++k) {
                    target[x * bytes + k] = source[(frame->w - 1 - x) * bytes + k];
                }
            }
        }
    }
    if (SDL_MUSTLOCK(display)) {
        SDL_UnlockSurface(display);
    }
    if (SDL_MUSTLOCK(frame)) {
        SDL_UnlockSurface(frame);
    }
    return 0;
}

#endif
