/*
 * pngresize.c
 *
 * Tiny PNG resizer using stb.
 *
 * Usage:
 *   pngresize [options] input.png output.png WIDTH HEIGHT
 *
 * Modes:
 *   default
 *       Preserve aspect ratio, fit inside WIDTH x HEIGHT,
 *       and never upscale.
 *
 *   --stretch
 *       Ignore aspect ratio and resize to exactly WIDTH x HEIGHT.
 *       Upscaling is allowed.
 *
 *   --stretch-no-upscale
 *       Ignore aspect ratio, but do not allow either output
 *       dimension to exceed the corresponding source dimension.
 *
 * Other options:
 *   --nearest
 *       Use nearest-neighbor scaling.
 *
 *   --overwrite
 *       Allow replacing an existing output file.
 *
 *   --quiet
 *       Suppress the success/status line.
 *
 *   --
 *       Stop parsing options. Useful if a filename begins with '-'.
 *
 * Required headers:
 *   stb_image.h
 *   stb_image_resize2.h
 *   stb_image_write.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


enum resize_mode {
    MODE_FIT = 0,
    MODE_STRETCH,
    MODE_STRETCH_NO_UPSCALE
};


static void usage(const char *program)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s [options] input.png output.png WIDTH HEIGHT\n"
        "\n"
        "Options:\n"
        "  --stretch             Force exact WIDTH x HEIGHT\n"
        "  --stretch-no-upscale  Stretch, but never enlarge either axis\n"
        "  --nearest             Use nearest-neighbor scaling\n"
        "  --overwrite           Replace existing output file\n"
        "  --quiet               Suppress success output\n"
        "  --help                 Show this help\n"
        "  --                     Stop option parsing\n",
        program);
}


static int parse_positive_int(const char *s, int *value)
{
    char *end;
    long v;

    if (!s || !*s)
        return 0;

    errno = 0;
    end = NULL;

    v = strtol(s, &end, 10);

    if (errno == ERANGE)
        return 0;

    if (end == s || *end != '\0')
        return 0;

    if (v <= 0)
        return 0;

    /*
     * On Miyoo's 32-bit armhf ABI, long and int are both 32-bit,
     * so strtol()'s ERANGE check is the meaningful overflow guard.
     *
     * Keep this test for portability to platforms where long is
     * wider than int.
     */
#if LONG_MAX > INT_MAX
    if (v > INT_MAX)
        return 0;
#endif

    *value = (int)v;
    return 1;
}


static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;

    fclose(f);
    return 1;
}


/*
 * RGBA nearest-neighbor scaler using destination-pixel center sampling.
 *
 * Mapping destination pixel centers rather than their top-left edges avoids
 * a systematic half-pixel bias toward the source image's upper-left corner.
 */
static void resize_nearest_rgba(
    const unsigned char *src,
    int src_w,
    int src_h,
    unsigned char *dst,
    int dst_w,
    int dst_h)
{
    int y;

    for (y = 0; y < dst_h; y++) {
        int sy;
        int x;

        /*
         * Equivalent to:
         *
         *   floor(((y + 0.5) * src_h) / dst_h)
         *
         * but using integer arithmetic.
         *
         * int64_t prevents overflow for large dimensions.
         */
        sy = (int)(
            (((int64_t)2 * y + 1) * src_h) /
            ((int64_t)2 * dst_h)
        );

        if (sy >= src_h)
            sy = src_h - 1;

        for (x = 0; x < dst_w; x++) {
            int sx;
            const unsigned char *s;
            unsigned char *d;

            sx = (int)(
                (((int64_t)2 * x + 1) * src_w) /
                ((int64_t)2 * dst_w)
            );

            if (sx >= src_w)
                sx = src_w - 1;

            s = src + ((size_t)sy * (size_t)src_w + (size_t)sx) * 4;
            d = dst + ((size_t)y  * (size_t)dst_w + (size_t)x)  * 4;

            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = s[3];
        }
    }
}


static int set_resize_mode(
    enum resize_mode *current,
    enum resize_mode requested)
{
    /*
     * Repeating the same mode is harmless:
     *
     *   --stretch --stretch
     *
     * Conflicting modes are still rejected:
     *
     *   --stretch --stretch-no-upscale
     */
    if (*current == requested)
        return 1;

    if (*current != MODE_FIT)
        return 0;

    *current = requested;
    return 1;
}


int main(int argc, char **argv)
{
    const char *input_path;
    const char *output_path;

    enum resize_mode mode = MODE_FIT;

    int nearest = 0;
    int overwrite = 0;
    int quiet = 0;

    int src_w;
    int src_h;

    int target_w;
    int target_h;

    int dst_w;
    int dst_h;

    int arg = 1;
    int parse_options = 1;

    unsigned char *src = NULL;
    unsigned char *dst = NULL;

    size_t dst_size;

    /*
     * Parse options.
     */
    while (arg < argc && parse_options) {

        if (strcmp(argv[arg], "--") == 0) {
            parse_options = 0;
            arg++;
            break;
        }

        if (argv[arg][0] != '-')
            break;

        if (strcmp(argv[arg], "--stretch") == 0) {

            if (!set_resize_mode(&mode, MODE_STRETCH)) {
                fprintf(stderr,
                        "Conflicting resize modes specified.\n");
                return 1;
            }
        }

        else if (strcmp(argv[arg], "--stretch-no-upscale") == 0) {

            if (!set_resize_mode(&mode, MODE_STRETCH_NO_UPSCALE)) {
                fprintf(stderr,
                        "Conflicting resize modes specified.\n");
                return 1;
            }
        }

        else if (strcmp(argv[arg], "--nearest") == 0) {
            nearest = 1;
        }

        else if (strcmp(argv[arg], "--overwrite") == 0) {
            overwrite = 1;
        }

        else if (strcmp(argv[arg], "--quiet") == 0) {
            quiet = 1;
        }

        else if (strcmp(argv[arg], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }

        else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg]);
            usage(argv[0]);
            return 1;
        }

        arg++;
    }

    if (argc - arg != 4) {
        usage(argv[0]);
        return 1;
    }

    input_path = argv[arg];
    output_path = argv[arg + 1];

    if (!parse_positive_int(argv[arg + 2], &target_w) ||
        !parse_positive_int(argv[arg + 3], &target_h)) {

        fprintf(stderr, "Invalid width or height.\n");
        return 1;
    }

    /*
     * This version intentionally does not perform in-place replacement.
     */
    if (strcmp(input_path, output_path) == 0) {
        fprintf(stderr,
                "Input and output paths must be different.\n");
        return 1;
    }

    /*
     * Refuse accidental overwrites unless explicitly requested.
     */
    if (!overwrite && file_exists(output_path)) {
        fprintf(stderr,
                "Output file already exists: %s\n"
                "Use --overwrite to replace it.\n",
                output_path);
        return 1;
    }

    /*
     * Always decode to RGBA.
     *
     * This provides predictable alpha behavior for RGB, RGBA,
     * palette-based PNGs, and PNGs with transparency chunks.
     */
    src = stbi_load(
        input_path,
        &src_w,
        &src_h,
        NULL,
        4
    );

    if (!src) {
        const char *reason = stbi_failure_reason();

        fprintf(stderr,
                "Failed to load '%s': %s\n",
                input_path,
                reason ? reason : "unknown error");

        return 1;
    }

    /*
     * Calculate destination dimensions.
     */
    if (mode == MODE_STRETCH) {

        dst_w = target_w;
        dst_h = target_h;
    }

    else if (mode == MODE_STRETCH_NO_UPSCALE) {

        dst_w = target_w;
        dst_h = target_h;

        if (dst_w > src_w)
            dst_w = src_w;

        if (dst_h > src_h)
            dst_h = src_h;
    }

    else {

        /*
         * Default:
         *
         * Preserve aspect ratio.
         * Fit inside target dimensions.
         * Never upscale.
         */
        dst_w = src_w;
        dst_h = src_h;

        if (dst_w > target_w) {

            dst_h = (int)(
                ((int64_t)dst_h * target_w + dst_w / 2) /
                dst_w
            );

            dst_w = target_w;
        }

        if (dst_h > target_h) {

            dst_w = (int)(
                ((int64_t)dst_w * target_h + dst_h / 2) /
                dst_h
            );

            dst_h = target_h;
        }
    }

    if (dst_w < 1)
        dst_w = 1;

    if (dst_h < 1)
        dst_h = 1;

    /*
     * Guard the RGBA allocation arithmetic.
     */
    if ((size_t)dst_w > SIZE_MAX / 4 ||
        (size_t)dst_h > SIZE_MAX / ((size_t)dst_w * 4)) {

        fprintf(stderr, "Output image is too large.\n");
        stbi_image_free(src);
        return 1;
    }

    dst_size =
        (size_t)dst_w *
        (size_t)dst_h *
        4;

    dst = malloc(dst_size);

    if (!dst) {
        fprintf(stderr, "Out of memory.\n");
        stbi_image_free(src);
        return 1;
    }

    /*
     * Resize.
     */
    if (nearest) {

        resize_nearest_rgba(
            src,
            src_w,
            src_h,
            dst,
            dst_w,
            dst_h
        );
    }

    else {

        /*
         * Alpha-aware sRGB resizing.
         */
        if (!stbir_resize_uint8_srgb(
                src,
                src_w,
                src_h,
                0,
                dst,
                dst_w,
                dst_h,
                0,
                STBIR_RGBA)) {

            fprintf(stderr, "Resize failed.\n");

            free(dst);
            stbi_image_free(src);
            return 1;
        }
    }

    /*
     * Write RGBA PNG.
     */
    if (!stbi_write_png(
            output_path,
            dst_w,
            dst_h,
            4,
            dst,
            dst_w * 4)) {

        fprintf(stderr,
                "Failed to write '%s'.\n",
                output_path);

        free(dst);
        stbi_image_free(src);
        return 1;
    }

    if (!quiet) {
        const char *mode_name;

        if (mode == MODE_STRETCH)
            mode_name = "stretch";
        else if (mode == MODE_STRETCH_NO_UPSCALE)
            mode_name = "stretch-no-upscale";
        else
            mode_name = "fit";

        printf(
            "%s: %dx%d -> %dx%d (%s%s)\n",
            input_path,
            src_w,
            src_h,
            dst_w,
            dst_h,
            mode_name,
            nearest ? ", nearest" : ""
        );
    }

    free(dst);
    stbi_image_free(src);

    return 0;
}