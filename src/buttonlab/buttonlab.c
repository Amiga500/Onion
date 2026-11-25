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

/* Compact, self-contained 5x7 font (columns stored as 5-bit row masks).
 * No need to locate an Onion or system TTF file. */
typedef struct { char c; unsigned char bits[7]; } Glyph;
static const Glyph font[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}},       {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},  {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},     {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},    {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},   {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'+',{0,4,4,31,4,4,0}},     {'-',{0,0,0,31,0,0,0}},
 {'/',{1,2,2,4,8,8,16}},     {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}},    {'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}},    {'=',{0,31,0,31,0,0,0}},
 {'(',{2,4,8,8,8,4,2}},     {')',{8,4,2,2,2,4,8}},
 {'#',{10,31,10,10,31,10,0}}, {'%',{17,2,4,8,17,0,0}},
 {' ',{0,0,0,0,0,0,0}}
};

/* Sharp 8x11 screen font: fixed-size monochrome glyphs rendered at native pixel
 * resolution. Unlike 5x7 nearest-neighbour stretching, bottom strokes stay
 * aligned and no characters are resampled at runtime.
 * Raster shapes adapted from the permissively licensed DejaVu Sans Mono. */
static const uint8_t small_font[][11] = {
    {0x38,0x38,0x38,0x2c,0x6c,0x7c,0x44,0xc6,0xc6,0x00,0x00}, /* A */
    {0x7c,0x4c,0x46,0x4c,0x7c,0x46,0x46,0x46,0x7c,0x00,0x00}, /* B */
    {0x3c,0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x3c,0x00,0x00}, /* C */
    {0x78,0x6c,0x66,0x66,0x66,0x66,0x66,0x6c,0x78,0x00,0x00}, /* D */
    {0x7e,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x7e,0x00,0x00}, /* E */
    {0x7e,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x60,0x00,0x00}, /* F */
    {0x3c,0x60,0x60,0x60,0x60,0x6e,0x66,0x66,0x3c,0x00,0x00}, /* G */
    {0x66,0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x66,0x00,0x00}, /* H */
    {0x7c,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7c,0x00,0x00}, /* I */
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x4c,0x78,0x00,0x00}, /* J */
    {0x66,0x6c,0x78,0x78,0x78,0x78,0x6c,0x6c,0x66,0x00,0x00}, /* K */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00,0x00}, /* L */
    {0xee,0xee,0xee,0xfe,0xde,0xde,0xc6,0xc6,0xc6,0x00,0x00}, /* M */
    {0x66,0x66,0x76,0x76,0x56,0x5e,0x4e,0x4e,0x4e,0x00,0x00}, /* N */
    {0x38,0x6c,0x66,0x46,0xc6,0x46,0x66,0x6c,0x38,0x00,0x00}, /* O */
    {0x7c,0x6e,0x66,0x6e,0x7c,0x60,0x60,0x60,0x60,0x00,0x00}, /* P */
    {0x38,0x6c,0x66,0x46,0xc6,0x46,0x66,0x6c,0x3c,0x0c,0x00}, /* Q */
    {0x7c,0x6c,0x64,0x6c,0x78,0x6c,0x6c,0x66,0x66,0x00,0x00}, /* R */
    {0x38,0x64,0x60,0x70,0x3c,0x1c,0x06,0x4c,0x7c,0x00,0x00}, /* S */
    {0xfe,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00}, /* T */
    {0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x6c,0x3c,0x00,0x00}, /* U */
    {0xc6,0x46,0x64,0x6c,0x6c,0x2c,0x38,0x38,0x38,0x00,0x00}, /* V */
    {0xc2,0xc6,0xde,0xde,0xfe,0x7e,0x6e,0x6c,0x6c,0x00,0x00}, /* W */
    {0xc6,0x6c,0x3c,0x38,0x38,0x38,0x7c,0x6c,0xc6,0x00,0x00}, /* X */
    {0xc6,0x66,0x6c,0x3c,0x38,0x18,0x18,0x18,0x18,0x00,0x00}, /* Y */
    {0x7e,0x0e,0x0c,0x1c,0x38,0x30,0x70,0x60,0x7e,0x00,0x00}, /* Z */
    {0x38,0x6c,0x64,0x66,0x7e,0x66,0x64,0x6c,0x38,0x00,0x00}, /* 0 */
    {0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0x00,0x00}, /* 1 */
    {0x38,0x4c,0x0c,0x0c,0x1c,0x18,0x30,0x60,0x7c,0x00,0x00}, /* 2 */
    {0x38,0x4c,0x0c,0x0c,0x38,0x0c,0x04,0x4c,0x78,0x00,0x00}, /* 3 */
    {0x1c,0x1c,0x3c,0x2c,0x6c,0x4c,0x7e,0x0c,0x0c,0x00,0x00}, /* 4 */
    {0x7c,0x60,0x60,0x78,0x0c,0x04,0x04,0x4c,0x78,0x00,0x00}, /* 5 */
    {0x3c,0x64,0x60,0x7c,0x6c,0x66,0x66,0x6c,0x3c,0x00,0x00}, /* 6 */
    {0x7c,0x0c,0x0c,0x1c,0x18,0x18,0x30,0x30,0x30,0x00,0x00}, /* 7 */
    {0x38,0x6c,0x64,0x6c,0x38,0x6c,0x46,0x6c,0x3c,0x00,0x00}, /* 8 */
    {0x38,0x6c,0x4c,0x4c,0x6e,0x3c,0x04,0x4c,0x38,0x00,0x00}, /* 9 */
    {0x00,0x00,0x10,0x10,0x10,0xfe,0x10,0x10,0x10,0x00,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x3c,0x3c,0x00,0x00,0x00,0x00,0x00}, /* - */
    {0x04,0x0c,0x08,0x08,0x18,0x10,0x30,0x20,0x60,0x40,0x00}, /* / */
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00}, /* : */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00}, /* . */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x10,0x00,0x00}, /* ! */
    {0x3c,0x4c,0x0c,0x18,0x10,0x30,0x00,0x30,0x30,0x00,0x00}, /* ? */
    {0x00,0x00,0x00,0x00,0xfe,0x00,0xfe,0x00,0x00,0x00,0x00}, /* = */
    {0x18,0x10,0x30,0x30,0x30,0x30,0x30,0x10,0x18,0x08,0x00}, /* ( */
    {0x30,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x30,0x30,0x00}, /* ) */
    {0x00,0x16,0x34,0x7e,0x2c,0x2c,0xfe,0x58,0x58,0x00,0x00}, /* # */
    {0x60,0xd0,0xd0,0x66,0x18,0x4e,0x1a,0x1a,0x0e,0x00,0x00}, /* % */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
};

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
static uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }
static void put(Canvas *c, int x, int y, uint32_t color) {
    if ((unsigned)x < W && (unsigned)y < H) c->pixels[y * c->pitch + x] = color;
}
static void rect(Canvas *c, int x, int y, int w, int h, uint32_t color) {
    int xx, yy;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (yy = y0; yy < y1; ++yy) {
        uint32_t *p = c->pixels + yy * c->pitch;
        for (xx = x0; xx < x1; ++xx) p[xx] = color;
    }
}
static void round_rect(Canvas *c, int x, int y, int w, int h, int r, uint32_t color) {
    int yy, xx;
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (yy = 0; yy < h; ++yy) {
        int inset = 0;
        if (r) {
            int dy = yy < r ? r - yy - 1 : (yy >= h-r ? yy - (h-r) : -1);
            if (dy >= 0) {
                /* Pixel-perfect curved corners without floating point. */
                while (inset < r && ((r-inset-1)*(r-inset-1) + dy*dy >= r*r)) ++inset;
            }
        }
        for (xx = inset; xx < w - inset; ++xx) put(c, x + xx, y + yy, color);
    }
}
static void framed_round(Canvas *c, int x, int y, int w, int h, int r,
                         uint32_t frame, uint32_t fill) {
    round_rect(c, x, y, w, h, r, frame);
    round_rect(c, x+1, y+1, w-2, h-2, r > 1 ? r-1 : 0, fill);
}
static void disc(Canvas *c, int cx, int cy, int radius, uint32_t color) {
    int dx, dy;
    for (dy = -radius; dy <= radius; ++dy)
        for (dx = -radius; dx <= radius; ++dx)
            if (dx*dx + dy*dy <= radius*radius) put(c,cx+dx,cy+dy,color);
}
static void ring(Canvas *c, int cx, int cy, int radius, int thickness, uint32_t color) {
    int dx, dy, inner = (radius-thickness)*(radius-thickness);
    for (dy = -radius; dy <= radius; ++dy)
        for (dx = -radius; dx <= radius; ++dx) {
            int d = dx*dx + dy*dy;
            if (d <= radius*radius && d >= inner) put(c,cx+dx,cy+dy,color);
        }
}
/* Small labels use 1:1 pixel glyphs: no scaling, interpolation or blur. */
#define SMALL_W 8
#define SMALL_H 11
#define SMALL_ADVANCE 8
static void glyph(Canvas *c, int x, int y, char ch, int scale, uint32_t color) {
    int i, row, col;
    const unsigned char *bits = NULL;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    for (i = 0; i < ARRAY_LEN(font); ++i)
        if (font[i].c == ch) { bits = font[i].bits; break; }
    if (!bits) return;
    if (scale == 1) {
        for (row = 0; row < SMALL_H; ++row) {
            uint8_t mask = small_font[i][row];
            for (col = 0; col < SMALL_W; ++col)
                if (mask & (1u << (7 - col)))
                    put(c, x + col, y + row, color);
        }
    } else {
        for (row = 0; row < 7; ++row)
            for (col = 0; col < 5; ++col)
                if (bits[row] & (1u << (4-col)))
                    rect(c,x+col*scale,y+row*scale,scale,scale,color);
    }
}
static int text_height(int scale) { return scale == 1 ? SMALL_H : 7*scale; }
static int text_width(const char *s, int scale) {
    int count = (int)strlen(s);
    if (!count) return 0;
    return scale == 1 ? count * SMALL_ADVANCE - 1 : count * 6*scale - scale;
}
static void label(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    int advance = scale == 1 ? SMALL_ADVANCE : 6*scale;
    while (*s) { glyph(c,x,y,*s++,scale,color); x += advance; }
}
static void centered(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    label(c,x-text_width(s,scale)/2,y,s,scale,color);
}
static uint32_t blend(uint32_t a, uint32_t b, int alpha) {
    unsigned ar=(a>>16)&255,ag=(a>>8)&255,ab=a&255;
    unsigned br=(b>>16)&255,bg=(b>>8)&255,bb=b&255;
    return rgb((ar*(255-alpha)+br*alpha)/255,
               (ag*(255-alpha)+bg*alpha)/255,
               (ab*(255-alpha)+bb*alpha)/255);
}
static uint32_t key_fill(ButtonId id) {
    if (buttons[id].down) return rgb(98,229,191);
    if (buttons[id].seen) return rgb(41,99,104);
    return rgb(38,50,70);
}
static uint32_t key_line(ButtonId id) {
    if (buttons[id].down) return rgb(180,255,221);
    if (buttons[id].seen) return rgb(92,214,186);
    return rgb(75,90,113);
}
static uint32_t key_text(ButtonId id) {
    return buttons[id].down ? rgb(9,40,40) : (buttons[id].seen ? rgb(180,247,226) : rgb(197,210,230));
}
static int core_seen(void) {
    int i, n=0;
    for (i=0;i<BUTTON_COUNT;++i) if (buttons[i].core && buttons[i].seen) ++n;
    return n;
}
static void flat_key(Canvas *c, int x, int y, int w, int h, ButtonId id, int fs) {
    round_rect(c,x,y+4,w,h,11,rgb(10,16,27));
    framed_round(c,x,y,w,h,11,key_line(id),key_fill(id));
    int label_y = y + (h - text_height(fs))/2;
    if (id == L1 || id == L2 || id == R1 || id == R2) label_y -= 3;
    centered(c,x+w/2,label_y,buttons[id].label,fs,key_text(id));
    /* A key's fill and outline already indicate tested/held state. */
}
static void arrow(Canvas *c, ButtonId id, int cx, int cy) {
    uint32_t ink = key_text(id);
    int t;
    for(t=0;t<7;++t) {
        switch(id) {
        case UP:    rect(c,cx-t,cy+t,2*t+1,2,ink);break;
        case DOWN:  rect(c,cx-t,cy-t,2*t+1,2,ink);break;
        case LEFT:  rect(c,cx+t,cy-t,2,2*t+1,ink);break;
        case RIGHT: rect(c,cx-t,cy-t,2,2*t+1,ink);break;
        default: break;
        }
    }
}
static void dpad_key(Canvas *c, int x, int y, int w, int h, ButtonId id) {
    framed_round(c,x,y,w,h,7,key_line(id),key_fill(id));
    arrow(c,id,x+w/2,y+h/2);
}
static void face_key(Canvas *c, int cx, int cy, ButtonId id, uint32_t accent) {
    uint32_t fill = buttons[id].down ? rgb(98,229,191) :
                    buttons[id].seen ? blend(rgb(24,37,55),accent,115) : blend(rgb(26,35,53),accent,50);
    disc(c,cx,cy+2,17,rgb(11,16,28));
    disc(c,cx,cy,17,key_line(id));
    disc(c,cx,cy,15,fill);
    centered(c,cx,cy-7,buttons[id].label,2,buttons[id].down ? rgb(9,40,40) : rgb(244,249,255));
    if (buttons[id].seen && !buttons[id].down) ring(c,cx,cy,12,1,accent);
}
static void status_chip(Canvas *c, int x, int y, int w, ButtonId id) {
    framed_round(c,x,y,w,34,10,key_line(id),key_fill(id));
    centered(c,x+w/2,y+10,buttons[id].label,2,key_text(id));
    /* No extra dot: the whole key changes color when detected. */
}
/* All positions below use a single 640 x 480 canvas. The center diagram
 * follows the Mini Plus front-panel arrangement: screen above controls,
 * shoulder buttons behind the upper shell, MENU as the small round button
 * above SELECT and START, and speaker slits at the lower right. */
static void speaker(Canvas *c, int x, int y) {
    int i, j;
    for (i = 0; i < 5; ++i)
        for (j = 0; j < 12; ++j) {
            /* Small diagonal speaker vents, clipped inside the body. */
            int xx = x + i * 8 + j/3;
            int yy = y - j + i/2;
            rect(c, xx, yy, 2, 2, rgb(9,16,26));
        }
}
static void menu_key(Canvas *c, int cx, int cy) {
    disc(c,cx,cy+2,10,rgb(8,13,24));
    disc(c,cx,cy,10,key_line(MENU));
    disc(c,cx,cy,8,key_fill(MENU));
    /* The full button illumination is the state indicator. */
    centered(c,cx,cy+14,"MENU",1,rgb(174,197,218));
}
static void panel(Canvas *c, int x, int y, int w, int h) {
    round_rect(c,x,y+3,w,h,16,rgb(6,11,21));
    framed_round(c,x,y,w,h,16,rgb(47,66,88),rgb(22,32,49));
    rect(c,x+14,y+49,w-28,1,rgb(47,66,88));
}
static void device_body(Canvas *c) {
    /* Two pairs of shoulder keys emerge from behind the shell. */
    /* Physical order on the Mini Plus, from left to right:
     * L1, L2, R2, R1. */
    flat_key(c,217,80,47,27,L1,1);
    flat_key(c,267,80,47,27,L2,1);
    flat_key(c,326,80,47,27,R2,1);
    flat_key(c,376,80,47,27,R1,1);

    /* Mini Plus vertical handheld silhouette. */
    round_rect(c,207,104,226,305,29,rgb(5,10,18));
    framed_round(c,204,99,232,308,30,rgb(86,106,128),rgb(34,46,63));
    round_rect(c,209,105,222,295,26,rgb(39,53,72));
    rect(c,219,115,202,1,rgb(114,137,158));

    /* Screen bezel, 4:3 display, subtle screen gradient. */
    round_rect(c,216,116,208,157,12,rgb(8,13,22));
    framed_round(c,218,117,204,153,10,rgb(89,109,129),rgb(10,17,28));
    framed_round(c,223,122,194,145,5,rgb(21,39,50),rgb(9,20,35));
    {
        int y;
        for (y=125;y<264;++y) {
            uint32_t shade=blend(rgb(12,25,42),rgb(8,51,55),(y-125)*145/139);
            rect(c,226,y,188,1,shade);
        }
    }
    centered(c,320,146,"MIYOO MINI PLUS",1,rgb(126,168,187));
    rect(c,247,163,146,1,rgb(37,70,81));
    {
        char tmp[32];
        int n=core_seen();
        snprintf(tmp,sizeof(tmp),"%02d / 15",n);
        centered(c,320,179,tmp,3,n==15 ? rgb(127,252,201) : rgb(224,247,248));
        centered(c,320,211,n==15 ? "TEST COMPLETE" : "BUTTONS TESTED",1,
                 n==15 ? rgb(124,248,193) : rgb(142,189,197));
        if (last_button >= 0) {
            snprintf(tmp,sizeof(tmp),"LAST: %s",buttons[last_button].label);
            centered(c,320,237,tmp,1,rgb(104,224,194));
        } else centered(c,320,237,"PRESS ANY BUTTON",1,rgb(116,150,173));
    }
    /* Real Mini Plus: four-way D-pad on the left. */
    dpad_key(c,252,287,24,25,UP);
    dpad_key(c,252,337,24,25,DOWN);
    dpad_key(c,227,312,25,25,LEFT);
    dpad_key(c,276,312,25,25,RIGHT);
    rect(c,253,313,22,23,rgb(26,38,55));
    disc(c,264,324,4,rgb(71,93,115));

    /* A/B/X/Y follow the physical Mini Plus diamond placement. */
    face_key(c,375,296,X,rgb(91,158,240));
    face_key(c,350,321,Y,rgb(231,179,83));
    face_key(c,400,321,A,rgb(92,229,157));
    face_key(c,375,346,B,rgb(229,98,144));

    /* MENU is the small round button ABOVE SELECT and START. */
    menu_key(c,319,287);
    /* Wider keys accommodate the larger small-text font. */
    flat_key(c,251,372,65,21,SELECT,1);
    flat_key(c,325,372,65,21,START,1);
    speaker(c,393,391);
}
static void draw_background(Canvas *c) {
    int y;
    for (y=0;y<H;++y) {
        uint32_t color=blend(rgb(9,16,29),rgb(17,29,48),y*200/H);
        rect(c,0,y,W,1,color);
    }
    rect(c,20,75,600,1,rgb(42,58,77));
}
static void draw(Canvas *c, Uint32 now) {
    char tmp[96];
    int n=core_seen(), i;
    draw_background(c);

    /* Compact, legible header including version number. */
    round_rect(c,22,19,36,36,10,rgb(39,207,180));
    rect(c,38,27,5,20,rgb(9,38,42));
    rect(c,30,35,21,5,rgb(9,38,42));
    label(c,69,25,"BUTTON LAB",3,rgb(240,249,255));
    framed_round(c,266,22,64,27,9,rgb(59,95,103),rgb(22,45,57));
    centered(c,298,30,"V0.1.1",1,rgb(119,235,207));
    framed_round(c,484,23,134,39,14,rgb(59,92,104),rgb(22,43,57));
    snprintf(tmp,sizeof(tmp),"%02d / 15",n);
    centered(c,552,29,tmp,2,n==15 ? rgb(121,251,196) : rgb(167,237,219));
    centered(c,552,46,"CORE TESTED",1,rgb(143,175,189));

    /* Side panels flank the actual handheld; they are not gamepad elements. */
    panel(c,18,103,174,302);
    rect(c,32,120,4,22,rgb(50,233,195));
    label(c,45,124,"LIVE INPUT",2,rgb(221,236,248));
    label(c,33,169,"LAST BUTTON",1,rgb(132,157,184));
    if (last_button < 0) {
        centered(c,105,203,"READY",2,rgb(170,187,211));
        centered(c,105,237,"PRESS A BUTTON",1,rgb(121,147,174));
    } else {
        centered(c,105,201,buttons[last_button].label,2,rgb(114,242,202));
        rect(c,35,234,141,1,rgb(54,72,92));
        snprintf(tmp,sizeof(tmp),"PRESSES: %u",buttons[last_button].presses);
        centered(c,105,251,tmp,1,rgb(176,213,224));
        centered(c,105,277,buttons[last_button].down ? "HELD DOWN" : "RELEASED",1,
                 buttons[last_button].down ? rgb(122,237,192) : rgb(148,168,189));
    }
    rect(c,35,320,141,1,rgb(47,65,82));
    centered(c,105,338,"GREEN = DETECTED",1,rgb(130,194,181));
    centered(c,105,366,raw_input ? "ALL 15 TO PASS" : "SDL INPUT MODE",1,
             raw_input ? rgb(137,157,181) : rgb(238,169,119));

    panel(c,448,103,174,302);
    rect(c,461,120,4,22,rgb(50,233,195));
    label(c,475,124,"VOLUME KEYS",2,rgb(221,236,248));
    centered(c,535,169,"OPTIONAL EVENTS",1,rgb(131,157,185));
    status_chip(c,467,206,136,VOL_DOWN);
    status_chip(c,467,280,136,VOL_UP);
    centered(c,535,369,"SYSTEM MAY OVERRIDE",1,rgb(124,146,170));

    device_body(c);

    /* Bottom progress and gestures, always kept outside the drawing. */
    rect(c,20,419,600,1,rgb(48,69,88));
    label(c,25,429,n==15 ? "ALL BUTTONS DETECTED" : "TEST EACH BUTTON ONCE",1,
          n==15 ? rgb(115,244,199) : rgb(172,190,210));
    snprintf(tmp,sizeof(tmp),"%02d OF 15",n);
    label(c,546,429,tmp,1,rgb(145,175,196));
    for (i=0;i<15;++i)
        round_rect(c,27+i*40,449,33,5,2,i<n ? rgb(67,232,191) : rgb(53,69,90));

    /* Timed gestures show progress after 1 second. SELECT+START still
     * requires no hold and never displays an exit progress popup. */
    if (menu_since && now - menu_since >= HOLD_POPUP_DELAY_MS) {
        unsigned elapsed = now - menu_since;
        int part = (int)((elapsed >= MENU_EXIT_MS ? MENU_EXIT_MS : elapsed)*276u/MENU_EXIT_MS);
        framed_round(c,152,420,336,57,11,rgb(70,134,133),rgb(13,32,43));
        centered(c,320,429,"HOLD MENU TO EXIT",1,rgb(200,238,230));
        round_rect(c,182,452,276,6,3,rgb(46,65,82));
        round_rect(c,182,452,part,6,3,rgb(89,232,188));
    } else if (reset_since && now - reset_since >= HOLD_POPUP_DELAY_MS) {
        unsigned elapsed = now - reset_since;
        int part = (int)((elapsed >= RESET_MS ? RESET_MS : elapsed)*276u/RESET_MS);
        framed_round(c,152,420,336,57,11,rgb(70,134,133),rgb(13,32,43));
        centered(c,320,429,"HOLD TO RESET",1,rgb(200,238,230));
        round_rect(c,182,452,276,6,3,rgb(46,65,82));
        round_rect(c,182,452,part,6,3,rgb(89,232,188));
    } else {
        centered(c,320,463,"SELECT+START: EXIT   MENU 3S: EXIT   X+Y 2S: RESET",1,
                 rgb(136,162,184));
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
