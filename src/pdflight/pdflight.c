/* pdflight: a light copy of a heavy PDF (a scanned manual) for Green.
 *
 * Green (poppler) decodes a page's whole scan every time it shows the page,
 * even on a 640x480 screen: a 2000x2700 scan per page makes every page
 * turn slow. pdflight renders each page once, at <width> pixels, and
 * writes a new PDF of the same page sizes in which every page is that
 * picture as a JPEG (cairo stores the JPEG as it is). Green then decodes a
 * small JPEG per page. Text is no longer searchable in the copy, so only
 * heavy PDFs are worth it (--auto).
 *
 *   pdflight [--auto] <in.pdf> <out.pdf> [width=800] [quality=80]
 *
 * Prints "pages <pages>" once it starts, then "page <n>/<pages>" after
 * each page. 800 pixels: sharp at the 640-pixel fit width, with some room
 * to zoom, and almost twice as fast to show as 960 pixels.
 * Exit 0: out.pdf written. 1: error. 3 (--auto): not worth it, because the
 * PDF has under 120 KB per page (text) or the copy would not be at least
 * 30% smaller. out.pdf exists only with exit 0. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libs.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../pngresize/stb_image_write.h"

#define EXIT_NOT_WORTH 3
#define HEAVY_BYTES_PER_PAGE (120 * 1024)

typedef struct {
    unsigned char *data;
    size_t length, capacity;
    int failed;
} Buffer;

static void buffer_write(void *context, void *data, int size)
{
    Buffer *buffer = context;
    if (buffer->failed || size <= 0)
        return;
    if (buffer->length + size > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 256 * 1024;
        while (capacity < buffer->length + size)
            capacity *= 2;
        unsigned char *grown = realloc(buffer->data, capacity);
        if (!grown) {
            buffer->failed = 1;
            return;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, size);
    buffer->length += size;
}

static long long file_size(const char *path)
{
    struct stat info;
    return stat(path, &info) ? -1 : (long long)info.st_size;
}

/* One page as a JPEG of <target> pixels wide, drawn on the PDF's page. */
static int light_page(PopplerPage *page, cairo_surface_t *pdf, cairo_t *out, int target, int quality)
{
    double pw = 0, ph = 0;
    poppler_page_get_size(page, &pw, &ph);
    if (pw < 1 || ph < 1)
        return 0;
    double scale = target / pw;
    int w = (int)(pw * scale + 0.5), h = (int)(ph * scale + 0.5);
    if (w < 1 || h < 1 || h > 8 * target)
        return 0;

    cairo_surface_t *picture = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    if (cairo_surface_status(picture) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(picture);
        return 0;
    }
    cairo_t *c = cairo_create(picture);
    cairo_set_source_rgb(c, 1, 1, 1);
    cairo_paint(c);
    cairo_scale(c, scale, scale);
    poppler_page_render(page, c);
    cairo_destroy(c);
    cairo_surface_flush(picture);

    /* RGB24 is one native-endian 0x00RRGGBB word per pixel. */
    unsigned char *rgb = malloc((size_t)w * h * 3);
    Buffer jpeg = {0};
    int ok = rgb != NULL;
    if (ok) {
        const unsigned char *data = cairo_image_surface_get_data(picture);
        int stride = cairo_image_surface_get_stride(picture);
        for (int y = 0; y < h; y++) {
            const unsigned int *row = (const unsigned int *)(data + (size_t)y * stride);
            unsigned char *d = rgb + (size_t)y * w * 3;
            for (int x = 0; x < w; x++, d += 3) {
                d[0] = row[x] >> 16;
                d[1] = row[x] >> 8;
                d[2] = row[x];
            }
        }
        ok = stbi_write_jpg_to_func(buffer_write, &jpeg, w, h, 3, rgb, quality) && !jpeg.failed;
        free(rgb);
    }
    /* cairo writes the JPEG itself into the PDF and frees it after. */
    if (ok && cairo_surface_set_mime_data(picture, CAIRO_MIME_TYPE_JPEG, jpeg.data, jpeg.length, free,
                                          jpeg.data) == CAIRO_STATUS_SUCCESS) {
        cairo_pdf_surface_set_size(pdf, pw, ph);
        cairo_save(out);
        cairo_scale(out, pw / w, ph / h);
        cairo_set_source_surface(out, picture, 0, 0);
        cairo_paint(out);
        cairo_restore(out);
        cairo_show_page(out);
        ok = cairo_status(out) == CAIRO_STATUS_SUCCESS;
    }
    else {
        free(jpeg.data);
        ok = 0;
    }
    cairo_surface_destroy(picture);
    return ok;
}

int main(int argc, char **argv)
{
    int automatic = argc > 1 && strcmp(argv[1], "--auto") == 0;
    if (automatic) {
        argc--;
        argv++;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: pdflight [--auto] <in.pdf> <out.pdf> [width=800] [quality=80]\n");
        return 1;
    }
    const char *input = argv[1], *output = argv[2];
    int target = argc > 3 ? atoi(argv[3]) : 800;
    int quality = argc > 4 ? atoi(argv[4]) : 80;
    if (target < 200 || target > 4000 || quality < 30 || quality > 100) {
        fprintf(stderr, "pdflight: width 200-4000, quality 30-100\n");
        return 1;
    }

    char path[4096];
    if (!realpath(input, path)) {
        perror(input);
        return 1;
    }
    GError *error = NULL;
    char *uri = g_filename_to_uri(path, NULL, &error);
    PopplerDocument *document = uri ? poppler_document_new_from_file(uri, NULL, &error) : NULL;
    g_free(uri);
    if (!document) {
        fprintf(stderr, "pdflight: %s: %s\n", input,
                error && error->message ? error->message : "cannot open");
        return 1;
    }
    int pages = poppler_document_get_n_pages(document);
    long long size = file_size(path);
    if (pages < 1 || size < 0) {
        fprintf(stderr, "pdflight: %s: no pages\n", input);
        g_object_unref(document);
        return 1;
    }
    if (automatic && size / pages < HEAVY_BYTES_PER_PAGE) {
        g_object_unref(document);
        return EXIT_NOT_WORTH;
    }

    printf("pages %d\n", pages);
    fflush(stdout);

    cairo_surface_t *pdf = cairo_pdf_surface_create(output, 612, 792);
    cairo_t *out = cairo_create(pdf);
    int ok = cairo_status(out) == CAIRO_STATUS_SUCCESS;
    for (int i = 0; i < pages && ok; i++) {
        PopplerPage *page = poppler_document_get_page(document, i);
        ok = page && light_page(page, pdf, out, target, quality);
        if (page)
            g_object_unref(page);
        printf("page %d/%d\n", i + 1, pages);
        fflush(stdout);
    }
    cairo_destroy(out);
    cairo_surface_finish(pdf);
    ok = ok && cairo_surface_status(pdf) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(pdf);
    g_object_unref(document);

    if (!ok) {
        unlink(output);
        fprintf(stderr, "pdflight: %s: failed\n", input);
        return 1;
    }
    long long light = file_size(output);
    if (automatic && (light < 0 || light * 10 > size * 7)) {
        unlink(output);
        return EXIT_NOT_WORTH;
    }
    fprintf(stderr, "pdflight: %d pages, %lld KB -> %lld KB\n", pages, size / 1024, light / 1024);
    return 0;
}
