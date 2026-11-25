/* progressUI (RedOnion, new UI): shows a long task run by a script, in the
 * new look, instead of its output scrolling in the terminal.
 *
 *   some_task.sh | progressUI -t "Scraper" [-i image] [-s "Game Boy Advance"]
 *
 * It reads the task's output on stdin. Lines starting with "@@" tell it
 * where the task is; the others are shown, last four, in the log box:
 *   @@title <text>        what is being done (big, under the header)
 *   @@total <n>           how many items there are
 *   @@item <n> <name>     working on item n
 *   @@found / @@notfound / @@skipped    one more of each
 *   @@note <text>         the line in the frame at the bottom
 *   @@panel <1-3> <label>|<value>|<under>   a panel of one's own (a
 *                         download's size, speed...) instead of the counts
 *   @@tip <text>          one more line for the frame: the note and the
 *                         tips take turns there, 6 seconds each
 *   @@autoclose           close as soon as the task ends (exit 0)
 *   @@nostop              B does nothing (a task that must not be cut, as
 *                         an update being written to the card)
 * Lines with "Scraped!", "No match found" or "already Scraped" count too,
 * for scripts that do not send @@found and the like.
 *
 * B asks "Stop?" (B continues, A stops). Stopping closes stdin, so the task
 * ends at its next line (SIGPIPE), and progressUI exits with 2. When the
 * task ends, the totals stay on screen until A or B: exit 0. MENU is the
 * same as B. */

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "system/battery.h"
#include "system/keymap_sw.h"
#include "theme/render/canvas_ui.h"

#define LOG_LINES 4
#define LINE_MAX_LEN 256

static char app_title[64] = "Working";
static char app_icon[24] = "refresh";
static char task_title[LINE_MAX_LEN] = "";
static char item_name[LINE_MAX_LEN] = "";
#define DEFAULT_NOTE "If it freezes, hold POWER for 10 seconds."
static char note[LINE_MAX_LEN] = DEFAULT_NOTE;
static char log_lines[LOG_LINES][LINE_MAX_LEN];
static int log_count = 0;
static int total = 0, item = 0, found = 0, notfound = 0, skipped = 0;
static bool finished = false, asking = false, autoclose = false, nostop = false;
static char panels[3][3][64];
#define MAX_TIPS 8
static char tips[MAX_TIPS][LINE_MAX_LEN];
static int tip_count = 0;
static bool panel_set[3];
static Uint32 started_at = 0;
#ifdef PROGRESSUI_NO_MAIN
#define SDL_GetTicks() 120000
#endif

static void add_log(const char *line)
{
    if (log_count == LOG_LINES) {
        memmove(log_lines[0], log_lines[1], sizeof(log_lines[0]) * (LOG_LINES - 1));
        log_count--;
    }
    snprintf(log_lines[log_count++], LINE_MAX_LEN, "%s", line);
}

/* Takes out terminal colour codes (ESC [ ... m) and the line end. */
static void clean(char *s)
{
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == 27 && r[1] == '[') {
            r += 2;
            while (*r && !((*r >= 'A' && *r <= 'Z') || (*r >= 'a' && *r <= 'z')))
                r++;
            if (!*r)
                break;
            continue;
        }
        if (*r == '\r' || *r == '\n')
            continue;
        *w++ = *r;
    }
    *w = '\0';
}

static void handle_line(char *line)
{
    clean(line);
    if (strncmp(line, "@@", 2) == 0) {
        char *arg = strchr(line, ' ');
        if (arg)
            *arg++ = '\0';
        const char *cmd = line + 2;
        if (!strcmp(cmd, "title") && arg)
            snprintf(task_title, sizeof(task_title), "%s", arg);
        else if (!strcmp(cmd, "total") && arg)
            total = atoi(arg);
        else if (!strcmp(cmd, "item") && arg) {
            item = atoi(arg);
            char *name = strchr(arg, ' ');
            snprintf(item_name, sizeof(item_name), "%s", name ? name + 1 : "");
        }
        else if (!strcmp(cmd, "found"))
            found++;
        else if (!strcmp(cmd, "notfound"))
            notfound++;
        else if (!strcmp(cmd, "skipped"))
            skipped++;
        else if (!strcmp(cmd, "note") && arg)
            snprintf(note, sizeof(note), "%s", arg);
        else if (!strcmp(cmd, "tip") && arg && tip_count < MAX_TIPS)
            snprintf(tips[tip_count++], LINE_MAX_LEN, "%s", arg);
        else if (!strcmp(cmd, "autoclose"))
            autoclose = true;
        else if (!strcmp(cmd, "nostop"))
            nostop = true;
        else if (!strcmp(cmd, "panel") && arg && arg[0] >= '1' && arg[0] <= '3' && arg[1] == ' ') {
            int n = arg[0] - '1';
            char *f = arg + 2;
            for (int k = 0; k < 3; k++) {
                char *bar = strchr(f, '|');
                if (bar)
                    *bar = '\0';
                snprintf(panels[n][k], sizeof(panels[n][k]), "%s", f);
                f = bar ? bar + 1 : f + strlen(f);
            }
            panel_set[n] = true;
        }
        return;
    }
    if (strstr(line, "already Scraped"))
        skipped++;
    else if (strstr(line, "Scraped!"))
        found++;
    else if (strstr(line, "No match found"))
        notfound++;
    /* separators and empty lines are not worth a log line */
    bool only_marks = true;
    for (const char *p = line; *p; p++)
        if (*p != '-' && *p != '*' && *p != ' ' && *p != '=')
            only_marks = false;
    if (!only_marks)
        add_log(line);
}

static void panel(CuiCanvas *c, int x, int w, const char *label, const char *value, const char *sub, uint32_t color)
{
    cui_rrect(c, x, 196, w, 82, 10, CUI_PANEL);
    cui_text(c, x + 14, 214, label, 12, CUI_GREY, CUI_LEFT);
    cui_text(c, x + 14, 242, value, 26, color, CUI_LEFT);
    cui_text(c, x + 14, 266, sub, 14, CUI_GREY, CUI_LEFT);
}

static void draw(CuiCanvas *c, int battery)
{
    cui_rect(c, 0, 0, CUI_W, CUI_H, CUI_BG);
    cui_header(c, app_icon, app_title, finished ? "Done" : "Working", battery);

    /* At the end, 100% only if every item was reached: a task that ended
     * early (an error, no network) shows where it stopped. */
    bool complete = !finished || total <= 0 || item >= total;
    int percent = total > 0 ? (finished ? (complete ? 100 : item * 100 / total)
                                        : (item > 0 ? (item - 1) * 100 / total : 0))
                            : -1;
    char fitted[LINE_MAX_LEN];
    cui_fit(task_title[0] ? task_title : app_title, 26, 470, fitted, sizeof(fitted));
    cui_text(c, 20, 92, fitted, 26, CUI_WHITE, CUI_LEFT);
    if (percent >= 0) {
        char p[8];
        snprintf(p, sizeof(p), "%d%%", percent);
        cui_text(c, 620, 92, p, 28, finished && complete ? CUI_GOOD : CUI_WARN, CUI_RIGHT);
        cui_progress(c, 20, 118, 600, 8, percent / 100.0f);
    }
    else
        cui_progress(c, 20, 118, 600, 8, finished ? 1.0f : (SDL_GetTicks() / 40 % 100) / 100.0f);

    char line[LINE_MAX_LEN];
    if (finished && complete)
        snprintf(line, sizeof(line), "Finished: %d of %d", total, total);
    else if (finished)
        snprintf(line, sizeof(line), "Ended at %d of %d", item, total);
    else if (total > 0 && item > 0 && !(panel_set[0] || panel_set[1] || panel_set[2]))
        snprintf(line, sizeof(line), "%d of %d · %s", item, total, item_name);
    else
        snprintf(line, sizeof(line), "%s", item_name);
    cui_fit(line, 16, 600, fitted, sizeof(fitted));
    cui_text(c, 20, 150, fitted, 16, CUI_GREY, CUI_LEFT);

    char v[32], s[48];
    if (panel_set[0] || panel_set[1] || panel_set[2]) {
        for (int k = 0; k < 3; k++)
            if (panel_set[k])
                panel(c, 20 + k * 204, 192, panels[k][0], panels[k][1], panels[k][2], CUI_WHITE);
    }
    else {
    snprintf(v, sizeof(v), "%d", found);
    snprintf(s, sizeof(s), total ? "of %d" : "so far", total);
    panel(c, 20, 192, "FOUND", v, s, CUI_WHITE);
    snprintf(v, sizeof(v), "%d", notfound);
    panel(c, 224, 192, "NOT FOUND", v, notfound ? "see the log" : "none so far", notfound ? CUI_BAD : CUI_GOOD);
    if (!finished && total > 0 && item > 1) {
        Uint32 elapsed = (SDL_GetTicks() - started_at) / 1000;
        Uint32 left = elapsed * (total - item + 1) / (item - 1);
        snprintf(v, sizeof(v), "%02u:%02u", left / 60, left % 60);
        panel(c, 428, 192, "TIME LEFT", v, "about", CUI_WHITE);
    }
    else {
        snprintf(v, sizeof(v), "%d", skipped);
        panel(c, 428, 192, "ALREADY THERE", v, "skipped", CUI_WHITE);
    }
    }

    /* the last lines of the task */
    if (log_count > 0)
        cui_rrect(c, 20, 292, 600, 76, 10, CUI_RGB(38, 38, 41));
    for (int i = 0; i < log_count; i++) {
        cui_fit(log_lines[i], 14, 576, fitted, sizeof(fitted));
        cui_text(c, 32, 307 + i * 17, fitted, 14, CUI_GREY, CUI_LEFT);
    }
    cui_rrect(c, 20, 378, 600, 34, 8, CUI_TRACK);
    cui_rrect(c, 21, 379, 598, 32, 7, CUI_BG);
    /* the note and the tips take turns, 6 s each; at the end, the note */
    const char *shown = note;
    if (!finished && tip_count > 0) {
        int turn = (SDL_GetTicks() / 6000) % (tip_count + 1);
        if (turn > 0)
            shown = tips[turn - 1];
    }
    if (finished && !strcmp(note, DEFAULT_NOTE))
        shown = "Done. Press A to close this screen.";
    cui_fit(shown, 14, 576, fitted, sizeof(fitted));
    cui_text(c, 32, 395, fitted, 14, CUI_GREY, CUI_LEFT);

    cui_rect(c, 0, 428, CUI_W, 52, CUI_HEAD);
    if (finished)
        cui_hint(c, 16, 454, "A", "CLOSE", 20);
    else if (!nostop)
        cui_hint(c, 16, 454, "B", "STOP", 20);

    if (asking) {
        for (int y = 0; y < CUI_H; y++)
            for (int x = 0; x < CUI_W; x++)
                c->px[y * c->pitch + x] = cui_mix(c->px[y * c->pitch + x], 0, 0.6f);
        int bx = 70, by = 150, bw = 500;
        cui_rrect(c, bx, by, bw, 170, 16, CUI_PANEL);
        cui_text(c, bx + 24, by + 38, "STOP NOW?", 24, CUI_WHITE, CUI_LEFT);
        cui_text(c, bx + 24, by + 74, "What is done so far is kept.", 17, CUI_GREY, CUI_LEFT);
        int bw2 = (bw - 60) / 2, y = by + 100;
        cui_rrect(c, bx + 24, y, bw2, 46, 10, CUI_WHITE);
        cui_rrect(c, bx + 27, y + 3, bw2 - 6, 40, 8, CUI_TRACK);
        cui_hint(c, bx + 24 + bw2 / 2 - 60, y + 23, "B", "CONTINUE", 20);
        int ax = bx + 36 + bw2;
        cui_rrect(c, ax, y, bw2, 46, 10, CUI_BAD);
        cui_rrect(c, ax + 2, y + 2, bw2 - 4, 42, 9, CUI_PANEL);
        cui_disc(c, ax + bw2 / 2 - 34, y + 23, 14, CUI_ACCENT);
        cui_text(c, ax + bw2 / 2 - 34, y + 23, "A", 18, CUI_WHITE, CUI_CENTER);
        cui_text(c, ax + bw2 / 2 - 13, y + 23, "STOP", 20, CUI_BAD, CUI_LEFT);
    }
}

#ifndef PROGRESSUI_NO_MAIN
int main(int argc, char *argv[])
{
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "-t"))
            snprintf(app_title, sizeof(app_title), "%s", argv[++i]);
        else if (!strcmp(argv[i], "-i"))
            snprintf(app_icon, sizeof(app_icon), "%s", argv[++i]);
        else if (!strcmp(argv[i], "-s"))
            snprintf(task_title, sizeof(task_title), "%s", argv[++i]);
    }
    signal(SIGPIPE, SIG_IGN);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();
    SDL_Surface *video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    if (!video || !screen)
        return 1;

    started_at = SDL_GetTicks();
    int battery = battery_getPercentage();
    Uint32 battery_at = started_at, drawn_at = 0;
    char buf[4096];
    size_t used = 0;
    bool input_open = true, quit = false, dirty = true;
    int code = 0;

    while (!quit) {
        if (input_open) {
            ssize_t n = read(STDIN_FILENO, buf + used, sizeof(buf) - 1 - used);
            if (n > 0) {
                used += n;
                buf[used] = '\0';
                char *start = buf, *nl;
                while ((nl = strchr(start, '\n')) != NULL) {
                    *nl = '\0';
                    handle_line(start);
                    start = nl + 1;
                }
                used = strlen(start);
                memmove(buf, start, used + 1);
                if (used >= sizeof(buf) - 1) { /* a very long line */
                    handle_line(buf);
                    used = 0;
                }
                dirty = true;
            }
            else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                if (used) {
                    buf[used] = '\0';
                    handle_line(buf);
                }
                input_open = false;
                finished = true;
                dirty = true;
                if (autoclose)
                    quit = true;
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* SDL_QUIT (SDL 1.2 raises it on SIGTERM/SIGINT) does not
             * close a task that must not be cut (@@nostop) before its end */
            if (event.type == SDL_QUIT && (!nostop || finished))
                quit = true;
            if (event.type != SDL_KEYDOWN)
                continue;
            SDLKey key = event.key.keysym.sym;
            if (finished) {
                if (key == SW_BTN_A || key == SW_BTN_B || key == SW_BTN_MENU)
                    quit = true;
            }
            else if (asking) {
                if (key == SW_BTN_A) {
                    code = 2;
                    quit = true;
                }
                else if (key == SW_BTN_B || key == SW_BTN_MENU)
                    asking = false;
            }
            else if ((key == SW_BTN_B || key == SW_BTN_MENU) && !nostop)
                asking = true;
            dirty = true;
        }

        Uint32 now = SDL_GetTicks();
        if (now - battery_at > 15000) {
            battery = battery_getPercentage();
            battery_at = now;
            dirty = true;
        }
        /* the time left and an unknown total's bar move by themselves */
        if (!finished && now - drawn_at > 1000)
            dirty = true;
        if (dirty && !quit) {
            CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
            draw(&c, battery);
            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);
            dirty = false;
            drawn_at = now;
        }
        SDL_Delay(30);
    }

    if (input_open)
        close(STDIN_FILENO); /* the task gets SIGPIPE at its next line */
    SDL_FreeSurface(screen);
    TTF_Quit();
    SDL_Quit();
    return code;
}
#endif
