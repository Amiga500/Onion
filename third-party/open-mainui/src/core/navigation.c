/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/core.h"
#include <limits.h>

/* Device IDs follow Onion 4.5-dev runtime.sh, not marketing-name guesses.
 * Keeping this table runtime-selected is the basis for one device executable.
 */
MainUIDevice mainui_device(int model)
{
    switch (model) {
    case 283:
        return (MainUIDevice){283, "Miyoo Mini", false, false, false};
    case 285:
        return (MainUIDevice){285, "Miyoo Mini Flip", true, true, true};
    case 354:
        return (MainUIDevice){354, "Miyoo Mini Plus", true, true, false};
    default:
        return (MainUIDevice){0, "Unknown", false, false, false};
    }
}

void mainui_viewport_restore(MainUIViewport *v, int total, int rows, int selected, int start,
                             int end)
{
    if (rows < 1) {
        rows = 6;
    }
    if (total <= 0) {
        *v = (MainUIViewport){0, -1, 0, -1};
        return;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (selected >= total) {
        selected = total - 1;
    }
    /* A changed row count or smaller result set can invalidate the saved
     * window independently of selection. Re-anchor only when necessary; do not
     * force every final page to fill upward, which changes reference behavior.
     */
    if (start < 0 || end < start || start > selected || end < selected || start >= total ||
        (int64_t)end - start >= rows) {
        start = selected;
    }
    if (start < 0) {
        start = 0;
    }
    int64_t last = (int64_t)start + rows - 1;
    *v = (MainUIViewport){total, selected, start, last >= total ? total - 1 : (int)last};
}

void mainui_viewport_move(MainUIViewport *v, int rows, int delta, bool wrap)
{
    if (v->total <= 0) {
        return;
    }
    /* Widen before addition so corrupt state or a large jump cannot overflow
     * signed int. Modulo normalization also handles backward edge wrapping.
     */
    int64_t next = (int64_t)v->selected + delta;
    if (wrap) {
        next = ((next % v->total) + v->total) % v->total;
    }
    else if (next < 0) {
        next = 0;
    }
    else if (next >= v->total) {
        next = v->total - 1;
    }
    int start = v->start;
    if (next < start) {
        start = (int)next;
    }
    if (next > v->end) {
        /* Scroll just far enough. The window end is stale when the list has
         * grown (Paste or Create), so the start can come out negative; the
         * window then begins at the top instead of at the selection. */
        start = (int)next - (rows > 0 ? rows : 6) + 1;
        if (start < 0) {
            start = 0;
        }
    }
    mainui_viewport_restore(v, v->total, rows, (int)next, start, (int)next);
}

void mainui_viewport_refit(MainUIViewport *v, int total, int rows, int selected)
{
    if (rows < 1) {
        rows = 6;
    }
    if (total <= 0) {
        *v = (MainUIViewport){0, -1, 0, -1};
        return;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (selected >= total) {
        selected = total - 1;
    }
    int64_t start = v->start < 0 ? 0 : v->start;
    if (selected < start) {
        start = selected;
    }
    if (selected > start + rows - 1) {
        start = (int64_t)selected - rows + 1;
    }
    if (start + rows > total) {
        start = total > rows ? total - rows : 0;
    }
    int64_t last = start + rows - 1;
    *v = (MainUIViewport){total, selected, (int)start, last >= total ? total - 1 : (int)last};
}

int mainui_marquee(uint64_t elapsed, int speed, int title, int visible, MainUIBlit out[2])
{
    if (title <= visible) {
        return 0;
    }
    return mainui_marquee_stream(elapsed, speed, title, visible, out);
}

uint64_t mainui_marquee_pixels(uint64_t moving_ms, int speed)
{
    return mainui_marquee_step_pixels(moving_ms / MAINUI_MARQUEE_FRAME_MS, speed);
}

uint64_t mainui_marquee_step_pixels(uint64_t frames, int speed)
{
    if (speed <= 0) {
        return 0;
    }
    /* Thousandths of a pixel per frame; speed is in pixels per second. */
    uint64_t per_frame = (uint64_t)speed * MAINUI_MARQUEE_FRAME_MS;
    if (per_frame >= 1000) {
        return frames * ((per_frame + 500) / 1000);
    }
    return frames / ((1000 + per_frame / 2) / per_frame);
}

int mainui_marquee_stream(uint64_t elapsed, int speed, int title, int visible, MainUIBlit out[2])
{
    if (speed <= 0 || title <= 0 || visible <= 0 || title > INT_MAX - 60) {
        return 0;
    }
    uint64_t cycle = (uint64_t)title + 60;
    /* Reduce before multiplication: remains bounded even after a very long uptime. */
    uint64_t pixels =
        ((elapsed / 1000) % cycle * (unsigned)speed + (elapsed % 1000) * (unsigned)speed / 1000) %
        cycle;
    int phase = (int)pixels, count = 0;
    if (phase < title) {
        int width = title - phase;
        if (width > visible) {
            width = visible;
        }
        out[count++] = (MainUIBlit){phase, width, 0};
    }
    /* The title may wrap into view after its suffix and the blank 60px gap.
     * A second source rectangle always begins at the start of the title.
     */
    int prefix = (int)cycle - phase;
    if (prefix < visible) {
        int width = visible - prefix;
        if (width > title) {
            width = title;
        }
        out[count++] = (MainUIBlit){0, width, prefix};
    }
    return count;
}

bool mainui_text_rtl(const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    while (*p) {
        if (*p < 0x80) {
            if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                return false;
            }
            p++;
            continue;
        }
        unsigned code = 0;
        int length = *p >= 0xf0 ? 4 : *p >= 0xe0 ? 3 : *p >= 0xc0 ? 2 : 1;
        code = length == 1 ? *p : *p & (0x7f >> length);
        for (int i = 1; i < length; i++) {
            if ((p[i] & 0xc0) != 0x80) {
                return false;
            }
            code = code << 6 | (p[i] & 0x3f);
        }
        /* Latin-1 punctuation and signs are neutral too. */
        if (code >= 0x80 && code < 0xc0) {
            p += length;
            continue;
        }
        return (code >= 0x0590 && code <= 0x08ff) || (code >= 0xfb1d && code <= 0xfdff) ||
               (code >= 0xfe70 && code <= 0xfefc);
    }
    return false;
}
