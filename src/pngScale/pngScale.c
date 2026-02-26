#include <mi_gfx.h>
#include <mi_sys.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/utils/neon_pixel.h"

#define ALIGN4K(val) ((val + 4095) & (~4095))
#define ERROR(str)                 \
    {                              \
        fprintf(stderr, str "\n"); \
        goto error;                \
    }

//
//	Swap R and B channels: ABGR8888 <-> ARGB8888
//	Delegates to shared NEON assembly in neon_pixel.h
//
static inline void swap_rb_channels(const uint32_t *src, uint32_t *dst, uint32_t count)
{
    if (src == dst) {
        /* In-place swap: caller guarantees src is writable when src==dst */
        neon_swap_rb_inplace(dst, (int)count);
    } else {
        neon_argb_to_rgba((uint32_t *)dst, src, (int)count);
    }
}

//
//	Convert RGB888 to ARGB8888
//	Delegates to shared NEON assembly in neon_pixel.h
//
static inline void rgb_to_argb(const uint8_t *src_rgb, uint32_t *dst_argb, uint32_t count)
{
    neon_rgb888_to_argb(dst_argb, src_rgb, (int)count);
}

//
//	Convert gray8 (1 byte/pixel) to ARGB8888
//	Delegates to shared NEON assembly in neon_pixel.h
//
static inline void gray8_to_argb(const uint8_t *src_gray, uint32_t *dst_argb, uint32_t count)
{
    neon_gray8_to_argb(dst_argb, src_gray, (int)count);
}

//
//	Convert gray8+alpha (2 bytes/pixel) to ARGB8888
//	Delegates to shared NEON assembly in neon_pixel.h
//
static inline void gray8a_to_argb(const uint8_t *src_ga, uint32_t *dst_argb, uint32_t count)
{
    neon_gray8a_to_argb(dst_argb, src_ga, (int)count);
}

//
//	GFX BlitSurface with scale
//
void GFX_BlitSurface(MI_PHY srcPa, const void *srcVa, uint32_t sw, uint32_t sh,
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

    MI_SYS_FlushInvCache((void *)srcVa, ALIGN4K(sw * sh * 4));
    MI_SYS_FlushInvCache(dstVa, ALIGN4K(dw * dh * 4));
    MI_GFX_BitBlit(&Src, &SrcRect, &Dst, &DstRect, &Opt, &Fence);
    MI_GFX_WaitAllDone(FALSE, Fence);
}

//
//	Scale png
//
int main(int argc, char *argv[])
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte sig_bytes[8];
    png_byte ch;
    png_bytepp rows;
    FILE *fp;
    MI_PHY srcPa = 0, dstPa = 0;
    void *tmp, *srcVa = NULL, *dstVa = NULL;
    uint32_t *src, *dst;
    uint32_t y, sw, sh, dw, dh;
    uint32_t ss = 0, ds = 0, mw = 250, mh = 360;

    // Read commandline and open src
    if (argc < 3)
        goto usage;
    if (argc > 3)
        mw = (uint32_t)strtoul(argv[3], NULL, 10);
    if (argc > 4)
        mh = (uint32_t)strtoul(argv[4], NULL, 10);
    fp = fopen(argv[1], "rb");
    if ((!fp) || (!mw) || (!mh))
        goto usage;

    // Read png header and calc size
    if (fread(sig_bytes, sizeof(sig_bytes), 1, fp) != 1)
        ERROR("png format error");
    if (png_sig_cmp(sig_bytes, 0, sizeof(sig_bytes)))
        ERROR("png format error");
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, sizeof(sig_bytes));
    png_read_png(png_ptr, info_ptr,
                 PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_16 |
                     PNG_TRANSFORM_EXPAND,
                 NULL);
    ch = png_get_channels(png_ptr, info_ptr);
    sw = png_get_image_width(png_ptr, info_ptr);
    sh = png_get_image_height(png_ptr, info_ptr);
    if ((uint64_t)sw * sh * 4 > UINT32_MAX)
        ERROR("png dimensions too large");
    ss = ALIGN4K(sw * sh * 4);
    if ((!sw) || (!sh) || (!ss) || (!ch))
        ERROR("png format error");

    // Initialize MI_lib and allocate src mem
    MI_SYS_Init();
    MI_GFX_Open();
    MI_SYS_MMA_Alloc(NULL, ss, &srcPa);
    MI_SYS_Mmap(srcPa, ss, &srcVa, TRUE);

    // Read png
    rows = png_get_rows(png_ptr, info_ptr);

    // Convert png to src ARGB8888
    dst = srcVa;
    switch (ch) {
    case 1:
        for (y = 0; y < sh; y++) {
            gray8_to_argb(rows[y], dst, sw);
            dst += sw;
        }
        break;
    case 2:
        for (y = 0; y < sh; y++) {
            gray8a_to_argb(rows[y], dst, sw);
            dst += sw;
        }
        break;
    case 3:
        for (y = 0; y < sh; y++) {
            rgb_to_argb(rows[y], dst, sw);
            dst += sw;
        }
        break;
    case 4:
        for (y = 0; y < sh; y++) {
            swap_rb_channels((const uint32_t *)rows[y], dst, sw);
            dst += sw;
        }
        break;
    }

    // Close png
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    // Calculate dst size
    dw = mw;
    if (sw == 0 || sh == 0)
        ERROR("png has zero dimensions");
    dh = sh * dw / sw;
    if (dh > mh) {
        dh = mh;
        dw = sw * dh / sh;
    }
    if (dw == 0 || dh == 0)
        ERROR("scaled dimensions are zero");
    ds = ALIGN4K(dw * dh * 4);

    // Allocate dst png mem and scale
    MI_SYS_MMA_Alloc(NULL, ds, &dstPa);
    MI_SYS_Mmap(dstPa, ds, &dstVa, TRUE);
    GFX_BlitSurface(srcPa, srcVa, sw, sh, dstPa, dstVa, dw, dh);
    MI_SYS_Munmap(srcVa, ss);
    MI_SYS_MMA_Free(srcPa);
    srcVa = NULL;
    srcPa = 0;
    printf("png scaling : w:%d h:%d -> w:%d h:%d\n", sw, sh, dw, dh);

    // Create png
    fp = fopen(argv[2], "wb");
    if (!fp)
        ERROR("png write error");

    // Write png
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, dw, dh, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    src = dstVa;
    if (dw > SIZE_MAX / 4) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        MI_SYS_Munmap(dstVa, ds);
        MI_SYS_MMA_Free(dstPa);
        MI_GFX_Close();
        MI_SYS_Exit();
        return 1;
    }
    tmp = malloc((size_t)dw * 4);
    if (tmp == NULL) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        MI_SYS_Munmap(dstVa, ds);
        MI_SYS_MMA_Free(dstPa);
        MI_GFX_Close();
        MI_SYS_Exit();
        return 1;
    }
    for (y = 0; y < dh; y++) {
        swap_rb_channels(src, tmp, dw);
        src += dw;
        png_write_row(png_ptr, (png_bytep)tmp);
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(tmp);

    // Close png
    fclose(fp);
    sync();

    // Quit
    MI_SYS_Munmap(dstVa, ds);
    MI_SYS_MMA_Free(dstPa);
    MI_GFX_Close();
    MI_SYS_Exit();

    return 0;

usage:
    printf(
        "usage: %s src.png dst.png [max_width:def=250] [max_height:def=360]\n",
        argv[0]);
error:
    if (srcVa)
        MI_SYS_Munmap(srcVa, ss);
    if (dstVa)
        MI_SYS_Munmap(dstVa, ds);
    if (srcPa)
        MI_SYS_MMA_Free(srcPa);
    if (dstPa)
        MI_SYS_MMA_Free(dstPa);
    return 1;
}
