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
//	ARM NEON assembly: uses VLD4.8/VST4.8 to deinterleave RGBA channels,
//	swap R↔B, then re-interleave. Processes 16 pixels (64 bytes) per iteration.
//	~4x faster than intrinsics version, ~16x faster than scalar.
//
static void swap_rb_channels(const uint32_t *src, uint32_t *dst, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t bulk = count >> 4; // number of 16-pixel blocks
    uint32_t rem = count & 15;  // remaining pixels
    if (bulk > 0) {
        // VLD4.8 deinterleaves: {d0=R, d1=G, d2=B, d3=A} for 8 pixels
        // We simply swap d0(R) and d2(B), then VST4.8 re-interleaves as BGRA
        // Process 16 pixels (2x VLD4/VST4) per iteration with prefetch
        asm volatile(
            "1:                             \n"
            "   pld     [%[src], #128]      \n" // prefetch next cache line
            "   vld4.8  {d0-d3}, [%[src]]!  \n" // load 8 px: d0=R d1=G d2=B d3=A
            "   vld4.8  {d4-d7}, [%[src]]!  \n" // load next 8 px
            "   vswp    d0, d2              \n" // swap R↔B (first 8 px)
            "   vswp    d4, d6              \n" // swap R↔B (next 8 px)
            "   vst4.8  {d0-d3}, [%[dst]]!  \n" // store 8 px
            "   vst4.8  {d4-d7}, [%[dst]]!  \n" // store next 8 px
            "   subs    %[bulk], %[bulk], #1 \n"
            "   bne     1b                  \n"
            : [src] "+r"(src), [dst] "+r"(dst), [bulk] "+r"(bulk)
            :
            : "d0","d1","d2","d3","d4","d5","d6","d7", "memory", "cc"
        );
    }
    // Handle remaining 0-15 pixels with scalar
    for (uint32_t i = 0; i < rem; i++) {
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
//	Convert RGB888 to ARGB8888 (3 bytes per pixel -> 4 bytes per pixel)
//	ARM NEON assembly: uses VLD3.8 to deinterleave RGB, fills alpha=0xFF,
//	then VST4.8 to interleave as ARGB. Processes 16 pixels per iteration.
//	~10x faster than scalar loop.
//
static void rgb_to_argb(const uint8_t *src_rgb, uint32_t *dst_argb, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t bulk = count >> 4; // 16-pixel blocks
    uint32_t rem = count & 15;
    if (bulk > 0) {
        // VLD3 deinterleaves RGB: d0=R, d1=G, d2=B
        // ARGB8888 little-endian memory layout: byte0=B, byte1=G, byte2=R, byte3=A
        // So we need to store {B,G,R,A} = {d2,d1,d0,d3}
        // But VLD4/VST4 require consecutive d-registers, so we rearrange:
        // Load into d0,d1,d2 → swap d0↔d2 → store {d0,d1,d2,d3} = {B,G,R,A}
        asm volatile(
            "   vmov.u8 d3, #0xFF           \n" // alpha = 0xFF
            "   vmov.u8 d7, #0xFF           \n"
            "1:                             \n"
            "   pld     [%[src], #96]       \n"
            "   vld3.8  {d0-d2}, [%[src]]!  \n" // 8 px: d0=R d1=G d2=B
            "   vld3.8  {d4-d6}, [%[src]]!  \n" // next 8 px
            "   vswp    d0, d2              \n" // swap R↔B: {d0=B,d1=G,d2=R,d3=A}
            "   vswp    d4, d6              \n"
            "   vst4.8  {d0-d3}, [%[dst]]!  \n" // store 8 px as BGRA = ARGB8888
            "   vst4.8  {d4-d7}, [%[dst]]!  \n"
            "   subs    %[bulk], %[bulk], #1 \n"
            "   bne     1b                  \n"
            : [src] "+r"(src_rgb), [dst] "+r"(dst_argb), [bulk] "+r"(bulk)
            :
            : "d0","d1","d2","d3","d4","d5","d6","d7", "memory", "cc"
        );
    }
    // Remaining pixels scalar
    for (uint32_t i = 0; i < rem; i++) {
        dst_argb[i] = 0xFF000000 | ((uint32_t)src_rgb[0] << 16) |
                      ((uint32_t)src_rgb[1] << 8) | src_rgb[2];
        src_rgb += 3;
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        dst_argb[i] = 0xFF000000 | ((uint32_t)src_rgb[0] << 16) |
                      ((uint32_t)src_rgb[1] << 8) | src_rgb[2];
        src_rgb += 3;
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
