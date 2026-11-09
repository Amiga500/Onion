#ifndef SYSTEM_OSD_H__
#define SYSTEM_OSD_H__

#include <SDL/SDL.h>
#include <pthread.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/timer.h"

#include "./clock.h"
#include "./display.h"
#include "./osd_panel.h"

#define CHR_WIDTH (3 * 4 + 4)
#define CHR_HEIGHT (5 * 4)

#define OSD_COLOR_WHITE 0x00FFFFFF
#define OSD_COLOR_RED 0x00F80355
#define OSD_COLOR_GREEN 0x001CD577
#define OSD_COLOR_CYAN 0x0000ffD7
#define OSD_COLOR_YELLOW 0x00DCFF62

#define OSD_BRIGHTNESS_COLOR OSD_COLOR_WHITE
#define OSD_VOLUME_COLOR OSD_COLOR_GREEN
#define OSD_MUTE_ON_COLOR OSD_COLOR_RED

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static volatile bool osd_thread_active = false;
static pthread_t osd_pt;
// osd_pt was created and not joined yet (it may have finished on its own)
static bool osd_thread_joinable = false;

typedef struct {
    SDL_Surface *surface;
    int destX;
    int destY;
    int duration_ms;
    bool rotate;
    bool useMask;
    display_t display;
} overlay_thread_data;

typedef struct {
    bool cancelled;
    pthread_t thread_id;
    pthread_mutex_t lock;
} task_t;

static task_t g_overlay_task = {true, 0, PTHREAD_MUTEX_INITIALIZER};

// The last overlay thread, until it is joined. One that ended on its own
// (timeout) cleared thread_id and was never joined: every overlay leaked a
// thread, as the OSD bar did (see osd_showBar).
static pthread_t overlay_last;
static bool overlay_unjoined = false;

/**
 * @brief Cancels the overlay and cleans up resources.
 *
 * Called by overlay_surface() if there is an existing overlay, or manually by
 * the user to cancel an overlay. (future use - gameswitcher :))
 */
void cancel_overlay()
{
    if (g_overlay_task.thread_id == 0) {
        print_debug("No overlay to cancel\n");
        return;
    }
    // Mark abort
    pthread_mutex_lock(&g_overlay_task.lock);
    pthread_t thread_to_join = g_overlay_task.thread_id;
    printf_debug("Cancelling overlay thread ID %ld\n", thread_to_join);
    g_overlay_task.cancelled = true;
    pthread_mutex_unlock(&g_overlay_task.lock);

    pthread_join(thread_to_join, NULL);
    // Joined: never join it again (a thread that ended early left its id set,
    // and the next overlay joined it a second time).
    pthread_mutex_lock(&g_overlay_task.lock);
    g_overlay_task.thread_id = 0;
    pthread_mutex_unlock(&g_overlay_task.lock);
    overlay_unjoined = false;

    printf_debug("Cancelled overlay thread ID %ld\n", thread_to_join);
}

static void free_overlay_data(overlay_thread_data *data)
{
    if (data->surface) {
        SDL_FreeSurface(data->surface);
        data->surface = NULL;
    }
    display_free(&data->display);
    free(data);
}

// An overlay thread ending early: its data freed and the task marked over
// (it returned with both left as they were).
static void *_overlay_thread_end(overlay_thread_data *data)
{
    free_overlay_data(data);
    pthread_mutex_lock(&g_overlay_task.lock);
    g_overlay_task.cancelled = true;
    g_overlay_task.thread_id = 0;
    pthread_mutex_unlock(&g_overlay_task.lock);
    return NULL;
}

static void *_overlay_draw_thread(void *arg)
{
    overlay_thread_data *data = (overlay_thread_data *)arg;

    // Backup original fb content
    START_TIMER(framebuffer_backup);
    int numBuffers = data->display.vinfo.yres > 0
        ? data->display.vinfo.yres_virtual / data->display.vinfo.yres
        : 1;
    unsigned int **originalPixels = malloc(numBuffers * sizeof(unsigned int *));
    if (!originalPixels) {
        return _overlay_thread_end(data);
    }
    for (int b = 0; b < numBuffers; b++) {
        originalPixels[b] = malloc(data->surface->w * data->surface->h * sizeof(unsigned int));
        if (!originalPixels[b]) {
            for (int i = 0; i < b; i++) {
                free(originalPixels[i]);
            }
            free(originalPixels);
            return _overlay_thread_end(data);
        }
    }

    rect_t rect = {data->destX, data->destY, data->surface->w, data->surface->h};

    display_readBuffers(&data->display, originalPixels, rect, data->rotate, data->useMask);
    END_TIMER(framebuffer_backup);
    printf_debug("Backup buffer total size: %d KiB\n",
                 (numBuffers * data->surface->w * data->surface->h * sizeof(unsigned int)) / 1024);

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long elapsed_ms = -1;
    int draw_count = 0;

    // Continuously blit the overlay to each buffer
    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed_ms =
            (long)(now.tv_sec - start.tv_sec) * 1000L +
            (long)(now.tv_nsec - start.tv_nsec) / 1000000L;

        // Timeout
        if (data->duration_ms > 0 && elapsed_ms >= data->duration_ms)
            break;

        // Abort
        pthread_mutex_lock(&g_overlay_task.lock);
        bool local_abort = g_overlay_task.cancelled;
        pthread_mutex_unlock(&g_overlay_task.lock);
        if (local_abort)
            break;

        // Draw to each buffer
        numBuffers = data->display.vinfo.yres > 0
            ? data->display.vinfo.yres_virtual / data->display.vinfo.yres
            : 1;
        for (int b = 0; b < numBuffers; b++) {
            draw_count++;
            display_writeBuffer(b, &data->display, data->surface->pixels, rect, data->rotate, false);
        }

        // Throttle the draw loop to avoid a full-throttle busy-wait
        msleep(2);
    }

    printf_debug("Draw count: %d\n", draw_count);
    printf_debug("Draw speed: %f\n", (float)draw_count / (float)elapsed_ms * 1000.0f);

    // Restore original framebuffer content after overlay
    // TODO: If the content "behind" the overlay has changed, this will not restore it correctly, causing a 1 frame glitch. How to fix? Only backup if the overlay is (partially) outside the game screen?
    // TODO: Theoretically the backup/restore is only needed if the position of the overlay is outside the game screen because that part of the screen is not updated by the game.

    START_TIMER(framebuffer_restore);
    display_writeBuffers(&data->display, originalPixels, rect, data->rotate, data->useMask);

    // Free buffer backups
    numBuffers = data->display.vinfo.yres > 0
        ? data->display.vinfo.yres_virtual / data->display.vinfo.yres
        : 1;
    for (int b = 0; b < numBuffers; b++) {
        free(originalPixels[b]);
    }
    free(originalPixels);
    END_TIMER(framebuffer_restore);

    free_overlay_data(data);

    // Clean up thread data
    pthread_mutex_lock(&g_overlay_task.lock);
    g_overlay_task.cancelled = true;
    g_overlay_task.thread_id = 0;
    pthread_mutex_unlock(&g_overlay_task.lock);

    pthread_exit(NULL);
}

/**
 * @brief Overlays an SDL_Surface onto the framebuffer at a specified position and duration.
 *
 * Flickering is minimized by drawing to all buffers in the framebuffer.
 * Should work regardless of the count of buffers (double, triple, etc. buffering).
 * 
 * @param surface The SDL_Surface to overlay. The surface will be freed by this function.
 * @param destX The X coordinate on the screen where the overlay should be placed.
 * @param destY The Y coordinate on the screen where the overlay should be placed.
 * @param duration_ms The duration in milliseconds for which the overlay should be displayed. 0 for indefinite.
 * @param rotate Should the overlay be rotated 180 degrees? 
 * @return int Returns 0 on success, -1 on failure.
 */
int overlay_surface(SDL_Surface *surface, int destX, int destY, int duration_ms, bool rotate)
{
    printf_debug("Overlaying surface at %d, %d, size %dx%d, duration %d\n, rotate %d\n",
                 destX, destY, surface->w, surface->h, duration_ms, rotate);
    display_init(false);

    // If there's an existing thread, abort it
    if (g_overlay_task.thread_id != 0) {
        print_debug("Cancelling existing overlay\n");
        cancel_overlay();
    }
    // One that ended by itself is still to be joined.
    if (overlay_unjoined) {
        pthread_join(overlay_last, NULL);
        overlay_unjoined = false;
    }

    // Data for thread
    overlay_thread_data *data = (overlay_thread_data *)calloc(1, sizeof(overlay_thread_data));
    if (!data) {
        perror("Failed to allocate overlay_thread_data\n");
        return -1;
    }

    display_t *display = &data->display;
    display->finfo = g_display.finfo;

    // Query FB info
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &display->vinfo) == -1) {
        perror("Error reading variable screen info");
        free(data);
        return -1;
    }

    if (display->vinfo.bits_per_pixel != 32) {
        perror("Only 32bpp is supported for now\n");
        free(data);
        return -1;
    }

    data->surface = surface;
    data->destX = destX;
    data->destY = destY;
    data->duration_ms = duration_ms;
    data->rotate = rotate;
    data->useMask = false; // TODO: Use mask dependent on running application

    // Need to map every time in case of buffer changes
    display->fb_size = (long)g_display.finfo.line_length * (long)display->vinfo.yres_virtual;
    display->fb_addr = mmap(NULL, display->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (display->fb_addr == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
        free_overlay_data(data);
        return -1;
    }

    // Create the thread
    print_debug("Creating overlay drawing thread\n");
    pthread_mutex_lock(&g_overlay_task.lock);
    g_overlay_task.cancelled = false;
    int rc = pthread_create(&g_overlay_task.thread_id, NULL, _overlay_draw_thread, data);
    if (rc != 0) {
        fprintf(stderr, "Failed to create overlay drawing thread\n");
        free_overlay_data(data);
        pthread_mutex_unlock(&g_overlay_task.lock);
        return -1;
    }

    printf_debug("Thread ID %ld created\n", g_overlay_task.thread_id);
    overlay_last = g_overlay_task.thread_id;
    overlay_unjoined = true;
    pthread_mutex_unlock(&g_overlay_task.lock);

    return 0;
}

static int meterWidth = 4;
static volatile bool osd_bar_activated = false;
static volatile long _bar_timer = 0;
static volatile int _bar_value = 0;
static volatile int _bar_max = 0;
static volatile uint32_t _bar_color = 0x00FFFFFF;
static volatile int _bar_kind = OSD_PANEL_BRIGHTNESS;
static volatile bool _bar_mute = false;
#ifdef PLATFORM_MIYOOMINI
static uint32_t *_bar_savebuf;
#endif

// The indicator's look, read when it appears (like meterWidth) and kept
// while it is on screen: the panel at the bottom (default), at the top with
// the .osdPanelTop flag, or with .osdEdgeBar Onion's bar on the left edge.
// meterWidth 0 hides them all.
#define OSD_EDGE_BAR_FLAG ".osdEdgeBar"
#define OSD_PANEL_TOP_FLAG ".osdPanelTop"
static bool _osd_panel = true;
static bool _osd_panel_top = false;
#ifdef PLATFORM_MIYOOMINI
#define OSD_PANEL_MAX_BUFFERS 3
static osd_panel_rect_t _panel_rect;
static uint32_t *_panel_px;  // the picture, screen orientation
static uint32_t *_panel_rot; // the same, each row reversed (the screen is turned 180 degrees)
// What is behind the panel on each buffer, framebuffer order: each buffer
// gets its own back, as a program may show any of them.
static uint32_t *_panel_save[OSD_PANEL_MAX_BUFFERS];
// The panel is on that buffer as _panel_rot is now: a pixel there that
// differs from it was drawn by the program since.
static bool _panel_on[OSD_PANEL_MAX_BUFFERS];
// The program drew over the panel on that buffer while it showed: what was
// saved behind it may be old (see _panel_restoreBehind).
static bool _panel_redrawn[OSD_PANEL_MAX_BUFFERS];
// The program drew the panel over by itself once it went (no need to put
// the saved picture back there).
static bool _panel_covered[OSD_PANEL_MAX_BUFFERS];
static int _panel_drawn_kind = -1, _panel_drawn_value = -1, _panel_drawn_max = -1;
static uint32_t _panel_drawn_color;
static bool _panel_drawn_mute;
#endif

#ifdef PLATFORM_MIYOOMINI
// The screen the panel was placed on, taken when it appears and kept until
// it goes: keymon's g_display follows a mode change (SIGUSR1, 640x480 <->
// 752x560), and the rows and buffers must not move under the saved back.
// Rows are finfo.line_length apart, as display_readOrWriteBuffer() has
// them; each buffer is yres rows; only the buffers inside the mapping.
static int _panel_W, _panel_H, _panel_stride, _panel_nbuf;

static bool _panel_take_screen(void)
{
    _panel_W = g_display.width;
    _panel_H = g_display.height;
    _panel_stride = (int)(g_display.finfo.line_length / sizeof(uint32_t));
    if (_panel_stride < _panel_W)
        _panel_stride = _panel_W;
    if (!g_display.fb_addr || _panel_W <= 0 || _panel_H <= 0)
        return false;
    long one = (long)_panel_stride * _panel_H;
    long n = g_display.fb_size > 0 ? g_display.fb_size / (long)sizeof(uint32_t) / one : 1;
    _panel_nbuf = n > OSD_PANEL_MAX_BUFFERS ? OSD_PANEL_MAX_BUFFERS : (int)n;
    return _panel_nbuf >= 1;
}

// The panel's pixel (x, y) is on the framebuffer at row H-1-(r.y+y) and
// column W-1-(r.x+x): the screen is upside down. With each row reversed
// (_panel_rot), the panel's row is one run of the framebuffer row from
// column W-r.x-r.w, so it is copied with memcpy.
static inline uint32_t *_panel_fb_row(int buf, int y)
{
    int fy = _panel_H - 1 - (_panel_rect.y + y);
    return g_display.fb_addr + ((size_t)buf * _panel_H + (size_t)fy) * _panel_stride +
           (_panel_W - _panel_rect.x - _panel_rect.w);
}

static void _panel_render_if_changed(void)
{
    const int kind = _bar_kind, value = _bar_value, max = _bar_max;
    const uint32_t color = _bar_color;
    const bool mute = _bar_mute;
    if (kind == _panel_drawn_kind && value == _panel_drawn_value && max == _panel_drawn_max &&
        color == _panel_drawn_color && mute == _panel_drawn_mute)
        return;
    const int w = _panel_rect.w, h = _panel_rect.h;
    osd_panel_render(_panel_px, w, h, kind, value, max, mute, color);
    for (int y = 0; y < h; y++) {
        const uint32_t *src = _panel_px + (size_t)y * w;
        uint32_t *dst = _panel_rot + (size_t)y * w;
        for (int x = 0; x < w; x++)
            dst[w - 1 - x] = src[x];
    }
    // The buffers show the old picture: not a sign of the program drawing.
    for (int b = 0; b < OSD_PANEL_MAX_BUFFERS; b++)
        _panel_on[b] = false;
    _panel_drawn_kind = kind;
    _panel_drawn_value = value;
    _panel_drawn_max = max;
    _panel_drawn_color = color;
    _panel_drawn_mute = mute;
}

// Has the program drawn over the panel on this buffer? Nine pixels, so a
// pass stays fast: reading the framebuffer is slow, and while a pass is
// slow the panel misses frames of a game or of a menu that redraws all the
// time, and flickers.
static bool _panel_overdrawn(int buf)
{
    const int w = _panel_rect.w, h = _panel_rect.h;
    const int ys[3] = {h / 4, h / 2, 3 * h / 4}, xs[3] = {w / 4, w / 2, 3 * w / 4};
    for (int j = 0; j < 3; j++) {
        const uint32_t *fb = _panel_fb_row(buf, ys[j]);
        const uint32_t *rot = _panel_rot + (size_t)ys[j] * w;
        for (int i = 0; i < 3; i++)
            if (fb[xs[i]] != rot[xs[i]])
                return true;
    }
    return false;
}

// The buffer on screen now (the one the program last flipped to), or -1 if
// the driver doesn't say. Screenshots find it the same way.
static int _panel_shown_override = -2; // tests: the buffer "on screen" (-2: ask the driver)

static int _panel_shown_buffer(void)
{
    if (_panel_shown_override != -2)
        return _panel_shown_override;
    struct fb_var_screeninfo v;
    if (fb_fd < 0 || ioctl(fb_fd, FBIOGET_VSCREENINFO, &v) != 0 || _panel_H <= 0)
        return -1;
    int index = (int)(v.yoffset / (unsigned)_panel_H);
    return index < _panel_nbuf ? index : -1;
}

// Each pass draws the panel on the buffer on screen (a game draws the next
// frame on another one, then flips to it: the panel is back on it a pass
// after the flip, before the screen gets to it), and on another buffer only
// when the panel isn't there (not drawn yet, or the program drew over it).
// One buffer per pass instead of three keeps passes short, so the panel is
// missing from fewer frames.
static void _print_panel(void)
{
    if (!_panel_px)
        return;
    _panel_render_if_changed();
    const int w = _panel_rect.w, h = _panel_rect.h, nbuf = _panel_nbuf;
    const int shown = _panel_shown_buffer();
    for (int buf = 0; buf < nbuf; buf++) {
        if (!_panel_save[buf])
            continue;
        bool over = false;
        if (_panel_on[buf] && (!_panel_redrawn[buf] || buf != shown)) {
            over = _panel_overdrawn(buf);
            if (over)
                _panel_redrawn[buf] = true;
        }
        if (_panel_on[buf] && !over && buf != shown)
            continue;
        for (int y = 0; y < h; y++) {
            int inset = osd_panel_row_inset(w, h, y);
            if (w - 2 * inset <= 0)
                continue;
            memcpy(_panel_fb_row(buf, y) + inset, _panel_rot + (size_t)y * w + inset,
                   (size_t)(w - 2 * inset) * sizeof(uint32_t));
        }
        _panel_on[buf] = true;
        // Drawn again (a key came during _panel_settle): if it is hidden
        // now, this buffer needs its picture back.
        _panel_covered[buf] = false;
    }
}

static void _panel_free(void)
{
    free(_panel_px);
    free(_panel_rot);
    _panel_px = _panel_rot = NULL;
    for (int b = 0; b < OSD_PANEL_MAX_BUFFERS; b++) {
        free(_panel_save[b]);
        _panel_save[b] = NULL;
        _panel_on[b] = false;
        _panel_redrawn[b] = false;
        _panel_covered[b] = false;
    }
    _panel_drawn_kind = -1;
}

static void _panel_saveBehind(void)
{
    if (!_panel_take_screen())
        return;
    _panel_rect = osd_panel_geometry(_panel_W, _panel_H, _osd_panel_top);
    const size_t n = (size_t)_panel_rect.w * _panel_rect.h;
    const int nbuf = _panel_nbuf;
    _panel_px = malloc(n * sizeof(uint32_t));
    _panel_rot = malloc(n * sizeof(uint32_t));
    bool ok = n > 0 && _panel_px && _panel_rot;
    for (int b = 0; b < nbuf && ok; b++)
        ok = (_panel_save[b] = malloc(n * sizeof(uint32_t))) != NULL;
    if (!ok) {
        _panel_free();
        return;
    }
    _panel_drawn_kind = -1;
    for (int b = 0; b < nbuf; b++) {
        _panel_on[b] = false;
        _panel_redrawn[b] = false;
        for (int y = 0; y < _panel_rect.h; y++)
            memcpy(_panel_save[b] + (size_t)y * _panel_rect.w, _panel_fb_row(b, y),
                   (size_t)_panel_rect.w * sizeof(uint32_t));
    }
}

static void _panel_put_back(int buf)
{
    const int w = _panel_rect.w, h = _panel_rect.h;
    // Only where the panel was: the corners were never drawn on.
    for (int y = 0; y < h; y++) {
        int inset = osd_panel_row_inset(w, h, y);
        if (w - 2 * inset <= 0)
            continue;
        memcpy(_panel_fb_row(buf, y) + inset, _panel_save[buf] + (size_t)y * w + inset,
               (size_t)(w - 2 * inset) * sizeof(uint32_t));
    }
}

// When the panel goes, where the program never drew over it (a still menu,
// the border around a Game Boy game) what was behind it goes back. Where it
// did (a game, a menu with moving titles or a video), what was saved may be
// old: _panel_settle() first waits a little for the program to draw there
// again by itself, which covers the panel with the right picture, and only
// the buffers where it doesn't get the saved picture back.
#define OSD_PANEL_SETTLE_MS 100

// Returns false if a key brought the panel back meanwhile (keep drawing).
static bool _panel_settle(void)
{
    if (!_panel_px)
        return true;
    bool any = false;
    for (int b = 0; b < _panel_nbuf; b++) {
        _panel_covered[b] = false;
        any = any || (_panel_save[b] && _panel_redrawn[b]);
    }
    for (int t = 0; any && t < OSD_PANEL_SETTLE_MS; t += 5) {
        usleep(5000);
        if (getMilliseconds() - _bar_timer < 2000)
            return false;
        any = false;
        for (int b = 0; b < _panel_nbuf; b++) {
            if (!_panel_save[b] || !_panel_redrawn[b] || _panel_covered[b])
                continue;
            if (_panel_overdrawn(b))
                _panel_covered[b] = true;
            else
                any = true;
        }
    }
    return true;
}

static void _panel_restoreBehind(void)
{
    if (_panel_px)
        for (int b = 0; b < _panel_nbuf; b++)
            if (_panel_save[b] && !_panel_covered[b])
                _panel_put_back(b);
    _panel_free();
}
#endif

void _print_bar(void)
{
#ifdef PLATFORM_MIYOOMINI
    if (_osd_panel) {
        _print_panel();
        return;
    }
    uint32_t *ofs = g_display.fb_addr;
    const uint32_t height = g_display.height;
    // Snapshot volatile values into locals for consistent use within this frame
    const int bar_max = _bar_max;
    const int bar_value = _bar_value;
    const uint32_t percentage = bar_max > 0 ? (uint32_t)bar_value * height / (uint32_t)bar_max : 0;
    const uint32_t active_color = _bar_color;

    ofs += g_display.width - meterWidth;
    // Triple-buffer: write same pattern to 3 consecutive framebuffers
    for (uint32_t buf = 0; buf < 3; buf++) {
        uint32_t *row = ofs + buf * height * g_display.width;
        for (uint32_t i = 0; i < height; i++, row += g_display.width) {
            const uint32_t curr = i < percentage ? active_color : 0;
            for (uint32_t j = 0; j < (uint32_t)meterWidth; j++)
                row[j] = curr;
        }
    }
#endif
}

void _bar_restoreBufferBehind(void)
{
#ifdef PLATFORM_MIYOOMINI
    if (_osd_panel) {
        _panel_restoreBehind();
        return;
    }
    _bar_value = 0;
    _bar_max = 0;
    _bar_color = 0;
    _print_bar();
    if (_bar_savebuf) {
        uint32_t *ofs = g_display.fb_addr + g_display.width - meterWidth;
        const uint32_t *ofss = _bar_savebuf;
        const size_t row_bytes = meterWidth * sizeof(uint32_t);
        for (uint32_t i = 0; i < g_display.height; i++, ofs += g_display.width, ofss += meterWidth) {
            memcpy(ofs, ofss, row_bytes);
        }
        free(_bar_savebuf);
        _bar_savebuf = NULL;
    }
#endif
}

void _bar_saveBufferBehind(void)
{
#ifdef PLATFORM_MIYOOMINI
    if (_osd_panel) {
        _panel_saveBehind();
        return;
    }
    // Compact buffer: only meterWidth * height pixels (was width * height — 160x smaller!)
    if ((_bar_savebuf = (uint32_t *)malloc(meterWidth * g_display.height *
                                           sizeof(uint32_t)))) {
        const uint32_t *ofs = g_display.fb_addr + g_display.width - meterWidth;
        uint32_t *ofss = _bar_savebuf;
        const size_t row_bytes = meterWidth * sizeof(uint32_t);
        for (uint32_t i = 0; i < g_display.height; i++, ofs += g_display.width, ofss += meterWidth) {
            memcpy(ofss, ofs, row_bytes);
        }
    }
#endif
}

//
//    OSD draw thread
//
// The panel waits a little for the program to cover it (see
// _panel_settle); false if a key brought it back meanwhile.
static bool _osd_settled(void)
{
#ifdef PLATFORM_MIYOOMINI
    return !_osd_panel || _panel_settle();
#else
    return true;
#endif
}

static void *_osd_thread(void *_)
{
    do {
        while (getMilliseconds() - _bar_timer < 2000) {
            _print_bar();
            // The game redraws the framebuffer every frame and the bar is
            // drawn over it, unsynchronised with the game's page flips.
            // Redrawing once per frame (16 ms) left it missing from many
            // frames, so it flickered; Onion's 100 us keeps it on screen. The
            // loop only runs for the 2 s after a volume or brightness key.
            usleep(100);
        }
    } while (!_osd_settled());
    _bar_restoreBufferBehind();
    osd_thread_active = false;
    return 0;
}

/**
 * @brief Show OSD percentage bar
 * 
 * @param value 
 * @param value_max 
 * @param color 
 */
static void _osd_showIndicator(int kind, int value, int value_max, uint32_t color, bool mute)
{
    _bar_timer = getMilliseconds();
    _bar_kind = kind;
    _bar_value = value;
    _bar_max = value_max;
    _bar_color = color;
    _bar_mute = mute;
    osd_bar_activated = true;

    if (osd_thread_active)
        return;

    // Read meterWidth and the look when the bar appears, not on every step:
    // a cached value would go stale on theme change, but it must not change
    // while the bar is on screen either, since the saved strip behind the
    // bar (_bar_savebuf, or the panel's) is that size and is restored with it.
    config_get("display/meterWidth", CONFIG_INT, &meterWidth);
    if (meterWidth <= 0) {
        osd_bar_activated = false;
        return;
    }
    _osd_panel = !config_flag_get(OSD_EDGE_BAR_FLAG);
    _osd_panel_top = config_flag_get(OSD_PANEL_TOP_FLAG);

    // The previous bar's thread ends on its own after 2 s and nobody joined
    // it: every bar leaked a thread (stack and descriptor) in keymon until
    // pthread_create failed and the bar never showed again. Join it here.
    if (osd_thread_joinable) {
        pthread_join(osd_pt, NULL);
        osd_thread_joinable = false;
    }

    _bar_saveBufferBehind();
    if (pthread_create(&osd_pt, NULL, _osd_thread, _print_bar) != 0) {
        _bar_restoreBufferBehind();
        return;
    }
    osd_thread_joinable = true;
    osd_thread_active = true;
}

void osd_hideBar(void)
{
    osd_bar_activated = false;
    if (!osd_thread_active)
        return;
    pthread_cancel(osd_pt);
    pthread_join(osd_pt, NULL);
    osd_thread_joinable = false;
    _bar_restoreBufferBehind();
    osd_thread_active = false;
}

void osd_showBar(int value, int value_max, uint32_t color)
{
    _osd_showIndicator(OSD_PANEL_BRIGHTNESS, value, value_max, color, false);
}

void osd_showVolumeBar(int volume, bool mute)
{
    _osd_showIndicator(OSD_PANEL_VOLUME, volume, 20, mute ? OSD_MUTE_ON_COLOR : OSD_VOLUME_COLOR, mute);
}

void osd_showBrightnessBar(int brightness)
{
    _osd_showIndicator(OSD_PANEL_BRIGHTNESS, brightness, 10, OSD_BRIGHTNESS_COLOR, false);
}

#endif // SYSTEM_OSD_H__
