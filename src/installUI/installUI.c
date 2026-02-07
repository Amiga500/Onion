#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "system/keymap_sw.h"
#include "utils/config.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/str.h"

#ifndef ONION_VERSION
#define ONION_VERSION "4.x.x-dev-test"
#endif

#define TIMEOUT_M 10
#define CHECK_TIMEOUT 300
#define SLIDE_TIMEOUT 10000

static SDL_Surface *slide = NULL;

SDL_Surface *_loadSlide(int index)
{
    char image_path[STR_MAX];
    snprintf(image_path, sizeof(image_path), "res/installSlide%d.png", index);
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
            start_at = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--total") == 0)
            total_offset = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-m") == 0 ||
                 strcmp(argv[i], "--message") == 0)
            strncpy(message_str, argv[++i], STR_MAX - 1);
        else {
            printf_debug("Error: Unknown argument '%s'\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    // Validate total_offset to prevent division by zero
    if (total_offset <= 0) {
        fprintf(stderr, "Error: total offset must be positive (got %d)\n", total_offset);
        exit(EXIT_FAILURE);
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    TTF_Init();

    SDL_Surface *video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    SDL_Surface *screen =
        SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);

    if (video == NULL || screen == NULL) {
        fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Surface *waiting_bg = IMG_Load("res/waitingBG.png");
    SDL_Surface *progress_stripes = IMG_Load("res/progress_stripes.png");

    TTF_Font *font = TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 36);
    TTF_Font *font_small =
        TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 18);
    SDL_Color fg_color = {255, 255, 255, 0};

    char version_str[STR_MAX];
    snprintf(version_str, sizeof(version_str), "v%s", ONION_VERSION);

    SDL_Surface *surface_version = font_small
        ? TTF_RenderUTF8_Blended(font_small, version_str, fg_color)
        : NULL;
    SDL_Rect rect_version = {10, 10};

    Uint32 progress_bg = SDL_MapRGB(video->format, 29, 30, 37);
    Uint32 progress_color = SDL_MapRGB(video->format, 114, 71, 194);
    Uint32 failed_color = SDL_MapRGB(video->format, 194, 71, 71);

    SDL_Rect rectMessage = {10, 414};
    SDL_Rect rectProgress = {0, 470, 0, 10};
    SDL_Rect stripes_pos = {0, 470};
    SDL_Rect stripes_frame = {0, 0, 640, 10};

    int current_slide = -1;
    int num_slides = 9;
    config_get("currentSlide", CONFIG_INT, &current_slide);

    bool quit = false;
    bool failed = false;
    int progress = 0;
    int progress_div = 100 / total_offset;
    int spinner_tick = 0;
    char prev_message_str[STR_MAX] = "";
    SDL_Surface *cached_message = NULL;

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
            snprintf(message_str, sizeof(message_str), "Installation failed");
            progress = 100;
            failed = true;
            quit = true;
        }

        if (ticks - check_timer > CHECK_TIMEOUT) {
            if (exists("/tmp/.update_msg")) {
                file_readLastLine("/tmp/.update_msg", message_str);
                long n = 0;
                if (!exists(".installed") && str_getLastNumber(message_str, &n))
                    progress = (int)(start_at + n / progress_div);
                check_timer = ticks; // reset timeout
            }
            else if (!quit && ticks - check_timer > TIMEOUT_M * 60 * 1000 &&
                     !exists(".waitConfirm")) {
                snprintf(message_str, sizeof(message_str), "The installation timed out, exiting...");
                progress = 100;
                failed = true;
                quit = true;
            }
        }

        if (quit)
            break;

        if (acc_ticks >= time_step) {
            if (slide == NULL) {
                if (waiting_bg != NULL)
                    SDL_BlitSurface(waiting_bg, NULL, screen, NULL);
            }
            else
                SDL_BlitSurface(slide, NULL, screen, NULL);

            if (surface_version)
                SDL_BlitSurface(surface_version, NULL, screen, &rect_version);

            rectProgress.w = 640;
            SDL_FillRect(screen, &rectProgress, progress_bg);

            // spinner
            if (progress < 100 && progress_stripes != NULL) {
                stripes_frame.x = spinner_tick;
                SDL_BlitSurface(progress_stripes, &stripes_frame, screen,
                                &stripes_pos);
            }

            if (progress > 0) {
                rectProgress.w = (Uint16)(6.4 * progress);
                SDL_FillRect(screen, &rectProgress,
                             failed ? failed_color : progress_color);
            }

            // Cache message surface — only re-render when text changes
            if (strcmp(message_str, prev_message_str) != 0) {
                if (cached_message) SDL_FreeSurface(cached_message);
                cached_message = font
                    ? TTF_RenderUTF8_Blended(font, message_str, fg_color)
                    : NULL;
                strncpy(prev_message_str, message_str, STR_MAX - 1);
                prev_message_str[STR_MAX - 1] = '\0';
            }
            if (cached_message)
                SDL_BlitSurface(cached_message, NULL, screen, &rectMessage);

            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);

            spinner_tick += 4;
            if (spinner_tick >= 16)
                spinner_tick = 0;

            acc_ticks -= time_step;
        }

        msleep(15);
    }

    if (exists(".installed") && exists(".waitConfirm")) {
        remove(".waitConfirm");
        SDL_FillRect(video, NULL, 0);
        SDL_Flip(video);
    }

    config_setNumber("currentSlide", current_slide);

    if (cached_message != NULL)
        SDL_FreeSurface(cached_message);

    TTF_CloseFont(font);
    TTF_CloseFont(font_small);
    TTF_Quit();
    if (slide != NULL) {
        SDL_FreeSurface(slide);
    }
    SDL_FreeSurface(waiting_bg);
    SDL_FreeSurface(surface_version);
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    SDL_Quit();

    return EXIT_SUCCESS;
}
