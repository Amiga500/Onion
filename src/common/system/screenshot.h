#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <png/png.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>

#include "./display.h"
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
    uint32_t i;

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
        FILE *fp;
        char cmd[STR_MAX];
        file_get(fp, CMD_TO_RUN_PATH, "%[^\n]", cmd);
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

    if (name[0] == '\0')
        strncpy(name, "Screenshot", sizeof(name) - 1);

    for (i = 0; i < 1000; i++) {
        int n = snprintf(path_out, SCREENSHOT_PATH_MAX,
                         "/mnt/SDCARD/Screenshots/%s_%03d.png", name, i);
        if (n < 0 || (size_t)n >= SCREENSHOT_PATH_MAX)
            return false;
        if (!exists(path_out))
            break;
    }

    return i <= 999;
}

uint32_t *__screenshot_buffer(void)
{
    size_t buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint32_t);
    uint32_t *buffer = (uint32_t *)malloc(buffer_size);
    if (buffer == NULL)
        return NULL;

    ioctl(fb_fd, FBIOGET_VSCREENINFO, &g_display.vinfo);
    memcpy(buffer, g_display.fb_addr + DISPLAY_WIDTH * g_display.vinfo.yoffset, buffer_size);

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
bool screenshot_save(const uint32_t *buffer, const char *screenshot_path, bool do_rotate180)
{
    uint32_t *src;
    uint32_t line_buffer[g_display.width], x, y;

    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    // make sure render resolution is up to date
    display_getRenderResolution();

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

    src = (uint32_t *)buffer;
    if (do_rotate180) {
        /* Bottom-to-top + reverse each row, then NEON ARGB→RGBA */
        const uint32_t *end = buffer + g_display.width * g_display.height;
        for (y = 0; y < g_display.height; y++) {
            const uint32_t *row_start = end - (y + 1) * g_display.width;
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
            neon_argb_to_rgba(line_buffer, src, g_display.width);
            for (x = 0; x < g_display.width; x++) {
                line_buffer[x] |= 0xFF000000;
            }
            src += g_display.width;
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
