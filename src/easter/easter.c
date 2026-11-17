/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Cucuzziello's Game -- a ten-level warehouse puzzle for Onion/RedOnion on the
 * Miyoo Mini family, by Amiga500. It takes the place of Neon Breaker as the
 * easter egg and keeps its look, its sound code and the way it is started.
 *
 * Standalone binary, launched the way Onion's easter egg is: something writes
 * a command to .tmp_update/cmd_to_run.sh and kills MainUI, runtime.sh runs us,
 * and brings MainUI back when we exit. We deliberately never write
 * cmd_to_run.sh ourselves, so returning is runtime.sh's job.
 *
 * The rules are the classic ones: walk into a crate to push it, one crate at
 * a time, never pull; a level is done when every crate is on a marker. The
 * levels are the first ten of the Microban set by David W. Skinner.
 *
 * Everything is drawn with SDL_FillRect. SDL 1.2 on this hardware has no cheap
 * per-pixel alpha: the walls and floor of a level are cached, and only cached
 * text uses alpha blending. Drawing in panel orientation avoids full-frame
 * rotation.
 */
#include "easter.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_loadso.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __linux__
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#endif

/* ----------------------------------------------------------------- layout */

#define SCREEN_W 640
#define SCREEN_H 480
#define HUD_H 40
#define FOOT_H 32     /* the line of hints under the board */

#define TILE_MAX 40   /* a cell, in pixels; smaller when a level needs it */
#define MAX_W 16
#define MAX_H 12

#define MAX_UNDO 4096
#define MAX_PARTS 96
#define STARS 44

/* Timers, in frames at 60 Hz. */
#define T_INTRO 110
#define T_CONFIRM 150   /* X once asks, X again within this restarts */
#define REPEAT_DELAY 14 /* a held direction or B repeats after this... */
#define REPEAT_RATE 6   /* ...and then this often */

/* ----------------------------------------------------------------- palette */

/* A saturated blue/magenta/amber neon ramp against the dark Onion-style UI.
 * Shared with Neon Breaker, plus two floor tones. */
typedef enum {
    C_BG,
    C_CHROME,
    C_CHROME_DIM,
    C_WHITE,
    C_BLUE,
    C_VIOLET,
    C_LAVENDER,
    C_MAGENTA,
    C_AMBER,
    C_RED,
    C_GREEN,
    C_STEEL,
    C_STEEL_DARK,
    C_PANEL,
    C_STAR,
    C_STAR_BRIGHT,
    C_FLOOR,
    C_FLOOR_LINE,
    C_COUNT
} Colour;

static const Uint8 palette[C_COUNT][3] = {
    {8, 10, 14},      /* background, Onion's near-black            */
    {214, 220, 228},  /* chrome                                     */
    {116, 132, 180},    /* dim chrome, separators                     */
    {255, 255, 255},  /* highlight                                  */
    {64, 112, 255},  /* electric blue: accents, title, push count   */
    {139, 92, 246},  /* the porter                                  */
    {216, 180, 254}, /* the porter's trim                           */
    {255, 46, 184},   /* markers                                     */
    {255, 192, 32},   /* crates                                      */
    {255, 74, 88},    /* errors                                      */
    {50, 255, 163},  /* a crate on its marker, level clear          */
    {132, 140, 152},  /* wall face                                   */
    {72, 78, 88},     /* wall inner                                  */
    {20, 24, 53},     /* dialog panels                              */
    {40, 48, 62},     /* dim stars                                  */
    {120, 136, 168},  /* bright stars                               */
    {14, 17, 30},     /* warehouse floor                            */
    {22, 27, 46},     /* floor joints                               */
};

static Uint32 mapped[C_COUNT];
static Uint32 shades[C_COUNT][6];


/* ----------------------------------------------------------------- levels */

typedef struct {
    const char *rows[MAX_H];
} Level;

/* Microban 1 to 10, by David W. Skinner, unchanged, in the usual notation:
 * '#' wall, '@' player, '$' crate, '.' marker, '*' crate on a marker,
 * '+' player on a marker, ' ' floor. To add levels, add them here: a level
 * may be up to MAX_W by MAX_H cells. */
static const Level levels[] = {
    {{"####",
      "# .#",
      "#  ###",
      "#*@  #",
      "#  $ #",
      "#  ###",
      "####"}},
    {{"######",
      "#    #",
      "# #@ #",
      "# $* #",
      "# .* #",
      "#    #",
      "######"}},
    {{"  ####",
      "###  ####",
      "#     $ #",
      "# #  #$ #",
      "# . .#@ #",
      "#########"}},
    {{"########",
      "#      #",
      "# .**$@#",
      "#      #",
      "#####  #",
      "    ####"}},
    {{" #######",
      " #     #",
      " # .$. #",
      "## $@$ #",
      "#  .$. #",
      "#      #",
      "########"}},
    {{"###### #####",
      "#    ###   #",
      "# $$     #@#",
      "# $ #...   #",
      "#   ########",
      "#####"}},
    {{"#######",
      "#     #",
      "# .$. #",
      "# $.$ #",
      "# .$. #",
      "# $.$ #",
      "#  @  #",
      "#######"}},
    {{"  ######",
      "  # ..@#",
      "  # $$ #",
      "  ## ###",
      "   # #",
      "   # #",
      "#### #",
      "#    ##",
      "# #   #",
      "#   # #",
      "###   #",
      "  #####"}},
    {{"#####",
      "#.  ##",
      "#@$$ #",
      "##   #",
      " ##  #",
      "  ##.#",
      "   ###"}},
    {{"      #####",
      "      #.  #",
      "      #.# #",
      "#######.# #",
      "# @ $ $ $ #",
      "# # # # ###",
      "#       #",
      "#########"}},
};

#define LEVEL_COUNT ((int)(sizeof levels / sizeof levels[0]))

/* ----------------------------------------------------------------- state */

/* What a cell holds, as bits: a marker can be under a crate or the player. */
enum { CELL_WALL = 1, CELL_FLOOR = 2, CELL_GOAL = 4, CELL_CRATE = 8 };

/* Directions, and B (undo) as a fifth "held" key. */
enum { D_LEFT, D_RIGHT, D_UP, D_DOWN, K_UNDO, K_COUNT };
static const int dir_x[4] = {-1, 1, 0, 0};
static const int dir_y[4] = {0, 0, -1, 1};

typedef struct {
    /* Position and velocity in 1/256 pixel. */
    int x, y, vx, vy, life;
    Colour colour;
} Particle;

typedef struct {
    int x, y;
    Colour colour;
} Star;

typedef enum { S_TITLE, S_PLAY, S_CLEAR, S_WIN } State;

typedef struct {
    State state;
    int frame;
    int level;       /* 1 to LEVEL_COUNT */
    int start_level; /* chosen on the title screen */

    /* The board: w by h cells of tile pixels, its top-left at (ox, oy). */
    int w, h, tile, ox, oy;
    Uint8 cell[MAX_H][MAX_W];
    int px, py, face;
    int goals, home;

    int moves, pushes;             /* this level */
    int total_moves, total_pushes; /* the levels finished so far */

    /* One byte per move: the direction, and 4 when it pushed a crate. */
    Uint8 undo[MAX_UNDO];
    int undo_count;

    int t_intro, t_confirm;
    int held, t_repeat;
    unsigned down; /* which of the held keys were down last frame */

    Particle parts[MAX_PARTS];
    Star stars[STARS];
    bool field_dirty;
    bool running;
} Game;

/* SDL objects. Onion's own apps keep an offscreen drawing surface and blit it
 * to the hardware video surface. Primitives and cached text are oriented for
 * the inverted panel at draw time, so presentation is just one opaque blit. */
static SDL_Surface *video;
static SDL_Surface *screen;
static SDL_Surface *field_cache;
static TTF_Font *font_big;
static TTF_Font *font_small;

static int fixed(int pixels) { return pixels * 256; }
static int whole(int value) { return value / 256; }

/* ----------------------------------------------------------------- logging */

/* Onion's convention: quiet unless .tmp_update/config/.logging exists. */
static FILE *logfile;

static void log_open(void)
{
    if (access("/mnt/SDCARD/.tmp_update/config/.logging", F_OK) != 0) {
        return;
    }
    logfile = fopen("/mnt/SDCARD/.tmp_update/logs/easter.log", "a");
}

static void say(const char *format, ...)
{
    if (!logfile) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    vfprintf(logfile, format, arguments);
    va_end(arguments);
    fputc('\n', logfile);
    fflush(logfile);
}

/* ----------------------------------------------------------------- drawing */

static void rotate_frame(SDL_Surface *surface);

/* Render in panel orientation: avoid a 307,200-pixel reversal every frame.
 * SDL clips negative coordinates after the transform. */
static void fill(int x, int y, int w, int h, Uint32 colour)
{
    if (w <= 0 || h <= 0) return;
    SDL_Rect rect = {(Sint16)(SCREEN_W - x - w),
                     (Sint16)(SCREEN_H - y - h), (Uint16)w, (Uint16)h};
    SDL_FillRect(screen, &rect, colour);
}

static void box(int x, int y, int w, int h, Colour colour)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    fill(x, y, w, h, mapped[colour]);
}

/* Bounded cache: rasterize labels only when their content changes. */
#define TEXT_CACHE_SIZE 64
typedef struct {
    TTF_Font *font;
    Colour colour;
    char label[128];
    SDL_Surface *surface;
} TextCache;
static TextCache text_cache[TEXT_CACHE_SIZE];
static unsigned text_slot;

static void text(TTF_Font *font, const char *string, int x, int y, Colour colour)
{
    if (!font || !string || !*string) return;
    SDL_Surface *rendered = NULL;
    for (int i = 0; i < TEXT_CACHE_SIZE; ++i) {
        TextCache *entry = &text_cache[i];
        if (entry->surface && entry->font == font && entry->colour == colour &&
            !strcmp(entry->label, string)) {
            rendered = entry->surface;
            break;
        }
    }
    if (!rendered) {
        SDL_Color fg = {palette[colour][0], palette[colour][1], palette[colour][2], 0};
        SDL_Surface *raw = TTF_RenderUTF8_Blended(font, string, fg);
        if (!raw) return;
        rendered = SDL_DisplayFormatAlpha(raw);
        SDL_FreeSurface(raw);
        if (!rendered) return;
        rotate_frame(rendered); /* Once per cached label, preserving alpha. */
        TextCache *entry = &text_cache[text_slot++ % TEXT_CACHE_SIZE];
        if (entry->surface) SDL_FreeSurface(entry->surface);
        entry->font = font;
        entry->colour = colour;
        snprintf(entry->label, sizeof entry->label, "%s", string);
        entry->surface = rendered;
    }
    SDL_Rect at = {(Sint16)(SCREEN_W - x - rendered->w),
                   (Sint16)(SCREEN_H - y - rendered->h), 0, 0};
    SDL_BlitSurface(rendered, NULL, screen, &at);
}

static void text_centre(TTF_Font *font, const char *string, int y, Colour colour)
{
    if (!font || !string) {
        return;
    }
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, string, &w, &h) != 0) {
        return;
    }
    text(font, string, (SCREEN_W - w) / 2, y, colour);
}

/* The panel behind dialogs and the level card: a filled rectangle with a
 * one-pixel accent rule top and bottom, which is how Onion's own dialogs read. */
static void panel(int x, int y, int w, int h, Colour accent)
{
    box(x, y, w, h, C_PANEL);
    box(x, y, w, 2, accent);
    box(x, y + h - 2, w, 2, accent);
}

/* Reverse cached 32-bit glyphs once, or unrotate an exported screenshot. */
static void rotate_frame(SDL_Surface *surface)
{
    if (surface->format->BytesPerPixel != 4) {
        return;
    }
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) < 0) {
        return;
    }
    for (int y = 0; y < (surface->h + 1) / 2; ++y) {
        Uint32 *top = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
        Uint32 *bottom =
            (Uint32 *)((Uint8 *)surface->pixels + (surface->h - 1 - y) * surface->pitch);
        int limit = y == surface->h - 1 - y ? surface->w / 2 : surface->w;
        for (int x = 0; x < limit; ++x) {
            Uint32 pixel = top[x];
            top[x] = bottom[surface->w - 1 - x];
            bottom[surface->w - 1 - x] = pixel;
        }
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}

/* Precompute enveloped effects and an original music loop once. The callback
 * only mixes PCM: no allocation, file I/O, RNG or synthesis on that thread. */
typedef enum {
    FX_LAUNCH, FX_BOUNCE, FX_BRICK, FX_BOMB, FX_PICKUP, FX_LASER,
    FX_LOST, FX_CLEAR, FX_COUNT
} Sound;
#define AUDIO_RATE 48000
#define AUDIO_SAMPLES (AUDIO_RATE / 2)
#define AUDIO_VOICES 4
static Sint16 effects[FX_COUNT][AUDIO_SAMPLES];
static int effect_length[FX_COUNT];
static struct { int sound, position; } voices[AUDIO_VOICES];
static bool audio_ready;
static unsigned next_voice;
/* Mini Plus/SSD202D uses 48 kHz signed-16 stereo, as Onion MainUI does.
 * Store mono synth data and duplicate each mixed frame to both channels. */
static SDL_AudioSpec audio_spec;
static void audio_callback(void *unused, Uint8 *stream, int bytes);
static struct {
    void *module;
    bool opened, initialized;
    int (*open)(int, Uint16, int, int);
    void (*close)(void);
    int (*query)(int *, Uint16 *, int *);
    void (*hook)(void (*)(void *, Uint8 *, int), void *);
} mixer;
static SDL_mutex *audio_mutex;
/* SDL_mixer wraps SDL_OpenAudio, so it cannot repair a missing SDL backend.
 * On Linux/Miyoo use the firmware OSS /dev/dsp route when mixer fails. */
typedef enum { A_NONE, A_SDL_MIXER, A_OSS } AudioBackend;
static AudioBackend audio_backend;
#ifdef __linux__
static struct {
    int fd, failure_errno;
    Uint32 completed_buffers;
    bool running, paused;
    SDL_Thread *thread;
} oss = {.fd = -1};
#endif


static bool mixer_resolve(const char *name, void *function, size_t size)
{
    void *address = SDL_LoadFunction(mixer.module, name);
    if (!address || size != sizeof address) return false;
    memcpy(function, &address, size);
    return true;
}

/* The firmware mixer can use its own device thread. SDL_LockAudio does not
 * necessarily protect it; our PCM state has a separate shared mutex. */
static void audio_stop(void)
{
#ifdef __linux__
    if (oss.thread) {
        SDL_LockMutex(audio_mutex);
        oss.running = false;
        SDL_UnlockMutex(audio_mutex);
        SDL_WaitThread(oss.thread, NULL); /* nonblocking writes allow prompt join */
        oss.thread = NULL;
    }
    if (oss.fd >= 0) close(oss.fd);
    oss.fd = -1;
    oss.running = oss.paused = false;
    oss.failure_errno = 0;
    oss.completed_buffers = 0;
#endif
    if (mixer.opened) {
        mixer.hook(NULL, NULL);
        mixer.close(); /* Join playback before freeing callback state. */
    }
    audio_ready = false;
    audio_backend = A_NONE;
    if (mixer.initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if (mixer.module) SDL_UnloadObject(mixer.module);
    memset(&mixer, 0, sizeof mixer);
    if (audio_mutex) SDL_DestroyMutex(audio_mutex);
    audio_mutex = NULL;
}

static void audio_pause(bool paused)
{
    if (mixer.opened) mixer.hook(paused ? NULL : audio_callback, NULL);
#ifdef __linux__
    if (audio_backend == A_OSS) {
        SDL_LockMutex(audio_mutex);
        oss.paused = paused;
        SDL_UnlockMutex(audio_mutex);
    }
#endif
}
static Uint32 audio_callbacks;
static Uint32 audio_audible_frames;
static unsigned audio_peak;
static char audio_error[160];
static bool music_enabled = true;
#define MUSIC_STEP (AUDIO_RATE / 4) /* eighth notes, 120 BPM */
#define MUSIC_STEPS 64
#define MUSIC_SAMPLES (MUSIC_STEP * MUSIC_STEPS)
static Sint16 music_pcm[MUSIC_SAMPLES];
static unsigned music_position;

static int triangle_wave(unsigned phase)
{
    phase &= 65535u;
    return phase < 32768u ? (int)phase * 2 - 32768
                         : 98303 - (int)phase * 2;
}

/* Original eight-bar synth loop. Generate once, never on the audio thread.
 * Soft chord pads, a plucked arpeggio, bass, kick and brushed hi-hats. */
static void music_build(void)
{
    static const int chords[4][3] = {
        {220, 262, 330}, {175, 220, 262}, {262, 330, 392}, {196, 247, 294}
    };
    static const int melody[16] = {
        0, 1, 2, 1, 0, 2, 1, -1, 2, 1, 0, 1, 2, -1, 1, 2
    };
    unsigned noise = 0x735a2b1u;
    for (int step = 0; step < MUSIC_STEPS; ++step) {
        int chord = step / 16;
        int note = melody[step % 16];
        unsigned lead = 0, bass = 0, kick = 0, pad[3] = {0, 0, 0};
        int bass_hz = chords[chord][0] / 2;
        for (int i = 0; i < MUSIC_STEP; ++i) {
            int attack = i < 240 ? i * 256 / 240 : 256;
            int release = (MUSIC_STEP - i) * 256 / MUSIC_STEP;
            int mixed = 0;
            if (note >= 0) {
                lead += (unsigned)(chords[chord][note] * 2) * 65536u / AUDIO_RATE;
                mixed += triangle_wave(lead) / 14 * attack / 256 * release / 256;
            }
            bass += (unsigned)bass_hz * 65536u / AUDIO_RATE;
            mixed += triangle_wave(bass) / 18 * attack / 256 * release / 256;
            for (int j = 0; j < 3; ++j) {
                pad[j] += (unsigned)chords[chord][j] * 65536u / AUDIO_RATE;
                mixed += triangle_wave(pad[j]) / 80 * attack / 256 * release / 256;
            }
            if (step % 2 == 0) {
                kick += (unsigned)(45 + 100 * (MUSIC_STEP-i) / MUSIC_STEP) * 65536u / AUDIO_RATE;
                mixed += triangle_wave(kick) / 12 * attack / 256 * release / 256 * release / 256;
            }
            if (i < 1600) {
                noise = noise * 1664525u + 1013904223u;
                int hat = ((int)(noise >> 16) - 32768) / 30;
                mixed += hat * attack / 256 * (1600-i) / 1600;
            }
            music_pcm[step * MUSIC_STEP + i] = (Sint16)mixed;
        }
    }
    music_position = 0;
}


static void audio_callback(void *unused, Uint8 *stream, int bytes)
{
    (void)unused;
    memset(stream, 0, (size_t)bytes);
    Sint16 *out = (Sint16 *)stream;
    SDL_LockMutex(audio_mutex);
    audio_callbacks++;
    /* bytes counts interleaved stereo bytes, not mono sample frames. */
    for (int i = 0; i < bytes / (2 * (int)sizeof *out); ++i) {
        int mixed = 0;
        if (music_enabled) {
            mixed = music_pcm[music_position++];
            if (music_position == MUSIC_SAMPLES) music_position = 0;
        }
        for (int v = 0; v < AUDIO_VOICES; ++v) {
            int sound = voices[v].sound;
            int pos = voices[v].position;
            if (pos < effect_length[sound]) {
                mixed += effects[sound][pos];
                voices[v].position++;
            }
        }
        Sint16 sample = (Sint16)(mixed > 32767 ? 32767 : mixed < -32768 ? -32768 : mixed);
        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
        if (sample) audio_audible_frames++;
        unsigned magnitude = (unsigned)(sample < 0 ? -(int)sample : sample);
        if (magnitude > audio_peak) audio_peak = magnitude;
    }
    SDL_UnlockMutex(audio_mutex);
}

#ifdef __linux__
/* All PCM generation remains in audio_callback, exactly as for SDL_mixer.
 * The writer sends signed-16 stereo to OSS (or Miyoo's libpadsp proxy).
 * No file I/O, allocation or blocking occurs while holding audio_mutex. */
#define OSS_STALL_TIMEOUT_MS 2000u
static int oss_writer(void *unused)
{
    (void)unused;
    Uint8 pcm[4096];  /* 1024 frames at 48kHz */
    Uint32 deadline = SDL_GetTicks();
    unsigned timing_fraction = 0;
    for (;;) {
        SDL_LockMutex(audio_mutex);
        bool running = oss.running;
        bool paused = oss.paused;
        SDL_UnlockMutex(audio_mutex);
        if (!running) break;
        if (paused) {
            SDL_Delay(10);
            continue;
        }
        audio_callback(NULL, pcm, sizeof pcm);
        size_t written = 0;
        Uint32 last_progress = SDL_GetTicks();
        int failed = 0;
        while (written < sizeof pcm) {
            SDL_LockMutex(audio_mutex);
            running = oss.running;
            SDL_UnlockMutex(audio_mutex);
            if (!running) return 0;
            if (SDL_GetTicks() - last_progress >= OSS_STALL_TIMEOUT_MS) {
                failed = ETIMEDOUT;
                break;
            }
            /* Miyoo libpadsp documents interception of write/ioctl, not
             * select/poll on the virtual fd. Use nonblocking write only. */
            ssize_t count = write(oss.fd, pcm + written, sizeof pcm - written);
            if (count > 0) {
                written += (size_t)count;
                last_progress = SDL_GetTicks();
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                SDL_Delay(1);
                continue;
            }
            failed = count < 0 ? errno : EIO;
            break;
        }
        if (written != sizeof pcm) {
            SDL_LockMutex(audio_mutex);
            oss.failure_errno = failed;
            oss.running = false;
            SDL_UnlockMutex(audio_mutex);
            return 1;
        }
        SDL_LockMutex(audio_mutex);
        oss.completed_buffers++;
        SDL_UnlockMutex(audio_mutex);
        /* Some firmware /dev/dsp proxies accept writes immediately rather
         * than blocking for playback. Pace ourselves to 48 kHz, preventing
         * an accidental high-CPU audio spin or sped-up music. */
        timing_fraction += 1024 * 1000;
        deadline += timing_fraction / AUDIO_RATE;
        timing_fraction %= AUDIO_RATE;
        Sint32 sleep_ms = (Sint32)(deadline - SDL_GetTicks());
        if (sleep_ms > 0) SDL_Delay((Uint32)sleep_ms);
        else if (sleep_ms < -100) deadline = SDL_GetTicks();
    }
    return 0;
}

static bool audio_open_oss(char *error, size_t error_length)
{
    oss.fd = open("/dev/dsp", O_WRONLY | O_NONBLOCK);
    if (oss.fd < 0) {
        snprintf(error, error_length, "OSS /dev/dsp open: %s", strerror(errno));
        return false;
    }
    int format = AFMT_S16_LE, channels = 2, rate = AUDIO_RATE;
    if (ioctl(oss.fd, SNDCTL_DSP_SETFMT, &format) < 0 ||
        ioctl(oss.fd, SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        ioctl(oss.fd, SNDCTL_DSP_SPEED, &rate) < 0) {
        snprintf(error, error_length, "OSS /dev/dsp format ioctl: %s", strerror(errno));
        close(oss.fd);
        oss.fd = -1;
        return false;
    }
    if (format != AFMT_S16_LE || channels != 2 || rate != AUDIO_RATE) {
        snprintf(error, error_length, "OSS unexpected PCM format %d, %d ch, %d Hz",
                 format, channels, rate);
        close(oss.fd);
        oss.fd = -1;
        return false;
    }
    audio_mutex = SDL_CreateMutex();
    if (!audio_mutex) {
        snprintf(error, error_length, "OSS mutex: %s", SDL_GetError());
        close(oss.fd);
        oss.fd = -1;
        return false;
    }
    oss.running = true;
    oss.paused = false;
    oss.failure_errno = 0;
    oss.completed_buffers = 0;
    oss.thread = SDL_CreateThread(oss_writer, NULL);
    if (!oss.thread) {
        snprintf(error, error_length, "OSS thread: %s", SDL_GetError());
        audio_stop();
        return false;
    }
    audio_backend = A_OSS;
    audio_ready = true;
    fprintf(stderr, "Audio: backend=OSS /dev/dsp, 48000 Hz signed-16 stereo\n");
    say("Audio: backend=OSS /dev/dsp, 48000 Hz signed-16 stereo");
    return true;
}

/* Called from the game thread (not from the writer) to report write failure. */
static void audio_check_oss_failure(void)
{
    if (audio_backend != A_OSS) return;
    SDL_LockMutex(audio_mutex);
    int failed = oss.failure_errno;
    SDL_UnlockMutex(audio_mutex);
    if (failed && !audio_error[0]) {
        snprintf(audio_error, sizeof audio_error, "OSS write: %s", strerror(failed));
        fprintf(stderr, "Audio unavailable: %s\n", audio_error);
        say("Audio unavailable: %s", audio_error);
    }
}
#else
static void audio_check_oss_failure(void) { }
#endif

/* Generated PCM alone is not evidence of delivery to an OSS device. */
static bool audio_test_passed(void)
{
    if (!audio_ready || !audio_mutex) return false;
    SDL_LockMutex(audio_mutex);
    bool passed = audio_callbacks > 0 && audio_audible_frames > 0;
#ifdef __linux__
    if (audio_backend == A_OSS)
        passed = passed && oss.completed_buffers > 0 && !oss.failure_errno && oss.running;
#endif
    SDL_UnlockMutex(audio_mutex);
    return passed && !audio_error[0];
}
static void sound_play(Sound sound)
{
    if (!audio_ready) return;
    SDL_LockMutex(audio_mutex);
    int slot = -1;
    for (int v = 0; v < AUDIO_VOICES; ++v) {
        if (voices[v].position >= effect_length[voices[v].sound]) {
            slot = v;
            break;
        }
    }
    if (slot < 0) slot = (int)(next_voice++ % AUDIO_VOICES);
    voices[slot].sound = sound;
    voices[slot].position = 0;
    SDL_UnlockMutex(audio_mutex);
}

static void audio_start(void)
{
    audio_stop();
    audio_error[0] = '\0';
    audio_callbacks = 0;
    audio_audible_frames = 0;
    audio_peak = 0;
    if (music_enabled) music_build();
    static const int duration[FX_COUNT] = {120, 65, 90, 300, 300, 100, 400, 480};
    static const int start_hz[FX_COUNT] = {440, 780, 1050, 150, 660, 1800, 420, 523};
    static const int end_hz[FX_COUNT] = {880, 520, 650, 40, 1320, 160, 90, 1047};
    Uint32 noise = 0x12345678u;
    for (int s = 0; s < FX_COUNT; ++s) {
        int count = AUDIO_RATE * duration[s] / 1000;
        effect_length[s] = count;
        unsigned phase = 0;
        for (int i = 0; i < count; ++i) {
            int hz = start_hz[s] + (end_hz[s] - start_hz[s]) * i / count;
            if (s == FX_CLEAR || s == FX_PICKUP) {
                static const int notes[] = {523, 659, 784, 1047};
                hz = notes[i * 4 / count] + (s == FX_PICKUP ? 130 : 0);
            }
            phase = (phase + (unsigned)hz * 65536u / AUDIO_RATE) & 65535u;
            int triangle = phase < 32768 ? (int)phase * 2 - 32768
                                        : 98303 - (int)phase * 2;
            int wave = triangle / 5;
            if (s == FX_BOMB) {
                noise = noise * 1664525u + 1013904223u;
                wave = wave / 2 + ((int)(noise >> 16) - 32768) / 6;
            }
            int envelope = i < 110 ? i * 256 / 110 : (count - i) * 256 / (count - 110);
            effects[s][i] = (Sint16)(wave * envelope / 256);
        }
    }
    for (int v = 0; v < AUDIO_VOICES; ++v) {
        voices[v].sound = FX_LAUNCH;
        voices[v].position = effect_length[FX_LAUNCH];
    }
    /* SDL_mixer -> SDL_OpenAudio is first tried for ordinary SDL hosts.
     * Mini/Plus firmware often has no usable SDL audio backend; use OSS next.
     * Never fall back to SDL dummy, which would silently discard PCM. */
#ifdef _WIN32
    static const char *const libraries[] = {"SDL_mixer.dll", "SDL_mixer-1.2.dll"};
#else
    static const char *const libraries[] = {
        "libSDL_mixer-1.2.so.0", "libSDL_mixer-1.2.so", "libSDL_mixer.so"
    };
#endif
    char mixer_error[160] = "SDL_mixer not found";
    const char *library = NULL;
    for (size_t i = 0; i < sizeof libraries / sizeof libraries[0]; ++i) {
        mixer.module = SDL_LoadObject(libraries[i]);
        if (mixer.module) {
            library = libraries[i];
            break;
        }
    }
    if (mixer.module) {
#define LOAD(member, name) mixer_resolve(name, &mixer.member, sizeof mixer.member)
        bool linked = LOAD(open, "Mix_OpenAudio") && LOAD(close, "Mix_CloseAudio") &&
                      LOAD(query, "Mix_QuerySpec") && LOAD(hook, "Mix_HookMusic");
#undef LOAD
        if (!linked) {
            snprintf(mixer_error, sizeof mixer_error, "SDL_mixer symbol lookup: %.100s", SDL_GetError());
        } else {
            audio_mutex = SDL_CreateMutex();
            if (!audio_mutex) {
                snprintf(mixer_error, sizeof mixer_error, "SDL mutex: %.100s", SDL_GetError());
            } else if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
                snprintf(mixer_error, sizeof mixer_error, "SDL audio init: %.100s", SDL_GetError());
            } else {
                mixer.initialized = true;
                if (mixer.open(AUDIO_RATE, AUDIO_S16SYS, 2, 1024) != 0) {
                    snprintf(mixer_error, sizeof mixer_error, "Mix_OpenAudio: %.100s", SDL_GetError());
                } else {
                    mixer.opened = true;
                    int rate = 0, channels = 0;
                    Uint16 format = 0;
                    if (!mixer.query(&rate, &format, &channels) ||
                        rate != AUDIO_RATE || format != AUDIO_S16SYS || channels != 2) {
                        snprintf(mixer_error, sizeof mixer_error,
                                 "SDL mixer format: %d Hz / %d ch / 0x%x", rate, channels, format);
                    } else {
                        memset(&audio_spec, 0, sizeof audio_spec);
                        audio_spec.freq = rate;
                        audio_spec.format = format;
                        audio_spec.channels = (Uint8)channels;
                        audio_backend = A_SDL_MIXER;
                        audio_ready = true;
                        fprintf(stderr, "Audio: backend=SDL_mixer library=%s rate=%d channels=%d\n",
                                library, rate, channels);
                        say("Audio: backend=SDL_mixer rate=%d channels=%d", rate, channels);
                        sound_play(FX_LAUNCH);
                        audio_pause(false);
                        return;
                    }
                }
            }
        }
    }
    fprintf(stderr, "Audio: SDL_mixer unavailable (%s); trying OSS /dev/dsp\n", mixer_error);
    say("Audio: SDL_mixer unavailable: %s", mixer_error);
    /* Teardown SDL audio before using the independent OSS output path. */
    audio_stop();
#ifdef __linux__
    char oss_error[160];
    if (audio_open_oss(oss_error, sizeof oss_error)) {
        sound_play(FX_LAUNCH);
        return;
    }
    snprintf(audio_error, sizeof audio_error, "%.65s; %.65s", mixer_error, oss_error);
#else
    snprintf(audio_error, sizeof audio_error, "%s", mixer_error);
#endif
    fprintf(stderr, "Audio unavailable: %s\n", audio_error);
    say("Audio unavailable: %s", audio_error);

}

/* ----------------------------------------------------------------- helpers */

/* ----------------------------------------------------------------- helpers */

static void text_right(TTF_Font *font, const char *string, int right, int y, Colour colour)
{
    int w = 0, h = 0;
    if (!font || !string || TTF_SizeUTF8(font, string, &w, &h) != 0) {
        return;
    }
    text(font, string, right - w, y, colour);
}

static void spawn_particles(Game *game, int x, int y, Colour colour, int count)
{
    for (int i = 0, made = 0; i < MAX_PARTS && made < count; i++) {
        if (game->parts[i].life > 0) {
            continue;
        }
        game->parts[i].x = fixed(x);
        game->parts[i].y = fixed(y);
        game->parts[i].vx = (rand() % 1024) - 512;
        game->parts[i].vy = (rand() % 1024) - 716;
        game->parts[i].life = 14 + rand() % 12;
        game->parts[i].colour = colour;
        made++;
    }
}

static void tick_particles(Game *game)
{
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *part = &game->parts[i];
        if (part->life <= 0) {
            continue;
        }
        part->x += part->vx;
        part->y += part->vy;
        part->vy += 18; /* gravity, 1/256 px per frame squared */
        part->life--;
    }
}

/* ----------------------------------------------------------------- rules */

static bool inside(const Game *game, int x, int y)
{
    return x >= 0 && y >= 0 && x < game->w && y < game->h;
}

/* Can the player, or a crate, stand on this cell (crates aside)? */
static bool open_cell(const Game *game, int x, int y)
{
    return inside(game, x, y) && (game->cell[y][x] & CELL_FLOOR);
}

static int count_home(const Game *game)
{
    int count = 0;
    for (int y = 0; y < game->h; y++) {
        for (int x = 0; x < game->w; x++) {
            if ((game->cell[y][x] & (CELL_CRATE | CELL_GOAL)) == (CELL_CRATE | CELL_GOAL)) {
                count++;
            }
        }
    }
    return count;
}

/* The floor is what the player can reach with the crates taken away: the
 * blanks outside the walls of a level stay empty and show the stars. */
static void mark_floor(Game *game)
{
    int stack[MAX_W * MAX_H][2], top = 0;
    stack[top][0] = game->px;
    stack[top][1] = game->py;
    top++;
    while (top > 0) {
        top--;
        int x = stack[top][0], y = stack[top][1];
        if (!inside(game, x, y) || (game->cell[y][x] & (CELL_WALL | CELL_FLOOR))) {
            continue;
        }
        game->cell[y][x] |= CELL_FLOOR;
        for (int d = 0; d < 4; d++) {
            int nx = x + dir_x[d], ny = y + dir_y[d];
            if (inside(game, nx, ny) && !(game->cell[ny][nx] & (CELL_WALL | CELL_FLOOR)) &&
                top < MAX_W * MAX_H) {
                stack[top][0] = nx;
                stack[top][1] = ny;
                top++;
            }
        }
    }
}

static void load_level(Game *game, int index)
{
    const Level *level = &levels[index];
    memset(game->cell, 0, sizeof game->cell);
    memset(game->parts, 0, sizeof game->parts);
    game->w = game->h = 0;
    game->px = game->py = 0;
    game->goals = 0;
    for (int y = 0; y < MAX_H && level->rows[y]; y++) {
        const char *row = level->rows[y];
        game->h = y + 1;
        for (int x = 0; x < MAX_W && row[x]; x++) {
            Uint8 cell = 0;
            switch (row[x]) {
            case '#': cell = CELL_WALL; break;
            case '$': cell = CELL_CRATE; break;
            case '.': cell = CELL_GOAL; break;
            case '*': cell = CELL_GOAL | CELL_CRATE; break;
            case '+': cell = CELL_GOAL; /* fall through */
            case '@': game->px = x; game->py = y; break;
            default: break;
            }
            if (cell & CELL_GOAL) {
                game->goals++;
            }
            game->cell[y][x] = cell;
            if (x + 1 > game->w) {
                game->w = x + 1;
            }
        }
    }
    mark_floor(game);

    /* The largest even cell that fits the board between the HUD and the
     * hints, with a little margin; 40 pixels for most levels. */
    int tile = TILE_MAX;
    if (game->w > 0 && (SCREEN_W - 16) / game->w < tile) {
        tile = (SCREEN_W - 16) / game->w;
    }
    if (game->h > 0 && (SCREEN_H - HUD_H - FOOT_H - 8) / game->h < tile) {
        tile = (SCREEN_H - HUD_H - FOOT_H - 8) / game->h;
    }
    game->tile = tile & ~1;
    game->ox = (SCREEN_W - game->w * game->tile) / 2;
    game->oy = HUD_H + (SCREEN_H - HUD_H - FOOT_H - game->h * game->tile) / 2;

    game->level = index + 1;
    game->face = D_DOWN;
    game->home = count_home(game);
    game->moves = game->pushes = 0;
    game->undo_count = 0;
    game->t_intro = T_INTRO;
    game->t_confirm = 0;
    game->held = -1;
    game->field_dirty = true;
    game->state = S_PLAY;
    say("level %d: %dx%d cells of %d px, %d crates", game->level, game->w, game->h,
        game->tile, game->goals);
}

static void new_game(Game *game)
{
    game->total_moves = game->total_pushes = 0;
    load_level(game, game->start_level - 1);
}

static void remember(Game *game, Uint8 move)
{
    if (game->undo_count == MAX_UNDO) {
        /* Full: forget the oldest half, the newest moves stay undoable. */
        memmove(game->undo, game->undo + MAX_UNDO / 2, MAX_UNDO / 2);
        game->undo_count = MAX_UNDO / 2;
    }
    game->undo[game->undo_count++] = move;
}

/* One step. Walking into a crate pushes it if the cell behind it is free. */
static void step(Game *game, int dir)
{
    game->face = dir;
    game->t_intro = 0;
    game->t_confirm = 0;
    int nx = game->px + dir_x[dir], ny = game->py + dir_y[dir];
    if (!open_cell(game, nx, ny)) {
        return;
    }
    Uint8 move = (Uint8)dir;
    if (game->cell[ny][nx] & CELL_CRATE) {
        int bx = nx + dir_x[dir], by = ny + dir_y[dir];
        if (!open_cell(game, bx, by) || (game->cell[by][bx] & CELL_CRATE)) {
            return;
        }
        game->cell[ny][nx] &= (Uint8)~CELL_CRATE;
        game->cell[by][bx] |= CELL_CRATE;
        game->pushes++;
        move |= 4;
        if (game->cell[by][bx] & CELL_GOAL) {
            sound_play(FX_PICKUP);
            spawn_particles(game, game->ox + bx * game->tile + game->tile / 2,
                            game->oy + by * game->tile + game->tile / 2, C_GREEN, 14);
        }
        else {
            sound_play(FX_BOUNCE);
        }
    }
    game->px = nx;
    game->py = ny;
    game->moves++;
    remember(game, move);
    game->home = count_home(game);

    if (game->goals > 0 && game->home == game->goals) {
        game->total_moves += game->moves;
        game->total_pushes += game->pushes;
        game->state = game->level >= LEVEL_COUNT ? S_WIN : S_CLEAR;
        sound_play(FX_CLEAR);
        say("level %d solved in %d moves, %d pushes", game->level, game->moves, game->pushes);
    }
}

/* Takes the last move back, the crate with it if it was pushed. */
static void undo(Game *game)
{
    game->t_intro = 0;
    game->t_confirm = 0;
    if (game->undo_count <= 0) {
        return;
    }
    Uint8 move = game->undo[--game->undo_count];
    int dir = move & 3;
    if (move & 4) {
        int bx = game->px + dir_x[dir], by = game->py + dir_y[dir];
        game->cell[by][bx] &= (Uint8)~CELL_CRATE;
        game->cell[game->py][game->px] |= CELL_CRATE;
        game->pushes--;
    }
    game->px -= dir_x[dir];
    game->py -= dir_y[dir];
    game->face = dir;
    game->moves--;
    game->home = count_home(game);
}

static void command(Game *game, int key)
{
    if (game->state != S_PLAY) {
        return;
    }
    if (key == K_UNDO) {
        undo(game);
    }
    else if (key >= 0 && key < 4) {
        step(game, key);
    }
}

/* A: start, continue. */
static void action(Game *game)
{
    switch (game->state) {
    case S_TITLE:
        new_game(game);
        sound_play(FX_LAUNCH);
        break;
    case S_PLAY:
        game->t_intro = 0; /* puts the level card away */
        break;
    case S_CLEAR:
        load_level(game, game->level); /* level is 1-based: this is the next one */
        break;
    case S_WIN:
        game->state = S_TITLE;
        break;
    }
}

/* X: start the level again. Asked twice, so a slip of the thumb costs nothing. */
static void restart(Game *game)
{
    if (game->state != S_PLAY) {
        return;
    }
    if (game->t_confirm > 0) {
        load_level(game, game->level - 1);
        game->t_intro = 0;
        sound_play(FX_LOST);
    }
    else {
        game->t_intro = 0;
        game->t_confirm = T_CONFIRM;
    }
}

/* LEFT/RIGHT on the title screen: which level A starts from. */
static void choose_level(Game *game, int change)
{
    if (game->state != S_TITLE) {
        return;
    }
    game->start_level += change;
    if (game->start_level < 1) {
        game->start_level = LEVEL_COUNT;
    }
    if (game->start_level > LEVEL_COUNT) {
        game->start_level = 1;
    }
}

static const SDLKey held_keys[K_COUNT] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN,
                                          SDLK_LCTRL /* B */};

/* Once per frame. A direction or B acts when pressed, and again and again
 * while it stays down; a newly pressed key takes over from the held one. */
static void update(Game *game, const Uint8 *keys)
{
    game->frame++;
    tick_particles(game);
    if (game->t_intro > 0) {
        game->t_intro--;
    }
    if (game->t_confirm > 0) {
        game->t_confirm--;
    }

    unsigned down = 0;
    for (int k = 0; k < K_COUNT; k++) {
        if (keys[held_keys[k]]) {
            down |= 1u << k;
        }
    }
    unsigned fresh = down & ~game->down;
    game->down = down;
    if (game->state != S_PLAY) {
        game->held = -1;
        return;
    }
    if (fresh) {
        for (int k = 0; k < K_COUNT; k++) {
            if (fresh & (1u << k)) {
                game->held = k;
                game->t_repeat = REPEAT_DELAY;
                command(game, k);
                break;
            }
        }
    }
    else if (game->held >= 0 && (down & (1u << game->held))) {
        if (--game->t_repeat <= 0) {
            game->t_repeat = REPEAT_RATE;
            command(game, game->held);
        }
    }
    else {
        game->held = -1;
    }
}

/* ----------------------------------------------------------------- drawing */

/* The pieces are designed on a 40-pixel cell and scaled to the level's. */
static int piece_tile = TILE_MAX;
#define P(v) ((v) * piece_tile / TILE_MAX)

static void bands(int x, int y, int w, int h, Colour face)
{
    /* Precomputed lighting, no alpha blending per frame. */
    for (int band = 0; band < 6; ++band) {
        int top = band * h / 6, bottom = (band + 1) * h / 6;
        fill(x, y + top, w, bottom - top, shades[face][band]);
    }
}

static void draw_floor(int x, int y)
{
    int t = piece_tile;
    box(x, y, t, t, C_FLOOR);
    box(x, y, t, 1, C_FLOOR_LINE);
    box(x, y, 1, t, C_FLOOR_LINE);
}

/* Neon Breaker's steel brick, as a wall. */
static void draw_wall(int x, int y)
{
    int t = piece_tile;
    bands(x + 1, y + 1, t - 2, t - 2, C_STEEL);
    box(x + 1, y + 1, t - 2, 2, C_WHITE);
    box(x + 1, y + t - 3, t - 2, 2, C_BG);
    box(x + P(6), y + P(7), t - 2 * P(6), t - 2 * P(7), C_STEEL_DARK);
    box(x + P(10), y + P(11), P(8), 2, C_CHROME);
}

/* A marker: four corners and a dot, on the floor. */
static void draw_goal(int x, int y)
{
    int t = piece_tile, a = P(8), arm = P(8), far = t - a;
    draw_floor(x, y);
    box(x + a, y + a, arm, 2, C_MAGENTA);
    box(x + a, y + a, 2, arm, C_MAGENTA);
    box(x + far - arm, y + a, arm, 2, C_MAGENTA);
    box(x + far - 2, y + a, 2, arm, C_MAGENTA);
    box(x + a, y + far - 2, arm, 2, C_MAGENTA);
    box(x + a, y + far - arm, 2, arm, C_MAGENTA);
    box(x + far - arm, y + far - 2, arm, 2, C_MAGENTA);
    box(x + far - 2, y + far - arm, 2, arm, C_MAGENTA);
    box(x + t / 2 - P(3), y + t / 2 - P(3), 2 * P(3), 2 * P(3), C_MAGENTA);
}

/* Amber with crossed straps; green with a tick once it is on a marker. */
static void draw_crate(int x, int y, bool home)
{
    int t = piece_tile, m = P(4), side = t - 2 * m;
    Colour face = home ? C_GREEN : C_AMBER;
    bands(x + m, y + m, side, side - 2, face);
    box(x + m, y + m, side, 2, C_WHITE);
    box(x + m, y + m + side - 2, side, 2, C_BG);
    int ix = x + P(9), iy = y + P(10), iw = t - 2 * P(9), ih = t - 2 * P(10);
    box(ix, iy, iw, ih, C_PANEL);
    if (home) {
        int u = P(4);
        box(x + P(13), y + P(20), u, u, face);
        box(x + P(17), y + P(23), u, u, face);
        box(x + P(21), y + P(19), u, u, face);
        box(x + P(25), y + P(15), u, u, face);
    }
    else {
        box(ix, y + t / 2 - P(2), iw, 2 * P(2), face);
        box(x + t / 2 - P(2), iy, 2 * P(2), ih, face);
    }
}

/* The porter: the violet of Neon Breaker's paddle, eyes where he is going. */
static void draw_player(int x, int y, int face)
{
    int bx = x + P(8), by = y + P(9), bw = piece_tile - 2 * P(8), bh = P(24);
    bands(bx, by, bw, bh, C_VIOLET);
    box(bx, by, bw, 2, C_WHITE);
    box(bx, by, P(3), bh, C_LAVENDER);
    box(bx + bw - P(3), by, P(3), bh, C_LAVENDER);
    box(x + P(11), y + P(4), piece_tile - 2 * P(11), P(6), C_LAVENDER);
    box(x + P(12), y + P(33), P(6), P(4), C_CHROME_DIM);
    box(x + piece_tile - P(12) - P(6), y + P(33), P(6), P(4), C_CHROME_DIM);
    if (face != D_UP) {
        int ex = P(2) * dir_x[face], ey = P(2) * dir_y[face], eye = P(5);
        int left = x + P(13) + ex, right = x + piece_tile - P(13) - eye + ex;
        box(left, y + P(16) + ey, eye, eye, C_WHITE);
        box(right, y + P(16) + ey, eye, eye, C_WHITE);
        box(left + P(2) + dir_x[face], y + P(18) + ey, P(2), P(3), C_BG);
        box(right + P(2) + dir_x[face], y + P(18) + ey, P(2), P(3), C_BG);
    }
}

static void draw_stars(Game *game)
{
    for (int i = 0; i < STARS; i++) {
        box(game->stars[i].x, game->stars[i].y, 2, 2, game->stars[i].colour);
    }
}

/* What never moves in a level: stars, floor, markers and walls. */
static void draw_board(Game *game)
{
    SDL_FillRect(screen, NULL, mapped[C_BG]);
    draw_stars(game);
    piece_tile = game->tile;
    for (int y = 0; y < game->h; y++) {
        for (int x = 0; x < game->w; x++) {
            int sx = game->ox + x * game->tile, sy = game->oy + y * game->tile;
            Uint8 cell = game->cell[y][x];
            if (cell & CELL_WALL) {
                draw_wall(sx, sy);
            }
            else if (cell & CELL_GOAL) {
                draw_goal(sx, sy);
            }
            else if (cell & CELL_FLOOR) {
                draw_floor(sx, sy);
            }
        }
    }
}

/* Rebuilt only when a level is loaded, then one blit per frame. */
static void draw_field(Game *game)
{
    if (!field_cache) {
        draw_board(game);
        return;
    }
    if (game->field_dirty) {
        SDL_Surface *frame = screen;
        screen = field_cache;
        draw_board(game);
        screen = frame;
        game->field_dirty = false;
    }
    SDL_BlitSurface(field_cache, NULL, screen, NULL);
}

static void draw_hud(Game *game)
{
    char buffer[64];
    box(0, 0, SCREEN_W, HUD_H, C_PANEL);
    box(0, HUD_H - 2, SCREEN_W, 2, C_CHROME_DIM);
    snprintf(buffer, sizeof buffer, "MOVES %03d", game->moves);
    text(font_small, buffer, 14, 8, C_CHROME);
    snprintf(buffer, sizeof buffer, "PUSHES %03d", game->pushes);
    text_centre(font_small, buffer, 8, C_BLUE);
    snprintf(buffer, sizeof buffer, "LV %d/%d", game->level, LEVEL_COUNT);
    text_right(font_small, buffer, SCREEN_W - 14, 8, C_AMBER);
}

static void draw_playfield(Game *game)
{
    char buffer[64];
    draw_field(game);
    piece_tile = game->tile;
    for (int y = 0; y < game->h; y++) {
        for (int x = 0; x < game->w; x++) {
            if (game->cell[y][x] & CELL_CRATE) {
                draw_crate(game->ox + x * game->tile, game->oy + y * game->tile,
                           (game->cell[y][x] & CELL_GOAL) != 0);
            }
        }
    }
    draw_player(game->ox + game->px * game->tile, game->oy + game->py * game->tile,
                game->face);
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *part = &game->parts[i];
        if (part->life > 0) {
            box(whole(part->x), whole(part->y), 3, 3, part->colour);
        }
    }

    /* The hints, and how many crates are on a marker. */
    int y = SCREEN_H - FOOT_H + 2;
    if (game->state == S_PLAY) {
        text(font_small, "B undo", 14, y, C_CHROME_DIM);
        if (game->t_confirm > 0) {
            text_right(font_small, "X again to restart", SCREEN_W - 14, y, C_AMBER);
        }
        else {
            text_right(font_small, "X restart", SCREEN_W - 14, y, C_CHROME_DIM);
        }
    }
    snprintf(buffer, sizeof buffer, "%d / %d", game->home, game->goals);
    text_centre(font_small, buffer, y, game->home == game->goals ? C_GREEN : C_CHROME_DIM);

    if (game->t_intro > 0 && game->state == S_PLAY) {
        panel(0, 204, SCREEN_W, 84, C_BLUE);
        snprintf(buffer, sizeof buffer, "LEVEL %d", game->level);
        text_centre(font_small, buffer, 216, C_AMBER);
        snprintf(buffer, sizeof buffer, "microban %d", game->level);
        text_centre(font_big, buffer, 238, C_CHROME);
    }
}

static void draw_title(Game *game)
{
    char buffer[64];
    draw_stars(game);
    text_centre(font_big, "Cucuzziello's", 40, C_BLUE);
    text_centre(font_big, "Game", 86, C_MAGENTA);
    text_centre(font_small, "by Amiga500", 138, C_AMBER);
    text_centre(font_small, "push every crate onto a marker", 172, C_CHROME_DIM);
    text_centre(font_small, "levels: Microban by David W. Skinner", 198, C_CHROME_DIM);
    box(160, 236, 320, 2, C_CHROME_DIM);

    /* The rule in one strip: porter, crate, marker, crate at home, wall. */
    int x = (SCREEN_W - 7 * TILE_MAX) / 2, y = 250;
    piece_tile = TILE_MAX;
    for (int i = 0; i < 7; i++) {
        draw_floor(x + i * TILE_MAX, y);
    }
    draw_player(x, y, D_RIGHT);
    draw_crate(x + TILE_MAX, y, false);
    draw_goal(x + 3 * TILE_MAX, y);
    draw_crate(x + 5 * TILE_MAX, y, true);
    draw_wall(x + 6 * TILE_MAX, y);

    text_centre(font_small, "A  start", 308, C_CHROME);
    snprintf(buffer, sizeof buffer, "LEFT/RIGHT  level %d/%d", game->start_level, LEVEL_COUNT);
    text_centre(font_small, buffer, 336, C_AMBER);
    text_centre(font_small, "D-PAD move    B undo    X restart level", 392, C_CHROME_DIM);
    text_centre(font_small, "MENU  quit", 418, C_CHROME_DIM);
}

static void draw(Game *game)
{
    if (game->state == S_TITLE) {
        SDL_FillRect(screen, NULL, mapped[C_BG]);
        draw_title(game);
    }
    else {
        draw_playfield(game);
        draw_hud(game);
        char buffer[64];
        if (game->state == S_CLEAR) {
            panel(90, 176, 460, 130, C_GREEN);
            snprintf(buffer, sizeof buffer, "LEVEL %d CLEAR", game->level);
            text_centre(font_big, buffer, 192, C_GREEN);
            snprintf(buffer, sizeof buffer, "%d moves   %d pushes", game->moves, game->pushes);
            text_centre(font_small, buffer, 238, C_AMBER);
            text_centre(font_small, "A  continue", 268, C_CHROME);
        }
        else if (game->state == S_WIN) {
            panel(70, 160, 500, 160, C_AMBER);
            text_centre(font_big, "ALL CLEAR", 176, C_AMBER);
            text_centre(font_small, "every crate is where it belongs", 224, C_CHROME);
            snprintf(buffer, sizeof buffer, "%d moves   %d pushes", game->total_moves,
                     game->total_pushes);
            text_centre(font_small, buffer, 250, C_GREEN);
            text_centre(font_small, "A  title", 284, C_CHROME_DIM);
        }
    }

    if (audio_error[0]) {
        panel(0, SCREEN_H - 27, SCREEN_W, 27, C_RED);
        text_centre(font_small, "AUDIO UNAVAILABLE - run --audio-test", SCREEN_H - 26, C_WHITE);
    }
    /* One opaque full-frame blit also covers single-buffer backends. */
    SDL_Rect at = {0, 0, 0, 0};
    SDL_BlitSurface(screen, NULL, video, &at);
    SDL_Flip(video);
}

#ifdef __linux__
/* Miyoo's audioserver handles OSS writes via libpadsp.so. MainUI launched
 * binaries may not inherit MainUI's LD_PRELOAD, so explicitly restore it
 * for this process only, prior to initializing SDL. Never kill/restart the
 * global audio server, and never alter the system or SD-card launch script. */
static bool audioserver_running(void)
{
    DIR *dir = opendir("/proc");
    if (!dir) return false;
    struct dirent *entry;
    bool found = false;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        char path[80], name[40] = {0};
        long pid = strtol(entry->d_name, NULL, 10);
        if (pid <= 0) continue;
        snprintf(path, sizeof path, "/proc/%ld/comm", pid);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;
        if (fgets(name, sizeof name, fp) &&
            (!strncmp(name, "audioserver", 11))) found = true;
        fclose(fp);
        if (found) break;
    }
    closedir(dir);
    return found;
}

static void preload_miyoo_audiobridge(int argc, char **argv, bool audio_requested)
{
    (void)argc;
    if (!audio_requested || access("/customer/lib/libpadsp.so", R_OK) != 0 ||
        !audioserver_running()) return;
    const char *preload = getenv("LD_PRELOAD");
    const char *dummy = getenv("SDL_AUDIODRIVER");
    const char *disabled = getenv("PADSP_NO_DSP");
    if ((preload && strstr(preload, "libpadsp.so")) ||
        (dummy && !strcmp(dummy, "dummy")) ||
        (disabled && !strcmp(disabled, "1"))) return;
    char with_bridge[1024];
    int n = snprintf(with_bridge, sizeof with_bridge, "%s%s/customer/lib/libpadsp.so",
                     preload ? preload : "", (preload && *preload) ? " " : "");
    if (n < 0 || (size_t)n >= sizeof with_bridge) return;
    char original_preload[1024];
    if (preload) snprintf(original_preload, sizeof original_preload, "%s", preload);
    if (setenv("LD_PRELOAD", with_bridge, 1) != 0) return;
    fprintf(stderr, "Audio: relaunching with Miyoo audioserver bridge: %s\n", with_bridge);
    execvp(argv[0], argv);
    fprintf(stderr, "Audio: bridge relaunch failed: %s\n", strerror(errno));
    /* Avoid unexpectedly inheriting a preload which has not been applied. */
    if (preload) setenv("LD_PRELOAD", original_preload, 1);
    else unsetenv("LD_PRELOAD");
}
#else
static void preload_miyoo_audiobridge(int argc, char **argv, bool audio_requested)
{ (void)argc; (void)argv; (void)audio_requested; }
#endif

/* ----------------------------------------------------------------- startup */

/* Onion's own apps hunt for a usable face rather than assuming one. These are
 * the paths Onion ships; the last two are for a host build. */
static TTF_Font *open_font(int size)
{
    static const char *const candidates[] = {
        "/mnt/SDCARD/miyoo/app/Exo-2-Bold-Italic.ttf",
        "/customer/app/Exo-2-Bold-Italic.ttf",
        "/mnt/SDCARD/miyoo/app/BPreplayBold.otf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
#ifdef _WIN32
        "C:/Windows/Fonts/segoeuib.ttf",
#endif
    };
    for (size_t i = 0; i < sizeof candidates / sizeof candidates[0]; i++) {
        TTF_Font *font = TTF_OpenFont(candidates[i], size);
        if (font) {
            say("font %s at %d", candidates[i], size);
            return font;
        }
    }
    return NULL;
}


static void seed_stars(Game *game)
{
    for (int i = 0; i < STARS; i++) {
        game->stars[i].x = rand() % SCREEN_W;
        game->stars[i].y = HUD_H + rand() % (SCREEN_H - HUD_H);
        game->stars[i].colour = rand() % 5 == 0 ? C_STAR_BRIGHT : C_STAR;
    }
}

int main(int argc, char **argv)
{
    int frames = 0, snapshot_after = 0, first_level = 1;
    bool mute = false, benchmark = false, audio_test = false;
    Uint32 render_ms = 0, work_ms = 0;
    const char *snapshot = NULL;
    const char *script = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) {
            snapshot = argv[++i];
        }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            snapshot_after = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) {
            script = argv[++i];
        }
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) {
            first_level = atoi(argv[++i]);
            if (first_level < 1 || first_level > LEVEL_COUNT) {
                fprintf(stderr, "--level takes 1 to %d\n", LEVEL_COUNT);
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--mute")) {
            mute = true;
        }
        else if (!strcmp(argv[i], "--no-music")) {
            music_enabled = false;
        }
        else if (!strcmp(argv[i], "--audio-test")) {
            audio_test = true;
        }
        else if (!strcmp(argv[i], "--benchmark")) {
            benchmark = true;
        }
        else if (!strcmp(argv[i], "--help")) {
            puts("easter [--input KEYS] [--frames N] [--snapshot BMP] [--level N] [--mute] [--no-music] [--audio-test] [--benchmark]");
            puts("  D-PAD move, B undo, X restart the level (twice), A start and continue, MENU quit.");
            puts("  --input plays one key per frame: L R U D move, B undo, X restart, A, M quit, '.' waits.");
            return 0;
        }
    }

    preload_miyoo_audiobridge(argc, argv, !mute && !snapshot);
    if (audio_test) {
        if (SDL_Init(0) < 0) return 1;
        audio_start();
        if (!audio_ready) { SDL_Quit(); return 1; }
        for (int s = 0; s < FX_COUNT; ++s) {
            sound_play((Sound)s);
            SDL_Delay(500);
        }
        audio_check_oss_failure();
        SDL_LockMutex(audio_mutex);
        Uint32 callbacks = audio_callbacks;
        Uint32 active_frames = audio_audible_frames;
        unsigned peak = audio_peak;
        SDL_UnlockMutex(audio_mutex);
        fprintf(stderr, "Audio test: %u callbacks, %u non-silent frames, peak %u/32767 in 4 seconds\n",
                (unsigned)callbacks, (unsigned)active_frames, peak);
        if (getenv("SDL_AUDIODRIVER") && !strcmp(getenv("SDL_AUDIODRIVER"), "dummy"))
            fprintf(stderr, "Audio test: SDL_AUDIODRIVER=dummy does NOT play through speakers\n");
        bool audio_ok = audio_test_passed();
        if (!audio_ok) {
            fprintf(stderr, "Audio test: FAIL - missing PCM, failed delivery, or audio error\n");
        } else {
            fprintf(stderr, "Audio test: PASS - PCM generated (speaker output is not verified)\n");
        }
#ifdef __linux__
        if (audio_backend == A_OSS) {
            SDL_LockMutex(audio_mutex);
            Uint32 delivered = oss.completed_buffers;
            SDL_UnlockMutex(audio_mutex);
            fprintf(stderr, "Audio test: %u complete OSS buffers delivered\n", (unsigned)delivered);
        }
#endif
        audio_stop();
        SDL_Quit();
        return audio_ok ? 0 : 1;
    }
    if (benchmark) {
        if (!snapshot_after) snapshot_after = 3600;
    }
    log_open();
    say("--- cucuzziello's game start ---");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    /* HWSURFACE like Onion's SDL_InitDefault: asking for SWSURFACE makes SDL
     * build a shadow surface whose format differs from the video surface, and
     * every flip then pays a converting blit. */
    video = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (!video) {
        fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_W, SCREEN_H,
                                  video->format->BitsPerPixel, video->format->Rmask,
                                  video->format->Gmask, video->format->Bmask,
                                  video->format->Amask);
    if (!screen) {
        fprintf(stderr, "offscreen surface: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    for (int i = 0; i < C_COUNT; i++) {
        mapped[i] = SDL_MapRGB(screen->format, palette[i][0], palette[i][1], palette[i][2]);
    }
    /* Full frames are opaque, including backends with an alpha mask. */
    SDL_SetAlpha(screen, 0, SDL_ALPHA_OPAQUE);
    for (int c = 0; c < C_COUNT; ++c) {
        for (int band = 0; band < 6; ++band) {
            int rgb[3];
            for (int k = 0; k < 3; ++k) {
                int base = palette[c][k];
                rgb[k] = band < 2 ? base + (255 - base) * (2 - band) / 5
                                 : base * (110 - band * 9) / 100;
            }
            shades[c][band] = SDL_MapRGB(screen->format, rgb[0], rgb[1], rgb[2]);
        }
    }
    field_cache = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_W, SCREEN_H,
        screen->format->BitsPerPixel, screen->format->Rmask,
        screen->format->Gmask, screen->format->Bmask, 0);
    if (field_cache) SDL_SetAlpha(field_cache, 0, SDL_ALPHA_OPAQUE);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(0, 0);
    font_big = open_font(38);
    font_small = open_font(22);
    if (!font_big || !font_small) {
        say("no usable font; continuing without text");
    }

    Game *game = calloc(1, sizeof *game);
    if (!game) {
        TTF_Quit();
        SDL_Quit();
        return 3;
    }
    if (!mute && !snapshot) audio_start();
    srand((snapshot || benchmark) ? 1u : (unsigned)time(NULL));
    seed_stars(game);
    game->state = S_TITLE;
    game->start_level = first_level;
    game->held = -1;
    game->running = true;

    /* A script presses keys itself, so the held keys it sees are all up. */
    Uint8 scripted[SDLK_LAST];
    memset(scripted, 0, sizeof scripted);

    Uint32 audio_last_callbacks = 0;
    Uint32 audio_check_at = SDL_GetTicks();
    Uint32 deadline = SDL_GetTicks();
    unsigned phase = 0;
    while (game->running) {
        audio_check_oss_failure();
        if (audio_ready && SDL_GetTicks() - audio_check_at >= 3000) {
            SDL_LockMutex(audio_mutex);
            Uint32 callbacks = audio_callbacks;
            SDL_UnlockMutex(audio_mutex);
            if (callbacks == audio_last_callbacks && !audio_error[0]) {
                snprintf(audio_error, sizeof audio_error, "audio driver is not requesting samples");
                fprintf(stderr, "Audio unavailable: %s\n", audio_error);
                say("Audio unavailable: %s", audio_error);
            }
            audio_last_callbacks = callbacks;
            audio_check_at = SDL_GetTicks();
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                game->running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_SPACE:   /* A */
                    action(game);
                    break;
                case SDLK_LSHIFT:  /* X */
                    restart(game);
                    break;
                case SDLK_LEFT:    /* in play these are read as held keys */
                    choose_level(game, -1);
                    break;
                case SDLK_RIGHT:
                    choose_level(game, 1);
                    break;
                case SDLK_ESCAPE:  /* MENU: leave, runtime.sh restores MainUI */
                    game->running = false;
                    break;
                case SDLK_LCTRL:   /* B: undo in play, back out of the title */
                    if (game->state == S_TITLE) {
                        game->running = false;
                    }
                    break;
                default:
                    break;
                }
            }
        }

        const Uint8 *keys = SDL_GetKeyState(NULL);
        if (script) {
            /* One script character per frame, each a single press. */
            char key = *script ? *script++ : 0;
            switch (key) {
            case 'L': command(game, D_LEFT); break;
            case 'R': command(game, D_RIGHT); break;
            case 'U': command(game, D_UP); break;
            case 'D': command(game, D_DOWN); break;
            case 'B': command(game, K_UNDO); break;
            case 'X': restart(game); break;
            case 'A': action(game); break;
            case 'M': game->running = false; break;
            default: break;
            }
            keys = scripted;
        }

        /* A benchmark draws a level over and over. */
        if (benchmark) {
            if (game->state != S_PLAY) new_game(game);
            game->t_intro = 0;
        }
        Uint32 work_start = SDL_GetTicks();
        update(game, keys);
        Uint32 render_start = SDL_GetTicks();
        draw(game);
        Uint32 finished = SDL_GetTicks();
        render_ms += finished - render_start;
        work_ms += finished - work_start;
        frames++;
        if (snapshot_after && frames >= snapshot_after) {
            if (snapshot) {
                /* Un-rotate so the saved frame reads the right way up. */
                rotate_frame(screen);
                SDL_SaveBMP(screen, snapshot);
            }
            game->running = false;
        }
        if (!snapshot && !benchmark) {
            /* 60 Hz deadline includes rendering and vsync time. */
            phase += 1000;
            deadline += phase / 60;
            phase %= 60;
            Uint32 now = SDL_GetTicks();
            Sint32 remaining_ms = (Sint32)(deadline - now);
            if (remaining_ms > 0) SDL_Delay((Uint32)remaining_ms);
            else if (remaining_ms < -100) deadline = now;
        }
    }

    say("exit after %d frames, state %d, level %d, %d moves, %d pushes", frames,
        (int)game->state, game->level, game->moves, game->pushes);
    if (script)
        printf("state=%d level=%d moves=%d pushes=%d home=%d/%d total_moves=%d total_pushes=%d\n",
               (int)game->state, game->level, game->moves, game->pushes, game->home,
               game->goals, game->total_moves, game->total_pushes);
    if (benchmark)
        printf("frames=%d render_ms=%u work_ms=%u render_fps=%.1f\n",
               frames, (unsigned)render_ms, (unsigned)work_ms,
               render_ms ? frames * 1000.0 / render_ms : 0.0);
    audio_stop();
    free(game);
    for (int i = 0; i < TEXT_CACHE_SIZE; ++i) {
        if (text_cache[i].surface) SDL_FreeSurface(text_cache[i].surface);
    }
    if (font_big) {
        TTF_CloseFont(font_big);
    }
    if (font_small) {
        TTF_CloseFont(font_small);
    }
    if (field_cache) SDL_FreeSurface(field_cache);
    SDL_FreeSurface(screen);
    TTF_Quit();
    /* Blank before leaving so the panel does not hold our last frame while
     * runtime.sh brings MainUI back, then a full SDL_Quit: unlike the launcher
     * we are not handing off to an emulator, so there is nothing to race. */
    SDL_FillRect(video, NULL, 0);
    SDL_Flip(video);
    SDL_Quit();
    if (logfile) {
        fclose(logfile);
    }
    return 0;
}
