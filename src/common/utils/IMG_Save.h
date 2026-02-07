#ifndef SAVE_IMAGE_H__
#define SAVE_IMAGE_H__

#include "png/png.h"
#include "neon_pixel.h"
#include <SDL/SDL.h>
#include <stdlib.h>

void IMG_Save(SDL_Surface *image, char *path)
{
    int width = image->w;
    int height = image->h;
    int pitch = image->pitch;

    png_structp png_ptr;
    png_infop info_ptr;
    FILE *fp;

    if (!(fp = fopen(path, "wb")))
        return;

    Uint32 *linebuffer = (Uint32 *)malloc(width * 4);
    if (linebuffer == NULL) {
        fclose(fp);
        return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info_ptr = png_create_info_struct(png_ptr);
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
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    free(linebuffer);
}

#endif // SAVE_IMAGE_H__
