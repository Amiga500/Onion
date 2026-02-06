#include <mi_gfx.h>
#include <mi_sys.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#define ALIGN4K(val) ((val + 4095) & (~4095))
#define ERROR(str)                 \
    {                              \
        fprintf(stderr, str "\n"); \
        goto error;                \
    }

//
//	Swap R and B channels: ABGR8888 <-> ARGB8888 (4 bytes per pixel)
//	Processes pixels in bulk using NEON SIMD when available.
//
static void swap_rb_channels(const uint32_t *src, uint32_t *dst, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    // Process 4 pixels (16 bytes) at a time with NEON
    // Swap byte 0 (R) and byte 2 (B) within each 32-bit pixel
    static const uint8_t swap_tbl[8] = {2,1,0,3, 6,5,4,7};
    uint8x8_t idx = vld1_u8(swap_tbl);
    for (; i + 4 <= count; i += 4) {
        uint8x16_t pixels = vld1q_u8((const uint8_t *)(src + i));
        uint8x8_t lo = vget_low_u8(pixels);
        uint8x8_t hi = vget_high_u8(pixels);
        uint8x8_t swapped_lo = vtbl1_u8(lo, idx);
        uint8x8_t swapped_hi = vtbl1_u8(hi, idx);
        vst1_u8((uint8_t *)(dst + i), swapped_lo);
        vst1_u8((uint8_t *)(dst + i + 2), swapped_hi);
    }
    // Handle remaining pixels
    for (; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00) | ((pix & 0x00FF0000) >> 16) |
                 ((pix & 0x000000FF) << 16);
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00) | ((pix & 0x00FF0000) >> 16) |
                 ((pix & 0x000000FF) << 16);
    }
#endif
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
    uint8_t *src8;
    uint32_t *src, *dst, pix, x, y, sw, sh, dw, dh, ss = 0, ds = 0, mw = 250,
                                                    mh = 360;

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
            src8 = rows[y];
            for (x = 0; x < sw; x++, src8++) {
                *dst++ =
                    0xFF000000 | (src8[0] << 16) | (src8[0] << 8) | src8[0];
            }
        }
        break;
    case 2:
        for (y = 0; y < sh; y++) {
            src8 = rows[y];
            for (x = 0; x < sw; x++, src8 += 2) {
                *dst++ = (src8[1] << 24) | (src8[0] << 16) | (src8[0] << 8) |
                         src8[0];
            }
        }
        break;
    case 3:
        for (y = 0; y < sh; y++) {
            src8 = rows[y];
            for (x = 0; x < sw; x++, src8 += 3) {
                *dst++ = 0xFF000000 | (src8[0] << 16) | (src8[1] << 8) | src8[2];
            }
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
    tmp = malloc(dw * 4);
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
