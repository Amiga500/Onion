// RedAmp - a classic-skin music player for the Miyoo Mini Plus.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "mini2d.h"
#include "audio.h"
#include "decode.h"
#include "radio.h"
#include "meta.h"
#include "library.h"
#include "album_art.h"
#include "cover_paths.h"
#include "covers.h"
#include "netclock.h"
#include "skin.h"
#include "font.h"
#include "vis.h"
#include "playlist.h"
#include "collections.h"
#include "utf8_text.h"
#include "core.h"
#include "gamemusic.h"
#include "bgm_shm.h"
#include "stayawake.h"

#define SCREEN_W 640
#define SCREEN_H 480
#define X_VIS_HOLD_SECONDS 1.0
#define SELECT_OPTIONS_HOLD_SECONDS 0.85
#define LIBRARY_VIEWS_HOLD_SECONDS 0.65

// evdev key codes of the Miyoo Mini buttons
enum { K_UP = 103, K_DOWN = 108, K_LEFT = 105, K_RIGHT = 106, K_A = 57, K_B = 29, K_X = 42, K_Y = 56,
       K_L = 18, K_R = 20, K_L2 = 15, K_R2 = 14, K_START = 28, K_SELECT = 97, K_MENU = 1, K_POWER = 116 };

enum { SCR_PLAYLIST, SCR_EQ, SCR_BROWSER, SCR_OPTIONS, SCR_SKINS, SCR_VIS, SCR_PRESETS, SCR_GM_CONFIRM, SCR_LIBRARY, SCR_DISPLAY, SCR_LIBRARY_MENU, SCR_LIBRARY_VIEWS, SCR_COLLECTION_MENU, SCR_COLLECTION_NAME, SCR_COLLECTION_PICK, SCR_COLLECTION_DELETE, SCR_QUEUE_CLEAR };

// ---------------------------------------------------------------- state
static m2d_surf *bb;
static skin *sk_default, *sk;
static font *f_row, *f_small, *f_mid, *f_head, *f_key, *f_title, *f_big, *f_hint;
typedef struct { font *pl, *lib, *aux; int pl_h, lib_h, br_h; } list_metrics;
static list_metrics metrics[3];
static list_metrics *LM;
static char regular_font[1024], fallback_font[1024];
static void select_list_size(void)
{
    static const int plsz[]={15,17,20}, libsz[]={16,18,21}, auxsz[]={13,14,16};
    static const int plh[]={20,24,28}, libh[]={28,30,36};
    int i=S.list_size;
    if(i<0||i>2)i=1;
    list_metrics *m=&metrics[i];
    if(!m->pl) m->pl=font_open(regular_font,fallback_font,plsz[i]);
    if(!m->lib) m->lib=font_open(regular_font,fallback_font,libsz[i]);
    if(!m->aux) m->aux=font_open(regular_font,fallback_font,auxsz[i]);
    m->pl_h=plh[i];m->lib_h=libh[i];m->br_h=libh[i];
    LM=m;
}

static m2d_surf *mw, *mw_scaled, *vis_surf;
static int mw_cache_valid;
static vis_data vd;
static int screen = SCR_PLAYLIST, panel = SCR_PLAYLIST;  // panel: what sits under the main window
static int options_return_screen = SCR_PLAYLIST;
static volatile sig_atomic_t quit, resumed;
static double now_t;
static int sel, scroll;       // playlist cursor
static int screen_off;
static double sleep_at;       // 0 = no sleep timer
static int sleep_min;
static int pressed = PB_NONE;
static double pressed_until;
static char marquee[400];
static char msg[120];
static double msg_until;
static double marquee_t0;
static char previous_marquee[400];
static int cover_done, cover_total, cover_running, cover_manual, cover_saved;
static double cover_status_visible_at;
static char toast[160];
static double toast_until;
static int has_dsp;
static double last_save;
static int playlist_dirty;
static collection favorites, saved;
static char collection_dir[1024], favorite_file[1024];
static char collection_names[COL_NAMES][COL_NAME+1], opened_name[COL_NAME+1];
static int collection_count, favorites_readable=1;
static void refresh_collections(void){collection_count=col_list(collection_dir,collection_names);}
static void collection_file(char *out,int n,const char *name){snprintf(out,n,"%s/%s.m3u",collection_dir,name);}
static void fill(int x,int y,int w,int h,uint32_t c);
/* Coverage mask from a supersampled curved heart; blend over the row's
 * actual background, including selected rows and different Winamp skins. */
static void draw_heart(int x,int cy,uint32_t color){
    static const unsigned char coverage[14][14]={
        {0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0},
        {0, 57, 201, 253, 233, 128, 6, 3, 119, 229, 254, 207, 66, 0},
        {35, 245, 255, 255, 255, 255, 159, 143, 255, 255, 255, 255, 249, 47},
        {136, 255, 255, 255, 255, 255, 255, 253, 255, 255, 255, 255, 255, 152},
        {175, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 191},
        {146, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 162},
        {42, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 56},
        {0, 139, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 155, 0},
        {0, 6, 183, 255, 255, 255, 255, 255, 255, 255, 255, 195, 10, 0},
        {0, 0, 12, 189, 255, 255, 255, 255, 255, 255, 200, 17, 0, 0},
        {0, 0, 0, 6, 171, 255, 255, 255, 255, 183, 10, 0, 0, 0},
        {0, 0, 0, 0, 1, 138, 255, 255, 152, 3, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 114, 129, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    for(int y=0;y<14;y++)for(int dx=0;dx<14;dx++){
        unsigned a=coverage[y][dx];if(!a)continue;
        uint32_t c=(a<<24)|((((color>>16)&255)*a/255)<<16)|
                   ((((color>>8)&255)*a/255)<<8)|((color&255)*a/255);
        fill(x+dx,cy-7+y,1,1,c);
    }
}

static int gm_orphaned; // wrapper is present despite the option being Off
/* Suppress the external low-battery overlay while this foreground app runs.
 * Never remove a marker that another process created, or replaced later. */
static int battery_marker_owned;
static dev_t battery_marker_dev;
static ino_t battery_marker_ino;
static const char *battery_marker_path(void)
{
#ifdef M2D_HOST
    const char *test = getenv("MA_BATTERY_MARKER");
    if (test && *test) return test;
#endif
    return "/tmp/hasBatteryDisplay";
}
static void restore_battery_marker(void)
{
    if (!battery_marker_owned) return;
    struct stat st;
    const char *path = battery_marker_path();
    if (lstat(path, &st) == 0 && st.st_dev == battery_marker_dev &&
        st.st_ino == battery_marker_ino && S_ISREG(st.st_mode)) unlink(path);
    battery_marker_owned = 0;
}
static void set_battery_marker(void)
{
    const char *path = battery_marker_path();
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd >= 0) {
        struct stat st;
        if (fstat(fd, &st) == 0) {
            battery_marker_owned = 1;
            battery_marker_dev = st.st_dev;
            battery_marker_ino = st.st_ino;
            atexit(restore_battery_marker);
        }
        close(fd);
    } else if (errno != EEXIST) {
        fprintf(stderr, "[battery] cannot create overlay marker: %s\n", strerror(errno));
    }
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void show_toast(const char *fmt, const char *arg)
{
    snprintf(toast, sizeof toast, fmt, arg);
    toast_until = now_t + 2.2;
}

static void show_msg(const char *s)
{
    snprintf(msg, sizeof msg, "%s", s);
    msg_until = now_t + 1.2;
}

static void press_flash(int b) { pressed = b; pressed_until = now_t + 0.18; }

// ---------------------------------------------------------------- colors
#define C_BG 0xff101216u
#define C_PANEL 0xff1a1d24u
#define C_LINE 0xff2f343eu
#define C_TEXT 0xffeef0f4u
#define C_DIM 0xff8a909cu
#define C_MUTED 0xffc9ced8u
#define C_KEY 0xff2c3038u
#define C_SEL 0xff1f2a48u
#define C_GREEN 0xff5fd38du
#define C_GREENBG 0xff1d3a2cu

static int luma(uint32_t c) { return (int)(((c >> 16) & 255) * 0.3 + ((c >> 8) & 255) * 0.59 + (c & 255) * 0.11); }

static uint32_t accent(void)
{
    uint32_t c = sk ? sk->pl_current : 0xffffb347u;
    return luma(c) < 90 ? 0xffffb347u : c;
}

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return 0xff000000u | r << 16 | g << 8 | bl;
}

static void fill(int x, int y, int w, int h, uint32_t c) { m2d_fill(bb, x, y, w, h, c, (c >> 24) == 255 ? M2D_REPLACE : M2D_ALPHA); }

static uint32_t premul(uint32_t c)
{
    uint32_t a = c >> 24;
    return a << 24 | (((c >> 16) & 255) * a / 255) << 16 | (((c >> 8) & 255) * a / 255) << 8 | ((c & 255) * a / 255);
}

static void rrect(int x, int y, int w, int h, int r, uint32_t c)
{
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    uint32_t pc = premul(c);
    int blend = (c >> 24) == 255 ? M2D_REPLACE : M2D_ALPHA;
    m2d_fill(bb, x, y + r, w, h - 2 * r, pc, blend);
    for (int i = 0; i < r; i++) {
        float dy = r - i - 0.5f;
        int s = (int)(r - sqrtf((float)r * r - dy * dy) + 0.5f);
        m2d_fill(bb, x + s, y + i, w - 2 * s, 1, pc, blend);
        m2d_fill(bb, x + s, y + h - 1 - i, w - 2 * s, 1, pc, blend);
    }
}

static void fmt_time(char *out, int n, int ms)
{
    int s = ms / 1000;
    if (s >= 3600) snprintf(out, n, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
    else snprintf(out, n, "%d:%02d", s / 60, s % 60);
}

// ---------------------------------------------------------------- hint bar
typedef struct { const char *key, *label; } hint;

static void hint_bar(const hint *h, int n)
{
    const int y = 448;
    fill(0, y, SCREEN_W, 32, 0xff14161bu);
    int gap=18, margin=20, total=0;
    for(int i=0;i<n;i++) {
        int kw=font_width(f_key,h[i].key), one=strlen(h[i].key)==1;
        int bw=one?(kw+10>20?kw+10:20):kw+14;
        total+=bw+6+font_width(f_hint,h[i].label);
    }
    if(total+gap*(n-1)>SCREEN_W-2*margin){gap=8;margin=8;}
    int x = SCREEN_W - margin;
    for (int i = n - 1; i >= 0; i--) {
        int lw = font_width(f_hint, h[i].label);
        int kw = font_width(f_key, h[i].key);
        int one = (int)strlen(h[i].key) == 1;
        int bw = one ? (kw + 10 > 20 ? kw + 10 : 20) : kw + 14;
        if (x - lw - 6 - bw < 8) break;
        x -= lw;
        font_draw_mid(bb, f_hint, x, y + 16, h[i].label, C_DIM);
        x -= 6 + bw;
        rrect(x, y + 6, bw, 20, one ? 10 : 4, C_KEY);
        font_draw_mid(bb, f_key, x + (bw - kw) / 2, y + 16, h[i].key, C_TEXT);
        x -= gap;
    }
}

// ---------------------------------------------------------------- playback
static void update_marquee(void)
{
    pl_item it;
    if (current >= 0 && pl_get(current, &it)) {
        char d[16] = "";
        if (it.dur_ms > 0) { char t[12]; fmt_time(t, sizeof t, it.dur_ms); snprintf(d, sizeof d, " (%s)", t); }
        snprintf(marquee, sizeof marquee, "%d. %s%s", current + 1, it.name, d);
    } else {
        snprintf(marquee, sizeof marquee, "REDAMP");
    }
}

static void save_all(void);
static void play_index(int i, int start_ms, int paused)
{
    if (core_play_index(i, start_ms, paused)) {
        marquee_t0 = now_t;
        save_all(); // persist position and playlist state after track changes
    }
}

static void skip(int dir)
{
    int before = current;
    core_skip(dir);
    if (current != before || audio_pos_ms() < 1000) { sel = current; marquee_t0 = now_t; }
}

static void toggle_pause(void) {
    core_toggle_pause();
    stayawake_update(audio_state() == A_PLAYING, S.play_standby);
    settings_save();
}
static void apply_audio_settings(void) { core_apply_audio(); }
static int next_index(int dir) { return core_next_index(dir); }
static void build_shuffle(void) { core_build_shuffle(); }

// ---------------------------------------------------------------- backlight
static int bl_saved = -1;
static const char *BL = "/sys/class/pwm/pwmchip0/pwm0/duty_cycle";

static void set_screen_off(int off)
{
    if (off == screen_off) return;
#ifdef M2D_HOST
    if(getenv("MA_EVENT_TEST")){
        screen_off=off;
        if(off){m2d_clear(m2d_backbuffer(),0xff000000u,0);m2d_present();}
        return;
    }
#endif
    char marker[1024], tmp[1050];
    path_join(marker, sizeof marker, data_dir, "backlight.saved");
    snprintf(tmp, sizeof tmp, "%s.tmp", marker);
    if (off) {
        FILE *f = fopen(BL, "r");
        if (f) { if (fscanf(f, "%d", &bl_saved) != 1) bl_saved = -1; fclose(f); }
        // Persist recovery information *before* turning off the PWM output.
        FILE *m = fopen(tmp, "w");
        if (!m) { fprintf(stderr, "[screen] cannot save recovery brightness\n"); return; }
        int ok = fprintf(m, "%d\n", bl_saved > 0 ? bl_saved : 50) > 0;
        if (fflush(m) != 0 || fsync(fileno(m)) != 0) ok = 0;
        if (fclose(m) != 0) ok = 0;
        if (!ok || rename(tmp, marker) != 0) {
            unlink(tmp);
            fprintf(stderr, "[screen] cannot persist recovery marker\n");
            return;
        }
        m2d_clear(m2d_backbuffer(), 0xff000000u, 0);
        m2d_present();
        m2d_clear(m2d_backbuffer(), 0xff000000u, 0);
        m2d_present();
        f = fopen(BL, "w");
        int switched = f && fputs("0", f) != EOF;
        if (f && fclose(f) != 0) switched = 0;
        if (!switched) {
            // Keep the marker if the PWM write was partial. Try restoring
            // immediately, then allow the launcher to retry after exit.
            f = fopen(BL, "w");
            if (f) { fprintf(f, "%d", bl_saved > 0 ? bl_saved : 50); fclose(f); }
            fprintf(stderr, "[screen] cannot switch backlight off\n");
            return;
        }
        screen_off = 1;
        fprintf(stderr, "[screen] off (saved brightness %d)\n", bl_saved);
    } else {
        FILE *f = fopen(BL, "w");
        if (f) {
            int ok = fprintf(f, "%d", bl_saved > 0 ? bl_saved : 50) > 0;
            if (fclose(f) != 0) ok = 0;
            if (ok) unlink(marker);
            else fprintf(stderr, "[screen] restore failed; recovery marker retained\n");
        }
        screen_off = 0;
        fprintf(stderr, "[screen] on\n");
    }
}

// ---------------------------------------------------------------- main window
static mainwin_state main_window_state(void)
{
    mainwin_state st;
    memset(&st, 0, sizeof st);
    int as = audio_state();
    st.state = as;
    int pos = audio_pos_ms(), len = audio_len_ms();
    if (len <= 0 && current >= 0) { pl_item it; if (pl_get(current, &it)) len = it.dur_ms; }
    if (len > 0 && pos > len) pos = len;
    st.time_s = S.remaining && len > 0 ? (len - pos + 999) / 1000 : pos / 1000;
    st.remaining = S.remaining && len > 0;
    st.blink_off = as == A_PAUSED && fmod(now_t, 1.0) < 0.5;
    if (audio_buffering() && as!=A_PAUSED) st.marquee = "Loading radio...";
    else if (msg_until > now_t) st.marquee = msg;
    else {
        char live[160];
        audio_stream_title(live,sizeof live);
        if (live[0]) {
            snprintf(marquee,sizeof marquee,"%s",live);
        } else update_marquee();
        if (skin_text_width(marquee)>154) {
            size_t used=strlen(marquee);
            snprintf(marquee+used,sizeof marquee-used,"  ***  ");
        }
        if(strcmp(previous_marquee,marquee)) {
            snprintf(previous_marquee,sizeof previous_marquee,"%s",marquee);
            marquee_t0=now_t;
        }
        st.marquee = marquee;
        int width=skin_text_width(marquee);
        if(width>154) st.marquee_px=(int)fmod(fmax(0,now_t-marquee_t0)*22,width);
    }
    int rate, ch, kbps;
    audio_stream_info(&rate, &ch, &kbps);
    if (kbps <= 0 && current >= 0) {
        static int cached_idx = -2, cached_kbps;
        if (cached_idx != current) {
            track_meta m;
            const char *p = pl_path(current);
            cached_kbps = p && meta_read(p, &m) ? m.kbps : 0;
            cached_idx = current;
        }
        kbps = cached_kbps;
    }
    st.kbps = kbps;
    st.khz = (rate + 500) / 1000;
    st.channels = ch;
    st.volume = S.volume;
    st.balance = S.balance;
    st.eq_on = S.eq_on;
    st.pl_on = panel == SCR_PLAYLIST;
    /* Quantize to the actual thumb position (219 native pixels). */
    st.pos = len > 0 && as != A_STOPPED ? (float)((int64_t)pos*219/len)/219.0f : -1;
    if(st.pos>=0) st.pos=nextafterf(st.pos,1.0f);
    st.shuffle = S.shuffle;
    st.repeat = S.repeat;
    st.pressed = pressed_until > now_t ? pressed : PB_NONE;
    st.vis_mode = S.vis_mode;
    st.bars = vd.bars19;
    st.peaks = vd.peaks19;
    st.scope = vd.scope76;
    return st;
}

static void draw_main_window(void)
{
    mainwin_state st=main_window_state();
    static mainwin_state cached;
    static char cached_text[400];
    static skin *cached_skin;
    static int cached_smooth;
    const char *text=st.marquee;
    st.marquee=NULL;
    int dirty=!mw_cache_valid||cached_skin!=sk||cached_smooth!=S.skin_smooth||
              memcmp(&cached,&st,sizeof st)||strcmp(cached_text,text)||st.vis_mode;
    if(dirty||!mw_scaled){
        int full=!mw_cache_valid||cached_skin!=sk||cached_smooth!=S.skin_smooth||!mw_scaled;
        static uint32_t previous[MW_W*MW_H];
        cached=st;cached_skin=sk;cached_smooth=S.skin_smooth;
        snprintf(cached_text,sizeof cached_text,"%s",text);
        st.marquee=text;
        skin_draw_main(sk,mw,&st);
        m2d_surf *dest=mw_scaled?mw_scaled:bb;
        int x=mw_scaled?0:45,y=mw_scaled?0:8;
        int x0=0,y0=0,x1=MW_W,y1=MW_H;
        if(!full){
            x0=MW_W;y0=MW_H;x1=y1=0;
            for(int row=0;row<MW_H;row++){
                uint32_t *old=previous+row*MW_W,*cur=mw->px+row*MW_W;
                if(!memcmp(old,cur,MW_W*sizeof *old))continue;
                if(row<y0)y0=row;
                y1=row+1;
                for(int col=0;col<MW_W;col++)if(old[col]!=cur[col]){
                    if(col<x0)x0=col;
                    if(col>=x1)x1=col+1;
                }
            }
        }
        if(x1>x0&&y1>y0){
            if(S.skin_smooth){
                /* Bilinear taps can touch one adjacent native pixel. */
                if(x0>0)x0--;
                if(y0>0)y0--;
                if(x1<MW_W)x1++;
                if(y1<MW_H)y1++;
                m2d_blit_smooth2x_region(mw,dest,x,y,x0,y0,x1,y1);
            }else m2d_blit(mw,x0,y0,x1-x0,y1-y0,dest,x+x0*2,y+y0*2,
                           (x1-x0)*2,(y1-y0)*2,0xffffffffu,M2D_REPLACE,0);
        }
        memcpy(previous,mw->px,sizeof previous);
        mw_cache_valid=1;
    }
    if(mw_scaled)m2d_blit(mw_scaled,0,0,MW_W*2,MW_H*2,bb,45,8,MW_W*2,MW_H*2,
                          0xffffffffu,M2D_REPLACE,0);
    /* Consistent RedAmp branding on all classic skins without rewriting or
     * misrepresenting the original CC0 bitmap archives. */
    rrect(52, 12, 80, 20, 3, 0xe015171bu);
    font_draw_mid(bb, f_key, 60, 22, "RedAmp", C_TEXT);
}


/* The player, library and expanded list all report the same audio clock.
 * Unknown/live lengths never produce a misleading percentage. */
static void draw_track_progress(int x, int y, int width, uint32_t bg)
{
    int length = audio_len_ms();
    if (length <= 0 && current >= 0) {
        pl_item it;
        if (pl_get(current, &it)) length = it.dur_ms;
    }
    fill(x, y, width, 3, mix(bg, 0xffffffffu, 0.21f));
    if (current >= 0 && length > 0) {
        int64_t pos = audio_pos_ms();
        if (pos < 0) pos = 0;
        if (pos > length) pos = length;
        int done = (int)(pos * width / length);
        if (done > 0) fill(x, y, done, 3, accent());
    }
}

// ---------------------------------------------------------------- playlist panel
#define PX 45
#define PY 248
#define PW 550
#define PH 190
#define ROW_H (LM->pl_h)
#define PL_NORMAL_LIST_TOP 22
#define PL_EXPANDED_LIST_TOP 20
#define ROWS ((PH - PL_NORMAL_LIST_TOP) / ROW_H)
#define LARGE_PY 8
#define LARGE_PH 435
/* The expanded playlist reserves a small now-playing footer. */
static int playlist_rows(void)
{
    return S.playlist_expanded ? (LARGE_PH - PL_EXPANDED_LIST_TOP - 26) / ROW_H : ROWS;
}

static void panel_frame_at(const char *title, const char *right, int top, int height)
{
    uint32_t bg = sk->pl_bg;
    fill(PX, top, PW, height, bg);
    int header = S.playlist_expanded ? 19 : 21;
    fill(PX, top, PW, header, mix(bg, 0xffffffffu, 0.07f));
    fill(PX, top + header, PW, 1, mix(bg, 0xffffffffu, 0.15f));
    font_draw_mid(bb, f_head, PX + 12, top + 10, title, accent());
    if (right) {
        char b[200];
        font_fit_left(LM->aux, right, PW - 140, b, sizeof b);
        font_draw_mid(bb, LM->aux, PX + PW - 12 - font_width(LM->aux, b), top + 10, b, mix(sk->pl_normal, bg, 0.35f));
    }
}

static void panel_frame(const char *title, const char *right)
{
    panel_frame_at(title, right, PY, PH);
}

static void clamp_scroll(int n)
{
    if (sel >= n) sel = n - 1;
    if (sel < 0) sel = 0;
    if (sel < scroll) scroll = sel;
    int visible = playlist_rows();
    if (sel >= scroll + visible) scroll = sel - visible + 1;
    if (scroll > n - visible) scroll = n - visible;
    if (scroll < 0) scroll = 0;
}

static void draw_playlist_panel(void)
{
    int n = pl_count();
    int complete;
    long long tot = pl_total_ms(&complete);
    char right[120], t[24];
    fmt_time(t, sizeof t, (int)(tot > 2000000000LL ? 2000000000LL : tot));
    if (n) snprintf(right, sizeof right, "%d track%s  ·  %s%s", n, n == 1 ? "" : "s", t, complete ? "" : "+");
    else snprintf(right, sizeof right, "empty");
    int top = S.playlist_expanded ? LARGE_PY : PY;
    int height = S.playlist_expanded ? LARGE_PH : PH;
    int visible = playlist_rows();
    panel_frame_at("PLAY QUEUE", right, top, height);
    m2d_surf *player_cover=NULL;
    const char *track_path=current>=0?pl_path(current):NULL;
    if(S.cover_player&&track_path&&!radio_is_url(track_path))
        player_cover=album_art_player(track_path,S.playlist_expanded?22:150);
    int list_width=(player_cover&&!S.playlist_expanded)?382:PW;
    if(player_cover&&!S.playlist_expanded) {
        /* Art remains inside the original skin's playlist panel, not over the footer. */
        m2d_blit(player_cover,0,0,150,150,bb,PX+392,top+26,150,150,
                 0xffffffffu,M2D_REPLACE,0);
    }
    clamp_scroll(n);
    pl_set_hint(scroll);
    if (!n) {
        const char *a = "Your play queue is empty.", *b = "Tap SELECT to browse your music library.";
        font_draw_mid(bb, LM->pl, PX + (PW - font_width(LM->pl, a)) / 2, top + 90, a, sk->pl_normal);
        font_draw_mid(bb, f_small, PX + (PW - font_width(f_small, b)) / 2, top + 116, b, mix(sk->pl_normal, sk->pl_bg, 0.35f));
        return;
    }
    for (int r = 0; r < visible && scroll + r < n; r++) {
        int i = scroll + r;
        pl_item it;
        if (!pl_get(i, &it)) break;
        int y = top + (S.playlist_expanded ? PL_EXPANDED_LIST_TOP : PL_NORMAL_LIST_TOP) + r * ROW_H;
        int is_cur = i == current, is_sel = i == sel && screen == SCR_PLAYLIST;
        if (is_sel) fill(PX + 3, y, list_width - 16, ROW_H, sk->pl_selbg);
        uint32_t col = is_cur ? sk->pl_current : sk->pl_normal;
        char num[16];
        snprintf(num, sizeof num, "%d.", i + 1);
        font_draw_mid(bb, LM->pl, PX + 12, y + ROW_H / 2, num, is_cur ? col : mix(col, sk->pl_bg, 0.4f));
        int nx = PX + 16 + font_width(LM->pl, n >= 1000 ? "0000." : n >= 100 ? "000." : "00.");
        char d[16] = "";
        if (it.dur_ms > 0) fmt_time(d, sizeof d, it.dur_ms);
        int dw = font_width(LM->pl, d);
        char nm[220];
        int fav=col_find(&favorites,it.path)>=0;
        font_fit(LM->pl, it.name, PX + list_width - 30 - dw - nx - (fav?18:0), nm, sizeof nm);
        if(fav)draw_heart(PX+list_width-27-dw-15,y+ROW_H/2,accent());
        font_draw_mid(bb, LM->pl, nx, y + ROW_H / 2, nm, col);
        if (d[0]) font_draw_mid(bb, LM->pl, PX + list_width - 20 - dw, y + ROW_H / 2, d, col);
    }
    // scroll bar
    if (n > visible) {
        int track = visible * ROW_H - 2, th = track * visible / n;
        if (th < 12) th = 12;
        int list_top = S.playlist_expanded ? PL_EXPANDED_LIST_TOP : PL_NORMAL_LIST_TOP;
        int ty = top + list_top + (track - th) * scroll / (n - visible);
        fill(PX + list_width - 9, top + list_top, 4, track, mix(sk->pl_bg, 0xffffffffu, 0.08f));
        fill(PX + list_width - 9, ty, 4, th, mix(sk->pl_bg, 0xffffffffu, 0.3f));
    }
    if (S.playlist_expanded) {
        fill(PX, top + height - 26, PW, 26, mix(sk->pl_bg, 0xffffffffu, 0.07f));
        if (current >= 0) {
            pl_item it;
            if (pl_get(current, &it)) {
                char name[210], tm[18], len[18], status[64];
                font *status_font = LM->pl;
                fmt_time(tm, sizeof tm, audio_pos_ms());
                int ms = audio_len_ms();
                if (ms <= 0) ms = it.dur_ms;
                if (ms > 0) fmt_time(len, sizeof len, ms);
                else snprintf(len, sizeof len, "--:--");
                snprintf(status, sizeof status, "%s / %s", tm, len);
                const char *label = audio_state() == A_PLAYING ? "Playing:" : "Paused:";
                int cy=top+height-15;
                int time_x=PX+PW-12-font_width(status_font,status);
                int title_x=PX+16+font_width(status_font,label)+8;
                font_draw_mid(bb,status_font,PX+12,cy,label,accent());
                if(player_cover){
                    m2d_blit(player_cover,0,0,22,22,bb,title_x,top+height-27,22,22,
                             0xffffffffu,M2D_REPLACE,0);
                    title_x+=26;
                }
                int max_name=time_x-title_x-12;
                if(max_name<20)max_name=20;
                font_fit(status_font,it.name,max_name,name,sizeof name);
                font_draw_mid(bb,status_font,title_x,cy,name,sk->pl_normal);
                font_draw_mid(bb,status_font,time_x,cy,status,sk->pl_normal);
            }
        }
        draw_track_progress(PX + 2, top + height - 3, PW - 4, sk->pl_bg);
    }
}

// ---------------------------------------------------------------- equalizer
typedef struct { const char *name; float pre; float b[EQ_BANDS]; } eq_preset;
/* Stronger, recognisably different profiles. The preamps leave room for the
 * overlapping peaking filters (especially 12k/14k/16k) to avoid clipping.
 * No EQ boost is applied to Flat. Manual band adjustments remain unchanged. */
static const eq_preset PRESETS[] = {
    { "Flat",           0, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 } },
    { "Small Speaker", -9, { -6, -4,  0,  4,  6,  5,  3,  1,  0, -1 } },
    { "Headphones",    -8, {  6,  5,  1, -3, -3,  1,  3,  4,  2,  0 } },
    { "Bass Boost",   -12, {  9,  8,  5,  2,  0, -2, -3, -3, -2, -1 } },
    { "Treble Boost",  -8, { -2, -2, -1,  0,  0,  3,  5,  4,  2,  0 } },
    { "Rock",          -9, {  7,  5,  1, -3, -3,  2,  4,  3,  1,  0 } },
    { "Pop",          -10, { -3, -1,  4,  6,  5,  1, -2, -3, -2,  0 } },
    { "Dance",        -10, {  8,  7,  3, -2, -4, -2,  0,  3,  3,  1 } },
    { "Classical",     -4, {  2,  1,  0,  1,  2,  2, -3, -4, -3, -1 } },
    { "Vocal",        -10, { -5, -4,  0,  4,  6,  5,  3, -2, -3, -1 } },
    { "Loudness",     -10, {  8,  6,  1, -2, -3,  0,  3,  3,  2,  0 } },
};
#define NPRESETS ((int)(sizeof PRESETS / sizeof PRESETS[0]))
static int eq_sel = 1;      // 0 = preamp, 1..10 bands
static int preset_sel;

static void draw_eq_panel(void)
{
    char right[80];
    snprintf(right, sizeof right, "Preset: %s", S.eq_preset[0] ? S.eq_preset : "Custom");
    panel_frame("EQUALIZER", right);
    uint32_t acc = accent();
    // on/off pill
    int bx = PX + 112;
    rrect(bx, PY + 4, 44, 15, 4, S.eq_on ? C_GREENBG : mix(sk->pl_bg, 0xffffffffu, 0.12f));
    const char *lab = S.eq_on ? "ON" : "OFF";
    font_draw_mid(bb, f_key, bx + (44 - font_width(f_key, lab)) / 2, PY + 11, lab, S.eq_on ? C_GREEN : C_DIM);

    static const char *names[11] = { "PRE", "60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K" };
    const int top = PY + 42, bot = PY + 152, mid = (top + bot) / 2;
    uint32_t grid = mix(sk->pl_bg, 0xffffffffu, 0.1f);
    fill(PX + 92, mid, PW - 112, 1, grid);
    font_draw_mid(bb, f_small, PX + 14, top + 2, "+12", C_DIM);
    font_draw_mid(bb, f_small, PX + 20, mid, "0", C_DIM);
    font_draw_mid(bb, f_small, PX + 14, bot - 2, "-12", C_DIM);
    int px_prev = -1, py_prev = 0;
    for (int i = 0; i < 11; i++) {
        float v = i == 0 ? S.eq_pre : S.eq[i - 1];
        int cx = PX + 64 + i * 44 + (i > 0 ? 18 : 0);
        rrect(cx - 3, top, 7, bot - top, 3, mix(sk->pl_bg, 0xffffffffu, 0.1f));
        int ty = (int)(mid - v / 12.f * (bot - top) / 2);
        int a = ty < mid ? ty : mid, b = ty < mid ? mid : ty;
        fill(cx - 3, a, 7, b - a + 1, i ? mix(acc, sk->pl_bg, 0.45f) : mix(C_DIM, sk->pl_bg, 0.4f));
        int is_sel = screen == SCR_EQ && i == eq_sel;
        rrect(cx - 12, ty - 6, 25, 12, 3, is_sel ? acc : C_MUTED);
        font_draw_mid(bb, f_key, cx - font_width(f_key, names[i]) / 2, bot + 16, names[i], is_sel ? acc : C_DIM);
        if (is_sel) {
            char vb[16];
            snprintf(vb, sizeof vb, "%+.0f dB", v);
            font_draw_mid(bb, f_small, cx - font_width(f_small, vb) / 2, top - 10, vb, acc);
        }
        if (i > 0) {
            if (px_prev >= 0) m2d_line(bb, px_prev, py_prev, cx, ty, 2, premul((acc & 0x00ffffffu) | 0x90000000u), M2D_ALPHA);
            px_prev = cx; py_prev = ty;
        }
    }
}

static void eq_apply_preset(int i)
{
    S.eq_pre = PRESETS[i].pre;
    memcpy(S.eq, PRESETS[i].b, sizeof S.eq);
    snprintf(S.eq_preset, sizeof S.eq_preset, "%s", PRESETS[i].name);
    S.eq_on = 1;
    apply_audio_settings();
}

// ---------------------------------------------------------------- generic list card (options, presets)
static void card(int x, int y, int w, int h, const char *title)
{
    fill(0, 0, SCREEN_W, SCREEN_H, 0xc8060709u);
    rrect(x - 1, y - 1, w + 2, h + 2, 13, C_LINE);
    rrect(x, y, w, h, 12, C_PANEL);
    font_draw_mid(bb, f_title, x + 24, y + 26, title, accent());
}

static void pill(int right_x, int cy, const char *val, int on)
{
    int tw = font_width(f_key, val) + 20;
    rrect(right_x - tw, cy - 11, tw, 22, 11, on ? C_GREENBG : 0xff262a31u);
    font_draw_mid(bb, f_key, right_x - tw + 10, cy, val, on ? C_GREEN : C_MUTED);
}

// ---------------------------------------------------------------- options
enum { OPT_SCREEN, OPT_GAMEMUSIC, OPT_GAMEVOL, OPT_SHUFFLE, OPT_REPEAT, OPT_SLEEP,
       OPT_VOLUME, OPT_BALANCE, OPT_DISPLAY, OPT_STANDBY, OPT_INSECURE, OPT_CLEAR, OPT_QUIT, OPT_N };
enum { D_SKIN, D_SMOOTH, D_VIS, D_TIME, D_LIST, D_FETCH, D_FOLDER, D_PLAYER, D_LIBRARY, D_N };
static int display_sel, library_menu_sel;
static int opt_sel;
static const int SLEEP_STEPS[] = { 0, 15, 30, 45, 60, 90, 120 };

static void opt_value(int i, char *out, int n, int *on)
{
    *on = 0;
    out[0] = 0;
    switch (i) {
    case OPT_GAMEMUSIC: {
        char marker[1024];
        path_join(marker, sizeof marker, data_dir, "game_music.launch_hook_seen");
        snprintf(out, n, "%s", S.game_music ?
                 (access(marker, F_OK) == 0 ? "On (seen)" : "Unverified") :
                 gm_orphaned ? "Repair" : "Off");
        *on = S.game_music; break;
    }
    case OPT_GAMEVOL: snprintf(out, n, "%d%%", S.game_mix_volume); break;
    case OPT_SHUFFLE: snprintf(out, n, "%s", S.shuffle ? "On" : "Off"); *on = S.shuffle; break;
    case OPT_REPEAT: snprintf(out, n, "%s", S.repeat == 1 ? "All" : S.repeat == 2 ? "One" : "Off"); *on = S.repeat != 0; break;
    case OPT_SLEEP:
        if (sleep_at > 0) { int m = (int)((sleep_at - now_t) / 60 + 0.99); snprintf(out, n, "%d min left", m); *on = 1; }
        else snprintf(out, n, "Off");
        break;
    case OPT_VOLUME: snprintf(out, n, "%d%%", S.volume); break;
    case OPT_BALANCE:
        if (S.balance == 0) snprintf(out, n, "Center");
        else snprintf(out, n, "%d%% %s", abs(S.balance), S.balance < 0 ? "Left" : "Right");
        break;
    case OPT_STANDBY: snprintf(out,n,"%s",S.play_standby?"On":"Off"); *on=S.play_standby; break;
    case OPT_INSECURE: snprintf(out,n,"%s",S.insecure_https?"On":"Off"); *on=S.insecure_https; break;
    case OPT_DISPLAY: snprintf(out,n,"Open"); break;
    }
}

static void draw_options(void)
{
    static const char *labels[OPT_N] = { "Turn screen off", "Music in games", "Game music level", "Shuffle", "Repeat", "Sleep timer", "Volume", "Balance",
                                         "Display...", "Play during standby", "Allow insecure HTTPS", "Clear play queue", "Quit RedAmp" };
    const int x = 85, y = 14, w = 470, h = 420, rh = 26;
    card(x, y, w, h, "OPTIONS");
    for (int i = 0; i < OPT_N; i++) {
        int ry = y + 49 + i * (rh + 1);
        if (i == opt_sel) rrect(x + 12, ry, w - 24, rh - 2, 8, C_SEL);
        font_draw_mid(bb, f_mid, x + 26, ry + rh / 2, labels[i], C_TEXT);
        char v[64];
        int on;
        opt_value(i, v, sizeof v, &on);
        if (i == OPT_SCREEN && i == opt_sel) font_draw_mid(bb, f_small, x + w - 26 - font_width(f_small, "START pauses · L/R skip"), ry + rh / 2, "START pauses · L/R skip", C_DIM);
        if (i == OPT_GAMEMUSIC && i == opt_sel) {
            char fit[64];
            const char *note=gm_orphaned ? "Repair required" :
                S.game_music ? "Launch hook required" : "Requires RedOnion";
            font_fit(f_small,note,132,fit,sizeof fit);
            font_draw_mid(bb,f_small,x+226,ry+rh/2,fit,C_DIM);
        }
        if (v[0]) {
            char fv[64];
            font_fit(f_key, v, 170, fv, sizeof fv);
            pill(x + w - 26, ry + rh / 2, fv, on);
        }
    }
}

static void draw_gm_confirm(void)
{
    const int x = 64, y = 96;
    card(x, y, 512, 263, "ENABLE MUSIC IN GAMES?");
    font_draw_mid(bb, f_mid, x + 25, y + 78,
                  "Enables background music in RedOnion games.", C_TEXT);
    font_draw_mid(bb, f_mid, x + 25, y + 109,
                  "Requires RedOnion's launch hook to be installed.", C_TEXT);
    font_draw_mid(bb, f_mid, x + 25, y + 140,
                  "No shared system libraries will be replaced.", C_TEXT);
    font_draw_mid(bb, f_small, x + 25, y + 184,
                  "Legacy installations can use redamp-recover.sh", C_DIM);
    font_draw_mid(bb, f_mid, x + 25, y + 220,
                  "A  Enable                 B  Cancel", accent());
}

static void open_skins(void);

static void option_change(int dir, int activate)
{
    switch (opt_sel) {
    case OPT_SCREEN: if (activate) { screen = options_return_screen; set_screen_off(1); } break;
    case OPT_GAMEMUSIC:
        if (!S.game_music && gm_orphaned) {
            if (gm_uninstall()) {
                gm_orphaned = 0;
                show_toast("%s", "Original audio library restored");
            } else show_toast("%s", "Audio repair FAILED; use recovery script");
        } else if (!S.game_music) {
            // An explicit confirmation is required before modifying system audio.
            screen = SCR_GM_CONFIRM;
        } else {
            S.game_music = 0;
            gm_revoke_consent();
            settings_save();
            show_toast("%s", "Music in games disabled (launch-only)");
        }
        break;
    case OPT_GAMEVOL:
        if (!activate) S.game_mix_volume += dir * 5;
        if (S.game_mix_volume < 0) S.game_mix_volume = 0;
        if (S.game_mix_volume > 100) S.game_mix_volume = 100;
        break;
    case OPT_SHUFFLE: S.shuffle = !S.shuffle; if (S.shuffle) build_shuffle(); break;
    case OPT_REPEAT: S.repeat = (S.repeat + (dir < 0 ? 2 : 1)) % 3; break;
    case OPT_SLEEP: {
        int k = 0;
        for (int i = 0; i < 7; i++) if (SLEEP_STEPS[i] == sleep_min) k = i;
        k = (k + (dir < 0 ? 6 : 1)) % 7;
        sleep_min = SLEEP_STEPS[k];
        sleep_at = sleep_min ? now_t + sleep_min * 60.0 : 0;
        break;
    }
    case OPT_VOLUME:
        S.volume += activate ? 0 : dir * 5;
        if (S.volume < 0) S.volume = 0;
        if (S.volume > 100) S.volume = 100;
        apply_audio_settings();
        { char b[40]; snprintf(b, sizeof b, "VOLUME: %d%%", S.volume); show_msg(b); }
        break;
    case OPT_BALANCE:
        if (activate) S.balance = 0; else S.balance += dir * 10;
        if (S.balance < -100) S.balance = -100;
        if (S.balance > 100) S.balance = 100;
        apply_audio_settings();
        break;
    case OPT_STANDBY:
        if (!activate) break;
        S.play_standby=!S.play_standby;
        stayawake_update(audio_state()==A_PLAYING,S.play_standby);
        settings_save();
        break;
    case OPT_INSECURE:
        if(!activate)break; /* deliberate A press, not key repeat */
        S.insecure_https=!S.insecure_https;
        netclock_set_insecure(S.insecure_https);
        if(!S.insecure_https)netclock_start();
        if(cover_running)covers_cancel(); /* restart the batch manually under the new policy */
        if(current>=0&&audio_state()==A_PLAYING&&radio_is_url(pl_path(current)))
            play_index(current,0,0); /* reconnect under the new policy */
        settings_save();
        show_toast("%s",S.insecure_https?"HTTPS: server identity NOT verified":"HTTPS verification enabled");
        break;
    case OPT_DISPLAY: if (activate) { display_sel = 0; screen = SCR_DISPLAY; } break;
    case OPT_CLEAR:
        if (activate) {
            audio_stop();
            pl_clear();
            current = -1; sel = 0; scroll = 0; S.cur_index = -1;
            playlist_dirty = 1;
            screen = panel = SCR_PLAYLIST;
            show_toast("%s", "Play queue cleared");
        }
        break;
    case OPT_QUIT: if (activate) quit = 1; break;
    }
}

static void display_change(int dir,int activate)
{
    switch(display_sel) {
    case D_SKIN: if(activate)open_skins();break;
    case D_SMOOTH:S.skin_smooth=!S.skin_smooth;break;
    case D_VIS:S.vis_mode=(S.vis_mode+(dir<0?2:1))%3;break;
    case D_TIME:S.remaining=!S.remaining;break;
    case D_LIST:S.list_size=(S.list_size+(dir<0?2:1))%3;select_list_size();break;
    case D_FETCH:S.cover_fetch=!S.cover_fetch;if(!S.cover_fetch)covers_cancel();break;
    case D_FOLDER:S.cover_save_to_folder=!S.cover_save_to_folder;break;
    case D_PLAYER:S.cover_player=!S.cover_player;break;
    case D_LIBRARY:S.cover_library=!S.cover_library;break;
    }
}
static void draw_display(void)
{
    static const char *labels[D_N]={"Skin", "Smooth scaling", "Mini visualizer",
        "Time display", "List text size", "Download album covers",
        "Save covers in album folders", "Show cover on player", "Show covers in Library"};
    const int x=85,y=25,w=470,rh=37;
    card(x,y,w,400,"DISPLAY & COVERS");
    for(int i=0;i<D_N;i++){
        int yy=y+50+i*rh;
        if(i==display_sel)rrect(x+12,yy-2,w-24,rh-3,8,C_SEL);
        font_draw_mid(bb,f_mid,x+22,yy+16,labels[i],C_TEXT);
        char v[96]="";int on=0;
        switch(i){
        case D_SKIN:snprintf(v,sizeof v,"%s",sk->name);break;
        case D_SMOOTH:snprintf(v,sizeof v,"%s",S.skin_smooth?"Smooth":"Sharp");break;
        case D_VIS:snprintf(v,sizeof v,"%s",S.vis_mode==1?"Spectrum":S.vis_mode==2?"Scope":"Off");break;
        case D_TIME:snprintf(v,sizeof v,"%s",S.remaining?"Remaining":"Elapsed");break;
        case D_LIST:snprintf(v,sizeof v,"%s",S.list_size==0?"Small":S.list_size==1?"Medium":"Large");break;
        case D_FETCH:on=S.cover_fetch;snprintf(v,sizeof v,"%s",on?"On":"Off");break;
        case D_FOLDER:on=S.cover_save_to_folder;snprintf(v,sizeof v,"%s",on?"On":"Off");break;
        case D_PLAYER:on=S.cover_player;snprintf(v,sizeof v,"%s",on?"On":"Off");break;
        case D_LIBRARY:on=S.cover_library;snprintf(v,sizeof v,"%s",on?"On":"Off");break;
        }
        char fit[96];font_fit(f_key,v,110,fit,sizeof fit);
        pill(x+w-20,yy+16,fit,on);
    }
}
static void draw_library_menu(void)
{
    const int x=90,y=100,w=460;
    card(x,y,w,246,"LIBRARY MENU");
    const char *labels[]={"Fetch missing covers now","Rescan music library",cover_running?"Cancel cover fetch":"Back to library"};
    for(int i=0;i<3;i++){
        int yy=y+54+i*56;
        if(i==library_menu_sel)rrect(x+15,yy-2,w-30,48,8,C_SEL);
        font_draw_mid(bb,f_mid,x+29,yy+20,labels[i],C_TEXT);
    }
}
// ---------------------------------------------------------------- presets card
static void draw_presets(void)
{
    const int x = 170, y = 40, w = 300, h = 400, rh = 30;
    card(x, y, w, h, "PRESETS");
    for (int i = 0; i < NPRESETS; i++) {
        int ry = y + 50 + i * rh;
        if (i == preset_sel) rrect(x + 12, ry, w - 24, rh - 2, 8, C_SEL);
        font_draw_mid(bb, f_mid, x + 26, ry + rh / 2 - 1, PRESETS[i].name, !strcmp(S.eq_preset, PRESETS[i].name) ? accent() : C_TEXT);
    }
}

// ---------------------------------------------------------------- skins
typedef struct { char path[600]; char name[128]; } skin_entry;
static skin_entry *skins;
static int nskins, skin_sel, skin_scroll;
static skin *preview;
static double preview_due;
static int preview_idx = -1;
static skin *skin_before;

static int cmp_skin(const void *a, const void *b) { return name_cmp(((const skin_entry *)a)->name, ((const skin_entry *)b)->name); }

static void scan_skins(void)
{
    nskins = 0;
    const char *dirs[2];
    char d0[600];
    path_join(d0, sizeof d0, app_dir, "skins");
    dirs[0] = d0;
    dirs[1] = getenv("MA_SKINS");
    int cap = 0;
    for (int k = 0; k < 2; k++) {
        if (!dirs[k]) continue;
        DIR *d = opendir(dirs[k]);
        if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[600];
            snprintf(p, sizeof p, "%s/%s", dirs[k], e->d_name);
            const char *dot = strrchr(e->d_name, '.');
            struct stat st;
            if (stat(p, &st)) continue;
            int ok = (dot && !strcasecmp(dot, ".wsz") && S_ISREG(st.st_mode)) || S_ISDIR(st.st_mode);
            if (!ok) continue;
            if (nskins == cap) { cap = cap ? cap * 2 : 32; skins = realloc(skins, sizeof *skins * cap); }
            snprintf(skins[nskins].path, sizeof skins[nskins].path, "%s", p);
            snprintf(skins[nskins].name, sizeof skins[nskins].name, "%s", e->d_name);
            char *dt = strrchr(skins[nskins].name, '.');
            if (dt && !strcasecmp(dt, ".wsz")) *dt = 0;
            for (char *c = skins[nskins].name; *c; c++) if (*c == '_') *c = ' ';
            nskins++;
        }
        closedir(d);
    }
    if (nskins) qsort(skins, nskins, sizeof *skins, cmp_skin);
}

static void open_skins(void)
{
    scan_skins();
    skin_sel = 0;
    for (int i = 0; i < nskins; i++) if (!strcmp(skins[i].path, S.skin)) skin_sel = i;
    skin_scroll = 0;
    skin_before = sk;
    preview_idx = skin_sel;
    screen = SCR_SKINS;
}

static void close_skins(int apply)
{
    if (apply && preview && preview != sk) {
        if (sk != sk_default) skin_free(sk);
        sk = preview;
        snprintf(S.skin, sizeof S.skin, "%s", skins[preview_idx].path);
    } else if (apply && preview_idx >= 0 && preview_idx < nskins) {
        snprintf(S.skin, sizeof S.skin, "%s", skins[preview_idx].path);
    }
    if (!apply && preview && preview != sk && preview != sk_default) skin_free(preview);
    preview = NULL;
    mw_cache_valid=0;
    sk = apply ? sk : skin_before;
    screen = SCR_DISPLAY;
}

static void skins_update(void)
{
    if (skin_sel == preview_idx || now_t < preview_due || !nskins) return;
    skin *old = preview;
    skin *s = NULL;
    // the default skin is already loaded
    char defp[600];
    path_join(defp, sizeof defp, app_dir, "skins/RedOnion.wsz");
    if (!strcmp(skins[skin_sel].path, defp)) s = sk_default;
    else s = skin_load(skins[skin_sel].path, sk_default);
    preview_idx = skin_sel;
    if (!s) { show_toast("%s", "That skin couldn't be loaded"); return; }
    if (old && old != sk && old != sk_default && old != skin_before) skin_free(old);
    preview = s;
    mw_cache_valid=0;
}

static void draw_skins_panel(void)
{
    char right[64];
    snprintf(right, sizeof right, "%d of %d  ·  put .wsz files in App/RedAmp/skins", nskins ? skin_sel + 1 : 0, nskins);
    panel_frame("SKINS", right);
    if (skin_sel < skin_scroll) skin_scroll = skin_sel;
    if (skin_sel >= skin_scroll + ROWS) skin_scroll = skin_sel - ROWS + 1;
    for (int r = 0; r < ROWS && skin_scroll + r < nskins; r++) {
        int i = skin_scroll + r, y = PY + PL_NORMAL_LIST_TOP + r * ROW_H;
        if (i == skin_sel) fill(PX + 3, y, PW - 6, ROW_H, sk->pl_selbg);
        int active = !strcmp(skins[i].path, S.skin);
        char nm[160];
        font_fit(LM->pl, skins[i].name, PW - 120, nm, sizeof nm);
        font_draw_mid(bb, LM->pl, PX + 14, y + ROW_H / 2, nm, active ? sk->pl_current : sk->pl_normal);
        if (active) font_draw_mid(bb, f_small, PX + PW - 20 - font_width(f_small, "in use"), y + ROW_H / 2, "in use", mix(sk->pl_normal, sk->pl_bg, 0.3f));
    }
}

// ---------------------------------------------------------------- browser
typedef struct { char *name; int dir, m3u, radio; } b_entry;
static char cwd[1024];
static b_entry *ents;
static int nents, b_sel, b_scroll;
static int browser_return = SCR_PLAYLIST;
#define LIB_TOP 52
#define LIB_BOTTOM 419
#define B_TOP LIB_TOP
#define B_ROWH (LM->br_h)
#define B_ROWS ((LIB_BOTTOM - B_TOP) / B_ROWH)

static int cmp_ent(const void *a, const void *b)
{
    const b_entry *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return name_cmp(x->name, y->name);
}

static int is_m3u(const char *n)
{
    const char *d = strrchr(n, '.');
    return d && (!strcasecmp(d, ".m3u") || !strcasecmp(d, ".m3u8") || !strcasecmp(d, ".pls"));
}

static void browse(const char *dir, const char *select_name)
{
    for (int i = 0; i < nents; i++) free(ents[i].name);
    nents = 0;
    snprintf(cwd, sizeof cwd, "%s", dir);
    DIR *d = opendir(dir);
    int cap = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[1400];
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            struct stat st;
            if (stat(p, &st)) continue;
            int isdir = S_ISDIR(st.st_mode);
            int m3u = !isdir && is_m3u(e->d_name);
            if (!isdir && !m3u && !dec_supported(e->d_name)) continue;
            if (nents == cap) { cap = cap ? cap * 2 : 64; ents = realloc(ents, sizeof *ents * cap); }
            ents[nents].name = strdup(e->d_name);
            ents[nents].dir = isdir;
            ents[nents].m3u = m3u;
            ents[nents].radio = 0;
            nents++;
        }
        closedir(d);
    }
    /* Virtual radio collection: App/RedAmp/stations.m3u, without touching user music. */
    char music[1024];
    path_join(music,sizeof music,root_dir,"Media/Music");
    if (!strcmp(dir,music) || !strcmp(dir,root_dir)) {
        char radiofile[1024];
        path_join(radiofile,sizeof radiofile,app_dir,"stations.m3u");
        if (access(radiofile,R_OK)==0) {
            b_entry *more=realloc(ents,sizeof *ents*(size_t)(nents+1));
            if (more) {
                ents=more;
                ents[nents].name=strdup("Internet Radio");
                ents[nents].dir=1;ents[nents].m3u=1;ents[nents].radio=1;
                if (ents[nents].name) nents++;
            }
        }
    }
    if (nents) qsort(ents, nents, sizeof *ents, cmp_ent);
    b_sel = 0;
    b_scroll = 0;
    if (select_name) for (int i = 0; i < nents; i++) if (!strcmp(ents[i].name, select_name)) b_sel = i;
    snprintf(S.last_dir, sizeof S.last_dir, "%s", cwd);
}

static int dir_exists(const char *p)
{
    struct stat st;
    return p && p[0] && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void open_browser(void)
{
    if (dir_exists(S.last_dir) && !strncmp(S.last_dir, root_dir, strlen(root_dir))) browse(S.last_dir, NULL);
    else {
        static const char *cands[] = { "Media/Music", "Music", "Media", NULL };
        char p[1024];
        const char *pick = root_dir;
        for (int i = 0; cands[i]; i++) {
            path_join(p, sizeof p, root_dir, cands[i]);
            if (dir_exists(p)) { pick = p; break; }
        }
        browse(pick, NULL);
    }
    screen = SCR_BROWSER;
}

static void library_return_from_folders(void);
static void browser_up(void)
{
    if (!strcmp(cwd, root_dir)) {
        library_return_from_folders();
        return;
    }
    char parent[1024], name[512];
    snprintf(parent, sizeof parent, "%s", cwd);
    char *sl = strrchr(parent, '/');
    if (!sl) { screen = browser_return; return; }
    snprintf(name, sizeof name, "%s", sl + 1);
    if (sl == parent) sl[1] = 0; else *sl = 0;
    if (strlen(parent) < strlen(root_dir)) snprintf(parent, sizeof parent, "%s", root_dir);
    browse(parent, name);
}

static void play_after_load(int idx, const char *what, int n)
{
    current = -1;
    sel = idx; scroll = 0;
    playlist_dirty = 1;
    if (S.shuffle) { current = idx; build_shuffle(); }
    play_index(idx, 0, 0);
    char b[160];
    (void)what; (void)n; /* No redundant "Library (n tracks)" toast. */
    /* Browser and indexed-library playback must not force the main screen. */
}

/* Individual track playback preserves the queue in every Library view. */
static void play_library_track(const char *path, const char *title)
{
    if (!path || (!radio_is_url(path) && access(path, R_OK) != 0)) {
        show_toast("%s", "Track unavailable"); return;
    }
    int n = pl_count(), idx = -1;
    const char *playing = pl_path(current);
    if (playing && !strcmp(playing, path)) idx = current;
    else for (int i = 0; i < n; i++) {
        const char *p = pl_path(i);
        if (p && !strcmp(p, path)) { idx = i; break; }
    }
    if (idx < 0) {
        if (!pl_add_named(path, title)) {
            show_toast("%s", "Could not add track to queue"); return;
        }
        idx = n;
        playlist_dirty = 1;
    }
    sel = idx;
    play_index(idx, 0, 0);
    /* Manual selection starts a new shuffle pass at the selected track. */
    if (S.shuffle) build_shuffle();
}

static void browser_activate(int action)   // 0 = A, 1 = X (add), 2 = Y (play folder)
{
    char p[1400];
    if (action == 2) {
        const char *dir = cwd;
        if (nents && ents[b_sel].dir && !ents[b_sel].radio) { path_join(p, sizeof p, cwd, ents[b_sel].name); dir = p; }
        if (!pl_dir_has_tracks(dir, 1)) { show_toast("%s", "No music in that folder"); return; }
        audio_stop();
        pl_clear();
        int n = pl_add_dir(dir, 1);
        if (!n) { show_toast("%s", "Could not load music from folder"); return; }
        play_after_load(0, "Playing folder", n);
        return;
    }
    if (!nents) return;
    b_entry *e = &ents[b_sel];
    if (e->radio) path_join(p,sizeof p,app_dir,"stations.m3u");
    else path_join(p, sizeof p, cwd, e->name);
    if (action == 1) {
        int n = e->m3u ? pl_load_m3u(p) : e->dir ? pl_add_dir(p, 1) : pl_add(p);
        playlist_dirty = 1;
        char b[64];
        snprintf(b, sizeof b, "Added %d track%s", n, n == 1 ? "" : "s");
        show_toast("%s", b);
        return;
    }
    if (e->dir && !e->radio) { browse(p, NULL); return; }
    if (e->m3u && !pl_m3u_has_tracks(p)) {
        show_toast("%s", "Playlist is empty");
        return;
    }
    if (e->m3u) {
        audio_stop();
        pl_clear();
        int n = pl_load_m3u(p);
        if (n) play_after_load(0, "Playing list", n);
        else show_toast("%s", "Could not load playlist");
        return;
    }
    // A plays this file without replacing the queue; Y plays the folder.
    play_library_track(p, NULL);
}

static void folder_icon(int x, int cy, uint32_t c)
{
    fill(x, cy - 6, 7, 2, c);
    fill(x, cy - 4, 18, 11, c);
    fill(x + 2, cy - 2, 14, 7, mix(c, C_BG, 0.25f));
}

/* Draw play/pause markers with pixel geometry, not glyph baselines: the latter
 * are offset differently in the primary and fallback fonts. */
static void draw_play_marker(int x, int cy, int paused, uint32_t color)
{
    if (paused) {
        fill(x, cy - 5, 3, 11, color);
        fill(x + 5, cy - 5, 3, 11, color);
    } else {
        for (int dy = -5; dy <= 5; dy++) {
            int reach = 6 - abs(dy);
            fill(x, cy + dy, 1 + reach, 1, color);
        }
    }
}

/* Folders and indexed-library views share precisely the same playback strip,
 * font sizes and spacing above the progress line. */
static void draw_library_playback_strip(void)
{
    fill(0, 425, SCREEN_W, 23, C_PANEL);
    if (current >= 0) {
        pl_item it;
        if (pl_get(current, &it)) {
            char name[300], timing[48], pos[18], length[18];
            fmt_time(pos, sizeof pos, audio_pos_ms());
            int ms = audio_len_ms();
            if (ms <= 0) ms = it.dur_ms;
            if (ms > 0) fmt_time(length, sizeof length, ms);
            else snprintf(length, sizeof length, "--:--");
            snprintf(timing, sizeof timing, "%s / %s", pos, length);
            int time_w = font_width(LM->aux, timing);
            int max_title = SCREEN_W - time_w - 94;
            if (max_title < 100) max_title = 100;
            font_fit(LM->lib, it.name, max_title, name, sizeof name);
            uint32_t color = accent();
            draw_play_marker(17, 431, audio_state() != A_PLAYING, color);
            font_draw_mid(bb, LM->lib, 38, 430, name, C_MUTED);
            font_draw_mid(bb, LM->aux, 620 - time_w, 430, timing, C_DIM);
        }
    }
    draw_track_progress(4, 445, SCREEN_W - 8, C_PANEL);
}

static void draw_browser(void)
{
    m2d_clear(bb, C_BG, 0);
    fill(0, 0, SCREEN_W, 50, C_PANEL);
    fill(0, 50, SCREEN_W, 1, C_LINE);
    font_draw_mid(bb, f_title, 20, 25, "LIBRARY", accent());
    const char *rel = cwd + strlen(root_dir);
    char shown[1100];
    snprintf(shown, sizeof shown, "%s", rel[0] ? rel : "/");
    char fit[1100];
    font_fit_left(f_small, shown, 440, fit, sizeof fit);
    font_draw_mid(bb, f_small, SCREEN_W - 20 - font_width(f_small, fit), 25, fit, C_DIM);
    if (!nents) {
        const char *t = "No folders or music files here";
        font_draw_mid(bb, f_mid, (SCREEN_W - font_width(f_mid, t)) / 2, 200, t, C_DIM);
    }
    if (b_sel < b_scroll) b_scroll = b_sel;
    if (b_sel >= b_scroll + B_ROWS) b_scroll = b_sel - B_ROWS + 1;
    uint32_t acc = accent();
    const char *playing_path = current >= 0 ? pl_path(current) : NULL;
    for (int r = 0; r < B_ROWS && b_scroll + r < nents; r++) {
        int i = b_scroll + r, y = B_TOP + r * B_ROWH;
        b_entry *e = &ents[i];
        if (i == b_sel) rrect(10, y, SCREEN_W - 20, B_ROWH - 2, 8, C_SEL);
        int cy = y + B_ROWH / 2 - 1;
        int playing_file = 0;
        if (!e->dir && !e->m3u && playing_path) {
            char fullpath[1400];
            path_join(fullpath, sizeof fullpath, cwd, e->name);
            playing_file = strcmp(fullpath, playing_path) == 0;
        }
        if (e->dir) folder_icon(24, cy, acc);
        else if (playing_file) draw_play_marker(27, cy, audio_state() != A_PLAYING, acc);
        else font_draw_mid(bb, LM->lib, 26, cy, e->m3u ? "≡" : "♪", e->m3u ? C_GREEN : C_DIM);
        char nm[600];
        char base[512];
        snprintf(base, sizeof base, "%s", e->name);
        if (!e->dir) { char *d = strrchr(base, '.'); if (d && d != base) *d = 0; }
        char fp[1400];path_join(fp,sizeof fp,cwd,e->name);
        int fav=!e->dir&&!e->m3u&&col_find(&favorites,fp)>=0;
        font_fit(LM->lib, base, SCREEN_W - 140-(fav?18:0), nm, sizeof nm);
        if(fav)draw_heart(SCREEN_W-80,cy,acc);
        font_draw_mid(bb, LM->lib, 54, cy, nm,
                      playing_file ? acc : i == b_sel ? C_TEXT : C_MUTED);
        if (!e->dir) {
            const char *ext = strrchr(e->name, '.');
            if (ext) {
                char up[8];
                int k = 0;
                for (ext++; *ext && k < 5; ext++) up[k++] = (*ext >= 'a' && *ext <= 'z') ? *ext - 32 : *ext;
                up[k] = 0;
                font_draw_mid(bb, LM->aux, SCREEN_W - 26 - font_width(LM->aux, up), cy, up, C_DIM);
            }
        }
    }
    if (nents > B_ROWS) {
        int track = B_ROWS * B_ROWH, th = track * B_ROWS / nents;
        if (th < 16) th = 16;
        int ty = B_TOP + (track - th) * b_scroll / (nents - B_ROWS);
        fill(SCREEN_W - 6, B_TOP, 3, track, C_LINE);
        fill(SCREEN_W - 6, ty, 3, th, C_DIM);
    }
    draw_library_playback_strip();
}

// ---------------------------------------------------------------- indexed library (Artists / Albums / Songs / Folders)
enum { LV_HOME, LV_ARTISTS, LV_ALBUMS, LV_ARTIST_ALBUMS, LV_TRACKS, LV_SONGS, LV_PLAYLISTS, LV_SAVED, LV_FAVORITES, LV_SEARCH };
static int lv = LV_HOME, lv_sel, lv_scroll, lv_artist, lv_album, lv_parent;
static int library_visited;
static char search_query[COL_NAME+1];
static int search_rows[LIB_MAX_TRACKS], search_count;
static void begin_search(void);
/* Run once per query or completed index refresh, never during rendering. */
static void search_refresh(void){
    search_count=0;
    if(!search_query[0])return;
    for(int i=0;i<library_count()&&search_count<LIB_MAX_TRACKS;i++){
        const library_track *t=library_song(i);if(!t)continue;
        const char *base=strrchr(t->path,'/');base=base?base+1:t->path;
        if(text_contains(t->title,search_query)||text_contains(t->artist,search_query)||
           text_contains(t->album_artist,search_query)||text_contains(t->album,search_query)||
           text_contains(base,search_query))search_rows[search_count++]=i;
    }
}
static void start_cover_fetch(int manual);
#define LIB_ROW_H (LM->lib_h)
#define LIB_ROWS ((LIB_BOTTOM - LIB_TOP) / LIB_ROW_H)
static void lib_view(int v) { lv=v; lv_sel=lv_scroll=0; album_art_clear_slot(0); if(v==LV_PLAYLISTS)refresh_collections(); }
static collection *active_collection(void){return lv==LV_FAVORITES?&favorites:lv==LV_SAVED?&saved:NULL;}
static const col_track *collection_track(int row){collection *c=active_collection();return c&&row>=0&&row<c->count?c->items+row:NULL;}
static int opened_row(void){for(int i=0;i<collection_count;i++)if(!strcmp(opened_name,collection_names[i]))return i;return 0;}
static void library_return_from_folders(void)
{
    screen = SCR_LIBRARY;
    lib_view(LV_HOME);
    lv_sel = 3;
}
static int lib_nrows(void)
{
    switch(lv){
    case LV_HOME:return 7;
    case LV_SEARCH:return search_count;
    case LV_PLAYLISTS:return collection_count+1; /* Create playlist is always available. */
    case LV_SAVED:return saved.count;
    case LV_FAVORITES:return favorites.count;
    case LV_ARTISTS:return library_artist_count();
    case LV_ALBUMS:return library_album_count();
    case LV_ARTIST_ALBUMS:{const library_artist *a=library_get_artist(lv_artist);return a?a->album_count:0;}
    case LV_TRACKS:{const library_album *a=library_get_album(lv_album);return a?a->count:0;}
    case LV_SONGS:return library_count();
    }
    return 0;
}
static int lib_album_at(int row)
{
    if(lv==LV_ALBUMS)return row;
    const library_artist *a=library_get_artist(lv_artist);
    return a?a->first_album+row:-1;
}
static int lib_song_at(int row)
{
    if(lv==LV_SONGS)return row;
    if(lv==LV_SEARCH)return row>=0&&row<search_count?search_rows[row]:-1;
    const library_album *a=library_get_album(lv_album);
    return a?a->first+row:-1;
}
static void open_library(void)
{
    if (!library_visited) {lib_view(LV_HOME);library_visited=1;}
    if(!library_count() && !library_is_scanning()) library_start_scan();
    screen=SCR_LIBRARY;
    S.last_view = 1;
    if(S.cover_fetch&&!library_is_scanning())start_cover_fetch(0);
}
static void library_back(void)
{
    if (lv == LV_HOME) return; /* SELECT is the only Player/Library toggle. */
    if(lv==LV_SAVED){lib_view(LV_PLAYLISTS);lv_sel=opened_row();}
    else if(lv==LV_TRACKS){lib_view(lv_parent); if(lv_parent==LV_ARTIST_ALBUMS){
        const library_artist *a=library_get_artist(lv_artist);if(a)lv_sel=lv_album-a->first_album;
    }else lv_sel=lv_album;}
    else if(lv==LV_ARTIST_ALBUMS){lib_view(LV_ARTISTS);lv_sel=lv_artist;}
    else {
        int home_sel = lv == LV_ARTISTS ? 0 : lv == LV_ALBUMS ? 1 : lv==LV_PLAYLISTS?4:lv==LV_FAVORITES?5:lv==LV_SEARCH?6:2;
        lib_view(LV_HOME);
        lv_sel = home_sel; /* Return focus to the category just exited. */
    }
    if (lv_sel < lv_scroll) lv_scroll = lv_sel;
    if (lv_sel >= lv_scroll + LIB_ROWS) lv_scroll = lv_sel - LIB_ROWS + 1;
}
static void library_add_range(int first,int count,int play,int start)
{
    if(first<0||count<=0||first+count>library_count())return;
    if(play){
        audio_stop();pl_clear();
    }
    int n=0;
    for(int i=first;i<first+count;i++){
        const library_track *t=library_song(i);
        if(t&&pl_add(t->path))n++;
    }
    if(!n){show_toast("%s","Nothing added to playlist");return;}
    playlist_dirty=1;
    if(play){
        if(start<0||start>=n)start=0;
        play_after_load(start,"Library",n);
        /* Playing from an album should not throw the user back to the main
         * window. Keep artist, album, cursor and scroll position intact. */
        screen = SCR_LIBRARY;
    }else{
        char m[80];snprintf(m,sizeof m,"Added %d tracks",n);show_toast("%s",m);
    }
}
/* Snapshot album representatives; the worker copies each title/path/artist. */
static int same_cover_directory(const char *a,const char *b)
{
    const char *x=strrchr(a,'/'),*y=strrchr(b,'/');
    return x&&y&&x-a==y-b&&!strncmp(a,b,(size_t)(x-a));
}
static int folder_art_safe(const char *path,const char *artist,const char *album)
{
    /* No folder image to consider: avoid a library walk for most albums. */
    const char *names[]={"cover.jpg","cover.jpeg","cover.png","folder.jpg","folder.png","front.jpg","front.png",NULL};
    char full[1400];int present=0;
    for(int i=0;names[i];i++)if(cover_folder_image(path,names[i],full,sizeof full)&&!access(full,R_OK)){present=1;break;}
    if(!present)return 0;
    char key[17];int known=cover_album_key(artist,album,key),indexed=0;
    for(int i=0;i<library_count();i++){
        const library_track *t=library_song(i);
        if(!strcmp(path,t->path)){indexed=1;continue;}
        if(!same_cover_directory(path,t->path))continue;
        char other[17];
        if(!known||!cover_album_key(t->album_artist,t->album,other)||strcmp(key,other))return 0;
    }
    if(indexed&&!library_is_scanning())return 1;
    /* Outside the index (or while scanning), check siblings before trusting
     * a generic image. Bound work and err on the side of no cover. */
    const char *slash=strrchr(path,'/');if(!slash)return 0;
    char dir[1024];if(slash-path>=(int)sizeof dir)return 0;
    snprintf(dir,sizeof dir,"%.*s",(int)(slash-path),path);
    DIR *d=opendir(*dir?dir:"/");if(!d)return 0;
    struct dirent *e;int safe=1,checked=0;
    while((e=readdir(d))){
        if(e->d_name[0]=='.'||!dec_supported(e->d_name)||!strcmp(e->d_name,slash+1))continue;
        int n=snprintf(full,sizeof full,"%s/%s",dir,e->d_name);
        if(n<0||n>=(int)sizeof full){safe=0;break;}
        struct stat st;if(lstat(full,&st)||!S_ISREG(st.st_mode))continue;
        track_meta m;char other[17];
        if(++checked>64||!known||!meta_read(full,&m)||
           !cover_album_key(m.album_artist[0]?m.album_artist:m.artist,m.album,other)||strcmp(key,other)){
            safe=0;break;
        }
    }
    closedir(d);return safe;
}
static int resolve_cover_album(const char *path,char artist[160],char album[160])
{
    for(int i=0;i<library_count();i++){
        const library_track *t=library_song(i);
        if(!strcmp(t->path,path)){
            snprintf(artist,160,"%s",t->album_artist);snprintf(album,160,"%s",t->album);
            return folder_art_safe(path,artist,album);
        }
    }
    track_meta m;
    if(meta_read(path,&m)){
        snprintf(artist,160,"%s",m.album_artist[0]?m.album_artist:m.artist);
        snprintf(album,160,"%s",m.album);
    }
    return folder_art_safe(path,artist,album);
}
static void start_cover_fetch(int manual)
{
    if(!netclock_network_available()){if(manual)show_toast("%s","Network unavailable");return;}
    if(cover_running){if(manual)show_toast("%s","Cover fetch already running");return;}
    if(library_is_scanning()){
        show_toast("%s","Library scan still running");return;
    }
    int n=library_album_count();
    if(!n){show_toast("%s","No albums indexed");return;}
    cover_job *jobs=calloc((size_t)n,sizeof *jobs);
    if(!jobs)return;
    int count=0;
    for(int i=0;i<n;i++){
        const library_album *a=library_get_album(i);
        const library_track *t=a?library_song(a->first):NULL;
        if(!t||!t->path||!t->artist||!t->album)continue;
        jobs[count++]=(cover_job){t->path,a->artist,a->album,folder_art_safe(t->path,a->artist,a->album)};
    }
    int result=0;
    if(count&&!screen_off&&!service_running())
        result=manual?covers_start_manual(jobs,count,S.cover_save_to_folder,data_dir):
                      covers_start(jobs,count,S.cover_save_to_folder,data_dir);
    if(result){cover_manual=manual;cover_saved=0;cover_running=1;}
    free(jobs);
    if(manual)show_toast("%s",result?"Fetching missing covers":"Cover fetch already running / unavailable");
}
/* Hold B in any Library view, including nested Folders, to jump directly
 * between the top-level categories without repeatedly backing out. */
enum { LIBCAT_ARTISTS, LIBCAT_ALBUMS, LIBCAT_SONGS, LIBCAT_FOLDERS, LIBCAT_PLAYLISTS, LIBCAT_FAVORITES, LIBCAT_SEARCH, LIBCAT_HOME, LIBCAT_COUNT };
static const char *const library_categories[LIBCAT_COUNT] = {
    "Artists", "Albums", "Songs", "Folders", "Playlists", "Favorites", "Search", "Library home"
};
static int library_views_sel, library_views_return = SCR_LIBRARY;
static int current_library_category(void)
{
    if (screen == SCR_BROWSER) return LIBCAT_FOLDERS;
    if (lv == LV_ARTISTS || lv == LV_ARTIST_ALBUMS) return LIBCAT_ARTISTS;
    if (lv == LV_ALBUMS) return LIBCAT_ALBUMS;
    if (lv == LV_TRACKS) return lv_parent == LV_ARTIST_ALBUMS ? LIBCAT_ARTISTS : LIBCAT_ALBUMS;
    if (lv == LV_SONGS) return LIBCAT_SONGS;
    if(lv==LV_PLAYLISTS||lv==LV_SAVED)return LIBCAT_PLAYLISTS;
    if(lv==LV_FAVORITES)return LIBCAT_FAVORITES;
    if(lv==LV_SEARCH)return LIBCAT_SEARCH;
    return LIBCAT_HOME;
}
static void open_library_views(void)
{
    if (screen != SCR_LIBRARY && screen != SCR_BROWSER) return;
    library_views_return = screen;
    library_views_sel = current_library_category();
    screen = SCR_LIBRARY_VIEWS;
}
static void choose_library_category(int cat)
{
    if (cat < 0 || cat >= LIBCAT_COUNT) return;
    if (cat == LIBCAT_FOLDERS) {
        if (library_views_return == SCR_BROWSER) screen = SCR_BROWSER;
        else { open_browser(); browser_return = SCR_LIBRARY; }
    } else if(cat==LIBCAT_SEARCH){
        screen=SCR_LIBRARY;begin_search();
    } else {
        int v = cat == LIBCAT_ARTISTS ? LV_ARTISTS : cat == LIBCAT_ALBUMS ? LV_ALBUMS :
                cat == LIBCAT_SONGS ? LV_SONGS : cat==LIBCAT_PLAYLISTS?LV_PLAYLISTS:cat==LIBCAT_FAVORITES?LV_FAVORITES:LV_HOME;
        lib_view(v);
        screen = SCR_LIBRARY;
    }
    S.last_view = 1;
}
static void draw_library_views(void)
{
    const int x = 158, y = 28, w = 324, h = 400, row_h = 42;
    card(x, y, w, h, "LIBRARY VIEWS");
    for (int i = 0; i < LIBCAT_COUNT; i++) {
        int ry = y + 52 + i * row_h;
        if (i == library_views_sel) rrect(x + 13, ry, w - 26, row_h - 3, 7, C_SEL);
        font_draw_mid(bb, f_mid, x + 30, ry + row_h / 2,
                      library_categories[i], i == library_views_sel ? accent() : C_TEXT);
    }
}

/* Saved playlists and favorites never mutate the queue until Play or Add. */
enum { NAME_NEW, NAME_SAVE, NAME_RENAME, NAME_SEARCH };
enum { CM_FAVORITE,CM_ADD,CM_REMOVE,CM_SAVE,CM_NEW,CM_RENAME,CM_DELETE,CM_CLEAR,CM_SEARCH };
static int collection_return, cm_count, cm_sel, cm_actions[8], name_mode, name_row, name_col, name_page, name_shift, pick_sel, pick_scroll;
static char name_text[COL_NAME+1], target_name[COL_NAME+1];
static col_track selected_track;
static char selected_path[2048], selected_title[512];
static const col_track *selected_collection_track(void){
    selected_path[0]=selected_title[0]=0;
    if(screen==SCR_PLAYLIST){pl_item it;if(!pl_get(sel,&it))return NULL;snprintf(selected_path,sizeof selected_path,"%s",it.path);snprintf(selected_title,sizeof selected_title,"%s",it.name);}
    else if(screen==SCR_BROWSER){if(!nents||ents[b_sel].dir||ents[b_sel].m3u||ents[b_sel].radio)return NULL;
        path_join(selected_path,sizeof selected_path,cwd,ents[b_sel].name);meta_display_name(NULL,selected_path,selected_title,sizeof selected_title);}
    else if(screen==SCR_LIBRARY){
        if(active_collection()){const col_track *t=collection_track(lv_sel);if(!t)return NULL;snprintf(selected_path,sizeof selected_path,"%s",t->path);snprintf(selected_title,sizeof selected_title,"%s",t->title);}
        else if(lv==LV_TRACKS||lv==LV_SONGS||lv==LV_SEARCH){const library_track *t=library_song(lib_song_at(lv_sel));if(!t)return NULL;snprintf(selected_path,sizeof selected_path,"%s",t->path);snprintf(selected_title,sizeof selected_title,"%s",t->title);}
        else return NULL;
    }else return NULL;
    selected_track=(col_track){selected_path,selected_title};return &selected_track;
}
static void toggle_favorite(void){
    if(!selected_path[0])return;
    if(!favorites_readable){show_toast("%s","Cannot read favorites file");return;}
    int i=col_find(&favorites,selected_path),ok=1;collection next={0};
    for(int j=0;j<favorites.count&&ok;j++)if(j!=i)ok=col_add(&next,favorites.items[j].path,favorites.items[j].title);
    if(i<0&&ok)ok=col_add(&next,selected_path,selected_title);
    if(ok)ok=col_write(&next,favorite_file);
    if(ok){col_clear(&favorites);favorites=next;show_toast("%s",i>=0?"Removed from Favorites":"Added to Favorites");}
    else {col_clear(&next);show_toast("%s","Cannot save favorites");}
}
static void collection_enqueue(const collection *c,int play,int start,int single){
    if(!c->count){show_toast("%s","Playlist is empty");return;}
    if(start<0||start>=c->count)start=0;
    int first=single?start:0,last=single?start+1:c->count;
    int available=0;
    for(int i=first;i<last;i++)if(radio_is_url(c->items[i].path)||access(c->items[i].path,R_OK)==0)available++;
    if(!available){show_toast("%s","No available tracks");return;}
    if(play){audio_stop();pl_clear();current=-1;}
    int n=0,idx=-1;
    for(int i=first;i<last;i++){
        const col_track *t=c->items+i;
        if(!radio_is_url(t->path)&&access(t->path,R_OK)!=0)continue;
        if(pl_add_named(t->path,t->title)){if(i>=start&&idx<0)idx=n;n++;}
    }
    playlist_dirty=1;
    if(play&&n)play_after_load(idx<0?0:idx,"Saved playlist",n);
    else if(play){save_all();show_toast("%s","Could not load playlist");}
    else {char text[80];snprintf(text,sizeof text,"Added %d tracks to queue",n);show_toast("%s",text);}
}
static void begin_name(int mode,const char *initial){
    name_mode=mode;name_row=1;name_col=0;name_page=0;name_shift=0;snprintf(name_text,sizeof name_text,"%s",initial?initial:"");screen=SCR_COLLECTION_NAME;
}
static void begin_search(void){
    collection_return=SCR_LIBRARY;
    begin_name(NAME_SEARCH,search_query);
}
static int name_exists(const char *name){for(int i=0;i<collection_count;i++)if(!text_casecmp(name,collection_names[i]))return 1;return 0;}
static void finish_name(void){
    if(name_mode==NAME_SEARCH){
        if(!name_text[0]||strspn(name_text," ")==strlen(name_text)){show_toast("%s","Enter a search term");return;}
        strcpy(search_query,name_text);search_refresh();lib_view(LV_SEARCH);screen=SCR_LIBRARY;return;
    }
    if(!col_valid_name(name_text)){show_toast("%s","Enter a valid playlist name");return;}
    refresh_collections();
    if(name_mode==NAME_RENAME&&!strcmp(name_text,target_name)){screen=collection_return;return;}
    if(name_exists(name_text)){show_toast("%s","Playlist name already exists");return;}
    if(name_mode!=NAME_RENAME&&collection_count>=COL_NAMES){show_toast("%s","Playlist limit reached");return;}
    char dest[1200],src[1200];collection_file(dest,sizeof dest,name_text);int ok=0;
    struct stat st;if(lstat(dest,&st)==0){show_toast("%s","Playlist name already exists");return;}
    if(name_mode==NAME_RENAME){collection_file(src,sizeof src,target_name);ok=rename(src,dest)==0;}
    else {collection c={0};ok=1;if(name_mode==NAME_SAVE){for(int i=0;i<pl_count();i++){pl_item it;if(!pl_get(i,&it)||!col_add(&c,it.path,it.name)){ok=0;break;}}}if(ok)ok=col_write(&c,dest);col_clear(&c);}
    if(!ok){show_toast("%s","Could not save playlist");return;}
    refresh_collections();screen=collection_return;
    if(screen==SCR_LIBRARY&&lv==LV_PLAYLISTS){for(int i=0;i<collection_count;i++)if(!strcmp(name_text,collection_names[i])){lv_sel=i;break;}}
    if(name_mode==NAME_RENAME&&lv==LV_SAVED&&!strcmp(opened_name,target_name))strcpy(opened_name,name_text);
    show_toast("%s",name_mode==NAME_RENAME?"Playlist renamed":"Playlist saved");
}
static void open_collection_actions(void){
    collection_return=screen;cm_count=cm_sel=0;selected_collection_track();target_name[0]=0;
    if(selected_path[0]){cm_actions[cm_count++]=CM_FAVORITE;cm_actions[cm_count++]=CM_ADD;}
    if(screen==SCR_PLAYLIST){if(pl_count())cm_actions[cm_count++]=CM_REMOVE;cm_actions[cm_count++]=CM_SAVE;}
    if(screen==SCR_LIBRARY){
        if(lv==LV_SAVED&&selected_path[0])cm_actions[cm_count++]=CM_REMOVE;
        if(lv==LV_PLAYLISTS&&lv_sel<collection_count)strcpy(target_name,collection_names[lv_sel]);
        else if(lv==LV_SAVED)strcpy(target_name,opened_name);
        if(target_name[0]){cm_actions[cm_count++]=CM_RENAME;cm_actions[cm_count++]=CM_DELETE;}
    }
    cm_actions[cm_count++]=CM_NEW;
    if(screen!=SCR_PLAYLIST)cm_actions[cm_count++]=CM_SAVE;
    if(screen==SCR_PLAYLIST&&pl_count())cm_actions[cm_count++]=CM_CLEAR;
    if(screen==SCR_LIBRARY&&lv==LV_SEARCH)cm_actions[cm_count++]=CM_SEARCH;
    screen=SCR_COLLECTION_MENU;
}
static void remove_selected(void){
    if(collection_return==SCR_PLAYLIST){
        if(sel<0||sel>=pl_count())return;
        if(sel==current){audio_stop();current=-1;}
        else if(sel<current)current--;
        pl_remove(sel);clamp_scroll(pl_count());playlist_dirty=1;
        if(S.shuffle)build_shuffle();
        save_all();show_toast("%s","Removed from play queue");
    }else if(lv==LV_FAVORITES){toggle_favorite();}
    else if(lv==LV_SAVED&&lv_sel<saved.count){
        char file[1200];collection_file(file,sizeof file,opened_name);collection next={0};int ok=1;
        for(int j=0;j<saved.count&&ok;j++)if(j!=lv_sel)ok=col_add(&next,saved.items[j].path,saved.items[j].title);
        if(ok&&col_write(&next,file)){col_clear(&saved);saved=next;show_toast("%s","Removed from saved playlist");}
        else {col_clear(&next);show_toast("%s","Cannot update playlist");}
    }
}
static void add_to_named(int row){
    char file[1200];collection_file(file,sizeof file,collection_names[row]);collection c={0};
    int ok=col_read(&c,file);
    if(ok){if(col_find(&c,selected_path)>=0){show_toast("%s","Already in playlist");col_clear(&c);screen=collection_return;return;}
        ok=col_add(&c,selected_path,selected_title)&&col_write(&c,file);}
    if(ok&&lv==LV_SAVED&&!strcmp(opened_name,collection_names[row]))col_read(&saved,file);
    col_clear(&c);screen=collection_return;show_toast("%s",ok?"Added to saved playlist":"Cannot update playlist");
}
static const char *action_label(int a){
    switch(a){case CM_FAVORITE:return col_find(&favorites,selected_path)>=0?"Remove from Favorites":"Add to Favorites";
    case CM_ADD:return "Add to saved playlist";case CM_REMOVE:return collection_return==SCR_PLAYLIST?"Remove from play queue":lv==LV_FAVORITES?"Remove favorite":"Remove track from playlist";
    case CM_SAVE:return "Save play queue as playlist";case CM_NEW:return "Create empty playlist";case CM_RENAME:return "Rename playlist";case CM_CLEAR:return "Clear play queue";case CM_SEARCH:return "Edit search";default:return "Delete playlist";}
}
/* Five rows, centered and staggered like a conventional keyboard. The final
 * row has wide controls; vertical navigation chooses the nearest key center. */
static const char *const key_rows[3][2][4]={
    {{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm"},
     {"!@#$%^&*()","QWERTYUIOP","ASDFGHJKL","ZXCVBNM"}},
    {{"àáâãäåæçčć","èéêëěìíîïł","ñńòóôõöøœř","śšßùúûüýÿž"},
     {"ÀÁÂÃÄÅÆÇČĆ","ÈÉÊËĚÌÍÎÏŁ","ÑŃÒÓÔÕÖØŒŘ","ŚŠẞÙÚÛÜÝŸŽ"}},
    {{"!@#$%^&*()","-_+=[]{}<>",".,:;!?/\\|~","'\"`€£¥°©®…"},
     {"!@#$%^&*()","-_+=[]{}<>",".,:;!?/\\|~","'\"`€£¥°©®…"}}
};
static int kb_cols(int row){return row==4?3:text_count(key_rows[name_page][name_shift][row]);}
static void kb_rect(int row,int col,int *x,int *w){
    if(row==4){static const int pos[]={70,180,450},width[]={100,260,120};*x=pos[col];*w=width[col];return;}
    int n=kb_cols(row),pitch=n>10?500/n:50;*x=(640-n*pitch)/2+col*pitch+3;*w=pitch-6;
}
static int kb_center(int row,int col){int x,w;kb_rect(row,col,&x,&w);return x+w/2;}
static void kb_vertical(int direction){
    int center=kb_center(name_row,name_col);name_row=(name_row+5+direction)%5;
    int best=10000;name_col=0;for(int i=0;i<kb_cols(name_row);i++){
        int d=abs(kb_center(name_row,i)-center);if(d<best){best=d;name_col=i;}}
}
static void kb_label(int row,int col,char out[20]){
    if(row==4){snprintf(out,20,"%s",col==0?"SHIFT":col==1?"Space":"Backspace");return;}
    const char *p=key_rows[name_page][name_shift][row];uint32_t cp;
    for(int i=0;i<col;i++)text_next(&p,&cp);
    const char *start=p;text_next(&p,&cp);memcpy(out,start,(size_t)(p-start));out[p-start]=0;
}
static int kb_allowed(const char *key){
    return name_mode==NAME_SEARCH||strlen(key)!=1||!strchr("<>:\"/\\|?*",key[0]);
}
static void kb_press(void){
    if(name_row==4&&name_col==0){if(name_page!=2)name_shift=!name_shift;return;}
    if(name_row==4&&name_col==2){text_erase(name_text);return;}
    char key[20];if(name_row==4)strcpy(key," ");else kb_label(name_row,name_col,key);
    if(!kb_allowed(key)){show_toast("%s","Not allowed in playlist names");return;}
    if(!text_append(name_text,sizeof name_text,COL_NAME_CHARS,key))show_toast("%s","40 character limit");
}
static void collection_key(int k,int repeat){
    if(screen==SCR_COLLECTION_NAME){
        if(k==K_LEFT)name_col=(name_col+kb_cols(name_row)-1)%kb_cols(name_row);
        else if(k==K_RIGHT)name_col=(name_col+1)%kb_cols(name_row);
        else if(k==K_UP)kb_vertical(-1);
        else if(k==K_DOWN)kb_vertical(1);
        else if(!repeat&&(k==K_L||k==K_R)){
            name_page=(name_page+(k==K_L?2:1))%3;name_shift=0;
            if(name_col>=kb_cols(name_row))name_col=kb_cols(name_row)-1;
        }
        else if(!repeat&&k==K_A)kb_press();
        else if(!repeat&&k==K_X)text_erase(name_text);
        else if(!repeat&&k==K_Y)finish_name();
        else if(!repeat&&k==K_B)screen=collection_return;
        return;
    }
    if(screen==SCR_QUEUE_CLEAR){
        if(!repeat&&k==K_A){audio_stop();current=-1;pl_clear();sel=scroll=0;
            if(S.shuffle)build_shuffle();
            playlist_dirty=1;save_all();screen=collection_return;show_toast("%s","Play queue cleared");}
        else if(!repeat&&k==K_B)screen=SCR_COLLECTION_MENU;
        return;
    }
    if(screen==SCR_COLLECTION_DELETE){if(!repeat&&k==K_A){char file[1200];collection_file(file,sizeof file,target_name);
            int ok=unlink(file)==0;if(ok){refresh_collections();if(lv==LV_SAVED){col_clear(&saved);lib_view(LV_PLAYLISTS);}}
            screen=collection_return;show_toast("%s",ok?"Playlist deleted":"Cannot delete playlist");}
        else if(!repeat&&k==K_B)screen=SCR_COLLECTION_MENU;
        return;}
    if(screen==SCR_COLLECTION_PICK){if(k==K_UP)pick_sel=(pick_sel+collection_count-1)%collection_count;
        else if(k==K_DOWN)pick_sel=(pick_sel+1)%collection_count;
        else if(!repeat&&k==K_A)add_to_named(pick_sel);
        else if(!repeat&&k==K_B)screen=SCR_COLLECTION_MENU;
        return;}
    if(k==K_UP)cm_sel=(cm_sel+cm_count-1)%cm_count;
    else if(k==K_DOWN)cm_sel=(cm_sel+1)%cm_count;
    else if(!repeat&&k==K_B)screen=collection_return;
    else if(!repeat&&k==K_A){switch(cm_actions[cm_sel]){
        case CM_FAVORITE:toggle_favorite();screen=collection_return;break;
        case CM_ADD:refresh_collections();if(collection_count){pick_sel=pick_scroll=0;screen=SCR_COLLECTION_PICK;}else show_toast("%s","Create a playlist first");break;
        case CM_REMOVE:remove_selected();screen=collection_return;break;
        case CM_SAVE:begin_name(NAME_SAVE,NULL);break;
        case CM_NEW:begin_name(NAME_NEW,NULL);break;
        case CM_RENAME:begin_name(NAME_RENAME,target_name);break;
        case CM_DELETE:screen=SCR_COLLECTION_DELETE;break;
        case CM_CLEAR:screen=SCR_QUEUE_CLEAR;break;
        case CM_SEARCH:begin_search();break;
    }}
}
static void draw_collection_dialog(void){
    m2d_clear(bb,C_BG,0);
    if(screen==SCR_COLLECTION_NAME){
        card(45,20,550,412,name_mode==NAME_SEARCH?"SEARCH MUSIC":name_mode==NAME_RENAME?"RENAME PLAYLIST":name_mode==NAME_SAVE?"SAVE PLAY QUEUE":"CREATE PLAYLIST");
        rrect(65,72,510,42,7,C_SEL);char fit[COL_NAME+8];font_fit_left(f_mid,name_text,477,fit,sizeof fit);
        font_draw_mid(bb,f_mid,78,93,fit,C_TEXT);
        static const char *pages[]={"ABC","International","Symbols"};
        for(int i=0;i<3;i++){
            int x=65+i*170;uint32_t c=i==name_page?accent():C_DIM;
            font_draw_mid(bb,f_small,x+(170-font_width(f_small,pages[i]))/2,134,pages[i],c);
            if(i==name_page)fill(x+18,148,134,2,c);
        }
        for(int r=0;r<5;r++)for(int i=0;i<kb_cols(r);i++){
            int x,w,y=162+r*45;kb_rect(r,i,&x,&w);char key[20];kb_label(r,i,key);
            int selected=r==name_row&&i==name_col,shift=r==4&&i==0;
            int disabled=(shift&&name_page==2)||(r<4&&!kb_allowed(key));
            uint32_t bg=selected?accent():(shift&&name_shift&&name_page!=2)?C_KEY:C_SEL;
            unsigned luma=299*((bg>>16)&255)+587*((bg>>8)&255)+114*(bg&255);
            uint32_t fg=selected?(luma>=140000?C_BG:C_TEXT):(disabled?C_DIM:C_TEXT);
            rrect(x,y,w,39,6,bg);
            if(shift&&name_shift&&name_page!=2)fill(x+12,y+32,w-24,2,selected?fg:accent());
            font_draw_mid(bb,r==4?f_small:f_mid,x+(w-font_width(r==4?f_small:f_mid,key))/2,y+19,key,fg);
        }
        hint h[]={{"L/R","Page"},{"A","Type"},{"X","Erase"},{"Y",name_mode==NAME_SEARCH?"Search":"Save"},{"B","Cancel"}};hint_bar(h,5);return;
    }
    if(screen==SCR_QUEUE_CLEAR){card(65,140,510,160,"CLEAR PLAY QUEUE?");
        font_draw_mid(bb,f_mid,88,210,"Stop playback and empty the queue",C_TEXT);
        font_draw_mid(bb,f_small,88,244,"Saved playlists, favorites and files are kept",C_DIM);
        hint h[]={{"A","Clear"},{"B","Cancel"}};hint_bar(h,2);return;}
    if(screen==SCR_COLLECTION_DELETE){card(65,140,510,160,"DELETE PLAYLIST?");char fit[COL_NAME+8];font_fit(f_mid,target_name,450,fit,sizeof fit);
        font_draw_mid(bb,f_mid,88,210,fit,C_TEXT);font_draw_mid(bb,f_small,88,244,"Music files and play queue are not deleted",C_DIM);
        hint h[]={{"A","Delete"},{"B","Cancel"}};hint_bar(h,2);return;}
    int pick=screen==SCR_COLLECTION_PICK,n=pick?collection_count:cm_count,selected=pick?pick_sel:cm_sel;
    if(pick){if(pick_sel<pick_scroll)pick_scroll=pick_sel;if(pick_sel>=pick_scroll+8)pick_scroll=pick_sel-7;}
    int first=pick?pick_scroll:0;card(65,28,510,400,pick?"ADD TO PLAYLIST":"ACTIONS");
    for(int i=first;i<n&&i<first+8;i++){int y=80+(i-first)*42;if(i==selected)rrect(78,y,484,39,7,C_SEL);char fit[COL_NAME+8];font_fit(f_mid,pick?collection_names[i]:action_label(cm_actions[i]),450,fit,sizeof fit);font_draw_mid(bb,f_mid,92,y+19,fit,i==selected?accent():C_TEXT);}
    hint h[]={{"A","Select"},{"B","Back"}};hint_bar(h,2);
}
static void library_action(int k)
{
    int n=lib_nrows();
    if(lv==LV_HOME){
        if(k==K_X){library_menu_sel=0;screen=SCR_LIBRARY_MENU;return;}
        if(k!=K_A)return;
        if(lv_sel==3){open_browser();browser_return=SCR_LIBRARY;return;}
        if(lv_sel==6){begin_search();return;}
        lib_view(lv_sel==0?LV_ARTISTS:lv_sel==1?LV_ALBUMS:lv_sel==4?LV_PLAYLISTS:lv_sel==5?LV_FAVORITES:LV_SONGS);return;
    }
    if(lv==LV_PLAYLISTS){
        if(lv_sel==collection_count){if(k==K_A){collection_return=SCR_LIBRARY;begin_name(NAME_NEW,NULL);}return;}
        if(k==K_A){char file[1200];collection_file(file,sizeof file,collection_names[lv_sel]);
            if(col_read(&saved,file)){strcpy(opened_name,collection_names[lv_sel]);lib_view(LV_SAVED);}
            else show_toast("%s","Cannot read playlist");}
        else if(k==K_X||k==K_Y){collection c={0};char file[1200];collection_file(file,sizeof file,collection_names[lv_sel]);
            if(col_read(&c,file))collection_enqueue(&c,k==K_Y,0,0);else show_toast("%s","Cannot read playlist");col_clear(&c);}
        return;
    }
    if(lv==LV_SEARCH){
        if(!n){if(k==K_A)begin_search();return;}
        if(k==K_X){library_add_range(lib_song_at(lv_sel),1,0,0);return;}
        if(k==K_A){
            const library_track *t=library_song(lib_song_at(lv_sel));
            if(t)play_library_track(t->path,NULL);
            return;
        }
        if(k==K_Y){
            audio_stop();pl_clear();current=-1;int added=0;
            for(int i=0;i<n;i++){const library_track *t=library_song(lib_song_at(i));
                if(t&&pl_add(t->path))added++;}
            playlist_dirty=1;
            if(added)play_after_load(0,"Search",added);
            else show_toast("%s","No available tracks");
        }return;
    }
    if(n<=0)return;
    if(active_collection()){
        if(k==K_A){const col_track *t=collection_track(lv_sel);
            if(t)play_library_track(t->path,t->title);}
        else if(k==K_X||k==K_Y)collection_enqueue(active_collection(),k==K_Y,k==K_Y?0:lv_sel,k==K_X);
        return;
    }
    if(lv==LV_ARTISTS){
        const library_artist *a=library_get_artist(lv_sel);
        if(!a)return;
        if(k==K_A){lv_artist=lv_sel;lib_view(LV_ARTIST_ALBUMS);return;}
        if(k==K_X||k==K_Y){
            const library_album *first=library_get_album(a->first_album);
            const library_album *last=library_get_album(a->first_album+a->album_count-1);
            if(first&&last)library_add_range(first->first,last->first+last->count-first->first,k==K_Y,0);
        }
    }else if(lv==LV_ALBUMS||lv==LV_ARTIST_ALBUMS){
        int idx=lib_album_at(lv_sel);
        const library_album *a=library_get_album(idx);
        if(!a)return;
        if(k==K_A){lv_parent=lv;lv_album=idx;lib_view(LV_TRACKS);}
        else if(k==K_X||k==K_Y)library_add_range(a->first,a->count,k==K_Y,0);
    }else if(lv==LV_TRACKS||lv==LV_SONGS){
        int idx=lib_song_at(lv_sel);
        if(k==K_X)library_add_range(idx,1,0,0);
        else if(k==K_A){const library_track *t=library_song(idx);
            if(t)play_library_track(t->path,NULL);}
        else if(k==K_Y){
            if(lv==LV_TRACKS){const library_album *a=library_get_album(lv_album);
                if(a)library_add_range(a->first,a->count,1,lv_sel);}
            else library_add_range(0,library_count(),1,lv_sel);
        }
    }
}
static void draw_library(void)
{
    m2d_clear(bb,C_BG,0);fill(0,0,SCREEN_W,51,C_PANEL);
    font_draw_mid(bb,f_title,20,25,lv==LV_SEARCH?"SEARCH":lv==LV_PLAYLISTS?"PLAYLISTS":lv==LV_FAVORITES?"FAVORITES":lv==LV_SAVED?"SAVED PLAYLIST":"MUSIC LIBRARY",accent());
    char sub[COL_NAME+64];
    if(lv==LV_SEARCH)snprintf(sub,sizeof sub,"%s  ·  %d matches",search_query,search_count);
    else if(lv==LV_SAVED)snprintf(sub,sizeof sub,"%s  ·  %d tracks",opened_name,saved.count);
    else if(lv==LV_FAVORITES)snprintf(sub,sizeof sub,"%d favorites",favorites.count);
    else if(lv==LV_PLAYLISTS)snprintf(sub,sizeof sub,"%d playlists",collection_count);
    else if(library_is_scanning())snprintf(sub,sizeof sub,"Indexing music…");
    else snprintf(sub,sizeof sub,"%d songs  ·  %d albums  ·  %d artists",library_count(),library_album_count(),library_artist_count());
    char subfit[COL_NAME+64];font_fit(f_small,sub,355,subfit,sizeof subfit);
    font_draw_mid(bb,f_small,SCREEN_W-20-font_width(f_small,subfit),25,subfit,C_DIM);
    static const char *roots[]={"Artists","Albums","Songs","Folders","Playlists","Favorites","Search"};
    int n=lib_nrows(),with_art=S.cover_library&&(lv==LV_ALBUMS||lv==LV_ARTIST_ALBUMS||lv==LV_TRACKS||lv==LV_SEARCH||active_collection());
    if(lv_sel>=n)lv_sel=n?n-1:0;
    if(lv_sel<0)lv_sel=0;
    if(lv_sel<lv_scroll)lv_scroll=lv_sel;
    if(lv_sel>=lv_scroll+LIB_ROWS)lv_scroll=lv_sel-LIB_ROWS+1;
    int right=with_art?422:SCREEN_W-22;
    const char *playing = current >= 0 ? pl_path(current) : NULL;
    for(int r=0;r<LIB_ROWS&&lv_scroll+r<n;r++){
        int i=lv_scroll+r,y=LIB_TOP+r*LIB_ROW_H;
        if(i==lv_sel)rrect(12,y,right-22,LIB_ROW_H-2,6,C_SEL);
        const char *label="",*tag="",*rowpath=NULL;char b[128]={0},fallback[400];
        if(lv==LV_HOME)label=roots[i];
        else if(lv==LV_PLAYLISTS)label=i==collection_count?"+ Create playlist":collection_names[i];
        else if(active_collection()){
            const col_track *t=collection_track(i);if(t){rowpath=t->path;meta_display_name(NULL,t->path,fallback,sizeof fallback);label=t->title[0]?t->title:fallback;}
        }
        else if(lv==LV_ARTISTS){const library_artist *a=library_get_artist(i);if(a){label=a->name;
            snprintf(b,sizeof b,"%d",a->album_count);tag=b;}}
        else if(lv==LV_ALBUMS||lv==LV_ARTIST_ALBUMS){const library_album *a=library_get_album(lib_album_at(i));
            if(a){label=a->album;snprintf(b,sizeof b,"%d",a->count);tag=b;}}
        else{const library_track *t=library_song(lib_song_at(i));if(t){label=t->title;rowpath=t->path;
            if(lv==LV_SEARCH){snprintf(fallback,sizeof fallback,"%s · %s%s%s",t->title,t->artist,
                with_art?"":" · ",with_art?"":t->album);label=fallback;}
            if(t->duration_ms){fmt_time(b,sizeof b,t->duration_ms);tag=b;}}}
        int is_playing=playing&&rowpath&&!strcmp(rowpath,playing);
        int fav=col_find(&favorites,rowpath)>=0;
        char fitted[400];font_fit(LM->lib,label,right-135-(fav?18:0),fitted,sizeof fitted);
        if(fav)draw_heart(right-84,y+LIB_ROW_H/2,accent());
        if (is_playing) draw_play_marker(20, y + LIB_ROW_H / 2,
                                         audio_state() != A_PLAYING, accent());
        font_draw_mid(bb,LM->lib,38,y+LIB_ROW_H/2,fitted,
                      is_playing?accent():i==lv_sel?C_TEXT:C_MUTED);
        if(tag[0])font_draw_mid(bb,LM->aux,right-16-font_width(LM->aux,tag),y+LIB_ROW_H/2,tag,C_DIM);
    }
    if(!n&&lv!=LV_HOME){
        const char *s=lv==LV_SEARCH?"No matches. Press A to search again.":lv==LV_FAVORITES?"No favorites. Hold Y on a song to add it.":lv==LV_SAVED?"Empty playlist. Hold X on a song to add it.":library_is_scanning()?"Scanning music files…":"No music indexed. Press B, then X to rescan.";
        font_draw_mid(bb,f_mid,22,180,s,C_DIM);
    }
    if(with_art){
        int idx=-1;
        if(active_collection()){idx=-1;}
        else if(lv==LV_TRACKS||lv==LV_SEARCH)idx=lib_song_at(lv_sel);
        else {const library_album *a=library_get_album(lib_album_at(lv_sel));if(a)idx=a->first;}
        const library_track *track=library_song(idx);
        const col_track *ct=collection_track(lv_sel);
        m2d_surf *art=track?album_art_for_track(track->path):ct&&!radio_is_url(ct->path)?album_art_for_track(ct->path):NULL;
        rrect(444,92,170,170,9,C_PANEL);
        if(art)m2d_blit(art,0,0,art->w,art->h,bb,450,98,158,158,0xffffffffu,M2D_REPLACE,0);
        else {font_draw_mid(bb,f_big,510,170,"♪",C_DIM);}
        if(track){char t[200],b[200];
            font_fit(f_row,track->album,167,t,sizeof t);font_draw_mid(bb,f_row,446,280,t,C_TEXT);
            font_fit(f_small,track->artist,167,b,sizeof b);font_draw_mid(bb,f_small,446,303,b,C_DIM);
        }
    }
    draw_library_playback_strip();
}

// ---------------------------------------------------------------- visualizer screen
static double vis_info_until;

static void draw_vis(void)
{
    vis_render(S.vis_preset, &vd, vis_surf, sk->vis);
    m2d_blit(vis_surf, 0, 0, VIS_W, VIS_H, bb, 0, 0, SCREEN_W, SCREEN_H, 0xffffffffu, M2D_REPLACE, 0);
    if (vis_info_until > now_t) {
        float a = (float)(vis_info_until - now_t);
        if (a > 1) a = 1;
        uint32_t al = (uint32_t)(a * 255);
        pl_item it;
        char line[260] = "", sub[120];
        if (current >= 0 && pl_get(current, &it)) font_fit(f_mid, it.name, 520, line, sizeof line);
        char t[16], l[16];
        fmt_time(t, sizeof t, audio_pos_ms());
        fmt_time(l, sizeof l, audio_len_ms());
        snprintf(sub, sizeof sub, "%s / %s   ·   %s", t, l, vis_preset_name(S.vis_preset));
        int w = font_width(f_mid, line);
        int sw = font_width(f_small, sub);
        if (sw > w) w = sw;
        rrect(16, 14, w + 32, 56, 8, (uint32_t)(170 * a) << 24 | 0x0a0c10);
        font_draw_mid(bb, f_mid, 32, 32, line, (al << 24) | 0xf0f2f6);
        font_draw_mid(bb, f_small, 32, 54, sub, (al << 24) | 0xc9ced8);
        hint h[] = { { "←/→", "Effect" }, { "A", "Pause" }, { "L/R", "Skip" }, { "B", "Back" } };
        hint_bar(h, 4);
    }
}

// ---------------------------------------------------------------- input
static void seek_by(int delta_ms)
{
    if (current < 0 || audio_state() == A_STOPPED) return;
    int len = audio_len_ms();
    int p = audio_pos_ms() + delta_ms;
    if (p < 0) p = 0;
    if (len > 0 && p > len - 500) p = len - 500;
    audio_seek(p);
    char a[16], b[16], m[64];
    fmt_time(a, sizeof a, p);
    fmt_time(b, sizeof b, len);
    snprintf(m, sizeof m, "SEEK TO: %s/%s (%d%%)", a, b, len > 0 ? p * 100 / len : 0);
    show_msg(m);
}

static double held_since;

static void on_key(int k, int repeat)
{
    /* MENU is handled on release in the event loop. Its key-down is reserved
     * for system chords (such as screenshots), never an immediate exit. */
    if (k == K_MENU) return;
    // keys that work everywhere except text-like lists
    if(screen==SCR_COLLECTION_MENU||screen==SCR_COLLECTION_NAME||screen==SCR_COLLECTION_PICK||screen==SCR_COLLECTION_DELETE||screen==SCR_QUEUE_CLEAR){collection_key(k,repeat);return;}
    if (screen == SCR_GM_CONFIRM) {
        if (repeat) return;
        if (k == K_A) {
            /* Native RedOnion mode: consent only, never call gm_install(). */
            if (!gm_installed() && gm_set_consent()) {
                S.game_music = 1;
                gm_orphaned = 0;
                settings_save();
                show_toast("%s", "Music in games enabled (launch hook required)");
            } else {
                show_toast("%s", "Enable failed: repair legacy wrapper or SD card");
            }
            screen = SCR_OPTIONS;
        } else if (k == K_B || k == K_MENU || k == K_SELECT)
            screen = SCR_OPTIONS;
        return;
    }
    if (screen == SCR_LIBRARY_MENU) {
        if(k==K_UP)library_menu_sel=(library_menu_sel+2)%3;
        else if(k==K_DOWN)library_menu_sel=(library_menu_sel+1)%3;
        else if(k==K_B){screen=SCR_LIBRARY;}
        else if(k==K_A){
            screen=SCR_LIBRARY;
            if(library_menu_sel==0)start_cover_fetch(1);
            else if(library_menu_sel==1&&library_start_scan())show_toast("%s","Refreshing library");
            else if(library_menu_sel==2&&cover_running)covers_cancel();
        }
        return;
    }
    if (screen == SCR_DISPLAY) {
        if(k==K_UP)display_sel=(display_sel+D_N-1)%D_N;
        else if(k==K_DOWN)display_sel=(display_sel+1)%D_N;
        else if(k==K_LEFT||k==K_RIGHT||k==K_A)display_change(k==K_LEFT?-1:1,k==K_A);
        else if(k==K_B)screen=SCR_OPTIONS;
        return;
    }
    if (screen == SCR_OPTIONS) {
        if (k == K_UP) opt_sel = (opt_sel + OPT_N - 1) % OPT_N;
        else if (k == K_DOWN) opt_sel = (opt_sel + 1) % OPT_N;
        else if (k == K_LEFT) option_change(-1, 0);
        else if (k == K_RIGHT) option_change(1, 0);
        else if (k == K_A && !repeat) option_change(1, 1);
        else if ((k == K_B || k == K_SELECT || k == K_MENU) && !repeat) screen = options_return_screen;
        return;
    }
    if (screen == SCR_PRESETS) {
        if (k == K_UP) preset_sel = (preset_sel + NPRESETS - 1) % NPRESETS;
        else if (k == K_DOWN) preset_sel = (preset_sel + 1) % NPRESETS;
        else if (k == K_A && !repeat) { eq_apply_preset(preset_sel); }
        else if (k == K_B || k == K_X || k == K_MENU) screen = SCR_EQ;
        return;
    }
    if (screen == SCR_SKINS) {
        if (k == K_UP && nskins) { skin_sel = (skin_sel + nskins - 1) % nskins; preview_due = now_t + 0.18; }
        else if (k == K_DOWN && nskins) { skin_sel = (skin_sel + 1) % nskins; preview_due = now_t + 0.18; }
        else if (k == K_L2 && nskins) { skin_sel = skin_sel - ROWS < 0 ? 0 : skin_sel - ROWS; preview_due = now_t + 0.18; }
        else if (k == K_R2 && nskins) { skin_sel = skin_sel + ROWS >= nskins ? nskins - 1 : skin_sel + ROWS; preview_due = now_t + 0.18; }
        else if (k == K_A && !repeat) { preview_due = 0; skins_update(); close_skins(1); show_toast("Skin: %s", sk->name); }
        else if ((k == K_B || k == K_MENU) && !repeat) close_skins(0);
        return;
    }
    if (screen == SCR_LIBRARY_VIEWS) {
        if (k == K_UP || k == K_LEFT)
            library_views_sel = (library_views_sel + LIBCAT_COUNT - 1) % LIBCAT_COUNT;
        else if (k == K_DOWN || k == K_RIGHT)
            library_views_sel = (library_views_sel + 1) % LIBCAT_COUNT;
        else if (!repeat && k == K_A) choose_library_category(library_views_sel);
        else if (!repeat && k == K_B) screen = library_views_return;
        return;
    }
    if (screen == SCR_LIBRARY) {
        int n=lib_nrows();
        if(k==K_UP&&n)lv_sel=(lv_sel+n-1)%n;
        else if(k==K_DOWN&&n)lv_sel=(lv_sel+1)%n;
        else if(k==K_L2){lv_sel-=LIB_ROWS;if(lv_sel<0)lv_sel=0;}
        else if(k==K_R2){lv_sel+=LIB_ROWS;if(lv_sel>=n)lv_sel=n?n-1:0;}
        else if(k==K_LEFT||k==K_RIGHT){
            if(!repeat)held_since=now_t;
            int step=now_t-held_since>2.0?30000:now_t-held_since>0.8?10000:5000;
            seek_by(k==K_LEFT?-step:step);
        }
        else if(repeat)return;
        else if(k==K_A||k==K_X||k==K_Y)library_action(k);
        else if(k==K_B){library_back();}
        else if(k==K_SELECT){screen=panel;}
        else if(k==K_START)toggle_pause();
        else if(k==K_L)skip(-1);
        else if(k==K_R)skip(1);
        return;
    }
    if (screen == SCR_BROWSER) {
        if (k == K_UP && nents) b_sel = (b_sel + nents - 1) % nents;
        else if (k == K_DOWN && nents) b_sel = (b_sel + 1) % nents;
        else if (k == K_L2) b_sel = b_sel - B_ROWS < 0 ? 0 : b_sel - B_ROWS;
        else if (k == K_R2) b_sel = nents ? (b_sel + B_ROWS >= nents ? nents - 1 : b_sel + B_ROWS) : 0;
        else if (k == K_LEFT || k == K_RIGHT) {
            if(!repeat)held_since=now_t;
            int step=now_t-held_since>2.0?30000:now_t-held_since>0.8?10000:5000;
            seek_by(k==K_LEFT?-step:step);
        }
        else if (repeat) return;
        else if (k == K_A) browser_activate(0);
        else if (k == K_X) browser_activate(1);
        else if (k == K_Y) browser_activate(2);
        else if (k == K_B) {browser_up();}
        else if (k == K_START) toggle_pause();
        else if (k == K_SELECT) {screen = panel;}
        else if (k == K_L) skip(-1);
        else if (k == K_R) skip(1);
        return;
    }
    if (screen == SCR_VIS) {
        if (repeat) return;
        if (k == K_LEFT) S.vis_preset = (S.vis_preset + VIS_PRESETS - 1) % VIS_PRESETS;
        else if (k == K_RIGHT) S.vis_preset = (S.vis_preset + 1) % VIS_PRESETS;
        else if (k == K_A || k == K_START) toggle_pause();
        else if (k == K_L) skip(-1);
        else if (k == K_R) skip(1);
        else if (k == K_B || k == K_X || k == K_MENU) { screen = panel; return; }
        vis_info_until = now_t + 3;
        return;
    }
    // player screens (playlist / EQ panel)
    if (k == K_L && !repeat) { press_flash(PB_PREV); skip(-1); return; }
    if (k == K_R && !repeat) { press_flash(PB_NEXT); skip(1); return; }
    if (k == K_START && !repeat) { int st = audio_state(); press_flash(st == A_PLAYING ? PB_PAUSE : PB_PLAY); toggle_pause(); return; }
    if (k == K_SELECT && !repeat) { open_library(); return; }
    /* X short/long handling lives in the event loop. */
    if (screen == SCR_PLAYLIST) {
        int n = pl_count();
        if (k == K_UP && n) sel = (sel + n - 1) % n;
        else if (k == K_DOWN && n) sel = (sel + 1) % n;
        else if (k == K_L2) { sel -= playlist_rows(); if (sel < 0) sel = 0; }
        else if (k == K_R2) { sel += playlist_rows(); if (sel >= n) sel = n - 1; }
        else if (k == K_LEFT || k == K_RIGHT) {
            if (!repeat) held_since = now_t;
            int step = now_t - held_since > 2.0 ? 30000 : now_t - held_since > 0.8 ? 10000 : 5000;
            seek_by(k == K_LEFT ? -step : step);
        }
        else if (repeat) return;
        else if (k == K_A && n) {
            if (sel == current && audio_state() != A_STOPPED) { press_flash(audio_state() == A_PLAYING ? PB_PAUSE : PB_PLAY); toggle_pause(); }
            else { press_flash(PB_PLAY); play_index(sel, 0, 0); }
        }
        /* B is not a view switch: SELECT alone toggles Library/Player. */
        else if (k == K_Y) { press_flash(PB_EQ); screen = panel = SCR_EQ; }
        return;
    }
    if (screen == SCR_EQ) {
        if (k == K_LEFT) eq_sel = (eq_sel + 10) % 11;
        else if (k == K_RIGHT) eq_sel = (eq_sel + 1) % 11;
        else if (k == K_UP || k == K_DOWN) {
            float *v = eq_sel == 0 ? &S.eq_pre : &S.eq[eq_sel - 1];
            *v += k == K_UP ? 1 : -1;
            if (*v > 12) *v = 12;
            if (*v < -12) *v = -12;
            S.eq_preset[0] = 0;
            S.eq_on = 1;
            apply_audio_settings();
        }
        else if (repeat) return;
        else if (k == K_A) { S.eq_on = !S.eq_on; apply_audio_settings(); press_flash(PB_EQ); }
        else if (k == K_X) {
            preset_sel = 0;
            for (int i = 0; i < NPRESETS; i++) if (!strcmp(PRESETS[i].name, S.eq_preset)) preset_sel = i;
            screen = SCR_PRESETS;
        }
        else if (k == K_B || k == K_Y) { press_flash(PB_PL); screen = panel = SCR_PLAYLIST; }
    }
}

// ---------------------------------------------------------------- frame
static void draw_frame(void)
{
    bb = m2d_backbuffer();
    if(screen==SCR_COLLECTION_MENU||screen==SCR_COLLECTION_NAME||screen==SCR_COLLECTION_PICK||screen==SCR_COLLECTION_DELETE||screen==SCR_QUEUE_CLEAR){draw_collection_dialog();}
    else if (screen == SCR_LIBRARY ||
        ((screen == SCR_OPTIONS || screen == SCR_DISPLAY || screen == SCR_GM_CONFIRM) &&
         (options_return_screen == SCR_LIBRARY || options_return_screen == SCR_BROWSER))) {
        if (options_return_screen == SCR_BROWSER && screen != SCR_LIBRARY) draw_browser();
        else draw_library();
        /* The same SELECT tap/hold actions work from both views. */
        if (screen == SCR_OPTIONS || screen == SCR_DISPLAY || screen == SCR_GM_CONFIRM) {
            if (screen == SCR_OPTIONS) draw_options();
            else if (screen == SCR_DISPLAY) draw_display();
            else draw_gm_confirm();
            hint h[]={{"B","Back"},{"←/→","Change"},{"A","Select"}};
            hint_bar(h,3);
        } else {
        if(cover_status_visible_at>0 && now_t>=cover_status_visible_at){
            hint h[]={{"B","Back"},{"SELECT","Player"},{"Hold SELECT","Options"}};
            hint_bar(h,3);
            char stat[64];snprintf(stat,sizeof stat,"Covers %d/%d (%d saved)",cover_done,cover_total,cover_saved);
            font_draw_mid(bb,f_hint,12,464,stat,C_DIM);
        } else {
            if(lv==LV_HOME){hint h[]={{"A","Open"},{"X","Menu"},{"Hold B","Views"},{"SELECT","Player"},{"Hold SELECT","Options"}};hint_bar(h,5);}
            else if(lv==LV_SONGS||lv==LV_TRACKS||lv==LV_SEARCH||active_collection()){hint h[]={{"A","Play"},{"X","Add"},{"Y","All"},{"Hold X","Menu"},{"Hold Y","Fav"},{"SELECT","Player"}};hint_bar(h,6);}
            else {hint h[]={{"A","Open"},{"X","Add"},{"Hold X","Menu"},{"B","Back"},{"SELECT","Player"}};hint_bar(h,5);}
        }
        }
    } else if (screen == SCR_LIBRARY_MENU) {
        draw_library();draw_library_menu();
        hint h[]={{"A","Select"},{"B","Back"}};hint_bar(h,2);
    } else if (screen == SCR_LIBRARY_VIEWS) {
        if (library_views_return == SCR_BROWSER) draw_browser();
        else draw_library();
        draw_library_views();
        hint h[] = { { "B", "Cancel" }, { "SELECT", "Player" } };
        hint_bar(h, 2);
    } else if (screen == SCR_BROWSER) {
        draw_browser();
        hint h[] = { { "A", nents&&!(ents[b_sel].dir||ents[b_sel].m3u)?"Play":"Open" },
                     { "Y", "Folder" }, { "X", "Add" }, { "Hold X", "Menu" },
                     { "Hold Y", "Fav" }, { "SELECT", "Player" } };
        hint_bar(h, 6);
    } else if (screen == SCR_VIS) {
        draw_vis();
    } else {
        m2d_clear(bb, C_BG, 0);
        skin *real = sk;
        if (screen == SCR_SKINS && preview) sk = preview;
        if (!(screen == SCR_PLAYLIST && panel == SCR_PLAYLIST && S.playlist_expanded))
            draw_main_window();
        sk = real;
        if (screen == SCR_SKINS) {
            skin *keep = sk;
            if (preview) sk = preview;
            draw_skins_panel();
            sk = keep;
            hint h[] = { { "A", "Use skin" }, { "B", "Cancel" } };
            hint_bar(h, 2);
        } else if (panel == SCR_EQ) {
            draw_eq_panel();
            hint h[] = { { "↑/↓", "Level" }, { "←/→", "Band" }, { "A", "On/Off" }, { "X", "Presets" }, { "B", "Back" } };
            hint_bar(h, 5);
        } else {
            draw_playlist_panel();
            hint h[] = { { "Y", "EQ" }, { "Hold Y", "Queue" }, { "X", S.playlist_expanded ? "Normal" : "List" },
                         { "Hold X", "Visualizer" }, { "SELECT", "Library" } };
            hint_bar(h, 5);
        }
        if (screen == SCR_GM_CONFIRM) {
            draw_gm_confirm();
        } else if (screen == SCR_DISPLAY) {
            draw_display();
            hint h[]={{"←/→","Change"},{"A","Select"},{"B","Back"}};
            hint_bar(h,3);
        } else if (screen == SCR_OPTIONS) {
            draw_options();
            hint h[] = { { "←/→", "Change" }, { "A", "Select" }, { "B", "Close" } };
            hint_bar(h, 3);
        } else if (screen == SCR_PRESETS) {
            draw_presets();
            hint h[] = { { "A", "Apply" }, { "B", "Back" } };
            hint_bar(h, 2);
        }
    }
    if (toast_until > now_t) {
        int w = font_width(f_mid, toast) + 36;
        int x = (SCREEN_W - w) / 2, y = screen == SCR_BROWSER ? 370 : screen == SCR_OPTIONS ? 443 : 396;
        rrect(x, y, w, 34, 17, 0xf0262b36u);
        font_draw_mid(bb, f_mid, x + 18, y + 17, toast, C_TEXT);
    }
}

/* UI clock / render scheduling is independent of the audio thread. */
static int main_visible(void)
{
    if(screen==SCR_VIS||screen==SCR_LIBRARY||screen==SCR_BROWSER||
       screen==SCR_LIBRARY_MENU||screen==SCR_LIBRARY_VIEWS||screen==SCR_COLLECTION_MENU||screen==SCR_COLLECTION_NAME||screen==SCR_COLLECTION_PICK||screen==SCR_COLLECTION_DELETE||screen==SCR_QUEUE_CLEAR)return 0;
    if((screen==SCR_OPTIONS||screen==SCR_DISPLAY||screen==SCR_GM_CONFIRM)&&
       (options_return_screen==SCR_LIBRARY||options_return_screen==SCR_BROWSER))return 0;
    return !(screen==SCR_PLAYLIST&&panel==SCR_PLAYLIST&&S.playlist_expanded);
}
static int mini_visible(void) { return main_visible()&&S.vis_mode!=0; }
/* Only values that affect visible pixels, plus revisions for worker updates. */
typedef struct {
    int screen, track, state, second, length, progress, scanning;
    int cover_visible, cover_done, cover_total, cover_saved, toast_active, sleep_left, preview;
    unsigned revision;
    mainwin_state mw;
    char text[400], toast_text[160];
} ui_snapshot;
static ui_snapshot snapshot(int *scrolling, double *clock_wait)
{
    ui_snapshot v;memset(&v,0,sizeof v);
    v.screen=screen;v.track=current;v.state=audio_state();
    int pos=audio_pos_ms(),len=audio_len_ms();
    if(len<=0&&current>=0){pl_item it;if(pl_get(current,&it))len=it.dur_ms;}
    int strip=(!main_visible()&&screen!=SCR_VIS)||
              (main_visible()&&panel==SCR_PLAYLIST&&S.playlist_expanded);
    int width=(screen==SCR_PLAYLIST||main_visible())?PW-4:SCREEN_W-8;
    if(strip&&current>=0){
        v.second=pos/1000;v.length=len>0?len/1000:0;
        v.progress=len>0?(int)((int64_t)(pos<len?pos:len)*width/len):0;
    }
    v.revision=pl_revision();v.scanning=library_is_scanning();
    v.cover_visible=screen==SCR_LIBRARY&&cover_status_visible_at>0&&now_t>=cover_status_visible_at;
    if(v.cover_visible){v.cover_done=cover_done;v.cover_total=cover_total;v.cover_saved=cover_saved;}
    v.toast_active=toast_until>now_t;
    if(v.toast_active)snprintf(v.toast_text,sizeof v.toast_text,"%s",toast);
    if(screen==SCR_OPTIONS&&sleep_at>0)v.sleep_left=(int)((sleep_at-now_t)/60+0.99);
    if(screen==SCR_SKINS)v.preview=preview_idx;
    if(main_visible()){
        v.mw=main_window_state();
        snprintf(v.text,sizeof v.text,"%s",v.mw.marquee);
        v.mw.marquee=NULL;
        *scrolling=msg_until<=now_t&&skin_text_width(v.text)>154;
    }
    if(v.state==A_PLAYING&&current>=0){
        /* Wake at the next visible clock or progress boundary, but never
         * chase sub-frame updates or assume the decoder clock is continuous. */
        double ms=1000-pos%1000;
        if(main_visible()&&S.remaining&&len>pos)ms=1+(len-pos-1)%1000;
        int pixels=strip?width:219;
        if(len>0&&pos<len){
            int64_t pixel=(int64_t)pos*pixels/len;
            double next=ceil((double)(pixel+1)*len/pixels)-pos;
            if(next<ms)ms=next;
        }
        *clock_wait=fmax(1.0/30.0,ms/1000.0);
    }
    return v;
}
static void sooner(double *due,double t)
{
    if(t>now_t&&t<*due)*due=t;
}

// ---------------------------------------------------------------- main
static void on_signal(int s) { (void)s; quit = 1; }
static void on_continue(int s) { (void)s; resumed = 1; }
/* SELECT has one meaning everywhere: tap to switch Player/Library,
 * hold to open Options from the screen currently being browsed. */
static void select_switch_view(void)
{
    if (screen == SCR_LIBRARY || screen == SCR_BROWSER || screen == SCR_LIBRARY_MENU ||
        screen == SCR_LIBRARY_VIEWS) {
        screen = panel;
        S.last_view = 0;
    } else if (screen == SCR_PLAYLIST || screen == SCR_EQ) {
        open_library();
    } else if (screen == SCR_OPTIONS) {
        screen = options_return_screen;
    } else if (screen == SCR_DISPLAY) {
        screen = SCR_OPTIONS;
    }
}
static void select_open_options(void)
{
    if (screen == SCR_LIBRARY || screen == SCR_BROWSER || screen == SCR_LIBRARY_MENU ||
        screen == SCR_LIBRARY_VIEWS || screen == SCR_PLAYLIST || screen == SCR_EQ) {
        options_return_screen = screen == SCR_LIBRARY_MENU || screen == SCR_LIBRARY_VIEWS ?
                                (screen == SCR_LIBRARY_VIEWS ? library_views_return : SCR_LIBRARY) : screen;
        opt_sel = 0;
        screen = SCR_OPTIONS;
    }
}


static void save_all(void)
{
    S.cur_index = current;
    S.cur_pos_ms = current >= 0 ? audio_pos_ms() : 0;
    settings_save();
    if (playlist_dirty) {
        char p[1024];
        path_join(p, sizeof p, data_dir, "playlist.m3u");
        if(pl_save_m3u(p))playlist_dirty=0;
        else fprintf(stderr,"[queue] failed to save %s\n",p);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    const char *home = getenv("MA_HOME");
    if (home) snprintf(app_dir, sizeof app_dir, "%s", home);
    else if (!getcwd(app_dir, sizeof app_dir)) strcpy(app_dir, ".");
    const char *dd = getenv("MA_DATA");
    if (dd) snprintf(data_dir, sizeof data_dir, "%s", dd);
    else path_join(data_dir, sizeof data_dir, app_dir, "data");
    mkdir(data_dir, 0755);
    const char *rd = getenv("MA_ROOT");
    snprintf(root_dir, sizeof root_dir, "%s", rd ? rd : dir_exists("/mnt/SDCARD") ? "/mnt/SDCARD" : (getenv("HOME") ? getenv("HOME") : "/"));
    if (strlen(root_dir) > 1 && root_dir[strlen(root_dir) - 1] == '/') root_dir[strlen(root_dir) - 1] = 0;
    if (argc > 1 && !strcmp(argv[1], "--cleanup-stay-awake")) {
        stayawake_cleanup_stale(); return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--service")) return service_main();
    if (argc > 1 && !strcmp(argv[1], "--uninstall")) {
        // undo everything outside the app folder (run by Mixtape before it erases RedAmp)
        if (!service_stop()) {
            fprintf(stderr, "uninstall: background service is still alive; refusing unsafe restore\n");
            return 1;
        }
        int ok = gm_uninstall();
        if (ok) {
            settings_load();
            S.game_music = 0;
            S.resume_playing = 0;
            settings_save();
            unlink(BGM_SHM_PATH);
        }
        fprintf(stderr, "uninstall: Onion's sound library %s\n", ok ? "is the original" : "could NOT be restored");
        return ok ? 0 : 1;
    }
    srand((unsigned)time(NULL));
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCONT, on_continue);

    stayawake_cleanup_stale();
    if (!service_stop()) {
        fprintf(stderr, "[gm] background service did not stop; cannot start player safely\n");
        return 1;
    }
    set_battery_marker();
    settings_load();
    gm_orphaned = !S.game_music && gm_installed();
    if (gm_orphaned)
        fprintf(stderr, "[gm] wrapper present while game music is Off; Options > Music in games can attempt repair\n");
    if (m2d_init(SCREEN_W, SCREEN_H) != 0) { fprintf(stderr, "display init failed\n"); return 1; }

    char p[1024], fb[1024];
    path_join(fb, sizeof fb, app_dir, "fonts/DejaVuSans.ttf");
    snprintf(fallback_font,sizeof fallback_font,"%s",fb);
    path_join(p, sizeof p, app_dir, "fonts/Rubik-400.ttf");
    snprintf(regular_font,sizeof regular_font,"%s",p);
    f_row = font_open(p, fb, 15);
    f_small = font_open(p, fb, 13);
    f_mid = font_open(p, fb, 16);
    f_hint = font_open(p, fb, 18);
    select_list_size();
    path_join(p, sizeof p, app_dir, "fonts/Rubik-700.ttf");
    f_key = font_open(p, fb, 13);
    path_join(p, sizeof p, app_dir, "fonts/ChakraPetch-Bold.ttf");
    f_head = font_open(p, fb, 14);
    f_title = font_open(p, fb, 20);
    f_big = font_open(p, fb, 28);
    if (!f_row || !f_key || !f_head || !f_hint || !LM->pl || !LM->lib) { fprintf(stderr, "fonts missing\n"); return 1; }

    path_join(p, sizeof p, app_dir, "skins/RedOnion.wsz");
    sk_default = skin_load(p, NULL);
    if (!sk_default) { fprintf(stderr, "default skin missing: %s\n", p); return 1; }
    sk = sk_default;
    /* New installs prefer the authentic CC0 Ember skin if present. Keep the
     * verified RedOnion graphics as a safe fallback when it is not bundled. */
    if (!S.skin[0]) {
        char ember[1024];
        path_join(ember, sizeof ember, app_dir, "skins/Ember.wsz");
        if (access(ember, R_OK) == 0) {
            skin *first = skin_load(ember, sk_default);
            if (first) {
                sk = first;
                snprintf(S.skin, sizeof S.skin, "%s", ember);
            }
        }
    } else if (strcmp(S.skin, p)) {
        skin *selected = skin_load(S.skin, sk_default);
        if (selected) sk = selected;
    }
    if (!S.skin[0]) snprintf(S.skin, sizeof S.skin, "%s", p);
    mw = m2d_surface_new(MW_W, MW_H);
    mw_scaled = m2d_surface_new(MW_W*2, MW_H*2);
    vis_surf = m2d_surface_new(VIS_W, VIS_H);

    album_art_set_cache_dir(data_dir);
    album_art_set_resolver(resolve_cover_album);
    netclock_set_insecure(S.insecure_https);
    netclock_start(); /* silent, asynchronous; survives station changes */
    has_dsp = audio_init();
    apply_audio_settings();
    pl_init();
    path_join(collection_dir,sizeof collection_dir,data_dir,"playlists");mkdir(collection_dir,0755);
    path_join(favorite_file,sizeof favorite_file,data_dir,"favorites.m3u");
    if(!col_read(&favorites,favorite_file)&&access(favorite_file,F_OK)==0)favorites_readable=0;
    refresh_collections();
    { char mus[1024],cache[1024];
      path_join(mus,sizeof mus,root_dir,"Media/Music");
      if(!dir_exists(mus)) path_join(mus,sizeof mus,root_dir,"Music");
      path_join(cache,sizeof cache,data_dir,"library-cache-v5.bin");
      library_init(mus,cache);
    }
    path_join(p, sizeof p, data_dir, "playlist.m3u");
    pl_load_m3u(p);
    if (S.cur_index >= 0 && S.cur_index < pl_count()) {
        sel = S.cur_index;
        // resume where we left off: playing if the background service was, else paused
        /* Restoring a paused radio station must not contact the network at
         * launch. START/A can open the selected station later. */
        const char *saved=pl_path(S.cur_index);
        if(saved&&radio_is_url(saved)&&(!S.resume_playing||!netclock_network_available()))
            current=S.cur_index;
        else play_index(S.cur_index, S.cur_pos_ms, !S.resume_playing);
        S.resume_playing = 0;
    }
    if (S.shuffle) build_shuffle();
    if (S.last_view) open_library();

    double last = now(), started_at=last;
    now_t = last;
    last_save = last;
    int held_key = -1;
    int select_held = 0, start_held = 0, combo_consumed = 0;
    int select_hold_fired = 0;
    double select_since = 0;
    /* Track other pressed buttons so MENU+button never exits on MENU release,
     * regardless of which button was pressed first. Keycodes are evdev codes. */
    unsigned char key_down[256] = {0};
    unsigned char menu_consumed_key[256] = {0};
    int menu_held = 0, menu_chord = 0;
    int playlist_x_held = 0, playlist_x_fired = 0;
    double playlist_x_since = 0;
    int action_held=0,action_fired=0,action_origin=0;double action_since=0;
    int back_held = 0, back_hold_fired = 0;
    double back_since = 0;
    double next_repeat = 0;
    /* On Miyoo, pump input frequently but only repaint when the UI changes
     * or an animation/progress update is due. Display memory traffic is much
     * costlier than the input poll. Host screenshot tests keep frame cadence. */
    double last_present = 0, last_input_at = 0;
    double housekeeping_due=0, gm_check_due=0;
    int gm_active=0;
    ui_snapshot presented={0};
    int have_presented=0, pending_redraw=0;
    int event_driven=1;
#ifdef M2D_HOST
    event_driven=getenv("MA_EVENT_TEST")!=NULL;
#endif
    int errors = 0;
    const char *mf = getenv("MA_MAX_FRAMES");
    int max_frames = mf ? atoi(mf) : 0;
    int profile = getenv("MA_PROFILE") != NULL;
    unsigned long loop_count=0;
    while (!quit) {
        loop_count++;
        if (max_frames && m2d_frame_count() >= max_frames) break;
        now_t = now();
        float dt = (float)(now_t - last);
        last = now_t;
        int code, val;
        int ui_wake = 0;
        int frame_screen = screen, was_off=screen_off;
        if (resumed) {
            resumed=0;
            fprintf(stderr,"[standby] resumed\n");
            held_key=-1;select_held=start_held=combo_consumed=select_hold_fired=0;
            menu_held=menu_chord=playlist_x_held=playlist_x_fired=0;
            back_held=back_hold_fired=0;action_held=action_fired=0;
            memset(key_down,0,sizeof key_down);
            memset(menu_consumed_key,0,sizeof menu_consumed_key);
            pressed=PB_NONE;pressed_until=msg_until=toast_until=vis_info_until=0;
            memset(&vd,0,sizeof vd);marquee_t0=now_t;
            dt=0;last_present=0;mw_cache_valid=0;have_presented=0;
            pending_redraw=ui_wake=1;last_input_at=now_t;housekeeping_due=0;
            if(screen_off)set_screen_off(0);
            /* Live connections cannot be assumed valid after keymon resumes us.
             * Do not start a manually paused station or disturb local decoding. */
            if((sleep_at<=0||now_t<sleep_at)&&current>=0&&radio_is_url(pl_path(current))&&
               (audio_state()==A_PLAYING || (audio_state()==A_STOPPED &&
                 (audio_take_error() || audio_take_finished()))))
                play_index(current,0,0);
        }
        while (m2d_poll_key(&code, &val)) {
            ui_wake = 1;
            last_input_at = now_t;
            if (code >= 0 && code < (int)sizeof key_down && (val == 0 || val == 1)) {
                if (val==0 && !key_down[code]) continue;
                key_down[code] = (unsigned char)val;
            }
            /* MENU exits on key-up only. Any button held with MENU consumes
             * the chord, including POWER, which keymon can use for screenshots.
             * Suppress chord button releases so they cannot trigger playback or
             * other single-key actions after MENU is released. */
            if (code == K_MENU) {
                if (val == 1 && !menu_held) {
                    menu_held = 1;
                    menu_chord = 0;
                    for (int i = 0; i < (int)sizeof key_down; i++) {
                        if (i != K_MENU && key_down[i]) {
                            menu_chord = 1;
                            menu_consumed_key[i] = 1;
                        }
                    }
                    /* Any pending single-key holds must not act during a
                     * screenshot chord, or keep repeating after it. */
                    held_key = -1;
                    playlist_x_held = 0;
                    playlist_x_fired = 0;
                    action_held=action_fired=0;
                    if (menu_chord) {
                        back_held = 0;
                        back_hold_fired = 0;
                        select_held = 0;
                        start_held = 0;
                        combo_consumed = 0;
                    }
                } else if (val == 0 && menu_held) {
                    menu_held = 0;
                    if (!menu_chord) { quit = 1; break; }
                    menu_chord = 0;
                }
                continue;
            }
            if (menu_held) {
                if (val == 1) {
                    menu_chord = 1;
                    if (code >= 0 && code < (int)sizeof menu_consumed_key)
                        menu_consumed_key[code] = 1;
                }
                if (val == 0 && code >= 0 && code < (int)sizeof menu_consumed_key)
                    menu_consumed_key[code] = 0;
                if (code == K_SELECT && val == 0) select_held = 0;
                if (code == K_B && val == 0) back_held = 0;
                if (code == K_START && val == 0) start_held = 0;
                continue;
            }
            if (code >= 0 && code < (int)sizeof menu_consumed_key &&
                menu_consumed_key[code]) {
                if (val == 0) menu_consumed_key[code] = 0;
                continue;
            }
            if (code == K_POWER) continue;
            /* B is Back on a short tap, Views on a 0.65s hold. Dispatch Back
             * only on release so folder navigation does not happen first. */
            if (code == K_B &&
                (screen == SCR_LIBRARY || screen == SCR_BROWSER || back_held)) {
                if (val == 1 && !back_held &&
                    (screen == SCR_LIBRARY || screen == SCR_BROWSER)) {
                    back_held = 1;
                    back_hold_fired = 0;
                    back_since = now_t;
                } else if (val == 0 && back_held) {
                    back_held = 0;
                    if (!back_hold_fired && !screen_off) on_key(K_B, 0);
                    back_hold_fired = 0;
                }
                continue;
            }
            /* Library X/Y and Player Y preserve their taps; holds open actions or favorite. */
            if ((code==action_held)||(val==1&&!action_held&&!screen_off&&
                ((screen==SCR_PLAYLIST&&code==K_Y)||((screen==SCR_LIBRARY||screen==SCR_BROWSER)&&(code==K_X||code==K_Y))))) {
                if(val==1&&!action_held){action_held=code;action_fired=0;action_since=now_t;action_origin=screen;}
                else if(val==0&&code==action_held){action_held=0;if(!action_fired&&screen==action_origin&&!screen_off)on_key(code,0);action_fired=0;}
                continue;
            }
            /* X taps switch playlist layout; a 1-second hold opens the
             * visualizer as soon as the timer fires, not on release. */
            if (code == K_X && (screen == SCR_PLAYLIST || playlist_x_held)) {
                if (val == 1 && screen == SCR_PLAYLIST && !playlist_x_held) {
                    playlist_x_held = 1;
                    playlist_x_fired = 0;
                    playlist_x_since = now_t;
                } else if (val == 0 && playlist_x_held) {
                    playlist_x_held = 0;
                    if (!playlist_x_fired && screen == SCR_PLAYLIST && !screen_off) {
                        S.playlist_expanded = !S.playlist_expanded;
                        scroll = 0;
                    }
                    playlist_x_fired = 0;
                }
                continue;
            }
            /* Single SELECT/START actions happen on release, so pressing the
             * other half of the exit chord cannot open Options or pause audio. */
            if (code == K_SELECT || code == K_START) {
                if (val == 1) {
                    if (code == K_SELECT) {
                        select_held = 1;
                        select_since = now_t;
                        select_hold_fired = 0;
                    } else start_held = 1;
                    if (select_held && start_held) {
                        combo_consumed = 1;
                        quit = 1;
                        break;
                    }
                } else if (val == 0) {
                    if (code == K_SELECT) select_held = 0;
                    else start_held = 0;
                    if (!combo_consumed) {
                        if (screen_off) {
                            if (code == K_START) toggle_pause();
                            else set_screen_off(0);
                        } else if (code == K_SELECT) {
                            if (!select_hold_fired) select_switch_view();
                        } else on_key(code, 0);
                    }
                    if (!select_held && !start_held) combo_consumed = 0;
                }
                continue;
            }
            if (screen_off) {
                if (val != 1) continue;
                if (code == K_START) toggle_pause();
                else if (code == K_L) skip(-1);
                else if (code == K_R) skip(1);
                else set_screen_off(0);
                continue;
            }
            if (val == 1) {
                on_key(code, 0);
                int rep = code == K_UP || code == K_DOWN || code == K_LEFT || code == K_RIGHT || code == K_L2 || code == K_R2;
                if (rep) { held_key = code; next_repeat = now_t + 0.32; }
            } else if (val == 0 && code == held_key) held_key = -1;
        }
        if(quit)break;
        /* Open Options without waiting for SELECT release. SELECT+START still
         * exits, and a long-press release cannot switch views afterward. */
        if (select_held && !select_hold_fired && !combo_consumed &&
            !menu_held && !screen_off &&
            now_t - select_since >= SELECT_OPTIONS_HOLD_SECONDS) {
            select_hold_fired = 1;
            ui_wake=1;last_input_at=now_t;
            select_open_options();
        }
        if (back_held && !back_hold_fired && !menu_held && !screen_off &&
            (screen == SCR_LIBRARY || screen == SCR_BROWSER) &&
            now_t - back_since >= LIBRARY_VIEWS_HOLD_SECONDS) {
            back_hold_fired = 1;
            ui_wake=1;last_input_at=now_t;
            open_library_views();
        }
        /* Run the long-press action in the frame loop while X is still down.
         * A later release is consumed and never toggles the layout. */
        if (playlist_x_held && !playlist_x_fired && !menu_held && !screen_off &&
            screen == SCR_PLAYLIST && now_t - playlist_x_since >= X_VIS_HOLD_SECONDS) {
            playlist_x_fired = 1;
            ui_wake=1;last_input_at=now_t;
            screen = SCR_VIS;
            vis_info_until = now_t + 3;
        }
        if(action_held&&!action_fired&&!menu_held&&!screen_off&&screen==action_origin&&now_t-action_since>=0.75){
            action_fired=1;ui_wake=1;last_input_at=now_t;
            if(action_held==K_Y&&screen!=SCR_PLAYLIST&&selected_collection_track())toggle_favorite();
            else open_collection_actions();
        }
        if (held_key >= 0 && now_t >= next_repeat && !screen_off) {
            ui_wake=1;last_input_at=now_t;
            on_key(held_key, 1);
            next_repeat = now_t + ((held_key == K_LEFT || held_key == K_RIGHT) &&
                                     (screen == SCR_PLAYLIST || screen == SCR_LIBRARY) ? 0.16 : 0.07);
        }
        audio_drain_events();
        if (audio_take_finished()) {
            ui_wake=1;
            errors = 0;
            if (S.repeat == 2 && current >= 0) play_index(current, 0, 0);
            else {
                int k = next_index(1);
                if (k >= 0) {
                    int followed = (screen != SCR_PLAYLIST || sel == current);
                    play_index(k, 0, 0);
                    if (followed) sel = k;
                }
                else S.cur_pos_ms = 0;
            }
        }
        if (audio_take_error()) {
            ui_wake=1;
            const char *bad = pl_path(current);
            fprintf(stderr, "[play] could not open %s\n", bad ? bad : "?");
            char audio_err[192]={0};
            audio_last_error(audio_err,sizeof audio_err);
            int net_fail=radio_take_network_error();
            if(audio_err[0])fprintf(stderr,"[play] decoder: %s\n",audio_err);
            show_toast("%s",net_fail==1?"DNS failed / network unavailable":
                       net_fail==2?"Cannot connect to station":
                       net_fail==3?"Cannot reach stream / check system date":
                       net_fail==4?"Station TLS certificate verification failed":
                       net_fail==5?"Station connection timed out":
                       net_fail==6?"Station unavailable / server error":
                       audio_err[0]?audio_err:"Cannot play file or stream");
            /* Never automatically cycle through an entire radio station list
             * after an outage, nor after a broken individual stream. Manual
             * selection and retries remain possible. */
            if(bad&&radio_is_url(bad))errors=0;
            else if (++errors < pl_count()) { int k = next_index(1); if (k >= 0) play_index(k, 0, 0); }
        }
        if (sleep_at > 0 && now_t >= sleep_at) {
            sleep_at = 0; sleep_min = 0;
            audio_set_paused(1);
            stayawake_update(0, S.play_standby);
            save_all();
            show_toast("%s", "Sleep timer: paused");
        }
        stayawake_update(audio_state() == A_PLAYING, S.play_standby);
        if (now_t - last_save > 60) { save_all(); last_save = now_t; }
        if (screen == SCR_SKINS) skins_update();
        int library_changed=0,downloaded=0;
        int housekeeping=now_t>=housekeeping_due||was_off!=screen_off;
        if(housekeeping){
            library_changed=library_poll();
            if(library_changed>0){
                album_art_clear();
                if(lv==LV_SEARCH){search_refresh();if(lv_sel>=search_count)lv_sel=search_count?search_count-1:0;lv_scroll=0;}
                else if(lv!=LV_PLAYLISTS&&lv!=LV_SAVED&&lv!=LV_FAVORITES)lib_view(LV_HOME);
                show_toast("%s","Music library updated");
                if(S.cover_fetch)start_cover_fetch(0);
            }else if(library_changed<0)show_toast("%s","Music library scan failed");
            if(now_t>=gm_check_due||was_off!=screen_off){gm_active=service_running()!=0;gm_check_due=now_t+(screen_off?1.0:3.0);}
            covers_poll(&cover_done,&cover_total,&cover_running,&downloaded);
            cover_saved+=downloaded;
            cover_result result;
            if(covers_take_result(&result)){
                char summary[120];
                const char *why=result.status==COVER_CLOCK?"Cannot reach time servers":
                    result.status==COVER_NETWORK?"Network error":result.status==COVER_TLS?"TLS verification failed":
                    result.status==COVER_BUSY?"Server temporarily busy":
                    result.status==COVER_SERVER?"Server error":result.status==COVER_STORAGE?"Cannot save image":
                    result.status==COVER_RESPONSE?"Invalid response":result.status==COVER_CANCELLED?"Cancelled":NULL;
                if(why)snprintf(summary,sizeof summary,"Covers: %d saved - %s",result.saved,why);
                else snprintf(summary,sizeof summary,"Covers: %d saved, %d existing, %d unavailable",result.saved,result.existing,result.missing+result.skipped);
                if(cover_manual||result.saved||(result.status!=COVER_OK&&result.status!=COVER_CLOCK&&
                    result.status!=COVER_NETWORK&&result.status!=COVER_TLS)){show_toast("%s",summary);toast_until=now_t+4.5;}
                cover_manual=0;
            }
            if(!cover_running||cover_total<=0||cover_done>=cover_total)cover_status_visible_at=0;
            else if(cover_status_visible_at<=0)cover_status_visible_at=now_t+1.25;
            if(downloaded)album_art_clear();
            housekeeping_due=now_t+(screen_off?1.0:0.25);
        }
        static int covers_paused=-1;
        if(covers_paused!=(screen_off||gm_active)||housekeeping||ui_wake){
            covers_paused=screen_off||gm_active;
            covers_set_paused(covers_paused);
        }
        double due=0;
        int animated=0;
        if(!event_driven&&screen_off){m2d_wait_input(40,audio_event_fd());continue;}
        if(event_driven){
        /* Input wakes poll immediately. Timed holds, repeat, blink and data
         * checks have explicit deadlines; no 16 ms busy polling. */
        due=now_t+(screen_off?0.5:0.25);
        sooner(&due,housekeeping_due);
        sooner(&due,last_save+60);
        sooner(&due,sleep_at);
        if(screen_off){
            have_presented=0;
            m2d_wait_input((int)ceil(fmax(0,due-now())*1000),audio_event_fd());
            continue;
        }
        if(select_held&&!select_hold_fired&&!combo_consumed&&!menu_held)
            sooner(&due,select_since+SELECT_OPTIONS_HOLD_SECONDS);
        if(back_held&&!back_hold_fired&&!menu_held)
            sooner(&due,back_since+LIBRARY_VIEWS_HOLD_SECONDS);
        if(playlist_x_held&&!playlist_x_fired&&!menu_held)
            sooner(&due,playlist_x_since+X_VIS_HOLD_SECONDS);
        if(action_held&&!action_fired&&!menu_held)sooner(&due,action_since+0.75);
        if(held_key>=0)sooner(&due,next_repeat);
        if(screen==SCR_SKINS&&preview_idx!=skin_sel)sooner(&due,preview_due);
        sooner(&due,pressed_until);sooner(&due,msg_until);sooner(&due,toast_until);
        if(screen==SCR_LIBRARY)sooner(&due,cover_status_visible_at);
        if(main_visible()&&audio_state()==A_PAUSED)sooner(&due,(floor(now_t*2)+1)/2);
        int scrolling=0;double clock_wait=1.0;
        ui_snapshot next=snapshot(&scrolling,&clock_wait);
        sooner(&due,now_t+clock_wait);
        animated=screen==SCR_VIS||(mini_visible()&&!audio_buffering())||scrolling||
                     now_t-last_input_at<0.6;
        pending_redraw |= downloaded||library_changed||ui_wake||screen!=frame_screen||was_off;
        int changed=!have_presented||memcmp(&next,&presented,sizeof next)||pending_redraw;
        double frame_due=last_present+1.0/30.0;
        if(!changed&&!animated){
            m2d_wait_input((int)ceil(fmax(0,due-now())*1000),audio_event_fd());
            continue;
        }
        if(now_t<frame_due&&!ui_wake&&!was_off){
            sooner(&due,frame_due);
            m2d_wait_input((int)ceil(fmax(0,due-now())*1000),audio_event_fd());
            continue;
        }
        presented=next;have_presented=1;pending_redraw=0;
        }
        // Do not run the FFT in folder browsing or when visualizers are off.
        if (audio_buffering()) memset(&vd,0,sizeof vd);
        else if (screen == SCR_VIS || mini_visible())
            vis_analyze(&vd, (float)(last_present > 0 ? now_t - last_present : dt),
                        audio_state() == A_PLAYING);
        draw_frame();
        double drawn = now();
        m2d_present();
        last_present = now_t;
        double spent = now() - now_t;
        if (profile) {
            static double acc, worst, t_last;
            static int frames;
            static long long cpu_last;
            acc += drawn - now_t;
            if (drawn - now_t > worst) worst = drawn - now_t;
            frames++;
            if (now_t - t_last > 10) {
                long long cpu = 0;
                FILE *ps = fopen("/proc/self/stat", "r");
                if (ps) {
                    char b[1024];
                    if (fgets(b, sizeof b, ps)) {
                        char *q = strrchr(b, ')');
                        unsigned long ut = 0, stt = 0;
                        if (q && sscanf(q + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stt) == 2) cpu = (long long)(ut + stt);
                    }
                    fclose(ps);
                }
                double hz = (double)sysconf(_SC_CLK_TCK);
                if (t_last > 0) fprintf(stderr, "[profile] %d frames, draw avg %.1f ms max %.1f ms, cpu %.0f%%\n",
                                        frames, acc / frames * 1000, worst * 1000, (cpu - cpu_last) / hz / (now_t - t_last) * 100);
                cpu_last = cpu; t_last = now_t; acc = 0; worst = 0; frames = 0;
            }
        }
        if(!event_driven){
        /* Host UI tests schedule key events by rendered frame numbers. */
        double target = screen == SCR_VIS ? 1.0 / 30.0 :
                        ((screen == SCR_PLAYLIST || screen == SCR_EQ) &&
                         !S.playlist_expanded && S.vis_mode != 0) ? 1.0 / 24.0 :
                        1.0 / 15.0;
        if (spent < target) usleep((useconds_t)((target - spent) * 1e6));
        }else{
        (void)spent;
        /* Re-evaluate immediately after a static frame, then sleep until the
         * next actual change; animations wait for the 30 fps deadline. */
        if(animated) {
            sooner(&due,last_present+1.0/30.0);
            m2d_wait_input((int)ceil(fmax(0,due-now())*1000),audio_event_fd());
        }
        }
    }
    if(profile)fprintf(stderr,"[profile] total loops %lu frames %d elapsed %.2f s\n",loop_count,m2d_frame_count(),now()-started_at);
    stayawake_release();
    m2d_surface_free(mw_scaled);
    if (screen_off) set_screen_off(0);
    /* Streaming network service is not handed into the system-wide game hook. */
    int handoff = S.game_music && current >= 0 && audio_state() == A_PLAYING
                  && !radio_is_url(pl_path(current))
                  && !dec_ffmpeg_source(pl_path(current));
    save_all();
    audio_quit();
    covers_shutdown();
    netclock_shutdown();
    library_quit();
    album_art_clear();
    col_clear(&favorites);col_clear(&saved);
    pl_quit();
    m2d_quit();
    if (handoff) {
        if (!gm_installed()) service_spawn();
        else fprintf(stderr, "[gm] handoff refused: legacy wrapper must be restored first\n");
    }
    return 0;
}
