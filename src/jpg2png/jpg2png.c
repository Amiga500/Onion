#include <jpeglib.h>
#include <mi_gfx.h>
#include <mi_sys.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../common/utils/neon_pixel.h"

#define ALIGN4K(val) ((val + 4095) & (~4095))

//
//	GFX BlitSurface with scale
//
void GFX_BlitSurface(MI_PHY srcPa, void *srcVa, uint32_t sw, uint32_t sh,
                     MI_PHY dstPa, void *dstVa, uint32_t dw, uint32_t dh)
{
    MI_GFX_Surface_t Src;
    MI_GFX_Surface_t Dst;
    MI_GFX_Rect_t SrcRect;
    MI_GFX_Rect_t DstRect;
    MI_GFX_Opt_t Opt;
    MI_U16 Fence;

    Src.phyAddr = srcPa;
    Src.u32Width = sw;
    Src.u32Height = sh;
    Src.u32Stride = sw * 4;
    Src.eColorFmt = E_MI_GFX_FMT_ARGB8888;
    SrcRect.s32Xpos = 0;
    SrcRect.s32Ypos = 0;
    SrcRect.u32Width = sw;
    SrcRect.u32Height = sh;

    Dst.phyAddr = dstPa;
    Dst.u32Width = dw;
    Dst.u32Height = dh;
    Dst.u32Stride = dw * 4;
    Dst.eColorFmt = E_MI_GFX_FMT_ARGB8888;
    DstRect.s32Xpos = 0;
    DstRect.s32Ypos = 0;
    DstRect.u32Width = dw;
    DstRect.u32Height = dh;

    memset(&Opt, 0, sizeof(Opt));
    Opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;

    MI_SYS_FlushInvCache(srcVa, ALIGN4K(sw * sh * 4));
    MI_SYS_FlushInvCache(dstVa, ALIGN4K(dw * dh * 4));
    MI_GFX_BitBlit(&Src, &SrcRect, &Dst, &DstRect, &Opt, &Fence);
    MI_GFX_WaitAllDone(FALSE, Fence);
}

//
//	Convert jpeg to png
//
int main(int argc, char *argv[])
{
    struct jpeg_decompress_struct jpeg;
    struct jpeg_error_mgr err;
    png_structp png_ptr;
    png_infop info_ptr;
    FILE *fp = NULL;
    MI_PHY jpgPa = 0, pngPa = 0;
    void *tmp, *jpgVa = NULL, *pngVa = NULL;
    uint8_t *src8;
    uint32_t *src, *dst, y, sw, sh, dw, dh, ss = 0, ds = 0, mw = 250,
                                                    mh = 360;
    char filename[256], *ptr;

    // Read commandline and open jpg
    if (argc < 2)
        goto usage;
    if (argc > 2)
        mw = (uint32_t)strtoul(argv[2], NULL, 10);
    if (argc > 3)
        mh = (uint32_t)strtoul(argv[3], NULL, 10);
    fp = fopen(argv[1], "rb");
    if ((!fp) || (!mw) || (!mh))
        goto usage;

    // Read jpg header and calc size
    jpeg.err = jpeg_std_error(&err);
    jpeg_create_decompress(&jpeg);
    jpeg_stdio_src(&jpeg, fp);
    jpeg_read_header(&jpeg, TRUE);
    jpeg_start_decompress(&jpeg);
    sw = jpeg.output_width;
    sh = jpeg.output_height;
    if ((uint64_t)sw * (uint64_t)sh * 4 > UINT32_MAX) {
        fprintf(stderr, "jpg dimensions too large: %ux%u\n", sw, sh);
        goto error;
    }
    ss = ALIGN4K(sw * sh * 4);
    if ((!sw) || (!sh) || (!ss) || (jpeg.out_color_components != 3)) {
        fprintf(stderr, "jpg format error\n");
        goto error;
    }

    // Initialize MI_lib and allocate jpg mem
    MI_SYS_Init();
    MI_GFX_Open();
    MI_SYS_MMA_Alloc(NULL, ss, &jpgPa);
    MI_SYS_Mmap(jpgPa, ss, &jpgVa, TRUE);

    // Read jpeg
    tmp = malloc(jpeg.output_width * 3);
    if (tmp == NULL) {
        jpeg_destroy_decompress(&jpeg);
        fclose(fp);
        fp = NULL;
        goto error;
    }
    dst = jpgVa;
    for (y = 0; y < sh; y++) {
        src8 = tmp;
        jpeg_read_scanlines(&jpeg, &src8, 1);
        /* NEON-accelerated RGB888 → ARGB8888 conversion (16 pixels per iteration) */
        neon_rgb888_to_argb(dst, tmp, sw);
        dst += sw;
    }
    free(tmp);
    jpeg_finish_decompress(&jpeg);
    jpeg_destroy_decompress(&jpeg);
    fclose(fp);
    fp = NULL;

    // Calculate png size
    if (sw == 0 || sh == 0) {
        fprintf(stderr, "jpg has zero dimensions\n");
        goto error;
    }
    dw = mw;
    dh = sh * dw / sw;
    if (dh > mh) {
        dh = mh;
        dw = sw * dh / sh;
    }
    if (dw == 0 || dh == 0) {
        fprintf(stderr, "scaled dimensions are zero\n");
        goto error;
    }
    if ((uint64_t)dw * dh * 4 > UINT32_MAX) {
        fprintf(stderr, "scaled dimensions too large: %ux%u\n", dw, dh);
        goto error;
    }
    ds = ALIGN4K(dw * dh * 4);

    // Allocate png mem and scale
    MI_SYS_MMA_Alloc(NULL, ds, &pngPa);
    MI_SYS_Mmap(pngPa, ds, &pngVa, TRUE);
    GFX_BlitSurface(jpgPa, jpgVa, sw, sh, pngPa, pngVa, dw, dh);
    MI_SYS_Munmap(jpgVa, ss);
    MI_SYS_MMA_Free(jpgPa);
    jpgVa = NULL;
    jpgPa = 0;
    printf("sw:%d sh:%d dw:%d dh:%d\n", sw, sh, dw, dh);

    // Create png
    snprintf(filename, sizeof(filename), "%s", argv[1]);
    ptr = strrchr(filename, '.');
    if (ptr)
        *ptr = 0;
    size_t fn_len = strlen(filename);
    if (fn_len + 4 < sizeof(filename))
        memcpy(filename + fn_len, ".png", 5);
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "png write error\n");
        goto error;
    }

    // Write png
    tmp = malloc(dw * 4);
    if (tmp == NULL) {
        fprintf(stderr, "malloc error\n");
        fclose(fp);
        fp = NULL;
        goto error;
    }
    dst = tmp;
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, dw, dh, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    src = pngVa;
    for (y = 0; y < dh; y++) {
        /* NEON-accelerated ARGB→RGBA conversion for PNG output */
        neon_argb_to_rgba(dst, src, dw);
        src += dw;
        png_write_row(png_ptr, (png_bytep)tmp);
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);
    sync();
    free(tmp);

    // Quit
    MI_SYS_Munmap(pngVa, ds);
    MI_SYS_MMA_Free(pngPa);
    MI_GFX_Close();
    MI_SYS_Exit();

    return 0;

usage:
    printf("usage: %s filename.jpg [max_width:def=250] [max_height:def=360]\n",
           argv[0]);
error:
    if (fp)
        fclose(fp);
    if (jpgVa)
        MI_SYS_Munmap(jpgVa, ss);
    if (pngVa)
        MI_SYS_Munmap(pngVa, ds);
    if (jpgPa)
        MI_SYS_MMA_Free(jpgPa);
    if (pngPa)
        MI_SYS_MMA_Free(pngPa);
    return 1;
}
