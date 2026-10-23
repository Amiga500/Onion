#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <png/png.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "./display.h"
#include "./screenshot_path.h"
#include "./state.h"
#include "utils/file.h"
#include "utils/hash.h"
#include "utils/log.h"
#include "utils/neon_pixel.h"
#include "utils/process.h"
#include "utils/str.h"

#define SCREENSHOT_PATH_MAX 512

bool __get_path_recent(char *path_out)
{
    char name[STR_MAX] = "";
    char *no_extension;

    system_state_update();

    if (system_state == MODE_GAME && (process_searchpid("retroarch") != 0 || process_searchpid("ra32") != 0)) {
        char file_path[STR_MAX];
        if (history_getRecentPath(file_path) != NULL) {
            no_extension = file_removeExtension(basename(file_path));
            if (no_extension != NULL) {
                strncpy(name, no_extension, sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0';
                free(no_extension);
            }
        }
    }
    else if (system_state == MODE_SWITCHER)
        strncpy(name, "GameSwitcher", sizeof(name) - 1);
    else if (system_state == MODE_MAIN_UI)
        strncpy(name, "MainUI", sizeof(name) - 1);
    else if ((system_state == MODE_GAME || system_state == MODE_APPS) && exists(CMD_TO_RUN_PATH)) {
        char cmd[PATH_MAX + STR_MAX];
        if (!file_read_line(CMD_TO_RUN_PATH, cmd, sizeof(cmd)))
            return screenshot_numberedPath(path_out, SCREENSHOT_PATH_MAX, SCREENSHOTS_DIR, name);
        printf_debug("cmd: '%s'\n", cmd);

        char app_name[STR_MAX] = "";

        if (strstr(cmd, "; chmod") != NULL)
            state_getAppName(app_name, cmd);
        else {
            no_extension = file_removeExtension(basename(cmd));
            if (no_extension != NULL) {
                strncpy(app_name, no_extension, sizeof(app_name) - 1);
                app_name[sizeof(app_name) - 1] = '\0';
                free(no_extension);
            }
        }
        printf_debug("app: '%s'\n", app_name);
        strncpy(name, app_name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    return screenshot_numberedPath(path_out, SCREENSHOT_PATH_MAX, SCREENSHOTS_DIR, name);
}

uint32_t *__screenshot_buffer(void)
{
    display_getRenderResolution();
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_display.vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &g_display.finfo);

    int width = g_display.width;
    int height = g_display.height;
    if (width <= 0 || height <= 0)
        return NULL;

    int fb_stride_pixels = (int)(g_display.finfo.line_length / sizeof(uint32_t));
    if (fb_stride_pixels < width)
        fb_stride_pixels = width;

    size_t buffer_size = (size_t)width * (size_t)height * sizeof(uint32_t);
    uint32_t *buffer = (uint32_t *)malloc(buffer_size);
    if (buffer == NULL)
        return NULL;

    const uint32_t *fb = g_display.fb_addr + (long)g_display.vinfo.yoffset * fb_stride_pixels;
    for (int y = 0; y < height; y++) {
        memcpy(buffer + (size_t)y * (size_t)width,
               fb + (size_t)y * (size_t)fb_stride_pixels,
               (size_t)width * sizeof(uint32_t));
    }

    return buffer;
}

/**
 * @brief Screenshot (640x480x32bpp only, rotate180, png)
 * 
 * @param buffer pointer to the frame buffer
 * @param screenshot_path image file save path
 * @return true Screenshot was saved
 * @return false Screenshot was not saved
 */
bool screenshot_save_stride(const uint32_t *buffer, const char *screenshot_path,
                            bool do_rotate180, int src_stride_pixels)
{
    // make sure render resolution is up to date
    display_getRenderResolution();

    // Guard against zero dimensions — the VLA below would be UB otherwise
    if (buffer == NULL || g_display.width == 0 || g_display.height == 0)
        return false;

    int row_stride = (src_stride_pixels > 0) ? src_stride_pixels : g_display.width;

    uint32_t line_buffer[g_display.width], x, y;

    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    if (!(fp = file_open_ensure_path(screenshot_path, "wb"))) {
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (!png_ptr) {
        fclose(fp);
        return false;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, g_display.width, g_display.height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    if (do_rotate180) {
        /* Bottom-to-top + reverse each row, then NEON ARGB→RGBA */
        for (y = 0; y < g_display.height; y++) {
            const uint32_t *row_start =
                (const uint32_t *)((const uint8_t *)buffer + (size_t)(g_display.height - 1 - y) * (size_t)row_stride * sizeof(uint32_t));
            for (x = 0; x < g_display.width; x++) {
                line_buffer[x] = row_start[g_display.width - 1 - x];
            }
            neon_swap_rb_inplace(line_buffer, g_display.width);
            /* Stock Onion forced opaque alpha on screenshots */
            for (x = 0; x < g_display.width; x++) {
                line_buffer[x] |= 0xFF000000;
            }
            png_write_row(png_ptr, (png_bytep)line_buffer);
        }
    }
    else {
        for (y = 0; y < g_display.height; y++) {
            const uint32_t *row =
                (const uint32_t *)((const uint8_t *)buffer + (size_t)y * (size_t)row_stride * sizeof(uint32_t));
            neon_argb_to_rgba(line_buffer, row, g_display.width);
            for (x = 0; x < g_display.width; x++) {
                line_buffer[x] |= 0xFF000000;
            }
            png_write_row(png_ptr, (png_bytep)line_buffer);
        }
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    return true;
}

bool screenshot_save(const uint32_t *buffer, const char *screenshot_path, bool do_rotate180)
{
    return screenshot_save_stride(buffer, screenshot_path, do_rotate180, 0);
}

/**
 * @brief Save a tightly packed ARGB8888 buffer (width * height pixels) as PNG,
 *        atomically: the PNG is written to "<path>.tmp", fsynced, then renamed
 *        over <path> and the directory fsynced. An interrupted write never
 *        leaves a truncated PNG at <path>.
 *
 * Safe to call from a worker thread: unlike screenshot_save_stride() it does
 * not read or update g_display, so the caller's buffer size and the encoded
 * size can never disagree.
 */
bool screenshot_save_buffer_atomic(const uint32_t *buffer, const char *path,
                                   uint32_t width, uint32_t height)
{
    if (buffer == NULL || path == NULL || width == 0 || height == 0)
        return false;

    char tmp_path[PATH_MAX];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path))
        return false;

    uint32_t *line_buffer = (uint32_t *)malloc((size_t)width * sizeof(uint32_t));
    if (line_buffer == NULL)
        return false;

    FILE *fp = file_open_ensure_path(tmp_path, "wb");
    if (fp == NULL) {
        free(line_buffer);
        return false;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_ptr = png_ptr ? png_create_info_struct(png_ptr) : NULL;
    if (png_ptr == NULL || info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
        // libpng error (e.g. SD write failure): drop the temp file and keep
        // whatever PNG was there before.
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        remove(tmp_path);
        free(line_buffer);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    for (uint32_t y = 0; y < height; y++) {
        neon_argb_to_rgba(line_buffer, buffer + (size_t)y * width, width);
        for (uint32_t x = 0; x < width; x++)
            line_buffer[x] |= 0xFF000000; // stock Onion forced opaque alpha
        png_write_row(png_ptr, (png_bytep)line_buffer);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(line_buffer);

    // fflush + fsync + rename + directory fsync; removes tmp on failure.
    return file_atomic_commit(fp, tmp_path, path);
}

bool __screenshot_perform(bool(get_path)(char *), pid_t p_id)
{
    bool retval = false;
    char path[SCREENSHOT_PATH_MAX];
    uint32_t *buffer;

    if (p_id != 0) {
        kill(p_id, SIGSTOP);
    }

    buffer = __screenshot_buffer();

    if (p_id != 0) {
        kill(p_id, SIGCONT);
    }

    if (buffer != NULL && get_path(path)) {
        retval = screenshot_save(buffer, path, true);
    }

    free(buffer);
    return retval;
}

pid_t get_game_pid(void)
{
    pid_t p_id = process_searchpid("retroarch");
    if (p_id == 0) {
        p_id = process_searchpid("drastic");
    }
    return p_id;
}

bool screenshot_recent(void)
{
    return __screenshot_perform(__get_path_recent, get_game_pid());
}

bool screenshot_system(void)
{
    pid_t p_id = get_game_pid();
    if (p_id != 0) {
        return __screenshot_perform(history_getRomscreenPath, p_id);
    }
    return false;
}

#endif // SCREENSHOT_H__
