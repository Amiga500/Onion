#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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

SDL_Surface *_loadSlide(int index)
{
    char image_path[STR_MAX];
    sprintf(image_path, "res/installSlide%d.png", index);
    if (exists(image_path))
        return IMG_Load(image_path);
    return NULL;
}

void nextSlide(int *current_slide, int num_slides, int direction)
{
    if (slide != NULL) {
        SDL_FreeSurface(slide);
    }

    int next_slide = *current_slide;

    do {
        next_slide += direction;

        if (next_slide >= num_slides)
            next_slide = -1;

        if (next_slide < -1)
            next_slide = num_slides - 1;
    } while ((slide = _loadSlide(next_slide)) == NULL && next_slide != *current_slide && next_slide != -1);

    *current_slide = next_slide;
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

    int current_slide = -1;
    int num_slides = 9;
    config_get("currentSlide", CONFIG_INT, &current_slide);

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
                    nextSlide(&current_slide, num_slides, -1);
                    slide_timer = ticks;
                    break;
                case SW_BTN_RIGHT:
                    nextSlide(&current_slide, num_slides, 1);
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
            nextSlide(&current_slide, num_slides, 1);
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
            cui_header(&c, "R", "RedOnion", version_str, -1);

            // footer: the message, then the progress bar under it
            cui_rect(&c, 0, 420, 640, 60, CUI_HEAD);
            char fitted[STR_MAX];
            cui_fit(message_str, 21, 592, fitted, sizeof(fitted));
            cui_text(&c, 24, 441, fitted, 21, failed ? CUI_BAD : CUI_WHITE, CUI_LEFT);
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
