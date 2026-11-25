/*
 * Button Lab v0.1.1 - Miyoo Mini Plus controller tester for Onion.
 * SPDX-License-Identifier: MIT
 *
 * No external artwork, fonts, or Onion internals are required. SDL 1.2 draws
 * the screen; /dev/input/event0 provides raw Linux key events (without a grab).
 */
#define _POSIX_C_SOURCE 200809L
#include <SDL/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define W 640
#define H 480
#define RESET_MS 2000u
#define MENU_EXIT_MS 3000u
#define HOLD_POPUP_DELAY_MS 1000u
#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof((x)[0])))

typedef enum {
    UP, DOWN, LEFT, RIGHT, A, B, X, Y, L1, L2, R1, R2,
    SELECT, START, MENU, VOL_UP, VOL_DOWN, BUTTON_COUNT
} ButtonId;

typedef struct {
    const char *label;
    unsigned short linux_code;
    SDLKey sdl_code;     /* Used only if evdev cannot be opened. */
    bool core;
    bool down;
    bool seen;
    unsigned presses;
} Button;

/* Raw codes match Onion's src/common/system/keymap_hw.h.
 * SDL fallback matches common SDL 1.x/2.x Miyoo keyboard assignments.
 * The SDL fallback is not the primary source and may differ by firmware. */
/* Designated initialization makes the untested state explicit and keeps
 * -Wmissing-field-initializers quiet with the Onion build flags. */
#define BTN(text, code, fallback, is_core) \
    { .label = text, .linux_code = code, .sdl_code = fallback, \
      .core = is_core, .down = false, .seen = false, .presses = 0u }
static Button buttons[BUTTON_COUNT] = {
    [UP]       = BTN("UP",     KEY_UP,         SDLK_UP,        true),
    [DOWN]     = BTN("DOWN",   KEY_DOWN,       SDLK_DOWN,      true),
    [LEFT]     = BTN("LEFT",   KEY_LEFT,       SDLK_LEFT,      true),
    [RIGHT]    = BTN("RIGHT",  KEY_RIGHT,      SDLK_RIGHT,     true),
    [A]        = BTN("A",      KEY_SPACE,      SDLK_LCTRL,     true),
    [B]        = BTN("B",      KEY_LEFTCTRL,   SDLK_LALT,      true),
    [X]        = BTN("X",      KEY_LEFTSHIFT,  SDLK_LSHIFT,    true),
    [Y]        = BTN("Y",      KEY_LEFTALT,    SDLK_SPACE,     true),
    [L1]       = BTN("L1",     KEY_E,          SDLK_TAB,       true),
    [L2]       = BTN("L2",     KEY_TAB,        SDLK_PAGEDOWN,  true),
    [R1]       = BTN("R1",     KEY_T,          SDLK_BACKSPACE, true),
    [R2]       = BTN("R2",     KEY_BACKSPACE,  SDLK_PAGEUP,    true),
    [SELECT]   = BTN("SELECT", KEY_RIGHTCTRL,  SDLK_ESCAPE,    true),
    [START]    = BTN("START",  KEY_ENTER,      SDLK_RETURN,    true),
    [MENU]     = BTN("MENU",   KEY_ESC,        SDLK_HOME,      true),
    /* POWER is deliberately not tested: it can suspend the display via Onion. */
    [VOL_UP]   = BTN("VOL +",  KEY_VOLUMEUP,   SDLK_UNKNOWN,   false),
    [VOL_DOWN] = BTN("VOL -",  KEY_VOLUMEDOWN, SDLK_UNKNOWN,   false)
};
#undef BTN


typedef struct { uint32_t *pixels; int pitch; } Canvas;
static volatile sig_atomic_t interrupted = 0;
static int input_fd = -1;
static bool raw_input = false;
static bool needs_redraw = true;
static int last_button = -1;
static Uint32 reset_since = 0;
static Uint32 menu_since = 0;
static bool exit_armed = false;
static bool menu_exit_armed = false;
static bool resetting_latch = false;

/* All diagnostics go to stderr; */
#define LOG(...) do { \
    fprintf(stderr, "buttonlab: " __VA_ARGS__); \
    fputc('\n', stderr); fflush(stderr); \
} while (0)

/* Stage marker also makes a signal-related crash identifiable in the log. */
static volatile sig_atomic_t crash_stage = '0';
static void on_fatal(int sig) {
    char message[] = "buttonlab: FATAL signal 00 at stage 0\n";
    message[24] = (char)('0' + (sig / 10) % 10);
    message[25] = (char)('0' + sig % 10);
    message[36] = (char)crash_stage;
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(128 + sig);
}

static void on_signal(int s) { (void)s; interrupted = 1; }
/* ---- New UI (RedOnion), on Button Lab's own layout ----
 * robcodedev's arrangement as it was (the Mini Plus drawn in the middle with
 * its keys where they are, the last button on the left, the volume keys on
 * the right), in the colours, fonts, header and footer of the other apps.
 * A key is grey until tested, outlined in green once seen, red while held. */
#include "theme/render/canvas_ui.h"
#define CUI(c) (&(CuiCanvas){(c)->pixels, (c)->pitch})
#define BL_SEEN CUI_RGB(124, 217, 165)
#define BL_BODY CUI_RGB(40, 40, 43)
#define BL_EDGE CUI_RGB(70, 70, 75)

static int core_seen(void) {
    int i, n=0;
    for (i=0;i<BUTTON_COUNT;++i) if (buttons[i].core && buttons[i].seen) ++n;
    return n;
}
/* A key's face: red while held, green outline once tested, grey before. */
static uint32_t key_face(Canvas *c, float x, float y, float w, float h, float r, ButtonId id) {
    CuiCanvas *cc = CUI(c);
    if (buttons[id].down) {
        cui_rrect(cc,x,y,w,h,r,CUI_ACCENT);
        return CUI_WHITE;
    }
    if (buttons[id].seen) {
        cui_rrect(cc,x,y,w,h,r,BL_SEEN);
        cui_rrect(cc,x+2,y+2,w-4,h-4,r-2 > 0 ? r-2 : 0,BL_BODY);
        return BL_SEEN;
    }
    cui_rrect(cc,x,y,w,h,r,CUI_TRACK);
    return CUI_WHITE;
}
static void text_key(Canvas *c, int x, int y, int w, int h, ButtonId id, const char *text, int size) {
    uint32_t ink = key_face(c,x,y,w,h,h/2.0f < 9 ? h/2.0f : 9,id);
    /* the shoulder keys' lower part is behind the shell: their label sits
     * in the part that shows */
    float cy = (id == L1 || id == L2 || id == R1 || id == R2) ? y + (99 - y) / 2.0f + 1 : y + h / 2.0f;
    cui_text(CUI(c),x+w/2,cy,text,size,ink,CUI_CENTER);
}
/* A filled triangle pointing up, down, left or right, centred on cx,cy. */
static void triangle(Canvas *c, float cx, float cy, ButtonId dir, uint32_t col) {
    CuiCanvas *cc = CUI(c);
    int t;
    for (t = 0; t <= 6; ++t) {
        float half = t * 0.8f;
        switch (dir) {
        case UP:    cui_rect(cc,(int)(cx-half),(int)(cy-3+t),(int)(2*half+1),1,col); break;
        case DOWN:  cui_rect(cc,(int)(cx-half),(int)(cy+3-t),(int)(2*half+1),1,col); break;
        case LEFT:  cui_rect(cc,(int)(cx-3+t),(int)(cy-half),1,(int)(2*half+1),col); break;
        case RIGHT: cui_rect(cc,(int)(cx+3-t),(int)(cy-half),1,(int)(2*half+1),col); break;
        default: break;
        }
    }
}
static void dpad_key(Canvas *c, int x, int y, int w, int h, ButtonId id) {
    uint32_t ink = key_face(c,x,y,w,h,6,id);
    triangle(c,x+w/2.0f,y+h/2.0f,id,ink);
}
static void face_key(Canvas *c, int cx, int cy, ButtonId id) {
    CuiCanvas *cc = CUI(c);
    uint32_t ink;
    if (buttons[id].down) {
        cui_disc(cc,cx,cy,17,CUI_ACCENT);
        ink = CUI_WHITE;
    } else if (buttons[id].seen) {
        cui_disc(cc,cx,cy,17,BL_SEEN);
        cui_disc(cc,cx,cy,15,BL_BODY);
        ink = BL_SEEN;
    } else {
        cui_disc(cc,cx,cy,17,CUI_TRACK);
        ink = CUI_WHITE;
    }
    cui_text(cc,cx,cy,buttons[id].label,17,ink,CUI_CENTER);
}
/* The count chip in the header: amber while testing, green once all 15. */
static void count_chip(Canvas *c, int n) {
    CuiCanvas *cc = CUI(c);
    char tmp[24];
    uint32_t col = n == 15 ? CUI_GOOD : CUI_WARN;
    snprintf(tmp,sizeof(tmp),"%d / 15",n);
    int w = cui_text_width(tmp,13) + 22;
    cui_rrect(cc,624-w,19,w,22,11,col);
    cui_rrect(cc,624-w+1.5f,20.5f,w-3,19,9.5f,CUI_HEAD);
    cui_text(cc,624-w/2,30,tmp,13,col,CUI_CENTER);
}
static void device_body(Canvas *c, int n) {
    CuiCanvas *cc = CUI(c);
    char tmp[32];
    /* Physical order on the Mini Plus, from left to right: L1, L2, R2, R1,
     * behind the upper shell. */
    text_key(c,217,74,47,33,L1,"L1",14);
    text_key(c,267,74,47,33,L2,"L2",14);
    text_key(c,326,74,47,33,R2,"R2",14);
    text_key(c,376,74,47,33,R1,"R1",14);

    /* Mini Plus vertical handheld silhouette, and its 4:3 screen */
    cui_rrect(cc,204,99,232,308,30,BL_EDGE);
    cui_rrect(cc,206,101,228,304,28,BL_BODY);
    cui_rrect(cc,218,117,204,153,10,CUI_RGB(18,18,20));
    cui_text(cc,320,140,"MIYOO MINI PLUS",12,CUI_DIM,CUI_CENTER);
    snprintf(tmp,sizeof(tmp),"%02d / 15",n);
    cui_text(cc,320,182,tmp,34,n == 15 ? CUI_GOOD : CUI_WHITE,CUI_CENTER);
    cui_text(cc,320,212,n == 15 ? "TEST COMPLETE" : "BUTTONS TESTED",13,
             n == 15 ? CUI_GOOD : CUI_GREY,CUI_CENTER);
    if (last_button >= 0) {
        snprintf(tmp,sizeof(tmp),"Last: %s",buttons[last_button].label);
        cui_text(cc,320,244,tmp,15,CUI_WHITE,CUI_CENTER);
    } else
        cui_text(cc,320,244,"Press any button",15,CUI_GREY,CUI_CENTER);

    /* Real Mini Plus: four-way D-pad on the left */
    dpad_key(c,252,287,24,25,UP);
    dpad_key(c,252,337,24,25,DOWN);
    dpad_key(c,227,312,25,25,LEFT);
    dpad_key(c,276,312,25,25,RIGHT);
    cui_rect(cc,253,313,22,23,CUI_TRACK);

    /* A/B/X/Y in the Mini Plus diamond */
    face_key(c,375,296,X);
    face_key(c,350,321,Y);
    face_key(c,400,321,A);
    face_key(c,375,346,B);

    /* MENU: the small round button above SELECT and START */
    text_key(c,299,279,42,20,MENU,"MENU",11);
    text_key(c,251,372,65,22,SELECT,"SELECT",13);
    text_key(c,325,372,65,22,START,"START",13);
}
static void side_panel(Canvas *c, int x, const char *title) {
    CuiCanvas *cc = CUI(c);
    cui_rrect(cc,x,103,174,302,14,CUI_PANEL);
    cui_text(cc,x+16,124,title,13,CUI_GREY,CUI_LEFT);
}
static void draw(Canvas *c, Uint32 now) {
    CuiCanvas *cc = CUI(c);
    char tmp[96];
    int n=core_seen();

    cui_rect(cc,0,0,W,H,CUI_BG);
    cui_header(cc,"pad","Button Lab",NULL,-1);
    cui_text(cc,60+cui_text_width("Button Lab",25)+10,31,"v0.1.1",14,CUI_DIM,CUI_LEFT);
    count_chip(c,n);

    side_panel(c,18,"LIVE INPUT");
    if (last_button < 0) {
        cui_text(cc,105,200,"Ready",24,CUI_GREY,CUI_CENTER);
        cui_text(cc,105,232,"Press a button",14,CUI_GREY,CUI_CENTER);
    } else {
        cui_text(cc,105,200,buttons[last_button].label,28,CUI_WHITE,CUI_CENTER);
        snprintf(tmp,sizeof(tmp),"%u press%s",buttons[last_button].presses,
                 buttons[last_button].presses == 1 ? "" : "es");
        cui_text(cc,105,234,tmp,15,CUI_GREY,CUI_CENTER);
        cui_text(cc,105,258,buttons[last_button].down ? "Held down" : "Released",15,
                 buttons[last_button].down ? CUI_RGB(255, 143, 143) : CUI_GREY,CUI_CENTER);
    }
    cui_text(cc,105,350,"Green = detected",13,BL_SEEN,CUI_CENTER);
    cui_text(cc,105,374,raw_input ? "All 15 to pass" : "SDL input mode",13,
             raw_input ? CUI_GREY : CUI_WARN,CUI_CENTER);

    side_panel(c,448,"VOLUME KEYS");
    /* as on the console's side: + above - */
    text_key(c,467,206,136,34,VOL_UP,"VOL +",17);
    text_key(c,467,256,136,34,VOL_DOWN,"VOL -",17);
    cui_text(cc,535,350,"Optional",13,CUI_GREY,CUI_CENTER);
    cui_text(cc,535,374,"The system may take them",12,CUI_DIM,CUI_CENTER);

    device_body(c,n);

    /* footer: the exits, or the progress of a held gesture */
    cui_rect(cc,0,420,W,60,CUI_HEAD);
    if ((menu_since && now - menu_since >= HOLD_POPUP_DELAY_MS) ||
        (reset_since && now - reset_since >= HOLD_POPUP_DELAY_MS)) {
        bool menu = menu_since && now - menu_since >= HOLD_POPUP_DELAY_MS;
        unsigned total = menu ? MENU_EXIT_MS : RESET_MS;
        unsigned elapsed = now - (menu ? menu_since : reset_since);
        if (elapsed > total) elapsed = total;
        cui_text(cc,20,440,menu ? "Hold MENU to exit" : "Hold X+Y to reset",17,CUI_WHITE,CUI_LEFT);
        cui_progress(cc,20,458,600,8,(float)elapsed/(float)total);
    } else {
        cui_hint(cc,20,450,"SELECT+START","EXIT",20);
        cui_text(cc,620,450,"MENU 3 s: exit · X+Y 2 s: reset",15,CUI_GREY,CUI_RIGHT);
    }
}

static void set_button(ButtonId id, bool down) {
    Button *b=&buttons[id];
    if (b->down==down) return;
    b->down=down;
    if (down) {
        b->seen=true;
        ++b->presses;
        last_button=id;
    }
    needs_redraw=true;
}
static void read_evdev(void) {
    struct input_event e;
    ssize_t size;
    while ((size=read(input_fd,&e,sizeof(e))) == (ssize_t)sizeof(e)) {
        int i;
        if (e.type != EV_KEY || e.value == 2) continue; /* Repeat isn't a new press. */
        for(i=0;i<BUTTON_COUNT;++i)
            if (e.code==buttons[i].linux_code) { set_button((ButtonId)i,e.value==1); break; }
    }
    if (size == 0 || (size < 0 && errno != EAGAIN && errno != EINTR)) {
        /* A disconnected input node is not expected on Miyoo, but SDL
           fallback is preferable to leaving a permanently unresponsive app. */
        close(input_fd); input_fd=-1; raw_input=false; needs_redraw=true;
    }
}
static void read_sdl(SDL_Event *e) {
    int i;
    if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return;
    if (raw_input) return; /* Never count the same physical event twice. */
    for(i=0;i<BUTTON_COUNT;++i)
        if (buttons[i].sdl_code != SDLK_UNKNOWN && e->key.keysym.sym == buttons[i].sdl_code) {
            set_button((ButtonId)i,e->type==SDL_KEYDOWN);
            break;
        }
}
static void reset_test(void) {
    int i;
    for(i=0;i<BUTTON_COUNT;++i) { buttons[i].seen=false; buttons[i].presses=0; }
    last_button=-1;
    needs_redraw=true;
}
static void process_gestures(Uint32 now, bool *quit) {
    /* SELECT+START is a single chord with no hold timer. Wait for both
     * releases before closing so held keys cannot leak into MainUI. */
    if (!exit_armed && buttons[SELECT].down && buttons[START].down) {
        exit_armed = true;
        LOG("SELECT+START exit requested; waiting for release");
    }
    if (exit_armed) {
        if (!buttons[SELECT].down && !buttons[START].down) *quit = true;
        return;
    }

    /* Holding MENU for 3 seconds also requests exit. Keep the app active
     * until MENU is released, avoiding a still-held MENU in MainUI. */
    if (buttons[MENU].down) {
        if (!menu_since) menu_since = now ? now : 1;
        if (!menu_exit_armed && now - menu_since >= MENU_EXIT_MS) {
            menu_exit_armed = true;
            LOG("MENU held for 3 seconds; waiting for release to exit");
        }
        needs_redraw = true;
    } else {
        if (menu_exit_armed) {
            *quit = true;
            return;
        }
        menu_since = 0;
    }
    if (menu_exit_armed) return;

    /* Reset is independent of exiting and retains the 2-second hold. */
    if (buttons[X].down && buttons[Y].down) {
        if (!reset_since && !resetting_latch) reset_since = now ? now : 1;
        if (reset_since && now - reset_since >= RESET_MS) {
            reset_test();
            reset_since = 0;
            resetting_latch = true;
        } else if (reset_since) needs_redraw = true;
    } else {
        reset_since = 0;
        resetting_latch = false;
    }
}
/* Copy the software-rendered 0x00RRGGBB canvas into the Miyoo SDL screen.
 *
 * On Miyoo Mini / Mini Plus the physical LCD is mounted 180 degrees relative
 * to the default SDL software framebuffer. Rotate BOTH axes here, at the
 * output boundary, so drawing coordinates and input/UI logic stay unchanged.
 * The inverse mapping is: destination(x,y) = source(W-1-x,H-1-y).
 */
static uint32_t convert_pixel(uint32_t color, const SDL_PixelFormat *fmt) {
    return (((color >> 16 & 255u) >> fmt->Rloss) << fmt->Rshift) |
           (((color >>  8 & 255u) >> fmt->Gloss) << fmt->Gshift) |
           (((color       & 255u) >> fmt->Bloss) << fmt->Bshift);
}

static bool present(SDL_Surface *screen, const uint32_t *pixels) {
    int y, x, bpp;
    SDL_PixelFormat *fmt = screen->format;
    bool locked = SDL_MUSTLOCK(screen);
    if (locked && SDL_LockSurface(screen) != 0) {
        LOG("SDL_LockSurface failed: %s", SDL_GetError());
        return false;
    }
    bpp = fmt->BytesPerPixel;
    if (!screen->pixels || (bpp != 2 && bpp != 4) ||
        screen->w != W || screen->h != H || screen->pitch < W * bpp ||
        screen->pitch % bpp != 0) {
        LOG("unsupported screen: %dx%d, pitch=%d, bpp=%d, pixels=%p",
            screen->w, screen->h, screen->pitch, bpp, screen->pixels);
        if (locked) SDL_UnlockSurface(screen);
        return false;
    }
    for (y = 0; y < H; ++y) {
        const uint32_t *src = pixels + (H - 1 - y) * W;
        void *row = (unsigned char *)screen->pixels + y * screen->pitch;
        if (bpp == 4) {
            uint32_t *dst = (uint32_t *)row;
            if (fmt->Rmask == 0x00ff0000u && fmt->Gmask == 0x0000ff00u &&
                fmt->Bmask == 0x000000ffu) {
                for (x = 0; x < W; ++x) dst[x] = src[W - 1 - x];
            } else {
                for (x = 0; x < W; ++x)
                    dst[x] = convert_pixel(src[W - 1 - x], fmt);
            }
        } else {
            uint16_t *dst = (uint16_t *)row;
            for (x = 0; x < W; ++x)
                dst[x] = (uint16_t)convert_pixel(src[W - 1 - x], fmt);
        }
    }
    if (locked) SDL_UnlockSurface(screen);
    if (SDL_Flip(screen) != 0) {
        LOG("SDL_Flip failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    SDL_Surface *screen = NULL;
    uint32_t *pixels = NULL;
    SDL_Event event;
    bool quit = false;
    Uint32 last_frame = 0;
    int result = 1;
    (void)argc; (void)argv;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGSEGV, on_fatal);
    signal(SIGBUS, on_fatal);
    signal(SIGILL, on_fatal);
    signal(SIGABRT, on_fatal);

    crash_stage = '1';
    LOG("v0.1.1 startup; init input");
    input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    raw_input = input_fd >= 0;
    if (!raw_input) LOG("cannot open /dev/input/event0; SDL fallback: %s", strerror(errno));
    else LOG("evdev input available");

    crash_stage = '2';
    LOG("calling SDL_Init");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        LOG("SDL_Init failed: %s", SDL_GetError());
        goto done;
    }
    LOG("SDL_Init OK");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(0, 0);

    crash_stage = '3';
    LOG("calling SDL_SetVideoMode(640,480,32)");
    screen = SDL_SetVideoMode(W, H, 32, SDL_SWSURFACE);
    if (!screen) {
        LOG("32-bit video mode failed: %s; trying RGB565", SDL_GetError());
        screen = SDL_SetVideoMode(W, H, 16, SDL_SWSURFACE);
    }
    if (!screen) {
        LOG("SDL_SetVideoMode failed: %s", SDL_GetError());
        goto done_sdl;
    }
    if (!screen->format) {
        LOG("SDL screen has no pixel format");
        goto done_sdl;
    }
    LOG("SDL screen: %dx%d, pitch=%d, bpp=%d",
        screen->w, screen->h, screen->pitch, screen->format->BitsPerPixel);

    crash_stage = '4';
    pixels = calloc((size_t)W * H, sizeof(*pixels));
    if (!pixels) {
        LOG("not enough RAM for 640x480 software canvas");
        goto done_sdl;
    }
    LOG("canvas allocated; entering render loop");
    while (!interrupted && !quit) {
        Uint32 now = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            else read_sdl(&event);
        }
        if (input_fd >= 0) read_evdev();
        process_gestures(now, &quit);
        if (needs_redraw && now - last_frame >= 30) {
            Canvas canvas = { pixels, W };
            crash_stage = '5';
            draw(&canvas, now);
            crash_stage = '6';
            if (!present(screen, pixels)) break;
            crash_stage = '7';
            if (!last_frame) LOG("first frame presented successfully");
            last_frame = now;
            needs_redraw = false;
        }
        SDL_Delay(16);
    }
    if (quit || interrupted) result = 0;
    LOG("render loop ended (quit=%d, signal=%d, result=%d)", quit, interrupted, result);
    free(pixels);
done_sdl:
    SDL_Quit();
done:
    if (input_fd >= 0) close(input_fd);
    LOG("shutdown");
    return result;
}
