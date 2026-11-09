#ifndef SAVE_IMAGE_H__
#define SAVE_IMAGE_H__

#include "neon_pixel.h"
#include "png/png.h"
#include <SDL/SDL.h>
#include <stdlib.h>
#include <setjmp.h> // after png.h: libpng 1.2 refuses it before
#include "file.h"

void IMG_Save(SDL_Surface *image, char *path)
{
    if (image == NULL || path == NULL)
        return;

    int width = image->w;
    int height = image->h;
    int pitch = image->pitch;

    // Written to "<path>.tmp" and renamed, with libpng errors caught:
    // without setjmp() a write error (SD card full) made libpng abort() the
    // program, and a cut write left a truncated PNG.
    char tmp_path[PATH_MAX];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path))
        return;

    Uint32 *linebuffer = (Uint32 *)malloc(width * sizeof(Uint32));
    if (linebuffer == NULL)
        return;

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        free(linebuffer);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_ptr ? png_create_info_struct(png_ptr) : NULL;
    if (png_ptr == NULL || info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        remove(tmp_path);
        free(linebuffer);
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; y++) {
        /* Use pitch to correctly advance row pointer for non-contiguous surfaces */
        Uint32 *src = (Uint32 *)((Uint8 *)image->pixels + y * pitch);
        /* NEON-accelerated ARGB→RGBA with alpha-conditional zeroing */
        neon_argb_to_rgba_alpha(linebuffer, src, width);
        png_write_row(png_ptr, (png_bytep)linebuffer);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(linebuffer);

    // fflush + fsync + rename + directory fsync; the temp file goes on failure.
    file_atomic_commit(fp, tmp_path, path);
}

#endif // SAVE_IMAGE_H__
