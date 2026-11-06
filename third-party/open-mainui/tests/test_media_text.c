/* SPDX-License-Identifier: GPL-3.0-only */
/* SDL-free regression fixture for exact TXT discovery and lazy line wrapping. */
#include "ui/media_text.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char *path, const char *data)
{
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data, 1, strlen(data), file) == strlen(data));
    assert(fclose(file) == 0);
}

static int measure(const char *text, void *unused)
{
    (void)unused;
    int count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if ((*p & 0xc0) != 0x80) {
            count++;
        }
    }
    return count * 8;
}

int main(void)
{
    char root[] = "/tmp/mainui-manuals-XXXXXX";
    assert(mkdtemp(root));
    char path[4096], exact[4096], line[MAINUI_TEXT_LINE_BYTES];
    snprintf(path, sizeof path, "%s/media", root);
    assert(!mkdir(path, 0700));
    snprintf(path, sizeof path, "%s/media/manuals", root);
    assert(!mkdir(path, 0700));
    snprintf(exact, sizeof exact, "%s/media/manuals/Test Game.txt", root);
    write_file(exact, "\xef\xbb\xbf"
                      "One two three four\r\n\r\nCaf\xc3\xa9"
                      " home\n"
                      "TheLastVeryLongUnbrokenWord\n"
                      "Broken \xff UTF8\n");
    char rom[4096];
    snprintf(rom, sizeof rom, "%s/Sub/Test Game.sfc", root);
    assert(mainui_text_find(root, rom, path) && !strcmp(path, exact));
    assert(mainui_text_find(root, "Test Game.sfc", path));
    assert(!mainui_text_find(root, "Another Game.sfc", path));
    assert(!mainui_text_find(root, "", path));
    assert(!mainui_text_find(root, rom, NULL));
    snprintf(path, sizeof path, "%s/Test Game.txt", root);
    write_file(path, "Wrong place");
    assert(mainui_text_find(root, rom, path) && !strcmp(path, exact));
    char pdf[4096];
    snprintf(pdf, sizeof pdf, "%s/media/manuals/Test Game.pdf", root);
    write_file(pdf, "pdf fixture");
    assert(mainui_manual_find(root, rom, ".pdf", path) && !strcmp(path, pdf));
    assert(!mainui_manual_find(root, rom, ".PDF", path));
    assert(!mainui_manual_find(root, rom, "../evil", path));
    assert(!mainui_manual_find(root, "Another Game.sfc", ".pdf", path));
    MainUITextReader reader = {0};
    assert(mainui_text_open(&reader, exact));
    assert(reader.count == 0 && !reader.complete);
    mainui_text_layout(&reader, 64, measure, NULL, 3);
    assert(reader.count == 3);
    mainui_text_line(&reader, 0, line);
    assert(!strcmp(line, "One two"));
    mainui_text_line(&reader, 1, line);
    assert(!strcmp(line, "three"));
    mainui_text_line(&reader, 2, line);
    assert(!strcmp(line, "four"));
    /* A phrase may span a skipped wrap separator. Highlight both pieces
     * using source-byte ranges instead of searching each visual line. */
    MainUITextLine spans[4];
    assert(mainui_text_match_segments(&reader, 0, "TWO THREE", spans, 4) == 1);
    assert(spans[0].end - spans[0].start == 3);
    assert(!memcmp(reader.data + spans[0].start, "two", 3));
    assert(mainui_text_match_segments(&reader, 1, "two three", spans, 4) == 1);
    assert(spans[0].end - spans[0].start == 5);
    assert(!memcmp(reader.data + spans[0].start, "three", 5));
    assert(mainui_text_match_segments(&reader, 2, "two three", spans, 4) == 0);
    mainui_text_layout(&reader, 64, measure, NULL, 100);
    assert(reader.complete && reader.count >= 9);
    bool saw_utf8 = false, saw_invalid = false, saw_empty = false;
    for (int i = 0; i < reader.count; i++) {
        mainui_text_line(&reader, i, line);
        if (strstr(line, "Caf\xc3\xa9")) {
            saw_utf8 = true;
        }
        if (strstr(line, "?")) {
            saw_invalid = true;
        }
        if (!*line) {
            saw_empty = true;
        }
    }
    assert(saw_utf8 && saw_invalid && saw_empty);
    mainui_text_layout(&reader, 32, measure, NULL, 100);
    assert(reader.count > 9 && reader.complete);
    mainui_text_close(&reader);
    assert(!reader.data && !reader.lines);

    /* Invalid UTF-8 at the 2 MiB limit and many lines cannot overrun buffers. */
    FILE *file = fopen(exact, "wb");
    assert(file);
    for (size_t i = 0; i < MAINUI_TEXT_FILE_LIMIT + 5; i++) {
        assert(fputc(i % 2 ? '\n' : 'A', file) != EOF);
    }
    assert(!fclose(file));
    assert(mainui_text_open(&reader, exact) && reader.truncated);
    mainui_text_layout(&reader, 32, measure, NULL, MAINUI_TEXT_MAX_LINES + 100);
    assert(reader.count == MAINUI_TEXT_MAX_LINES && reader.complete && reader.truncated);
    mainui_text_close(&reader);
    assert(unlink(pdf) == 0);
    assert(unlink(exact) == 0);
    snprintf(path, sizeof path, "%s/Test Game.txt", root);
    assert(unlink(path) == 0);
    snprintf(path, sizeof path, "%s/media/manuals", root);
    assert(rmdir(path) == 0);
    snprintf(path, sizeof path, "%s/media", root);
    assert(rmdir(path) == 0);
    assert(rmdir(root) == 0);
    puts("exact manuals discovery, UTF-8 wrapping, bounded reading OK");
    return 0;
}
