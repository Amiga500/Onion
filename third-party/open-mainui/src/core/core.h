/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_CORE_H
#define MAINUI_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool (*requested)(void *context);
    void *context;
} MainUICancel;

static inline bool mainui_cancelled(MainUICancel cancel)
{
    return cancel.requested && cancel.requested(cancel.context);
}

/* Process-lifetime patch settings. A zero font_size inherits the theme font;
 * scroll_status matches the reference model: 1 = disabled, 2 = enabled.
 * These values configure included functionality, never select build features.
 */
typedef struct {
    int rows, row_height, font_size;
    int repeat_delay, repeat_interval;
    int scroll_status, scroll_delay, scroll_speed;
    /* A long selected title travels to its end, pauses and travels back,
     * instead of rolling round: a third .romListTitleScroll value "bounce". */
    bool scroll_bounce;
    bool case_sensitive, dynamic_favorite_position, show_recents, show_expert;
    /* Shorten context menu labels too wide for the popup with "...". On
     * unless .noContextMenuEllipsis exists. */
    bool context_ellipsis;
    /* Footer with the page counter on the left and the button hints on the
     * right, B before A: .mainuiFooterAltLayout exists. */
    bool footer_alt;
    /* Milliseconds a game stays selected before its video plays in place of
     * its cover, or over the whole screen, or -1 for never:
     * .romListVideoPreview (see mainui_config_video_preview()). */
    int video_preview_delay;
    bool video_preview_fullscreen;
    /* Optional downward scrolling of the Game Info description. Missing
     * .gameInfoAutoScroll keeps the reference manual-only behavior. */
    int details_auto_delay, details_auto_speed;
} MainUIConfig;

/* Parse nullable file contents using the patcher's prefix/read-length rules.
 * out must be non-NULL. Every call resets all fields to baseline defaults;
 * mainui_config_load subsequently fills the marker-file flags.
 */
/* The contents of .romListVideoPreview, NULL when it is missing: the delay
 * and the mode, separated by commas or spaces, as "800,fullscreen". The
 * delay is -1 (no previews) without the file, otherwise a whole number of
 * milliseconds from 0 to 30000 (clamped), or 1000 when the file holds none.
 * The mode is "cover", the default, or "fullscreen" in any case; any other
 * word is cover. Fullscreen waits at least 100 ms. */
int mainui_config_video_preview(const char *text, bool *fullscreen);
void mainui_config_parse(MainUIConfig *out, const char *rows, const char *font, const char *repeat,
                         const char *scroll);
/* Files are read from an explicit config directory; never writes user data. */
void mainui_config_load(MainUIConfig *out, const char *directory);
/* NULL disables scrolling; an existing empty marker uses 2000 ms and
 * 20 pixels/s. Invalid fields use their defaults, values are clamped. */
void mainui_config_details_auto(const char *text, int *delay, int *speed);

typedef struct {
    int id;
    const char *name;
    bool wifi, axp, lid;
} MainUIDevice;

/* Unknown IDs expose no hardware capabilities. Callers must not infer a
 * power-control interface from an unrecognized device.
 */
MainUIDevice mainui_device(int model);

typedef struct {
    int total, selected, start, end;
} MainUIViewport;

/* Normalize untrusted persisted indices against the current list size.
 * Empty lists have selected=end=-1. Nonempty views always contain selection.
 * This is the internal normalized model, not a serializer for legacy sentinels.
 */
void mainui_viewport_restore(MainUIViewport *view, int total, int rows, int selected, int start,
                             int end);
/* Move a normalized viewport; delta may be negative or exceed the list size.
 * Single-step callers may request edge wrapping, while page moves clamp.
 */
void mainui_viewport_move(MainUIViewport *view, int rows, int delta, bool wrap);
/* Fit a window to a list that changed underneath it (an edit, a removal, a
 * reload): select `selected` (clamped), keep the old start where it still
 * shows the selection, otherwise scroll just far enough, and close any gap
 * below the last row. The old end is ignored, being stale when the size
 * changed. Never moves a window that still fits. */
void mainui_viewport_refit(MainUIViewport *view, int total, int rows, int selected);

typedef struct {
    int source_x, width, destination_x;
} MainUIBlit;

/* Returns 0..2 clipped segments for the seamless 60px-gap marquee.
 * elapsed_ms starts after the configured idle delay. out must hold two entries.
 * Segments describe source/destination rectangles; the caller owns all surfaces.
 */
int mainui_marquee(uint64_t elapsed_ms, int speed, int title_width, int visible_width,
                   MainUIBlit out[2]);

/* Caller has already established overflow against the unobstructed lane. The
 * draw viewport may be wider because preview artwork is composited afterwards. */
/* Marquee frames are paced on this period: four 10 ms kernel ticks on the device. */
#define MAINUI_MARQUEE_FRAME_MS 40
/* Whole-pixel marquee offset after moving_ms of scrolling: a constant step per
 * frame period (or one pixel every k periods), so the motion stays even. */
uint64_t mainui_marquee_pixels(uint64_t moving_ms, int speed);
/* The same offset after a number of marquee frames. */
uint64_t mainui_marquee_step_pixels(uint64_t frames, int speed);
int mainui_marquee_stream(uint64_t elapsed_ms, int speed, int title_width, int draw_width,
                          MainUIBlit out[2]);
/* Bounce scrolling rests at each end of the title as long as scrolling this
 * far takes at its speed, within these bounds, in whole marquee frames:
 * 720 ms at 120 px/s. */
#define MAINUI_MARQUEE_BOUNCE_REST_PX 84
#define MAINUI_MARQUEE_BOUNCE_REST_MIN_MS 400
#define MAINUI_MARQUEE_BOUNCE_REST_MAX_MS 2000
/* Each way speeds up from rest and slows into the end over about this. */
#define MAINUI_MARQUEE_BOUNCE_EASE_MS 240
#define MAINUI_PI 3.14159265358979323846
/* The rest at each end, in marquee frames; 0 for no speed. */
int mainui_marquee_bounce_rest(int speed);
/* Bounce offset, 0..distance, after moving_ms of scrolling: ease out to
 * distance, rest, ease back, rest, repeat, at the marquee's own step at full
 * speed. distance is the title width less the visible width; 0 when it is
 * not positive or speed is not. */
int mainui_marquee_bounce(uint64_t moving_ms, int speed, int distance);
/* True when the first strong character of a UTF-8 title is Hebrew or Arabic:
 * its scrolling title moves the other way. Digits, spaces and ASCII
 * punctuation are skipped; any other letter or symbol reads left to right. */
bool mainui_text_rtl(const char *text);

typedef struct {
    int title, type, selected, start, end;
} MainUIFrame;

#define MAINUI_STACK_MAX 32

typedef struct {
    MainUIFrame frames[MAINUI_STACK_MAX];
    size_t count;
} MainUIStack;

/* Parse the legacy /tmp/state.json stack, retaining numeric sentinel values.
 * text must be NUL-terminated; the file-reading layer must bound input size.
 * On failure out is untouched. No viewport repair occurs in this wire codec.
 */
bool mainui_state_parse(const char *json, MainUIStack *out);
/* Returns a newly allocated legacy JSON string, or NULL on failure.
 * The caller frees the result with free(); this project uses cJSON's default
 * allocator. No file publication or disk mutation occurs here.
 */
char *mainui_state_json(const MainUIStack *state);

#endif
