/* Arabic and Bengali text: script detection, display order and font runs
 * (ttf_bidi.c). Shaping and drawing are checked by tests/compare.py. */
#include "ttf_bidi.c"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Script fonts here have spaces and the Arabic comma but no brackets, as
 * Noto Sans Arabic UI. */
static bool covers(int script, uint32_t c, void *context)
{
    (void)script;
    (void)context;
    return c == ' ' || c == 0x060C || c == '.';
}

static int decode(const char *text, uint32_t *out)
{
    int n = 0;
    const unsigned char *s = (const unsigned char *)text;
    while (*s) {
        uint32_t c = *s;
        int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        c &= extra == 0 ? 0x7F : extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07;
        for (int i = 1; i <= extra; i++) {
            c = c << 6 | (s[i] & 0x3F);
        }
        out[n++] = c;
        s += extra + 1;
    }
    return n;
}

/* The runs of `text` in display order, as "level:script:text|...". */
static void layout(const char *text, unsigned available, char *out, size_t size)
{
    uint32_t cps[256];
    TTFBidiRun runs[64];
    int n = decode(text, cps);
    int count = ttf_bidi_layout(cps, n, available, covers, NULL, runs, 64);
    assert(count > 0);
    size_t used = 0;
    out[0] = 0;
    for (int i = 0; i < count; i++) {
        used += (size_t)snprintf(out + used, size - used, "%s%d:%d:", i ? "|" : "", runs[i].level,
                                 runs[i].script);
        for (int k = 0; k < runs[i].length; k++) {
            uint32_t c = cps[runs[i].start + k];
            /* ASCII as is, other code points as their script initial. */
            char shown = c < 0x80                     ? (char)c
                         : c >= 0x0600 && c <= 0x08FF ? 'a'
                         : c >= 0x0980 && c <= 0x09FF ? 'b'
                                                      : '?';
            used += (size_t)snprintf(out + used, size - used, "%c", shown);
        }
    }
}

static bool complex(const char *utf8)
{
    uint32_t cps[64];
    return ttf_bidi_complex(cps, decode(utf8, cps));
}

#define BOTH (1u << TTF_BIDI_ARABIC | 1u << TTF_BIDI_BENGALI)

int main(void)
{
    char out[1024];

    /* Detection: only Arabic and Bengali take the library path. */
    assert(!complex(""));
    assert(!complex("Settings"));
    assert(!complex("\xc3\x80 \xd0\x9f\xd1\x80\xd0\xb8"));     /* À При */
    assert(!complex("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));  /* 日本語 */
    assert(!complex("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d"));      /* Hebrew */
    assert(complex("\xd9\x85\xd9\x81\xd8\xb6\xd9\x84"));       /* مفضل */
    assert(complex("x \xe0\xa6\x97\xe0\xa7\x87\xe0\xa6\xae")); /* গেম */
    assert(complex("\xef\xbb\xbb"));                           /* ﻻ (FEFB) */

    /* A Bengali label is one left-to-right run. */
    layout("\xe0\xa6\x97\xe0\xa7\x87\xe0\xa6\xae\xe0\xa6\xb8", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:2:bbbb"));

    /* Words of one script keep their spaces in the script run. */
    layout("\xe0\xa6\xad\xe0\xa6\xbe \xe0\xa6\xaa", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:2:bb b"));

    /* Arabic with a Latin name in brackets, as in the language list: the
     * paragraph is right-to-left, so the Latin part goes to the left, and
     * the brackets around it are in the caller's font. */
    layout("\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9 (Arabic)", BOTH, out,
           sizeof out);
    assert(!strcmp(out, "1:0:)|2:0:Arabic|1:0: (|1:1:aaaaaaa"));

    /* Brackets stay around the word they enclose (N0), also between a
     * Latin name and Bengali in Arabic text, and in a Latin title. */
    layout("\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9 (Arabic) "
           "\xe0\xa6\xac\xe0\xa6\xbe\xe0\xa6\x82\xe0\xa6\xb2\xe0\xa6\xbe",
           BOTH, out, sizeof out);
    assert(!strcmp(out, "2:2:bbbbb|1:0:) |2:0:Arabic|1:0: (|1:1:aaaaaaa"));
    layout("Game (\xd9\x84\xd8\xb9\xd8\xa8\xd8\xa9) 2", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:0:Game (|1:1:aaaa|0:0:) |2:0:2"));
    layout("\xd9\x84\xd8\xb9\xd8\xa8\xd8\xa9 (Zelda) \xd8\xac\xd8\xaf\xd9\x8a\xd8\xaf\xd8\xa9",
           BOTH, out, sizeof out);
    assert(!strcmp(out, "1:1:aaaaa|1:0:) |2:0:Zelda|1:0: (|1:1:aaaa"));
    /* A ROM name with a Latin title and a region tag inside it (robcodedev):
     * the outer brackets go with the Arabic, the square ones with the Latin
     * title, which keeps its own order. Shown: الجديد (Mario [USA]) العربية */
    layout("\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9 (Mario [USA]) "
           "\xd8\xa7\xd9\x84\xd8\xac\xd8\xaf\xd9\x8a\xd8\xaf",
           BOTH, out, sizeof out);
    assert(!strcmp(out, "1:1:aaaaaa|1:0:) |2:0:Mario [USA]|1:0: (|1:1:aaaaaaa"));
    /* Nested brackets, and an Arabic name in a Latin title. */
    layout("\xd9\x84\xd8\xb9\xd8\xa8\xd8\xa9 (Mario (USA)) \xd9\xa2", BOTH, out, sizeof out);
    assert(!strcmp(out, "2:1:a|1:0:) |2:0:Mario (USA)|1:0: (|1:1:aaaa"));
    layout("Mario [USA] (\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a)", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:0:Mario [USA] (|1:1:aaaa|0:0:)"));
    /* Unmatched or crossed brackets stay ordinary neutrals. */
    layout("\xd9\x85 (x", BOTH, out, sizeof out);
    assert(!strcmp(out, "2:0:x|1:0: (|1:1:a"));

    /* A left-to-right paragraph with an Arabic word inside keeps the Latin
     * order and puts the Arabic word in its own right-to-left run. */
    layout("Menu \xd9\x85\xd9\x81\xd8\xb6\xd9\x84 OK", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:0:Menu |1:1:aaaa|0:0: OK"));

    /* European digits after Arabic letters are Arabic numbers: a version
     * keeps its own left-to-right order inside the right-to-left line, the
     * separator included (W4), and so does a percentage (W5). */
    layout("\xd8\xa5\xd8\xb5\xd8\xaf\xd8\xa7\xd8\xb1 1.2", BOTH, out, sizeof out);
    assert(!strcmp(out, "2:0:1.2|1:0: |1:1:aaaaa"));
    layout("\xd9\x85 50%", BOTH, out, sizeof out);
    assert(!strcmp(out, "1:0:%|2:0:50|1:0: |1:1:a"));
    layout("Wi-Fi 1-2", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:0:Wi-Fi 1-2"));

    /* Arabic marks join the letter before them. */
    layout("\xd9\x85\xd9\x8e\xd8\xb1", BOTH, out, sizeof out);
    assert(!strcmp(out, "1:1:aaa"));

    /* A script without its font stays with the caller's font but keeps its
     * display order. */
    layout("\xd9\x85\xd9\x81 x", 1u << TTF_BIDI_BENGALI, out, sizeof out);
    assert(!strcmp(out, "2:0:x|1:0:aa "));

    /* Bengali danda and joiners stay in the Bengali run. */
    layout("\xe0\xa6\x95\xe2\x80\x8c\xe0\xa6\xb7\xe0\xa5\xa4", BOTH, out, sizeof out);
    assert(!strcmp(out, "0:2:b?b?"));

    /* Trailing spaces take the paragraph level (rule L1). */
    layout("\xd9\x85  ", BOTH, out, sizeof out);
    assert(!strcmp(out, "1:1:a  "));

    /* Mirroring for brackets in right-to-left runs. */
    assert(ttf_bidi_mirror('(') == ')' && ttf_bidi_mirror(')') == '(');
    assert(ttf_bidi_mirror(0x00AB) == 0x00BB && ttf_bidi_mirror('a') == 'a');

    /* Too small a run array is reported, not overrun. */
    uint32_t cps[16];
    TTFBidiRun one[1];
    int n = decode("ab \xd9\x85 cd", cps);
    assert(ttf_bidi_layout(cps, n, BOTH, covers, NULL, one, 1) == -1);
    assert(ttf_bidi_layout(cps, 0, BOTH, covers, NULL, one, 1) == 0);

    puts("Script detection, bidi order, font runs and mirroring checks passed");
    return 0;
}
