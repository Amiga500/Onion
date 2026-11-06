/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/core.h"
#include "platform/device_adapter.h"
#include "platform/system_config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int mainui_suite_core(void)
{
    const int signal_dbm[] = {INT_MIN, -127, -126, -81, -80, -71, -70, 127, 128, INT_MAX};
    const int signal_levels[] = {0, 0, 1, 1, 2, 2, 3, 3, 0, 0};
    for (size_t i = 0; i < sizeof signal_dbm / sizeof *signal_dbm; ++i) {
        CHECK(mainui_wifi_signal_level(signal_dbm[i]) == signal_levels[i]);
    }
    MainUIConfig c;
    mainui_config_parse(&c, NULL, NULL, NULL, NULL);
    CHECK(c.rows == 6 && c.row_height == 60 && c.font_size == 0 && c.repeat_delay == 500 &&
          c.scroll_status == 1);
    mainui_config_parse(&c, " 14junk", "61", "50 10", "0,1");
    CHECK(c.rows == 14 && c.row_height == 25 && c.font_size == 60);
    CHECK(c.repeat_delay == 100 && c.repeat_interval == 30 && c.scroll_delay == 10 &&
          c.scroll_speed == 5);
    mainui_config_parse(&c, "999999999999999999999999999", NULL, NULL, NULL);
    CHECK(c.rows == 20);
    /* A third scroll value: "bounce" bounces; "rolling", nothing or anything
     * else rolls. Delay and speed parse exactly as before. */
    const char *bounce[] = {"700,120,bounce", "700 120 bounce\n", "700,120, ,BOUNCE",
                            "700,120,bounce,later"};
    for (size_t i = 0; i < sizeof bounce / sizeof *bounce; ++i) {
        mainui_config_parse(&c, NULL, NULL, NULL, bounce[i]);
        CHECK(c.scroll_bounce && c.scroll_status == 2 && c.scroll_delay == 700 &&
              c.scroll_speed == 120);
    }
    const char *rolling[] = {"700,120",      "700,120,rolling", "700,120bounce",  "700,120,bounced",
                             "700,120,boun", "700,120;bounce",  "700,120\tbounce"};
    for (size_t i = 0; i < sizeof rolling / sizeof *rolling; ++i) {
        mainui_config_parse(&c, NULL, NULL, NULL, rolling[i]);
        CHECK(!c.scroll_bounce && c.scroll_status == 2 && c.scroll_delay == 700 &&
              c.scroll_speed == 120);
    }
    mainui_config_parse(&c, NULL, NULL, NULL, "bounce");
    CHECK(!c.scroll_bounce && c.scroll_status == 1);
    /* .romListVideoPreview: no file, no previews; a delay in ms, clamped to
     * 0..30000, or 1000 without one; then cover (the default) or
     * fullscreen. */
    CHECK(c.video_preview_delay == -1 && !c.video_preview_fullscreen);
    CHECK(c.details_auto_speed == 0);
    int auto_delay, auto_speed;
    mainui_config_details_auto(NULL, &auto_delay, &auto_speed);
    CHECK(auto_delay == 0 && auto_speed == 0);
    mainui_config_details_auto("", &auto_delay, &auto_speed);
    CHECK(auto_delay == 2000 && auto_speed == 20);
    mainui_config_details_auto("1500,30", &auto_delay, &auto_speed);
    CHECK(auto_delay == 1500 && auto_speed == 30);
    mainui_config_details_auto("-30,9999", &auto_delay, &auto_speed);
    CHECK(auto_delay == 0 && auto_speed == 150);
    mainui_config_details_auto("bad,text", &auto_delay, &auto_speed);
    CHECK(auto_delay == 2000 && auto_speed == 20);

    const struct {
        const char *text;
        int delay;
        bool fullscreen;
    } previews[] = {{NULL, -1, false},
                    {"", 1000, false},
                    {"\n", 1000, false},
                    {"1500", 1500, false},
                    {" 250\r\n", 250, false},
                    {"0", 0, false},
                    {"-5", 0, false},
                    {"99999999999", 30000, false},
                    {"on", 1000, false},
                    {"800ms", 1000, false},
                    {"800,cover", 800, false},
                    {"800,fullscreen", 800, true},
                    {"0,fullscreen", 100, true},
                    {"99 fullscreen", 100, true},
                    {"100,fullscreen", 100, true},
                    {"-5,fullscreen", 100, true},
                    {"0,cover", 0, false},
                    {"800 FullScreen\n", 800, true},
                    {"800, ,fullscreen,later", 800, true},
                    {"fullscreen", 1000, true},
                    {"800,full", 800, false},
                    {"800,fullscreens", 800, false},
                    {"800fullscreen", 1000, false}};

    for (size_t i = 0; i < sizeof previews / sizeof *previews; ++i) {
        bool fullscreen = !previews[i].fullscreen;
        CHECK(mainui_config_video_preview(previews[i].text, &fullscreen) == previews[i].delay &&
              fullscreen == previews[i].fullscreen);
    }
    CHECK(mainui_device(285).lid && !mainui_device(354).lid && !mainui_device(999).axp);
    MainUIViewport v;
    mainui_viewport_restore(&v, 0, 10, 30, 20, 29);
    CHECK(v.selected == -1 && v.end == -1);
    mainui_viewport_restore(&v, 50, 10, 49, 10, 19);
    CHECK(v.selected == 49 && v.start == 49 && v.end == 49);
    mainui_viewport_move(&v, 10, 1, true);
    CHECK(v.selected == 0 && v.start == 0 && v.end == 9);
    mainui_viewport_move(&v, 10, INT_MAX, false);
    CHECK(v.selected == 49 && v.start == 40 && v.end == 49);
    /* A list that grew under a short window (Paste into an empty folder:
     * ".." plus the pasted Favorite) keeps its top row in view. */
    v = (MainUIViewport){2, 1, 0, 0};
    mainui_viewport_move(&v, 6, 0, false);
    CHECK(v.selected == 1 && v.start == 0 && v.end == 1);
    v = (MainUIViewport){9, 8, 0, 7};
    mainui_viewport_move(&v, 6, 0, false);
    CHECK(v.selected == 8 && v.start == 3 && v.end == 8);
    /* A list that changed under a window: grown or shrunk, short or full,
     * any old window and selection. The selection shows, no gap is left
     * below the last row, and a window that still fits does not move. */
    for (int rows = 1; rows <= 8; ++rows) {
        for (int before = 1; before <= 14; ++before) {
            for (int start = 0; start < before; ++start) {
                for (int total = 0; total <= 14; ++total) {
                    for (int selected = -1; selected <= total; ++selected) {
                        MainUIViewport w;
                        mainui_viewport_restore(&w, before, rows, start, start, start + rows - 1);
                        MainUIViewport old = w;
                        mainui_viewport_refit(&w, total, rows, selected);
                        if (!total) {
                            CHECK(w.total == 0 && w.selected == -1 && w.end == -1);
                            continue;
                        }
                        int want = selected < 0 ? 0 : selected >= total ? total - 1 : selected;
                        CHECK(w.total == total && w.selected == want);
                        CHECK(w.start >= 0 && w.start <= w.selected && w.selected <= w.end);
                        CHECK(w.end ==
                              (w.start + rows - 1 < total ? w.start + rows - 1 : total - 1));
                        CHECK(total < rows ? w.start == 0 : w.end - w.start == rows - 1);
                        if (old.start <= want && want < old.start + rows &&
                            old.start + rows <= total) {
                            CHECK(w.start == old.start);
                        }
                    }
                }
            }
        }
    }
    /* Exercise invariants across row counts, empty/small lists and repeated
     * navigation; fixed expected positions alone miss range/overflow failures.
     */
    for (int rows = 6; rows <= 20; rows++) {
        for (int total = 0; total < 80; total++) {
            mainui_viewport_restore(&v, total, rows, INT_MAX, INT_MAX, INT_MIN);
            for (int action = 0; action < 160; action++) {
                mainui_viewport_move(&v, rows, action % 2 ? -1 : rows, true);
                CHECK(v.total == total);
                if (total) {
                    CHECK(v.start >= 0 && v.start <= v.selected && v.selected <= v.end &&
                          v.end < total && v.end - v.start < rows);
                }
            }
        }
    }
    MainUIBlit blits[2];
    CHECK(mainui_marquee(0, 120, 100, 200, blits) == 0);
    CHECK(mainui_marquee(0, 120, 400, 200, blits) == 1 && blits[0].source_x == 0 &&
          blits[0].width == 200);
    /* Every emitted blit must remain inside both the title and destination.
     * Cover the wrap gap and second-segment region at non-frame-aligned times.
     */
    for (uint64_t t = 0; t < 10000; t += 13) {
        int n = mainui_marquee(t, 120, 401, 250, blits);
        CHECK(n >= 0 && n <= 2);
        for (int b = 0; b < n; b++) {
            CHECK(blits[b].source_x >= 0 && blits[b].source_x + blits[b].width <= 401 &&
                  blits[b].width > 0 && blits[b].destination_x >= 0 &&
                  blits[b].destination_x + blits[b].width <= 250);
        }
    }
    /* Titles whose first strong character is Hebrew or Arabic scroll right. */
    CHECK(mainui_text_rtl("\u0627\u0644\u0639\u0631\u0628\u064a\u0629 (Mario [USA]) "
                          "\u0627\u0644\u062c\u062f\u064a\u062f.nes"));
    CHECK(mainui_text_rtl("[!] 1-2 \u05e9\u05dc\u05d5\u05dd"));
    CHECK(mainui_text_rtl("\u00ab\ufefb"));
    CHECK(!mainui_text_rtl("Mario (\u0627\u0644\u0639\u0631\u0628\u064a\u0629).nes"));
    CHECK(!mainui_text_rtl("Pok\u00e9mon") && !mainui_text_rtl("\u00c9t\u00e9"));
    CHECK(!mainui_text_rtl("\u30de\u30ea\u30aa \u0627") && !mainui_text_rtl("123 [!]"));
    CHECK(!mainui_text_rtl("") && !mainui_text_rtl(NULL) && !mainui_text_rtl("\xd8"));
    /* Marquee offsets advance by a constant whole-pixel step per 40 ms frame. */
    CHECK(mainui_marquee_pixels(0, 40) == 0 && mainui_marquee_pixels(39, 40) == 0);
    CHECK(mainui_marquee_pixels(40, 40) == 2 && mainui_marquee_pixels(79, 40) == 2);
    CHECK(mainui_marquee_pixels(1000, 40) == 50);
    CHECK(mainui_marquee_pixels(1000, 25) == 25 && mainui_marquee_pixels(1000, 400) == 400);
    /* Slow speeds move one pixel every k frames: 5 px/s is one pixel per 200 ms. */
    CHECK(mainui_marquee_pixels(199, 5) == 0 && mainui_marquee_pixels(200, 5) == 1);
    CHECK(mainui_marquee_pixels(1000, 5) == 5 && mainui_marquee_pixels(1000, 0) == 0);
    /* Other slow speeds are quantized, as docs/TIMING.md lists: 20 px/s runs
     * at 25, 15 at 12.5, 10 at 8.33 and 7 at 6.25 px/s (12 s of frames). */
    CHECK(mainui_marquee_pixels(12000, 20) == 300 && mainui_marquee_pixels(12000, 15) == 150);
    CHECK(mainui_marquee_pixels(12000, 10) == 100 && mainui_marquee_pixels(12000, 7) == 75);
    int exact = 0;
    for (int speed = 5; speed <= 400; ++speed) {
        exact += mainui_marquee_pixels(12000, speed) == (uint64_t)speed * 12;
    }
    CHECK(exact == 17);
    for (int speed = 5; speed <= 400; ++speed) {
        uint64_t step = mainui_marquee_pixels(MAINUI_MARQUEE_FRAME_MS, speed);
        for (uint64_t frame = 1; frame < 200; ++frame) {
            uint64_t before = mainui_marquee_pixels((frame - 1) * MAINUI_MARQUEE_FRAME_MS, speed),
                     now = mainui_marquee_pixels(frame * MAINUI_MARQUEE_FRAME_MS, speed);
            /* Never backwards, and fast speeds never vary their step. */
            CHECK(now >= before && (step == 0 || now - before == step));
        }
    }
    CHECK(mainui_marquee_pixels(UINT32_MAX, 400) == (uint64_t)(UINT32_MAX / 40) * 16);
    /* Bounce rests as long as 84 px take at its speed, 400 ms to 2 s, in
     * whole 40 ms frames: 720 ms at 120 px/s, longer slower, shorter faster. */
    CHECK(mainui_marquee_bounce_rest(120) == 18 && mainui_marquee_bounce_rest(50) == 42);
    CHECK(mainui_marquee_bounce_rest(80) == 27 && mainui_marquee_bounce_rest(200) == 11);
    CHECK(mainui_marquee_bounce_rest(400) == 10 && mainui_marquee_bounce_rest(5) == 50);
    CHECK(mainui_marquee_bounce_rest(0) == 0);
    for (int speed = 5; speed <= 400; speed += 7) {
        int full = (int)mainui_marquee_step_pixels(1, speed);
        int rest = mainui_marquee_bounce_rest(speed);
        for (int distance = 1; distance <= 700; distance += 37) {
            /* One leg out and back, frame by frame. */
            static int at[20000];
            int frames = 0;
            for (uint64_t t = 0; frames < 20000; t += MAINUI_MARQUEE_FRAME_MS) {
                at[frames] = mainui_marquee_bounce(t, speed, distance);
                CHECK(at[frames] >= 0 && at[frames] <= distance);
                frames++;
                if (frames > 2 && at[frames - 1] && !at[frames - 2] && at[frames - 3] == 0 &&
                    frames > rest + 4) {
                    break; /* out again */
                }
            }
            CHECK(frames < 20000);
            /* Out only, rest at the far end, back only, rest at the start. */
            int turn = 0;
            while (turn < frames && at[turn] < distance) {
                turn++;
            }
            int far = 0, near = 0;
            for (int f = 1; f < frames - 1; f++) {
                CHECK(f <= turn ? at[f] >= at[f - 1] : at[f] <= at[f - 1] || at[f - 1] == 0);
                far += at[f] == distance;
                near += f > turn && at[f] == 0;
            }
            /* Easing may hold an end longer: by up to its 6 frames, or at
             * the slowest speeds (under a pixel a frame) half a way. */
            int extra = full ? 7 : 12;
            CHECK(far >= rest && far <= rest + extra && near >= rest - 1 && near <= rest + extra);
            /* Never more than a full step at once; the first and last steps
             * of a way are small; a long way runs at full speed. */
            int largest = 0, first = -1, last = 0;
            for (int f = 1; f <= turn; f++) {
                int step = at[f] - at[f - 1];
                CHECK(step <= (full > 1 ? full : 1) + 1);
                largest = step > largest ? step : largest;
                if (step && first < 0) {
                    first = step;
                }
                last = step ? step : last;
            }
            CHECK(first <= (full + 1) / 2 + 1 && last <= (full + 1) / 2 + 1);
            if (full > 1 && distance >= 4 * 6 * full) {
                CHECK(largest == full);
            }
        }
    }
    CHECK(mainui_marquee_bounce(UINT64_MAX, 400, 30000) >= 0);
    /* Failed parsing must leave the caller's prior state byte-for-byte intact. */
    MainUIStack state = {.count = 1}, before = state;
    CHECK(!mainui_state_parse("{\"list\":[{\"title\":1}]}", &state));
    CHECK(!memcmp(&state, &before, sizeof state));
    CHECK(!mainui_state_parse("{\"list\":[]}garbage", &state));
    CHECK(mainui_state_parse(
        "{\"list\":[{\"title\":157,\"type\":0,\"currpos\":2,\"pagestart\":0,\"pageend\":3}]}",
        &state));
    CHECK(state.count == 1 && state.frames[0].selected == 2);
    char *json = mainui_state_json(&state);
    CHECK(json != NULL);
    MainUIStack again;
    CHECK(mainui_state_parse(json, &again) && again.count == 1 && again.frames[0].title == 157);
    free(json);
    /* The button mapping handed to the driver: as configured, else stock's. */
    const char *keymaps[][2] = {
        {"{\"keymap\":\"L2,L,R2,R,B,A,B,Y\"}", "L2,L,R2,R,B,A,B,Y"},
        {"{}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":8}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"L2,L,R2,R,X,A,B\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"L2,L,R2,R,X,A,B,Y,Y\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"L2,L,R2,R,X,A,B,Z\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"L2,L,R2,R,,A,B,Y\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"L2,L,R2,R,X,A,B,Y,\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"l2,l,r2,r,x,a,b,y\"}", MAINUI_DEFAULT_KEYMAP},
        {"{\"keymap\":\"Y,Y,Y,Y,Y,Y,Y,Y\"}", "Y,Y,Y,Y,Y,Y,Y,Y"},
    };
    for (size_t i = 0; i < sizeof keymaps / sizeof *keymaps; ++i) {
        cJSON *settings = cJSON_Parse(keymaps[i][0]);
        CHECK(settings && !strcmp(mainui_keymap_value(settings), keymaps[i][1]));
        cJSON_Delete(settings);
    }
    CHECK(!strcmp(mainui_keymap_value(NULL), MAINUI_DEFAULT_KEYMAP));
    /* Stock's values for keymon's live copy: its defaults when a key is
     * missing or not a number, and lumination, hue, saturation and contrast
     * only from 0 to 20; the other values unchecked. */
    const char *names[] = {"vol",        "brightness", "mute",       "bgmvol",   "hibernate",
                           "lumination", "hue",        "saturation", "contrast", "audiofix"};
    const int defaults[] = {20, 7, 0, 8, 0, 7, 10, 10, 10, 1};
    cJSON *empty = cJSON_Parse("{\"vol\":\"5\",\"brightness\":true}");
    for (size_t i = 0; i < sizeof names / sizeof *names; ++i) {
        CHECK(mainui_system_monitor_value(empty, names[i]) == defaults[i]);
        CHECK(mainui_system_monitor_value(NULL, names[i]) == defaults[i]);
    }
    cJSON_Delete(empty);
    cJSON *set = cJSON_Parse("{\"vol\":25,\"brightness\":3,\"mute\":1,\"bgmvol\":-2,"
                             "\"hibernate\":5,\"lumination\":21,\"hue\":0,\"saturation\":20,"
                             "\"contrast\":-1,\"audiofix\":0}");
    const int expected[] = {25, 3, 1, -2, 5, 7, 0, 20, 10, 0};
    for (size_t i = 0; i < sizeof names / sizeof *names; ++i) {
        CHECK(mainui_system_monitor_value(set, names[i]) == expected[i]);
    }
    CHECK(mainui_system_monitor_value(set, "keymap") == 0);
    cJSON_Delete(set);
    puts("core tests passed (configuration, devices, viewport properties, marquee bounds, state "
         "codec, button mapping, live settings)");
    return 0;
}
