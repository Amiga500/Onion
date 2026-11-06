/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/details.h"
#include "platform/input.h"
#include "ui/media_viewer.h"
#include "ui/name_input.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static MainUINameResult press(MainUINameInput *input, SDLKey key, Uint16 code)
{
    SDL_keysym event = {0};
    event.sym = key;
    event.unicode = code;
    return mainui_name_input_key(input, &event);
}

/* A gallery video starts only once it has stayed shown: moving past it
 * opens no decoder. */
static void gallery_video_waits(void)
{
    char root[] = "/tmp/mainui-gallery-XXXXXX", path[4096];
    assert(mkdtemp(root));
    const char *dirs[] = {"media", "media/box2dfront", "media/videos"};
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof path, "%s/%s", root, dirs[i]);
        assert(!mkdir(path, 0755));
    }
    const char *files[] = {"media/box2dfront/Game.png", "media/videos/Game.mp4"};
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof path, "%s/%s", root, files[i]);
        FILE *file = fopen(path, "w");
        assert(file && !fclose(file));
    }
    char rom[4096];
    snprintf(rom, sizeof rom, "%s/Game.nes", root);
    SDL_Surface *screen =
        SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0xff0000, 0xff00, 0xff, 0);
    MainUIMediaViewer viewer = {0};
    assert(screen &&
           mainui_media_open(&viewer, root, rom, NULL, screen, NULL, MAINUI_MEDIA_START_GALLERY) &&
           viewer.count == 2);
    assert(!viewer.video_pending && mainui_media_due(&viewer, SDL_GetTicks()) == -1);
    mainui_media_key(&viewer, SDLK_RIGHT, screen); /* to the video: it waits */
    Uint32 now = SDL_GetTicks();
    int due = mainui_media_due(&viewer, now);
    assert(viewer.video_pending && !viewer.video && due >= 0 && due <= MAINUI_MEDIA_VIDEO_DELAY_MS);
    assert(mainui_media_due(&viewer, now + MAINUI_MEDIA_VIDEO_DELAY_MS) == 0);
    mainui_media_key(&viewer, SDLK_LEFT, screen); /* moved on: no start */
    assert(!viewer.video_pending && !viewer.video && mainui_media_due(&viewer, now) == -1);
    mainui_media_key(&viewer, SDLK_RIGHT, screen);
    mainui_media_key(&viewer, SDLK_ESCAPE, screen); /* closed: no start */
    assert(!viewer.open && !viewer.video_pending);
    /* Back from the PDF reader, the menu Manual (PDF) was chosen in is
     * closed: onto the gallery it was opened over, or Game Info. */
    assert(mainui_media_open(&viewer, root, rom, NULL, screen, NULL, MAINUI_MEDIA_START_GALLERY));
    int gallery = viewer.mode;
    mainui_media_key(&viewer, SDLK_RCTRL, screen); /* Select: the menu */
    assert(viewer.mode != gallery);
    mainui_media_suspend_for_pdf(&viewer);
    mainui_media_back_from_pdf(&viewer, screen);
    assert(viewer.open && viewer.mode == gallery);
    mainui_media_back_from_pdf(&viewer, screen); /* no menu: nothing */
    assert(viewer.open && viewer.mode == gallery);
    mainui_media_close(&viewer);
    assert(mainui_media_open(&viewer, root, rom, NULL, screen, NULL, MAINUI_MEDIA_START_MENU) &&
           mainui_media_over_details(&viewer));
    mainui_media_suspend_for_pdf(&viewer);
    mainui_media_back_from_pdf(&viewer, screen);
    assert(!viewer.open);
    SDL_FreeSurface(screen);
    for (int i = 1; i >= 0; i--) {
        snprintf(path, sizeof path, "%s/%s", root, files[i]);
        assert(!remove(path));
    }
    for (int i = 2; i >= 0; i--) {
        snprintf(path, sizeof path, "%s/%s", root, dirs[i]);
        assert(!rmdir(path));
    }
    assert(!rmdir(root));
}

int mainui_suite_name_input(void)
{
    gallery_video_waits();
    /* Patcher meta_input: a short final advance clamps the line offset, and
     * paging back subtracts a full visible page from that clamped position. */
    MainUIDetails details = {.line_count = 22, .lines_per_page = 10};
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_END, NULL);
    assert(details.scroll_line == 10);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_END, NULL);
    assert(details.scroll_line == 12);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_HOME, NULL);
    assert(details.scroll_line == 2);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_HOME, NULL);
    assert(details.scroll_line == 0);

    /* Returning from the media popup restarts manual/automatic description
     * scrolling, while preserving metadata and cached text layout. */
    details.open = true;
    details.scroll_line = 12;
    details.scroll_px = 180;
    details.auto_started = details.auto_paused = details.auto_finished = true;
    details.auto_held = true;
    details.swallow_page_key = SDLK_END;
    mainui_details_reset_scroll(&details);
    assert(details.scroll_line == 0 && details.scroll_px == 0);
    assert(!details.auto_started && !details.auto_paused && !details.auto_finished &&
           !details.auto_held && details.swallow_page_key == SDLK_UNKNOWN);

    /* Auto-scroll starts after delay, advances by pixels, and stops once
     * at the bottom. Holding the gallery freezes the elapsed clock. */
    MainUIDetails auto_details = {.open = true,
                                  .auto_speed = 40,
                                  .auto_delay = 1000,
                                  .auto_started = true,
                                  .auto_epoch = 100,
                                  .scroll_max_px = 120};
    assert(mainui_details_auto_due(&auto_details, 300) == 800);
    assert(!mainui_details_auto_step(&auto_details, 300));
    assert(mainui_details_auto_step(&auto_details, 1600));
    assert(auto_details.scroll_px == 20);
    mainui_details_auto_hold(&auto_details, true, 1600);
    assert(mainui_details_auto_due(&auto_details, 2100) == -1);
    mainui_details_auto_hold(&auto_details, false, 2100);
    assert(mainui_details_auto_step(&auto_details, 2600));
    assert(auto_details.scroll_px == 40);
    assert(mainui_details_auto_step(&auto_details, 6000));
    assert(auto_details.scroll_px == 120 && auto_details.auto_finished);
    assert(mainui_details_auto_due(&auto_details, 6100) == -1);
    auto_details.auto_finished = false;
    auto_details.description_line_height = 20;
    auto_details.line_count = 16;
    auto_details.lines_per_page = 10;
    auto_details.scroll_px = 23;
    mainui_details_key(&auto_details, NULL, NULL, NULL, NULL, 6, SDLK_END, NULL);
    assert(auto_details.auto_paused && auto_details.scroll_px == 23);
    assert(auto_details.swallow_page_key == SDLK_END);
    mainui_details_key(&auto_details, NULL, NULL, NULL, NULL, 6, SDLK_END, NULL);
    assert(auto_details.scroll_px == 120);
    /* Before it has moved: before its first frame, during its delay, and
     * after it but before the first whole pixel (5 px/s, 100 ms past a 1000
     * ms delay or past none), the first press pages at once and cancels it,
     * swallowing nothing. */
    for (int phase = 0; phase < 4; phase++) {
        auto_details.scroll_line = auto_details.scroll_px = 0;
        auto_details.auto_paused = auto_details.auto_finished = false;
        auto_details.swallow_page_key = SDLK_UNKNOWN;
        Uint32 now = SDL_GetTicks();
        auto_details.auto_started = phase > 0;
        auto_details.auto_speed = phase > 1 ? 5 : 40;
        auto_details.auto_delay = phase == 1 ? 30000 : phase == 2 ? 1000 : 0;
        auto_details.auto_epoch = now - (phase == 2 ? 1100 : phase == 3 ? 100 : 0);
        assert(phase < 2 || !mainui_details_auto_step(&auto_details, now));
        mainui_details_key(&auto_details, NULL, NULL, NULL, NULL, 6, SDLK_END, NULL);
        assert(auto_details.auto_paused && auto_details.swallow_page_key == SDLK_UNKNOWN);
        assert(auto_details.scroll_line == 6 && auto_details.scroll_px == 120);
        assert(mainui_details_auto_due(&auto_details, SDL_GetTicks()) == -1);
    }

    MainUINameInput input = {0};
    /* Printable aliases must remain text while naming, including shifted text. */
    press(&input, SDLK_s, 's');
    press(&input, SDLK_x, 'X');
    press(&input, SDLK_y, 'y');
    press(&input, SDLK_m, 'm');
    press(&input, SDLK_UNKNOWN, 0x20ac);
    assert(!strcmp(input.text, "sXym\xe2\x82\xac"));
    press(&input, SDLK_BACKSPACE, 0);
    assert(!strcmp(input.text, "sXym"));
    press(&input, SDLK_UNKNOWN, 0xd800);
    assert(!strcmp(input.text, "sXym"));
    input.text[0] = 0;
    input.selected = 10;
    press(&input, SDLK_RETURN, 0);
    press(&input, SDLK_HOME, 0);
    press(&input, SDLK_RETURN, 0);
    press(&input, SDLK_END, 0);
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ!"));
    input.selected = 39;
    press(&input, SDLK_DOWN, 0);
    assert(input.selected == 44);
    assert(press(&input, SDLK_RETURN, 0) == NAME_SUBMIT);
    press(&input, SDLK_RIGHT, 0);
    assert(input.selected == 40);
    press(&input, SDLK_LEFT, 0);
    assert(input.selected == 44);
    press(&input, SDLK_UP, 0);
    assert(input.selected == 38);
    input.selected = 42;
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ! "));
    press(&input, SDLK_LSHIFT, 0);
    assert(!strcmp(input.text, "qQ!  "));
    press(&input, SDLK_LALT, 0);
    input.selected = 43;
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ!"));
    memset(input.text, 'a', 126);
    input.text[126] = 0;
    press(&input, SDLK_UNKNOWN, 0x20ac);
    assert(strlen(input.text) == 126);
    press(&input, SDLK_z, 'z');
    press(&input, SDLK_z, 'z');
    assert(strlen(input.text) == 127);
    assert(press(&input, SDLK_F2, 0) == NAME_SUBMIT);
    assert(press(&input, SDLK_ESCAPE, 0) == NAME_CANCEL);
    input.text[0] = 0;
    input.selected = 10;
    input.shift = input.symbols = false;
    SDL_Event physical = {0};
    physical.type = SDL_KEYDOWN;
    physical.key.keysym.sym = SDLK_SPACE;
    physical.key.keysym.unicode = ' ';
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_EDITING);
    assert(!strcmp(input.text, "q"));
    physical.key.keysym.sym = SDLK_RETURN;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_SUBMIT);
    physical.key.keysym.sym = SDLK_ESCAPE;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_EDITING);
    physical.key.keysym.sym = SDLK_LCTRL;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_CANCEL);
    mainui_name_input_close(&input);
    return 0;
}
