/* ttf_bidi.c: which text needs shaping, and in what order to draw it.
 *
 * Part of OnionPlus's SDL_ttf (see SDL_ttf.c), also built alone by the
 * tests. No SDL or FreeType here: code points in, runs out.
 *
 * Copyright (C) 2026 OnionPlus contributors. Same terms as SDL_ttf (zlib),
 * see COPYING-SDL_ttf. */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Scripts drawn by libSDL_ttf-script.so; 0 is the caller's font. */
enum { TTF_BIDI_ARABIC = 1, TTF_BIDI_BENGALI = 2 };

/* A stretch of text with one direction level and one font. */
typedef struct {
    int start, length, level, script;
} TTFBidiRun;

/* Whether the font for `script` has a glyph for a code point. */
typedef bool (*TTFBidiCovers)(int script, uint32_t codepoint, void *context);

/* --------------------------------------------------------------- scripts */

static bool arabic(uint32_t c)
{
    return (c >= 0x0600 && c <= 0x06FF) || (c >= 0x0750 && c <= 0x077F) ||
           (c >= 0x0870 && c <= 0x08FF) || (c >= 0xFB50 && c <= 0xFDFF) ||
           (c >= 0xFE70 && c <= 0xFEFC);
}

static bool bengali(uint32_t c)
{
    return c >= 0x0980 && c <= 0x09FF;
}

/* Marks and joiners belong to the run of the letter before them. The dandas
 * are shared by Indic scripts and join the Bengali run they end. */
static bool inherits(uint32_t c)
{
    return (c >= 0x0300 && c <= 0x036F) || c == 0x200C || c == 0x200D || c == 0x0964 ||
           c == 0x0965 || (c >= 0x064B && c <= 0x065F) || c == 0x0670 ||
           (c >= 0x06D6 && c <= 0x06DC) || (c >= 0x06DF && c <= 0x06E4) || c == 0x06E7 ||
           c == 0x06E8 || (c >= 0x06EA && c <= 0x06ED) || (c >= 0x08CA && c <= 0x08FF);
}

static int script_of(uint32_t c)
{
    if (arabic(c)) {
        return TTF_BIDI_ARABIC;
    }
    return bengali(c) ? TTF_BIDI_BENGALI : 0;
}

/* Whether any code point needs the script library. */
static bool ttf_bidi_complex(const uint32_t *text, int length)
{
    for (int i = 0; i < length; i++) {
        if (text[i] >= 0x0600 && script_of(text[i])) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ bidi */

/* A reduced Unicode Bidirectional Algorithm for one line without embeddings
 * or isolates: rules W1-W7, N0 (paired brackets), N1-N2, I1-I2, L1 (trailing
 * spaces) and L2.
 * Labels are single lines, mostly of one script, which this orders as the
 * full algorithm does. */
enum {
    B_L,
    B_R,
    B_AL,
    B_EN,
    B_AN,
    B_ES,
    B_ET,
    B_CS,
    B_NSM,
    B_WS,
    B_ON
};

static int bidi_class(uint32_t c)
{
    if (c >= '0' && c <= '9') {
        return B_EN;
    }
    if ((c >= 0x0660 && c <= 0x0669) || c == 0x066B || c == 0x066C) {
        return B_AN;
    }
    if (c >= 0x06F0 && c <= 0x06F9) {
        return B_EN;
    }
    if (c == ' ' || c == '\t' || c == 0x00A0 || (c >= 0x2000 && c <= 0x200A)) {
        return B_WS;
    }
    if (c == '+' || c == '-' || c == 0x2212) {
        return B_ES;
    }
    if (c == ',' || c == '.' || c == '/' || c == ':' || c == 0x060C) {
        return B_CS;
    }
    if (c == '#' || c == '$' || c == '%' || (c >= 0x00A2 && c <= 0x00A5) || c == 0x00B0 ||
        c == 0x00B1 || c == 0x066A || c == 0x2030 || c == 0x20AC) {
        return B_ET;
    }
    if (inherits(c) && c != 0x0964 && c != 0x0965) {
        return B_NSM;
    }
    if (arabic(c)) {
        return B_AL;
    }
    if ((c >= 0x0590 && c <= 0x05FF) || (c >= 0xFB1D && c <= 0xFB4F)) {
        return B_R;
    }
    if (c < 0x80) {
        bool letter = (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
        return letter ? B_L : B_ON;
    }
    if ((c >= 0x00A1 && c <= 0x00BF && c != 0x00AA && c != 0x00B5 && c != 0x00BA) || c == 0x00D7 ||
        c == 0x00F7 || (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205E) ||
        (c >= 0x2190 && c <= 0x2BFF) || c == 0xFFFD) {
        return B_ON;
    }
    return B_L;
}

/* The direction a resolved type counts as for N0: L, R (numbers too), or
 * -1 for a neutral. */
static int strong_direction(int t)
{
    if (t == B_L) {
        return B_L;
    }
    return t == B_R || t == B_EN || t == B_AN ? B_R : -1;
}

/* N0: a pair of brackets takes one direction, so "(Arabic)" in Arabic text
 * keeps its brackets around the word. A pair with text of the paragraph's
 * direction inside takes that direction; with only the other direction inside,
 * it takes that one when the text before the opening bracket has it too. */
static void resolve_brackets(const uint32_t *text, int n, unsigned char *type, int paragraph)
{
    static const uint32_t pairs[][2] = {{'(', ')'}, {'[', ']'}, {'{', '}'}};
    enum { DEPTH = 63 }; /* as the Unicode algorithm bounds it */
    int stack[DEPTH], kinds[DEPTH], depth = 0;
    int embedding = paragraph ? B_R : B_L, opposite = paragraph ? B_L : B_R;
    for (int i = 0; i < n; i++) {
        if (type[i] != B_ON) {
            continue;
        }
        for (int k = 0; k < 3; k++) {
            if (text[i] == pairs[k][0]) {
                if (depth == DEPTH) {
                    return; /* stop, as the algorithm does */
                }
                stack[depth] = i;
                kinds[depth++] = k;
                break;
            }
            if (text[i] != pairs[k][1]) {
                continue;
            }
            int found = depth - 1;
            while (found >= 0 && kinds[found] != k) {
                found--;
            }
            if (found < 0) {
                break; /* no opening bracket: an ordinary neutral */
            }
            int open = stack[found];
            depth = found;
            bool inside_embedding = false, inside_opposite = false;
            for (int j = open + 1; j < i; j++) {
                int d = strong_direction(type[j]);
                inside_embedding = inside_embedding || d == embedding;
                inside_opposite = inside_opposite || d == opposite;
            }
            int resolved = -1;
            if (inside_embedding) {
                resolved = embedding;
            }
            else if (inside_opposite) {
                int before = paragraph ? B_R : B_L;
                for (int j = open - 1; j >= 0; j--) {
                    int d = strong_direction(type[j]);
                    if (d >= 0) {
                        before = d;
                        break;
                    }
                }
                resolved = before == opposite ? opposite : embedding;
            }
            if (resolved >= 0) {
                type[open] = type[i] = (unsigned char)resolved;
            }
            break;
        }
    }
}

static void resolve_levels(const uint32_t *text, int n, unsigned char *levels)
{
    unsigned char *type = malloc((size_t)n);
    if (!type) {
        memset(levels, 0, (size_t)n);
        return;
    }
    int paragraph = 0;
    for (int i = 0; i < n; i++) {
        int t = bidi_class(text[i]);
        if (t == B_L || t == B_R || t == B_AL) {
            paragraph = t == B_L ? 0 : 1;
            break;
        }
    }
    int sos = paragraph ? B_R : B_L;
    /* W1-W3: marks take the type before them, European digits after Arabic
     * letters are Arabic numbers, Arabic letters are right-to-left. */
    int last_strong = sos;
    int previous = sos;
    for (int i = 0; i < n; i++) {
        int t = bidi_class(text[i]);
        if (t == B_NSM) {
            t = previous;
        }
        previous = t;
        if (t == B_L || t == B_R || t == B_AL) {
            last_strong = t;
        }
        if (t == B_EN && last_strong == B_AL) {
            t = B_AN;
        }
        type[i] = (unsigned char)(t == B_AL ? B_R : t);
    }
    /* W4: one separator between two numbers of a kind joins them. */
    for (int i = 1; i + 1 < n; i++) {
        int before = type[i - 1], after = type[i + 1];
        if (type[i] == B_ES && before == B_EN && after == B_EN) {
            type[i] = B_EN;
        }
        else if (type[i] == B_CS && before == after && (before == B_EN || before == B_AN)) {
            type[i] = (unsigned char)before;
        }
    }
    /* W5: terminators (%, currency) next to European numbers join them. */
    for (int i = 0; i < n;) {
        if (type[i] != B_ET) {
            i++;
            continue;
        }
        int end = i;
        while (end < n && type[end] == B_ET) {
            end++;
        }
        if ((i && type[i - 1] == B_EN) || (end < n && type[end] == B_EN)) {
            memset(type + i, B_EN, (size_t)(end - i));
        }
        i = end;
    }
    /* W6 and W7: other separators are neutral; European numbers after
     * left-to-right text are left-to-right text. */
    last_strong = sos;
    for (int i = 0; i < n; i++) {
        int t = type[i];
        if (t == B_ES || t == B_ET || t == B_CS) {
            type[i] = B_ON;
        }
        if (t == B_L || t == B_R) {
            last_strong = t;
        }
        if (t == B_EN && last_strong == B_L) {
            type[i] = B_L;
        }
    }
    resolve_brackets(text, n, type, paragraph);
    /* N1-N2: neutrals take the direction around them, numbers counting as
     * right-to-left, or else the paragraph's. */
    for (int i = 0; i < n;) {
        if (type[i] != B_WS && type[i] != B_ON) {
            i++;
            continue;
        }
        int end = i;
        while (end < n && (type[end] == B_WS || type[end] == B_ON)) {
            end++;
        }
        int before = i ? type[i - 1] : sos;
        int after = end < n ? type[end] : sos;
        before = before == B_L ? B_L : B_R;
        after = after == B_L ? B_L : B_R;
        int resolved = before == after ? before : (paragraph ? B_R : B_L);
        for (int j = i; j < end; j++) {
            type[j] = (unsigned char)resolved;
        }
        i = end;
    }
    /* I1-I2. */
    for (int i = 0; i < n; i++) {
        int t = type[i];
        if (paragraph == 0) {
            levels[i] = t == B_R ? 1 : (t == B_AN || t == B_EN) ? 2 : 0;
        }
        else {
            levels[i] = t == B_R ? 1 : 2;
        }
    }
    /* L1: spaces at the end of the line take the paragraph level. */
    for (int i = n - 1; i >= 0 && bidi_class(text[i]) == B_WS; i--) {
        levels[i] = (unsigned char)paragraph;
    }
    free(type);
}

static uint32_t ttf_bidi_mirror(uint32_t c)
{
    static const uint32_t pairs[][2] = {
        {'(', ')'}, {'[', ']'}, {'{', '}'}, {'<', '>'}, {0x00AB, 0x00BB}, {0x2039, 0x203A},
    };
    for (size_t i = 0; i < sizeof pairs / sizeof *pairs; i++) {
        if (c == pairs[i][0]) {
            return pairs[i][1];
        }
        if (c == pairs[i][1]) {
            return pairs[i][0];
        }
    }
    return c;
}

/* ---------------------------------------------------------------- layout */

/* Split `length` code points into runs, returned in display order from left
 * to right, and return how many, or -1 when `capacity` is too small or memory
 * runs out. `available` has bit (1 << script) set for each script whose font
 * can be used; text in other scripts stays with the caller's font. */
static int ttf_bidi_layout(const uint32_t *text, int length, unsigned available,
                           TTFBidiCovers covers, void *context, TTFBidiRun *runs, int capacity)
{
    if (length <= 0) {
        return 0;
    }
    unsigned char *levels = malloc((size_t)length);
    unsigned char *scripts = malloc((size_t)length);
    if (!levels || !scripts) {
        free(levels);
        free(scripts);
        return -1;
    }
    resolve_levels(text, length, levels);
    for (int i = 0; i < length; i++) {
        int s = script_of(text[i]);
        scripts[i] = (unsigned char)(available & (1u << s) ? s : 0);
    }
    /* Marks and joiners follow the letter before them (or after, at the
     * start of the text). */
    for (int i = 0; i < length; i++) {
        if (scripts[i]) {
            continue;
        }
        uint32_t c = text[i];
        if (inherits(c)) {
            if (i && scripts[i - 1]) {
                scripts[i] = scripts[i - 1];
            }
            else if (i + 1 < length && scripts[i + 1] && script_of(text[i + 1])) {
                scripts[i] = scripts[i + 1];
            }
        }
    }
    /* Spaces and punctuation between two letters of one script join its run
     * when its font has them, so words keep the run's spacing; next to text
     * in the caller's font they keep that font, like the text beside them. */
    for (int i = 0; i < length;) {
        int t = bidi_class(text[i]);
        if (scripts[i] || (t != B_WS && t != B_ON)) {
            i++;
            continue;
        }
        int end = i;
        bool same_level = true;
        while (end < length && !scripts[end] &&
               (bidi_class(text[end]) == B_WS || bidi_class(text[end]) == B_ON)) {
            same_level = same_level && levels[end] == levels[i];
            end++;
        }
        int s = i ? scripts[i - 1] : 0;
        int next = end < length ? scripts[end] : s;
        bool fits = s && s == next && same_level && levels[i - 1] == levels[i] && covers;
        for (int j = i; fits && j < end; j++) {
            fits = covers(s, text[j], context);
        }
        for (int j = i; fits && j < end; j++) {
            scripts[j] = (unsigned char)s;
        }
        i = end;
    }
    int count = 0;
    int max_level = 0, min_odd = INT_MAX;
    for (int i = 0; i < length;) {
        int end = i + 1;
        while (end < length && levels[end] == levels[i] && scripts[end] == scripts[i]) {
            end++;
        }
        if (count == capacity) {
            free(levels);
            free(scripts);
            return -1;
        }
        runs[count++] = (TTFBidiRun){i, end - i, levels[i], scripts[i]};
        if (levels[i] > max_level) {
            max_level = levels[i];
        }
        if ((levels[i] & 1) && levels[i] < min_odd) {
            min_odd = levels[i];
        }
        i = end;
    }
    free(levels);
    free(scripts);
    /* L2: from the highest level down to the lowest odd one, reverse every
     * sequence of runs at that level or higher. */
    for (int level = max_level; level >= min_odd && level > 0; level--) {
        for (int i = 0; i < count;) {
            if (runs[i].level < level) {
                i++;
                continue;
            }
            int end = i;
            while (end < count && runs[end].level >= level) {
                end++;
            }
            for (int a = i, b = end - 1; a < b; a++, b--) {
                TTFBidiRun swap = runs[a];
                runs[a] = runs[b];
                runs[b] = swap;
            }
            i = end;
        }
    }
    return count;
}

