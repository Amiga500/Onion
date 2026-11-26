#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "system/keymap_sw.h"
#include "utils/config.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/str.h"

/* Test branch (unified look): header, footer and progress are drawn here,
 * the images (waitingBG, installSlideN) hold only the middle. */
#include "theme/render/canvas_ui.h"

#ifndef ONION_VERSION
#define ONION_VERSION "4.x.x-dev-test"
#endif

#define TIMEOUT_M 10
#define CHECK_TIMEOUT 300
#define SLIDE_TIMEOUT 10000
#define FAILED_HOLD_MS 5000 // how long "Installation failed" stays on screen

static SDL_Surface *slide = NULL;

/* How many tip slides there are: res/installSlide0.png, 1, 2... */
static int _countSlides(void)
{
    char image_path[STR_MAX];
    int n = 0;
    for (;; n++) {
        sprintf(image_path, "res/installSlide%d.png", n);
        if (!exists(image_path))
            return n;
    }
}

SDL_Surface *_loadSlide(int index)
{
    char image_path[STR_MAX];
    sprintf(image_path, "res/installSlide%d.png", index);
    if (exists(image_path))
        return IMG_Load(image_path);
    return NULL;
}

/* The tips in a random order, new at each installation: the logo (-1) and
 * the welcome (0) first, then the others shuffled. The order comes from a
 * seed kept in the config with the position, so the installer's second part
 * (after its restart) goes on in the same order; install.sh removes both
 * when an installation starts. */
#define MAX_SLIDES 64
static int slide_order[MAX_SLIDES + 1];
static int slide_order_n = 0;

static void buildSlideOrder(int num_slides, unsigned int seed)
{
    if (num_slides > MAX_SLIDES)
        num_slides = MAX_SLIDES;
    slide_order_n = 0;
    slide_order[slide_order_n++] = -1;
    if (num_slides > 0)
        slide_order[slide_order_n++] = 0;
    int first = slide_order_n;
    for (int i = 1; i < num_slides; i++)
        slide_order[slide_order_n++] = i;
    unsigned int x = seed ? seed : 1;
    for (int i = slide_order_n - 1; i > first; i--) {
        x ^= x << 13; /* xorshift32 */
        x ^= x >> 17;
        x ^= x << 5;
        int j = first + (int)(x % (unsigned int)(i - first + 1));
        int t = slide_order[i];
        slide_order[i] = slide_order[j];
        slide_order[j] = t;
    }
}

/* Moves *pos (an index in slide_order) by direction, skipping pictures that
 * cannot be loaded; the logo (-1) needs none. */
void nextSlide(int *pos, int direction)
{
    if (slide != NULL) {
        SDL_FreeSurface(slide);
        slide = NULL;
    }
    if (slide_order_n <= 0)
        return;
    int p = *pos;
    for (int tries = 0; tries < slide_order_n; tries++) {
        p = ((p + direction) % slide_order_n + slide_order_n) % slide_order_n;
        if (slide_order[p] == -1 || (slide = _loadSlide(slide_order[p])) != NULL)
            break;
    }
    *pos = p;
}

/* Page dots under the tip: where it is in this installation's order (the
 * pictures have none: with the order random, theirs would jump about). */
static void draw_dots(CuiCanvas *c, int pos)
{
    int n = slide_order_n - 1; /* the logo has no dot */
    if (n <= 1 || pos <= 0 || pos > n)
        return;
    float gap = n > 24 ? 300.0f / (n - 1) : 14.0f;
    float x0 = 320 - (n - 1) * gap / 2;
    for (int k = 0; k < n; k++) {
        float x = x0 + k * gap;
        if (k == pos - 1)
            cui_rrect(c, x - 9, 361, 18, 10, 5, CUI_ACCENT);
        else
            cui_disc(c, x, 366, 3.5f, CUI_RGB(80, 80, 85));
    }
}

/* Rounded track; grey stripes move along the part still to do while the
 * install runs, the done part is accent (or red when it failed). */
static void draw_progress(CuiCanvas *c, int x, int y, int w, int h, int progress, bool failed, int tick)
{
    float r = h / 2.0f;
    cui_rrect(c, x, y, w, h, r, CUI_TRACK);
    if (progress < 100) {
        uint32_t light = CUI_RGB(78, 78, 82);
        for (int yy = y + 1; yy < y + h - 1; yy++)
            for (int xx = x + h; xx < x + w - h; xx++)
                if (((xx - yy + 1024 - tick) / 8) % 2 == 0)
                    c->px[yy * c->pitch + xx] = light;
    }
    if (progress > 0) {
        float done = w * (progress > 100 ? 100 : progress) / 100.0f;
        cui_rrect(c, x, y, done < h ? h : done, h, r, failed ? CUI_BAD : CUI_ACCENT);
    }
}

int main(int argc, char *argv[])
{
    // The percentage to start at
    int start_at = 0;
    // The amount of progress constituting 100% of sub-install
    int total_offset = 100;
    // The initial message - if `/tmp/.update_msg` isn't found
    char message_str[STR_MAX] = " ";

    if (argc == 2 && strcmp("--version", argv[1]) == 0) {
        printf("%s\n", ONION_VERSION);
        return EXIT_SUCCESS;
    }

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--begin") == 0)
            start_at = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--total") == 0)
            total_offset = atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 ||
                 strcmp(argv[i], "--message") == 0)
            strncpy(message_str, argv[++i], STR_MAX - 1);
        else {
            printf_debug("Error: Unknown argument '%s'\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();

    SDL_Surface *video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    SDL_Surface *screen =
        SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);

    SDL_Surface *waiting_bg = IMG_Load("res/waitingBG.png");

    char version_str[STR_MAX];
    sprintf(version_str, "v%s", ONION_VERSION);

    int num_slides = _countSlides(); // 20 tips in the new UI (9 before)
    int seed = 0, current_slide = 0; // a position in slide_order
    if (!config_get("slideSeed", CONFIG_INT, &seed) || seed <= 0) {
        /* a new installation: a new order, from the logo */
        seed = (int)(((unsigned int)time(NULL) ^ (unsigned int)getpid() * 2654435761u) & 0x7fffffff);
        if (seed <= 0)
            seed = 1;
    }
    else if (!config_get("currentSlide", CONFIG_INT, &current_slide))
        current_slide = 0;
    buildSlideOrder(num_slides, (unsigned int)seed);
    if (current_slide < 0 || current_slide >= slide_order_n)
        current_slide = 0;
    if (slide_order[current_slide] != -1)
        slide = _loadSlide(slide_order[current_slide]);


    bool quit = false;
    bool failed = false;
    int progress = 0;
    int progress_div = 100 / total_offset;
    int spinner_tick = 0;

    SDL_Event event;

    uint32_t acc_ticks = 0, last_ticks = SDL_GetTicks(),
             time_step = 1000 / 24, // 12 fps
        check_timer = 0;
    uint32_t slide_timer = last_ticks;

    while (!quit) {
        uint32_t ticks = SDL_GetTicks();
        acc_ticks += ticks - last_ticks;
        last_ticks = ticks;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                quit = true;
            else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                case SW_BTN_LEFT:
                    nextSlide(&current_slide, -1);
                    slide_timer = ticks;
                    break;
                case SW_BTN_RIGHT:
                    nextSlide(&current_slide, 1);
                    slide_timer = ticks;
                    break;
                case SW_BTN_A:
                    if (exists(".waitConfirm"))
                        quit = true;
                    break;
                default:
                    break;
                }
            }
        }

        if (ticks - slide_timer > SLIDE_TIMEOUT) {
            nextSlide(&current_slide, 1);
            slide_timer = ticks;
        }

        if (exists(".installed") || exists(".waitConfirm")) {
            progress = 100;
            if (!exists(".waitConfirm"))
                quit = true;
        }

        if (exists(".installFailed")) {
            sprintf(message_str, "Installation failed");
            progress = 100;
            failed = true;
            quit = true;
        }

        // Once failed, the last line of .update_msg must not overwrite
        // "Installation failed" (and its full red bar) any more.
        if (!failed && ticks - check_timer > CHECK_TIMEOUT) {
            if (exists("/tmp/.update_msg")) {
                file_readLastLine("/tmp/.update_msg", message_str);
                long n = 0;
                if (!exists(".installed") && str_getLastNumber(message_str, &n))
                    progress = (int)(start_at + n / progress_div);
                check_timer = ticks; // reset timeout
            }
            else if (!quit && ticks - check_timer > TIMEOUT_M * 60 * 1000 &&
                     !exists(".waitConfirm")) {
                sprintf(message_str, "The installation timed out, exiting...");
                progress = 100;
                failed = true;
                quit = true;
            }
        }

        // A failure (or the time-out) is drawn once more and held: the loop
        // used to end before drawing, so "Installation failed" never showed
        // and the installer just went back to the stock firmware.
        if (quit && !failed)
            break;
        if (failed)
            acc_ticks = time_step;

        if (acc_ticks >= time_step) {
            if (slide != NULL)
                SDL_BlitSurface(slide, NULL, screen, NULL);
            else if (waiting_bg != NULL)
                SDL_BlitSurface(waiting_bg, NULL, screen, NULL);
            else
                SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 39, 39, 39));

            CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
            if (slide != NULL)
                draw_dots(&c, current_slide);
            cui_header(&c, "R", "RedOnion", version_str, -1);

            // footer: the message, centred like the logo and the tips above
            // it (robcodedev), then the progress bar under it
            cui_rect(&c, 0, 420, 640, 60, CUI_HEAD);
            char fitted[STR_MAX];
            cui_fit(message_str, 21, 592, fitted, sizeof(fitted));
            cui_text(&c, 320, 441, fitted, 21, failed ? CUI_BAD : CUI_WHITE, CUI_CENTER);
            draw_progress(&c, 24, 461, 592, 8, progress, failed, spinner_tick);
            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);

            spinner_tick = (spinner_tick + 1) % 16;

            acc_ticks -= time_step;
        }

        if (failed) {
            SDL_Delay(FAILED_HOLD_MS);
            break;
        }

        msleep(15);
    }

    if (exists(".installed") && exists(".waitConfirm")) {
        remove(".waitConfirm");
        SDL_FillRect(video, NULL, 0);
        SDL_Flip(video);
    }

    config_setNumber("currentSlide", current_slide);
    config_setNumber("slideSeed", seed);

    TTF_Quit();

    if (slide != NULL) {
        SDL_FreeSurface(slide);
    }
    if (waiting_bg != NULL)
        SDL_FreeSurface(waiting_bg);
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    SDL_Quit();

    return EXIT_SUCCESS;
}
