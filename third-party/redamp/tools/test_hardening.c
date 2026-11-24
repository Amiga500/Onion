// Run with: ./tools/test_hardening.sh
// Exercises the old ASan reproducers for ZIP stored-size and BMP-RLE palette.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// Pull static bmp_rle() into this translation unit; discard unused UI code.
#include "../src/skin.c"

static void wr16(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void wr32(unsigned char *p, uint32_t v) {
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static int test_rle(void)
{
    unsigned char b[80] = { 'B','M' };
    wr32(b + 10, 62); wr32(b + 14, 40);
    wr32(b + 18, 1); wr32(b + 22, 1);
    wr16(b + 26, 1); wr16(b + 28, 8);
    wr32(b + 30, 1); wr32(b + 46, UINT32_MAX); // used to overflow palette size
    int w = 0, h = 0;
    unsigned char *rgba = bmp_rle(b, sizeof b, &w, &h);
    if (rgba) { free(rgba); fprintf(stderr, "bad palette accepted\n"); return 0; }
    // Palette that extends beyond the pixel offset should also be rejected.
    wr32(b + 46, 4);
    rgba = bmp_rle(b, sizeof b, &w, &h);
    if (rgba) { free(rgba); fprintf(stderr, "palette crosses pixel offset\n"); return 0; }
    // Valid tiny RLE8 image with one color in the palette.
    wr32(b + 46, 1); b[54] = 31; b[55] = 22; b[56] = 11;
    b[62] = 1; b[63] = 0; b[64] = 0; b[65] = 1;
    rgba = bmp_rle(b, sizeof b, &w, &h);
    if (!rgba || w != 1 || h != 1 || rgba[0] != 11 || rgba[1] != 22 || rgba[2] != 31) {
        free(rgba); fprintf(stderr, "valid RLE8 rejected\n"); return 0;
    }
    free(rgba);
    return 1;
}

static int test_zip(const char *path, int expect_success)
{
    zip_archive *z = zip_open_file(path);
    if (!z) { fprintf(stderr, "zip_open failed: %s\n", path); return 0; }
    const zip_entry *e = zip_find(z, "main.bmp");
    size_t n = 0;
    unsigned char *v = zip_extract(z, e, &n);
    int ok = expect_success ? v && n == 4 : !v;
    if (!ok) fprintf(stderr, "ZIP extraction expectation failed: %s\n", path);
    free(v); zip_close(z);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "expected: stored.zip bad-store.zip bad-deflate.zip\n"); return 2; }
    int ok = test_rle();
    ok &= test_zip(argv[1], 1);
    ok &= test_zip(argv[2], 0);
    ok &= test_zip(argv[3], 0);
    puts(ok ? "skin parser regressions: PASS" : "skin parser regressions: FAIL");
    return ok ? 0 : 1;
}
