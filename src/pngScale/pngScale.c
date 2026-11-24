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
//
static inline void swap_rb_channels(const uint32_t *src, uint32_t *dst, uint32_t count)
{
    if (src == dst) {
        neon_swap_rb_inplace(dst, (int)count);
    }
    else {
        neon_argb_to_rgba(dst, src, (int)count);
    }
}

static inline void rgb_to_argb(const uint8_t *src_rgb, uint32_t *dst_argb, uint32_t count)
{
    neon_rgb888_to_argb(dst_argb, src_rgb, (int)count);
}

static inline void gray8_to_argb(const uint8_t *src_gray, uint32_t *dst_argb, uint32_t count)
{
    neon_gray8_to_argb(dst_argb, src_gray, (int)count);
}

static inline void gray8a_to_argb(const uint8_t *src_ga, uint32_t *dst_argb, uint32_t count)
{
    neon_gray8a_to_argb(dst_argb, src_ga, (int)count);
}

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
//	Video memory (MMA) for the GFX blit, checked: with none left
//	MI_SYS_MMA_Alloc fails and leaves the address at 0, and writing the
//	picture there and blitting from it could hang the console (Mini Flip,
//	scraper). Returns 0 when there is none: the caller scales in software.
//
static int mma_get(uint32_t size, MI_PHY *pa, void **va)
{
    *pa = 0;
    *va = NULL;
    if (MI_SYS_MMA_Alloc(NULL, size, pa) != 0 || !*pa) {
        *pa = 0;
        return 0;
    }
    if (MI_SYS_Mmap(*pa, size, va, TRUE) != 0 || !*va) {
        MI_SYS_MMA_Free(*pa);
        *pa = 0;
        *va = NULL;
        return 0;
    }
    return 1;
}

//
//	Software scale, ARGB8888: each destination pixel is the average of the
//	source pixels it covers (at least one, so enlarging repeats pixels).
//
static void soft_scale(const uint32_t *src, uint32_t sw, uint32_t sh,
                       uint32_t *dst, uint32_t dw, uint32_t dh)
{
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t y0 = (uint32_t)((uint64_t)dy * sh / dh);
        uint32_t y1 = (uint32_t)((uint64_t)(dy + 1) * sh / dh);
        if (y1 <= y0)
            y1 = y0 + 1;
        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t x0 = (uint32_t)((uint64_t)dx * sw / dw);
            uint32_t x1 = (uint32_t)((uint64_t)(dx + 1) * sw / dw);
            if (x1 <= x0)
                x1 = x0 + 1;
            uint64_t a = 0, r = 0, g = 0, b = 0, n = 0;
            for (uint32_t y = y0; y < y1; y++) {
                const uint32_t *row = src + (size_t)y * sw;
                for (uint32_t x = x0; x < x1; x++) {
                    uint32_t p = row[x];
                    a += p >> 24;
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                    n++;
                }
            }
            dst[(size_t)dy * dw + dx] =
                (uint32_t)((a + n / 2) / n) << 24 | (uint32_t)((r + n / 2) / n) << 16 |
                (uint32_t)((g + n / 2) / n) << 8 | (uint32_t)((b + n / 2) / n);
        }
    }
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
    uint32_t *src, *dst, *srcBuf = NULL, *dstBuf = NULL;
    // PNGSCALE_SOFTWARE=1: no GFX at all (the scraper's big box art).
    const char *soft_env = getenv("PNGSCALE_SOFTWARE");
    int soft = soft_env && strcmp(soft_env, "1") == 0, mi = 0;
    uint32_t y, sw, sh, dw, dh;
    uint32_t ss = 0, ds = 0, mw = 250, mh = 360;

    // Read commandline and open src
    if (argc < 3)
        goto usage;
    if (argc > 3)
        mw = atoi(argv[3]);
    if (argc > 4)
        mh = atoi(argv[4]);
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
    ss = ALIGN4K(sw * sh * 4);
    if ((!sw) || (!sh) || (!ss) || (!ch))
        ERROR("png format error");

    // Source in video memory for the GFX blit, else in normal memory
    if (!soft) {
        MI_SYS_Init();
        MI_GFX_Open();
        mi = 1;
        if (mma_get(ss, &srcPa, &srcVa))
            srcBuf = srcVa;
        else
            fprintf(stderr, "pngScale: no video memory for %ux%u, scaling in software\n", sw, sh);
    }
    if (!srcBuf) {
        srcBuf = malloc((size_t)sw * sh * 4);
        if (!srcBuf)
            ERROR("out of memory");
    }

    // Read png
    rows = png_get_rows(png_ptr, info_ptr);

    // Convert png to src ARGB8888
    dst = srcBuf;
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

    // Calculate dst size: fit within mw x mh keeping the aspect ratio, or
    // exactly mw x mh with a fifth argument "stretch".
    if (argc > 5 && strcmp(argv[5], "stretch") == 0) {
        dw = mw;
        dh = mh;
    }
    else {
        dw = mw;
        dh = sh * dw / sw;
        if (dh > mh) {
            dh = mh;
            dw = sw * dh / sh;
        }
    }
    if (!dw || !dh)
        ERROR("invalid size");
    ds = ALIGN4K(dw * dh * 4);

    // Allocate dst png mem and scale: GFX only with both in video memory
    if (srcVa && mma_get(ds, &dstPa, &dstVa)) {
        dstBuf = dstVa;
        GFX_BlitSurface(srcPa, srcVa, sw, sh, dstPa, dstVa, dw, dh);
    }
    else {
        if (srcVa)
            fprintf(stderr, "pngScale: no video memory for %ux%u, scaling in software\n", dw, dh);
        dstBuf = malloc((size_t)dw * dh * 4);
        if (!dstBuf)
            ERROR("out of memory");
        soft_scale(srcBuf, sw, sh, dstBuf, dw, dh);
    }
    if (srcVa) {
        MI_SYS_Munmap(srcVa, ss);
        MI_SYS_MMA_Free(srcPa);
        srcVa = NULL;
        srcPa = 0;
    }
    else
        free(srcBuf);
    srcBuf = NULL;
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
    src = dstBuf;
    tmp = malloc(dw * 4);
    if (!tmp)
        ERROR("out of memory");
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
    if (dstVa) {
        MI_SYS_Munmap(dstVa, ds);
        MI_SYS_MMA_Free(dstPa);
    }
    else
        free(dstBuf);
    if (mi) {
        MI_GFX_Close();
        MI_SYS_Exit();
    }

    return 0;

usage:
    printf(
        "usage: %s src.png dst.png [max_width:def=250] [max_height:def=360] [stretch]\n",
        argv[0]);
error:
    if (srcVa)
        MI_SYS_Munmap(srcVa, ss);
    else
        free(srcBuf);
    if (dstVa)
        MI_SYS_Munmap(dstVa, ds);
    else
        free(dstBuf);
    if (srcPa)
        MI_SYS_MMA_Free(srcPa);
    if (dstPa)
        MI_SYS_MMA_Free(dstPa);
    if (mi) {
        MI_GFX_Close();
        MI_SYS_Exit();
    }
    return 1;
}
