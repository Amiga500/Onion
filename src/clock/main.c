/*
Copyright (c) 2020 Gameblabla

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* Clock (RedOnion, new UI): one box per value - day, month, year, hour,
 * minute - left/right to move, up/down to change, START saves, B or MENU
 * leaves without saving. Seconds are set to 00 when saving. Drawn with the
 * shared canvas look (theme/render/canvas_ui.h), like SD Card Doctor. */

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "system/battery.h"
#include "system/keymap_sw.h"
#include "theme/render/canvas_ui.h"

#define MAX_YEAR 2100
#define MIN_YEAR 1970
#define DEFAULT_YEAR 2022

enum { F_DAY, F_MONTH, F_YEAR, F_HOUR, F_MIN, F_COUNT };

static int value[F_COUNT];
static int cursor = F_MIN;
static bool changed = false;

static int days_in_month(int month, int year)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : days[month - 1];
}

/* Wraps the selected value around its range; the day follows the month. */
static void step(int field, int delta)
{
    int lo[] = {1, 1, MIN_YEAR, 0, 0};
    int hi[] = {days_in_month(value[F_MONTH], value[F_YEAR]), 12, MAX_YEAR, 23, 59};
    int v = value[field] + delta;
    if (v > hi[field])
        v = lo[field];
    if (v < lo[field])
        v = hi[field];
    value[field] = v;
    int max_day = days_in_month(value[F_MONTH], value[F_YEAR]);
    if (value[F_DAY] > max_day)
        value[F_DAY] = max_day;
    changed = true;
}

static void triangle(CuiCanvas *c, float cx, float cy, bool up, uint32_t color, float alpha)
{
    for (int row = 0; row < 6; row++) {
        float half = (up ? row : 5 - row) * 0.9f + 0.4f;
        for (int x = (int)(cx - half + 0.5f); x <= (int)(cx + half); x++)
            cui_blend(c, x, (int)cy - 3 + row, color, alpha);
    }
}

static void triangle_side(CuiCanvas *c, float cx, float cy, bool right, uint32_t color)
{
    for (int col = 0; col < 6; col++) {
        float half = (right ? 5 - col : col) * 0.9f + 0.4f;
        for (int y = (int)(cy - half + 0.5f); y <= (int)(cy + half); y++)
            cui_blend(c, (int)cx - 3 + col, y, color, 1);
    }
}

/* A d-pad hint: a grey key with two small arrows, then its word. */
static int dpad_hint(CuiCanvas *c, int x, float cy, bool vertical, const char *label)
{
    cui_rrect(c, x, cy - 12, 36, 24, 6, CUI_TRACK);
    if (vertical) {
        triangle(c, x + 11, cy, true, CUI_WHITE, 1);
        triangle(c, x + 25, cy, false, CUI_WHITE, 1);
    }
    else {
        triangle_side(c, x + 11, cy, false, CUI_WHITE);
        triangle_side(c, x + 25, cy, true, CUI_WHITE);
    }
    int lw = cui_text(c, x + 43, cy, label, 20, CUI_WHITE, CUI_LEFT);
    return 36 + 7 + lw + 20;
}

static void draw(CuiCanvas *c, int battery)
{
    static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static const char *labels[] = {"DAY", "MONTH", "YEAR", "HOUR", "MIN"};
    static const int widths[] = {88, 104, 124, 88, 88};
    cui_rect(c, 0, 0, CUI_W, CUI_H, CUI_BG);
    cui_header(c, "clock", "Clock", changed ? "Not saved" : NULL, battery);

    int gap = 12, total = 0;
    for (int i = 0; i < F_COUNT; i++)
        total += widths[i] + (i ? gap : 0);
    int x = (CUI_W - total) / 2, top = 150;
    for (int i = 0; i < F_COUNT; i++) {
        int w = widths[i], cx = x + w / 2;
        bool sel = i == cursor;
        cui_text(c, cx, top - 20, labels[i], 13, CUI_GREY, CUI_CENTER);
        cui_rrect(c, x, top, w, 120, 14, sel ? CUI_ACCENT : CUI_PANEL);
        char text[8];
        if (i == F_MONTH)
            snprintf(text, sizeof text, "%s", months[value[i] - 1]);
        else if (i == F_YEAR)
            snprintf(text, sizeof text, "%04d", value[i]);
        else
            snprintf(text, sizeof text, "%02d", value[i]);
        cui_text(c, cx, top + 60, text, 40, CUI_WHITE, CUI_CENTER);
        triangle(c, cx, top + 18, true, CUI_WHITE, sel ? 0.9f : 0.45f);
        triangle(c, cx, top + 102, false, CUI_WHITE, sel ? 0.9f : 0.45f);
        x += w + gap;
    }
    cui_text(c, CUI_W / 2, 318, "The time shown stays still while you edit it.", 17, CUI_GREY, CUI_CENTER);
    cui_text(c, CUI_W / 2, 344, "Saving sets it, with the seconds at 00.", 17, CUI_GREY, CUI_CENTER);

    cui_rect(c, 0, 420, CUI_W, 60, CUI_HEAD);
    int hx = 16;
    hx += cui_hint(c, hx, 450, "START", "SAVE", 20);
    hx += dpad_hint(c, hx, 450, false, "MOVE");
    hx += dpad_hint(c, hx, 450, true, "CHANGE");
    cui_hint(c, hx, 450, "B", "CANCEL", 20);
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();
    SDL_Surface *video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    if (!video || !screen) {
        printf("Can't set video mode\n");
        return 0;
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    value[F_DAY] = tm.tm_mday;
    value[F_MONTH] = tm.tm_mon + 1;
    value[F_YEAR] = tm.tm_year + 1900;
    value[F_HOUR] = tm.tm_hour;
    value[F_MIN] = tm.tm_min;
    if (value[F_YEAR] <= MIN_YEAR)
        value[F_YEAR] = DEFAULT_YEAR;

    SDL_EnableKeyRepeat(500, 50);
    bool quit = false, save = false, dirty = true;
    int battery = battery_getPercentage();
    Uint32 battery_at = SDL_GetTicks();
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
                break;
            }
            if (event.type != SDL_KEYDOWN)
                continue;
            switch (event.key.keysym.sym) {
            case SW_BTN_UP:
                step(cursor, 1);
                dirty = true;
                break;
            case SW_BTN_DOWN:
                step(cursor, -1);
                dirty = true;
                break;
            case SW_BTN_LEFT:
                if (cursor > 0)
                    cursor--;
                dirty = true;
                break;
            case SW_BTN_RIGHT:
                if (cursor < F_COUNT - 1)
                    cursor++;
                dirty = true;
                break;
            case SW_BTN_START:
                save = quit = true;
                break;
            case SW_BTN_B:
            case SW_BTN_MENU:
                quit = true;
                break;
            default: /* nothing to draw again; a redraw already asked for
                      * (first frame, an earlier key) stays asked */
                break;
            }
        }
        if (SDL_GetTicks() - battery_at > 15000) {
            int b = battery_getPercentage();
            dirty |= b != battery;
            battery = b;
            battery_at = SDL_GetTicks();
        }
        if (dirty && !quit) {
            CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
            draw(&c, battery);
            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);
            dirty = false;
        }
        SDL_Delay(16);
    }

    SDL_FreeSurface(screen);
    TTF_Quit();
    SDL_Quit();

    if (save) {
        char cmd[128];
        snprintf(cmd, sizeof cmd, "date -s '%d-%d-%d %d:%d:00';hwclock --utc -w",
                 value[F_YEAR], value[F_MONTH], value[F_DAY], value[F_HOUR], value[F_MIN]);
        execlp("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
    }
    return 0;
}
