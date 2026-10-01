/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/artwork.h"
#include <SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t be32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

static bool bounded(uint32_t width, uint32_t height)
{
    return width > 0 && height > 0 && width <= 2000 && height <= 2000;
}

/* Walk length-delimited JPEG markers, never compressed entropy data.
 * Reject unsupported frames and deferred heights. */
static bool jpeg_bounds(FILE *file, long size)
{
    if (fseek(file, 2, SEEK_SET)) {
        return false;
    }
    while (ftell(file) < size) {
        if (fgetc(file) != 0xff) {
            return false;
        }
        int marker;
        do {
            marker = fgetc(file);
        } while (marker == 0xff);
        if (marker <= 0 || marker == 0xda || marker == 0xd9 || marker == 0xd8 || marker == 0x01 ||
            (marker >= 0xd0 && marker <= 0xd7)) {
            return false;
        }
        unsigned char length[2];
        if (fread(length, 1, 2, file) != 2) {
            return false;
        }
        unsigned int bytes = (unsigned int)length[0] * 256 + length[1];
        long position = ftell(file);
        if (position < 0 || position > size || bytes < 2 ||
            bytes - 2 > (unsigned long)(size - position)) {
            return false;
        }
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 &&
            marker != 0xcc) {
            unsigned char frame[6];
            if ((marker != 0xc0 && marker != 0xc1 && marker != 0xc2) || bytes < 8 ||
                fread(frame, 1, sizeof frame, file) != sizeof frame || frame[0] != 8 ||
                frame[5] < 1 || frame[5] > 4 || bytes != 8u + 3u * frame[5]) {
                return false;
            }
            return bounded((unsigned int)frame[3] * 256 + frame[4],
                           (unsigned int)frame[1] * 256 + frame[2]);
        }
        if (fseek(file, (long)bytes - 2, SEEK_CUR)) {
            return false;
        }
    }
    return false;
}

SDL_Surface *mainui_artwork_load(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    SDL_Surface *image = NULL;
    SDL_Surface *(*decode)(SDL_RWops *) = NULL;
    unsigned char header[54];
    if (fseek(file, 0, SEEK_END)) {
        goto done;
    }
    long size = ftell(file);
    if (size <= 0 || size > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET)) {
        goto done;
    }
    size_t count = fread(header, 1, sizeof header, file);
    if (count >= 33 && !memcmp(header, "\x89PNG\r\n\x1a\n", 8) && be32(header + 8) == 13 &&
        !memcmp(header + 12, "IHDR", 4) && bounded(be32(header + 16), be32(header + 20))) {
        decode = IMG_LoadPNG_RW;
    }
    else if (count >= 2 && header[0] == 0xff && header[1] == 0xd8 && jpeg_bounds(file, size)) {
        decode = IMG_LoadJPG_RW;
    }
    else if (count >= 54 && header[0] == 'B' && header[1] == 'M') {
        uint32_t dib = le32(header + 14), height = le32(header + 22);
        /* BMP permits signed top-down heights. INT32_MIN fails the bound. */
        if (height & UINT32_C(0x80000000)) {
            height = 0u - height;
        }
        if ((dib == 40 || dib == 108 || dib == 124) && bounded(le32(header + 18), height) &&
            le32(header + 30) <= 3) {
            decode = IMG_LoadBMP_RW;
        }
    }
    /* Same open file, exact validated decoder: no unchecked format fallback. */
    if (decode && !fseek(file, 0, SEEK_SET)) {
        SDL_RWops *rw = SDL_RWFromFP(file, 0);
        if (rw) {
            image = decode(rw);
            SDL_RWclose(rw);
        }
    }
done:
    fclose(file);
    return image;
}

/* UI composition happens in software even when the final display is hardware.
 * Explicit RGBA conversion also turns palette/RGB color keys into alpha, so
 * their hidden RGB (often bright green) never depends on a hardware key blit. */
SDL_Surface *mainui_artwork_software(SDL_Surface *image)
{
    if (!image) {
        return NULL;
    }
    SDL_Surface *format = SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1, 32, 0x00ff0000, 0x0000ff00,
                                               0x000000ff, 0xff000000);
    if (!format) {
        return NULL;
    }
    SDL_Surface *converted = SDL_ConvertSurface(image, format->format, SDL_SWSURFACE);
    SDL_FreeSurface(format);
    if (converted) {
        SDL_SetAlpha(converted, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
    }
    return converted;
}
