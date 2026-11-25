#ifndef PACMAN_RENDER_H__
#define PACMAN_RENDER_H__

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <math.h>

#include "system/keymap_sw.h"
#include "theme/render/canvas_ui.h"
#include "utils/keystate.h"
#include "utils/surfaceSetAlpha.h"

#include "./globals.h"
#include "./summary.h"

/* New UI: drawn in code with the shared canvas look (header with the cube
 * icon and a changes chip, tabs switched with L1/R1, pill selection, drawn
 * switches, amber dot on changed items, counts above a footer of round
 * buttons). Rows: 7 of 44 px from y=100; the summary scrolls in 300 px. */

#define PM_ROW_Y 100
#define PM_ROW_H 44
#define PM_ROWS 7
#define PM_AREA_H (PM_ROWS * PM_ROW_H)

static CuiCanvas pm_canvas(void)
{
    return (CuiCanvas){(uint32_t *)screen->pixels, screen->pitch / 4};
}

static void pm_switch(CuiCanvas *c, int right, float cy, bool on, bool selected)
{
    int w = 40, h = 22, x = right - w;
    uint32_t track = on ? (selected ? CUI_WHITE : CUI_ACCENT) : (selected ? cui_mix(CUI_ACCENT, CUI_WHITE, 0.3f) : CUI_TRACK);
    uint32_t knob = on ? (selected ? CUI_ACCENT : CUI_WHITE) : CUI_GREY;
    cui_rrect(c, x, cy - h / 2.0f, w, h, h / 2.0f, track);
    cui_disc(c, on ? x + w - h / 2.0f : x + h / 2.0f, cy, 8, knob);
}

void displayLayers(CuiCanvas *c)
{
    for (int i = 0; i < PM_ROWS; i++) {
        int pos = i + nListPosition;
        if (pos >= package_count[nTab])
            break;
        Package *package = &packages[nTab][pos];
        float cy = PM_ROW_Y + i * PM_ROW_H + PM_ROW_H / 2.0f;
        bool sel = i == nSelection;
        if (sel)
            cui_rrect(c, 10, cy - PM_ROW_H / 2.0f + 2, 600, PM_ROW_H - 4, 10, CUI_ACCENT);
        if (package->changed)
            cui_disc(c, 22, cy, 4, sel ? CUI_WHITE : CUI_WARN);

        /* name, then the part in parentheses in grey */
        char name[STR_MAX], parens[STR_MAX] = "";
        snprintf(name, sizeof(name), "%s", package->name);
        char *open = strchr(name, '(');
        if (open) {
            snprintf(parens, sizeof(parens), "%s", open);
            *open = '\0';
        }
        if (package->installed && !package->complete)
            strncat(parens, "*", sizeof(parens) - strlen(parens) - 1);
        int x = 34;
        char fitted[STR_MAX];
        cui_fit(name, 20, 470, fitted, sizeof(fitted));
        x += cui_text(c, x, cy, fitted, 20, CUI_WHITE, CUI_LEFT);
        if (parens[0] && x < 500) {
            cui_fit(parens, 20, 510 - x, fitted, sizeof(fitted));
            cui_text(c, x + 4, cy, fitted, 20, sel ? cui_mix(CUI_ACCENT, CUI_WHITE, 0.7f) : CUI_GREY, CUI_LEFT);
        }
        if (package->has_roms && surfaceIconHasRoms) {
            SDL_Rect r = {540 - surfaceIconHasRoms->w / 2, (Sint16)(cy - surfaceIconHasRoms->h / 2)};
            SDL_BlitSurface(surfaceIconHasRoms, NULL, screen, &r);
        }
        pm_switch(c, 600, cy, package->installed != package->changed, sel);
    }
}

void showScroller(CuiCanvas *c)
{
    int top = PM_ROW_Y + 4, height = PM_AREA_H - 8, x = 620;
    int thumb_h = height, thumb_y = top;

    if (nTab == summary_tab) {
        if (surfaceSummary == NULL || surfaceSummary->h <= PM_AREA_H)
            return;
        thumb_h = round((float)height * PM_AREA_H / (float)surfaceSummary->h);
        float scroll = (float)summaryScrollY / (float)(surfaceSummary->h - PM_AREA_H);
        thumb_y += round(scroll * (float)(height - thumb_h));
    }
    else if (package_count[nTab] > PM_ROWS) {
        thumb_h = round((float)height * PM_ROWS / (float)package_count[nTab]);
        if (thumb_h < 12)
            thumb_h = 12;
        float scroll = (float)nListPosition / (float)(package_count[nTab] - PM_ROWS);
        thumb_y += round(scroll * (float)(height - thumb_h));
    }
    else
        return;

    cui_rrect(c, x, top, 4, height, 2, CUI_TRACK);
    if (nTab != summary_tab)
        for (int i = 0; i < package_count[nTab]; i++)
            if (packages[nTab][i].changed) {
                int y = top + round((i + 0.5f) * height / (float)package_count[nTab]);
                cui_rect(c, x - 2, y - 1, 8, 3, CUI_WARN);
            }
    cui_rrect(c, x, thumb_y, 4, thumb_h, 2, CUI_GREY);
}

/* The question asked when leaving without any change: a box in the middle,
 * B (stay) already chosen, A leaves. */
bool confirmDoNothing(KeyState *keystate)
{
    bool quit = false, confirm = false;
    CuiCanvas c = pm_canvas();
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            c.px[y * c.pitch + x] = cui_mix(c.px[y * c.pitch + x], 0, 0.6f);
    int bx = 70, by = 150, bw = 500, bh = 170;
    cui_rrect(&c, bx, by, bw, bh, 16, CUI_PANEL);
    cui_text(&c, bx + 24, by + 38, "LEAVE WITHOUT CHANGES?", 24, CUI_WHITE, CUI_LEFT);
    cui_text(&c, bx + 24, by + 74, "Nothing will be installed or removed.", 17, CUI_GREY, CUI_LEFT);
    int btn_w = (bw - 48 - 12) / 2, btn_y = by + 100;
    cui_rrect(&c, bx + 24, btn_y, btn_w, 46, 10, CUI_WHITE);
    cui_rrect(&c, bx + 27, btn_y + 3, btn_w - 6, 40, 8, CUI_TRACK);
    cui_hint(&c, bx + 24 + btn_w / 2 - 50, btn_y + 23, "B", "STAY", 20);
    int ax = bx + 24 + btn_w + 12;
    cui_rrect(&c, ax, btn_y, btn_w, 46, 10, CUI_BAD);
    cui_rrect(&c, ax + 2, btn_y + 2, btn_w - 4, 42, 9, CUI_PANEL);
    cui_disc(&c, ax + btn_w / 2 - 38, btn_y + 23, 14, CUI_ACCENT);
    cui_text(&c, ax + btn_w / 2 - 38, btn_y + 23, "A", 18, CUI_WHITE, CUI_CENTER);
    cui_text(&c, ax + btn_w / 2 - 17, btn_y + 23, "LEAVE", 20, CUI_BAD, CUI_LEFT);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);

    while (!quit) {
        if (updateKeystate(keystate, &quit, true, NULL)) {
            if (keystate[SW_BTN_A] == PRESSED)
                confirm = true;
            else if (keystate[SW_BTN_A] == RELEASED && confirm)
                quit = true;
            else if (keystate[SW_BTN_B] == PRESSED)
                quit = true;
        }
        SDL_Delay(15); // wait for input without spinning
    }
    return confirm;
}

/* Tabs under the header: every name, the current one on a pill, an
 * asterisk on those with changes, L1 and R1 at both ends. */
void renderTabs(CuiCanvas *c)
{
    cui_rect(c, 0, 60, CUI_W, 38, CUI_RGB(34, 34, 36));
    int y = 79;
    cui_rrect(c, 12, y - 10, 24, 20, 5, CUI_TRACK);
    cui_text(c, 24, y, "L1", 12, CUI_GREY, CUI_CENTER);
    cui_rrect(c, 604, y - 10, 24, 20, 5, CUI_TRACK);
    cui_text(c, 616, y, "R1", 12, CUI_GREY, CUI_CENTER);

    char names[4][STR_MAX];
    int widths[4], total = 0;
    for (int i = 0; i < tab_count; i++) {
        bool changes = changes_installs[i] > 0 || changes_removals[i] > 0;
        snprintf(names[i], STR_MAX, changes && i != summary_tab ? "%s*" : "%s", layer_names[i]);
        widths[i] = cui_text_width(names[i], 16) + 24;
        total += widths[i];
    }
    int x = 48;
    for (int i = 0; i < tab_count; i++) {
        bool active = i == nTab;
        if (active)
            cui_rrect(c, x, y - 13, widths[i], 26, 8, CUI_TRACK);
        cui_text(c, x + widths[i] / 2, y, names[i], 16, active ? CUI_WHITE : CUI_GREY, CUI_CENTER);
        x += widths[i] + 4;
    }
}

/* "30 added · 0 removed · 0 installed · 56 total", added in green and
 * removed in red when they are not 0. */
static void renderCounts(CuiCanvas *c)
{
    char parts[4][48];
    snprintf(parts[0], 48, "%d added", changes_installs[nTab]);
    snprintf(parts[1], 48, "%d removed", changes_removals[nTab]);
    snprintf(parts[2], 48, "%d installed", package_installed_count[nTab]);
    snprintf(parts[3], 48, "%d total", package_count[nTab]);
    uint32_t colors[4] = {changes_installs[nTab] ? CUI_GOOD : CUI_GREY,
                          changes_removals[nTab] ? CUI_BAD : CUI_GREY, CUI_GREY, CUI_GREY};
    const char *dot = "  \xc2\xb7  ";
    int total = 0;
    for (int i = 0; i < 4; i++)
        total += cui_text_width(parts[i], 15) + (i ? cui_text_width(dot, 15) : 0);
    int x = 320 - total / 2;
    for (int i = 0; i < 4; i++) {
        if (i)
            x += cui_text(c, x, 419, dot, 15, CUI_DIM, CUI_LEFT);
        x += cui_text(c, x, 419, parts[i], 15, colors[i], CUI_LEFT);
    }
}

void renderApplication(void)
{
    CuiCanvas c = pm_canvas();
    cui_rect(&c, 0, 0, CUI_W, CUI_H, CUI_BG);

    int changes_total = changesTotal();
    char chip[32] = "";
    if (changes_total > 0)
        snprintf(chip, sizeof(chip), changes_total == 1 ? "%d change" : "%d changes", changes_total);
    cui_header(&c, "cube", "Package Manager", chip[0] ? chip : NULL, -1);
    renderTabs(&c);

    cui_rect(&c, 0, 432, CUI_W, 48, CUI_HEAD);
    int hx = 16, right = 624;
    if (nTab == summary_tab) {
        if (changes_total > 0) {
            renderSummary();
            showScroller(&c);
            hx += cui_hint(&c, hx, 456, "A", "APPLY", 20);
        }
        else {
            cui_text(&c, 320, 250, "NO CHANGES", 30, CUI_WHITE, CUI_CENTER);
            cui_text(&c, 320, 285, "Turn packages on or off in the other tabs.", 17, CUI_GREY, CUI_CENTER);
            hx += cui_hint(&c, hx, 456, "A", "EXIT", 20);
        }
        cui_hint(&c, hx, 456, "B", "EXIT", 20);
    }
    else {
        if (package_count[nTab] > 0) {
            displayLayers(&c);
            showScroller(&c);
        }
        else
            cui_text(&c, 320, 250, "No packages here", 22, CUI_GREY, CUI_CENTER);
        renderCounts(&c);
        hx += cui_hint(&c, hx, 456, "A", "ON/OFF", 20);
        hx += cui_hint(&c, hx, 456, "X", "ALL", 20);
        cui_hint(&c, hx, 456, "Y", "UNDO", 20);
        /* right side: B EXIT, START APPLY */
        int wstart = cui_text_width("START", 14) + 18 + 7 + cui_text_width("APPLY", 20);
        int wb = 28 + 7 + cui_text_width("EXIT", 20);
        cui_hint(&c, right - wstart, 456, "START", "APPLY", 20);
        cui_hint(&c, right - wstart - 20 - wb, 456, "B", "EXIT", 20);
    }

    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
}

#endif // PACMAN_RENDER_H__
