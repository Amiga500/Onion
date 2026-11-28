#ifndef RENDER_TEXTBOX_FIT_H__
#define RENDER_TEXTBOX_FIT_H__

// Smallest font size a message is shrunk to so it fits the screen: below
// this, text on a 640x480 screen stops being readable.
#define TEXTBOX_FIT_MIN_SIZE 14

/**
 * @brief Next font size to try for a text measured at `size` as w x h pixels
 *        so it fits in max_w x max_h. SDL-free so it can be unit tested.
 *
 * Shrinks in proportion to the overflow, by at least one point, and never
 * below min_size. Returns `size` unchanged when the text already fits or
 * cannot get smaller.
 */
static int textbox_fit_size(int size, int w, int h, int max_w, int max_h, int min_size)
{
    if ((w <= max_w && h <= max_h) || size <= min_size || w <= 0 || h <= 0)
        return size;
    double scale_w = w > max_w ? (double)max_w / w : 1.0;
    double scale_h = h > max_h ? (double)max_h / h : 1.0;
    int next = (int)(size * (scale_w < scale_h ? scale_w : scale_h));
    if (next >= size)
        next = size - 1;
    if (next < min_size)
        next = min_size;
    return next;
}

#include <string.h>

/* Whether a line is blank (nothing, or only spaces). */
static int textbox_line_blank(const char *line, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (line[i] != ' ')
            return 0;
    return 1;
}

/* Whether a line starts a list item or a heading of its own: "-", "*",
 * a bullet, "::", or a number followed by "." or ")". */
static int textbox_line_own(const char *line, size_t len)
{
    size_t i = 0;
    while (i < len && line[i] == ' ')
        i++;
    if (i >= len)
        return 0;
    if (line[i] == '-' || line[i] == '*' || (line[i] == ':' && i + 1 < len && line[i + 1] == ':'))
        return 1;
    if (len - i >= 3 && (unsigned char)line[i] == 0xE2 && (unsigned char)line[i + 1] == 0x80 &&
        (unsigned char)line[i + 2] == 0xA2)
        return 1;
    size_t d = i;
    while (d < len && line[d] >= '0' && line[d] <= '9')
        d++;
    return d > i && d < len && (line[d] == '.' || line[d] == ')');
}

/**
 * @brief The text with its soft line breaks joined: a note written in short
 *        lines for the old dialog (Tweaks' tooltips) is one paragraph again,
 *        to be wrapped at the width it is shown at. SDL-free, unit tested.
 *
 * A line break stays where the next line or this one is blank, where the
 * next line is a list item or a heading ("-", "*", a bullet, "::", "1."),
 * or where this line ends with ":". Every other one becomes a space.
 */
static void textbox_reflow(const char *in, char *out, size_t n)
{
    if (n == 0)
        return;
    size_t o = 0;
    const char *line = in;
    while (*line && o + 1 < n) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        for (size_t i = 0; i < len && o + 1 < n; i++)
            out[o++] = line[i];
        if (!end)
            break;
        const char *next = end + 1;
        const char *next_end = strchr(next, '\n');
        size_t next_len = next_end ? (size_t)(next_end - next) : strlen(next);
        int keep = textbox_line_blank(line, len) || textbox_line_blank(next, next_len) ||
                   textbox_line_own(next, next_len) || (len > 0 && line[len - 1] == ':');
        if (o + 1 < n) {
            if (keep)
                out[o++] = '\n';
            else if (o > 0 && out[o - 1] != ' ')
                out[o++] = ' ';
        }
        line = next;
        if (!keep)
            while (*line == ' ')
                line++;
    }
    out[o] = '\0';
}

#endif // RENDER_TEXTBOX_FIT_H__
