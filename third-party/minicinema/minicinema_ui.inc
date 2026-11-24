/* MiniCinema playback controls and OSD for the Miyoo SDL2 player.
 *
 * This code is compiled into the dedicated MiniCinema player.  It deliberately avoids an
 * LD_PRELOAD input shim: playback, position tracking and drawing all use the
 * same FFplay clock and the same SDL2 texture that is presented with vsync.
 */

#include <dirent.h>
#include <strings.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <linux/input.h>
#include "minicinema_menu_font.h"

typedef struct _TTF_Font TTF_Font;
extern int TTF_Init(void);
extern void TTF_Quit(void);
extern TTF_Font *TTF_OpenFont(const char *, int);
extern void TTF_CloseFont(TTF_Font *);
extern void TTF_SetFontOutline(TTF_Font *, int);
extern SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *, const char *, SDL_Color);
extern int TTF_SizeUTF8(TTF_Font *, const char *, int *, int *);
extern SDL_Surface *IMG_Load(const char *);

#define MC_STATS_LINES 7
#define KP_W 640
#define KP_H 480
#define KP_HEADER_TITLE_LINES 4
#define KP_MEDIA_PLAYING "/tmp/minicinema_media_playing"
#define KP_STAY_AWAKE "/tmp/stay_awake"
#define KP_BACKLIGHT "/sys/devices/soc0/soc/1f003400.pwm/pwm/pwmchip0/pwm0/duty_cycle"
#define KP_BACKLIGHT_SAVED "/tmp/minicinema_backlight"

typedef struct KidsPlayUi {
    int initialized;
    int audio_only;
    int menu_down;
    int menu_used; /* Another key / brightness hotkey used while MENU held. */
    int menu_double_candidate;
    long menu_brightness_start;
    int screenshot_latched;
    int overlay_drawn;
    int paused;
    int seek_key;
    int seek_forward;
    int vertical_key;
    int vertical_done;
    uint32_t vertical_at;
    uint32_t vertical_repeat_at;
    int shoulder_l1_r1;
    int shoulder_l2_r2;
    int menu_open;
    int menu_index;
    int menu_page; /* 0: main, 1: subtitles, 2: video actions */
    int menu_main_index;
    int menu_pending;
    int confirm_action; /* 0 none, 10 restart, 11 replace preview */
    int confirm_from_x;
    int chapter_return_index;
    int frame_stepping;
    int sleep_timer_fired;
    int64_t sleep_deadline;
    int preview_exists;
    int x_preview_pending;
    int b_second_press_exit;
    int start_aspect_shortcut;
    int a_pause_shortcut;
    int x_action;
    int osd_duration_seconds;
    int osd_hidden;
    int low_battery_notice;
    int low_battery_shown;
    uint32_t low_battery_last_check, low_battery_until;
    int help_open, help_resume_after;
    SDL_Keycode help_ignore_release;
    SDL_Surface *help_image;
    int so_baked, so_reupload;
    char screen_overlay[64];
    char overlay_names[32][64];
    unsigned overlay_count;
    int overlay_scanned;

    int notice_seek;
    int subtitle_size;
    int subtitle_position;
    int auto_next;
    int browser_previews;
    int remember_resume;
    int auto_preview_seconds;
    int pause_dim_seconds;
    int auto_preview_pending;
    char subtitle_font_setting[512];
    int awake_owned;
    uint32_t menu_released;
    TTF_Font *srt_font;
    char srt_font_path[512];
    SDL_Surface *menu_rows[20];
    SDL_Surface *menu_values[20];
    char menu_row_cache[20][96];
    char menu_value_cache[20][64];
    char menu_feedback[64];
    uint32_t menu_feedback_until;
    int battery_held;
    uint32_t seek_at;
    uint32_t seek_last;
    uint32_t progress_until;
    uint32_t seek_notice_until;
    uint32_t last_save;
    uint32_t last_status;
    long duration;
    long position;
    int battery;
    int startup_osd_pending;
    int startup_osd_immediate;
    uint32_t clock_ready_after;
    int clock_ready;
    int power_fd;
    int power_blanked;
    int power_resume_on_wake;
    int power_was_dark;
    int64_t power_last_press_wall;
    int64_t power_clock_offset;
    uint32_t power_ignore_until; /* discard POWER events queued during SIGSTOP */
    int backlight_stage;
    long backlight_saved;
    long backlight_written;
    long backlight_observed;
    uint32_t pause_activity_at;
    SDL_Keycode wake_ignore_key;
    int wake_ignore_release;
    int clean_ready;
    int frame_fresh;
    int clean_pitch;
    uint8_t *clean_pixels;
    size_t clean_capacity;
    SDL_Texture *audio_texture;
    SDL_Surface *audio_canvas;
    SDL_Surface *audio_base;
    SDL_Surface *artwork;
    SDL_Surface *text_elapsed;
    SDL_Surface *text_remaining;
    SDL_Surface *text_battery;
    SDL_Surface *text_muted;
    char cached_muted[24];
    /* Live stats panel: session only, refreshed twice a second. */
    int stats_on;
    uint32_t stats_next;
    uint32_t stats_last_ticks;
    int stats_last_drops;
    int stats_drop_rate;
    unsigned long long stats_cpu_busy, stats_cpu_total;
    int stats_cpu_percent;
    char stats_line[MC_STATS_LINES][64];
    char stats_cached[MC_STATS_LINES][64];
    SDL_Surface *stats_text[MC_STATS_LINES];
    SDL_Surface *text_notice;
    SDL_Surface *text_title;
    SDL_Surface *text_menu_title;
    SDL_Surface *text_menu_footer;
    SDL_Surface *text_menu_hint;
    int menu_focus_cache;
    SDL_Surface *text_header_title[KP_HEADER_TITLE_LINES];
    TTF_Font *font_small;
    TTF_Font *font_title;
    char notice[32];
    char position_file[512];
    char checkpoint_file[512];
    char seek_end_file[512];
    char screenshot_file[512];
    char preview_file[1024];
    char settings_file[512];
    char duration_file[512];
    char artwork_file[512];
    char media_title[512];
    char font_path[512];
    char cached_elapsed[32];
    char cached_remaining[32];
    char cached_battery[16];
    char cached_notice[32];
    char cached_title[512];
    char cached_menu_title[40];
    char cached_menu_footer[80];
    char cached_menu_hint[80];
    char cached_header_title[KP_HEADER_TITLE_LINES][512];
} KidsPlayUi;

static KidsPlayUi kp;

static void kp_copy_env(char *dst, size_t size, const char *name)
{
    const char *value = getenv(name);
    if (!value)
        value = "";
    av_strlcpy(dst, value, size);
}

static long kp_read_number(const char *path)
{
    char line[80];
    char *end = NULL;
    FILE *fp;
    long value;
    if (!path || !path[0] || !(fp = fopen(path, "r")))
        return -1;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    value = strtol(line, &end, 10);
    return end == line ? -1 : value;
}

static void kp_write_number(const char *path, long value)
{
    FILE *fp;
    if (!path || !path[0] || !(fp = fopen(path, "w")))
        return;
    fprintf(fp, "%ld\n", FFMAX(value, 0));
    fclose(fp);
}

/* Checkpoint files live on FAT. Never truncate the only valid position:
 * write a sibling file, flush it and rename over the old checkpoint. */
static void kp_write_checkpoint(long value)
{
    char temporary[sizeof(kp.checkpoint_file)+32];
    FILE *fp;
    if(!kp.checkpoint_file[0])return;
    if(snprintf(temporary,sizeof(temporary),"%s.tmp.%ld",kp.checkpoint_file,
                (long)getpid()) >= (int)sizeof(temporary))return;
    fp=fopen(temporary,"w");
    if(!fp)return;
    if(fprintf(fp,"%ld\n",FFMAX(value,0))<0 || fflush(fp)){
        fclose(fp);unlink(temporary);return;
    }
    if(fclose(fp)){unlink(temporary);return;}
    if(rename(temporary,kp.checkpoint_file))unlink(temporary);
}

static void kp_touch(const char *path, int present)
{
    if (!present) {
        unlink(path);
        return;
    }
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs("1\n", fp);
        fclose(fp);
    }
}

/* Do not remove a sleep-inhibit flag created by another application. */
static void kp_set_stay_awake(int active)
{
    if (!active) {
        if (kp.awake_owned) unlink(KP_STAY_AWAKE);
        kp.awake_owned = 0;
    } else if (!kp.awake_owned) {
        int fd = open(KP_STAY_AWAKE, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0) {
            close(fd);
            kp.awake_owned = 1;
        }
    }
}

/* Return a percentage only for a complete, valid 0..100 integer. */
static int kp_read_percent(const char *path)
{
    char line[64];
    const char *p;
    unsigned value = 0, digits = 0;
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    p = line;
    while (*p == ' ' || *p == '\t') p++;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (unsigned)(*p++ - '0');
        if (++digits > 3 || value > 100) return -1;
    }
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return digits && !*p ? (int)value : -1;
}

static int kp_battery_percentage(void)
{
    int result = kp_read_percent("/tmp/percBat");
    DIR *dir;
    struct dirent *entry;
    if (result >= 0)
        return result;
    dir = opendir("/sys/class/power_supply");
    if (!dir)
        return -1;
    while ((entry = readdir(dir)) != NULL) {
        char path[512], type[64];
        FILE *fp;
        if (entry->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", entry->d_name);
        fp = fopen(path, "r");
        if (!fp) continue;
        if (!fgets(type, sizeof(type), fp)) {
            fclose(fp);
            continue;
        }
        fclose(fp);
        if (strncmp(type, "Battery", 7) ||
            (type[7] && type[7] != '\n' && type[7] != '\r')) continue;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", entry->d_name);
        result = kp_read_percent(path);
        if (result >= 0) break;
    }
    closedir(dir);
    return result;
}

static void kp_update_status(VideoState *is, int force)
{
    uint32_t now = SDL_GetTicks();
    if (!force && now - kp.last_status < 1000)
        return;
    kp.last_status = now;
    if (is && is->ic && is->ic->duration > 0) {
        kp.duration = is->ic->duration / AV_TIME_BASE;
        if (kp.duration_file[0] && kp_read_number(kp.duration_file) != kp.duration)
            kp_write_number(kp.duration_file, kp.duration);
    }
}

#include "minicinema_srt.inc"

/* Persist video-player preferences globally, not per media file.
 * The subtitle visibility toggle and selected streams deliberately remain
 * per-play; only actual preferences are stored. */
static int mc_read_setting_int(const char *raw, int *result)
{
    char *end;
    long value;
    while (*raw == ' ' || *raw == '\t') raw++;
    errno = 0;
    value = strtol(raw, &end, 10);
    if (errno || end == raw || value < -15000 || value > 15000)
        return 0;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end) return 0;
    *result = (int)value;
    return 1;
}

/* Only plain PNG basenames may be stored in the shared settings file. */
static int mc_overlay_filename_valid(const char *name)
{
    size_t n;
    if (!name || !*name || name[0] == '.' || strchr(name, '/') ||
        strchr(name, '\\')) return 0;
    n = strlen(name);
    if (n < 5 || n >= sizeof(kp.screen_overlay)) return 0;
    return !strcasecmp(name + n - 4, ".png");
}

/* Consistent temporary OSD lifetime, including the low-battery warning. */
static uint32_t mc_osd_ms(void) { return (uint32_t)kp.osd_duration_seconds * 1000u; }

static void mc_settings_load(void)
{
    char line[768];
    FILE *fp;
    /* Defaults when the settings file does not yet exist. */
    kp.subtitle_size = 1; /* Normal */
    kp.subtitle_position = 0; /* Low */
    kp.auto_next = 0;
    kp.browser_previews = 1;
    kp.remember_resume = 1;
    kp.auto_preview_seconds = -1;
    kp.pause_dim_seconds = 30;
    kp.shoulder_l1_r1 = 0;
    kp.shoulder_l2_r2 = 0;
    kp.b_second_press_exit = 1;
    kp.start_aspect_shortcut = 1;
    kp.a_pause_shortcut = 0;
    kp.x_action = 0;
    kp.osd_duration_seconds = 2;
    kp.low_battery_notice = 0;
    kp.sleep_deadline = 0;
    kp.startup_osd_immediate = 0;
    mc_srt_delay_ms = 0;
    mc_audio_delay_ms = 0;
    kp.subtitle_font_setting[0] = 0;
    kp.screen_overlay[0] = 0;
    if (!kp.settings_file[0] || !(fp = fopen(kp.settings_file, "r")))
        return;
    while (fgets(line, sizeof(line), fp)) {
        char *sep = strchr(line, '=');
        int value;
        if (!sep) continue;
        *sep++ = '\0';
        if (!strcmp(line, "screen_overlay")) {
            sep[strcspn(sep, "\r\n")] = 0;
            if (!*sep || mc_overlay_filename_valid(sep))
                av_strlcpy(kp.screen_overlay, sep, sizeof(kp.screen_overlay));
            continue;
        }
        if (!strcmp(line, "subtitle_font")) {
            size_t length = strcspn(sep, "\r\n");
            sep[length] = '\0';
            if (sep[0] == '/' && length < sizeof(kp.subtitle_font_setting))
                av_strlcpy(kp.subtitle_font_setting, sep, sizeof(kp.subtitle_font_setting));
            continue;
        }
        if (!mc_read_setting_int(sep, &value)) continue;
        if (!strcmp(line, "audio_delay_ms") && value >= -2000 && value <= 2000)
            mc_audio_delay_ms = value;
        else if (!strcmp(line, "subtitle_size") && value >= 0 && value <= 2)
            kp.subtitle_size = value;
        else if (!strcmp(line, "subtitle_position") && value >= 0 && value <= 2)
            kp.subtitle_position = value;
        else if (!strcmp(line, "subtitle_delay_ms") && value >= -15000 && value <= 15000)
            mc_srt_delay_ms = value;
        else if (!strcmp(line, "auto_next") && (value == 0 || value == 1))
            kp.auto_next = value;
        else if (!strcmp(line, "browser_previews") && (value == 0 || value == 1))
            kp.browser_previews = value;
        else if (!strcmp(line, "remember_resume") && (value == 0 || value == 1))
            kp.remember_resume = value;
        else if (!strcmp(line, "auto_preview_seconds") && value >= -1 && value <= 3600)
            kp.auto_preview_seconds = value;
        else if (!strcmp(line, "pause_dim_seconds") &&
                 (value == 0 || value == 15 || value == 30 || value == 60 ||
                  value == 90 || value == 120))
            kp.pause_dim_seconds = value;
        else if (!strcmp(line, "shoulder_l1_r1") && (value == 0 || value == 1))
            kp.shoulder_l1_r1 = value;
        else if (!strcmp(line, "shoulder_l2_r2") && (value == 0 || value == 1))
            kp.shoulder_l2_r2 = value;
        else if (!strcmp(line, "b_second_press_exit") && (value == 0 || value == 1))
            kp.b_second_press_exit = value;
        else if (!strcmp(line, "start_aspect_shortcut") && (value == 0 || value == 1))
            kp.start_aspect_shortcut = value;
        else if (!strcmp(line, "a_pause_shortcut") && (value == 0 || value == 1))
            kp.a_pause_shortcut = value;
        else if (!strcmp(line, "x_button_action") && value >= 0 && value <= 13)
            kp.x_action = value;
        else if (!strcmp(line, "low_battery_notice") &&
                 (value == 0 || value == 5 || value == 10 || value == 15))
            kp.low_battery_notice = value;
        else if (!strcmp(line, "subtitles_enabled") && (value == 0 || value == 1))
            mc_srt_enabled = value;
        else if (!strcmp(line, "osd_duration_seconds") &&
                 (value == 1 || value == 2 || value == 3 || value == 5 ||
                  value == 8 || value == 10))
            kp.osd_duration_seconds = value;
        else if (!strcmp(line, "startup_osd_immediate") && (value == 0 || value == 1))
            kp.startup_osd_immediate = value;
    }
    fclose(fp);
}

/* Preserve unknown and manually edited settings, especially subtitle_font and
 * experimental renderer options. Only replace the keys that this player owns. */
static int mc_setting_is_managed(const char *line)
{
    static const char *keys[] = {
        "subtitle_size=", "subtitle_position=", "subtitle_delay_ms=",
        "audio_delay_ms=", "auto_next=", "browser_previews=",
        "shoulder_l1_r1=", "shoulder_l2_r2=", "remember_resume=",
        "auto_preview_seconds=", "b_second_press_exit=",
        "start_aspect_shortcut=", "a_pause_shortcut=", "screen_overlay="
    };
    for (unsigned i = 0; i < FF_ARRAY_ELEMS(keys); ++i)
        if (!strncmp(line, keys[i], strlen(keys[i]))) return 1;
    return 0;
}

static void mc_settings_save(void)
{
    char temporary[sizeof(kp.settings_file) + 32];
    char line[1024];
    FILE *fp, *old;
    int okay = 1;
    int last_char = '\n';
    if (!kp.settings_file[0]) return;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", kp.settings_file,
                 (long)getpid()) >= (int)sizeof(temporary)) return;
    fp = fopen(temporary, "w");
    if (!fp) return;
    old = fopen(kp.settings_file, "r");
    if (old) {
        while (fgets(line, sizeof(line), old)) {
            if (!mc_setting_is_managed(line)) {
                size_t len = strlen(line);
                if (fputs(line, fp) == EOF) okay = 0;
                if (len) last_char = (unsigned char)line[len - 1];
            }
        }
        if (ferror(old)) okay = 0;
        fclose(old);
    }
    if (last_char != '\n' && fputc('\n', fp) == EOF) okay = 0;
    if (fprintf(fp,
                "subtitle_size=%d\nsubtitle_position=%d\n"
                "subtitle_delay_ms=%d\naudio_delay_ms=%d\n"
                "auto_next=%d\nbrowser_previews=%d\n"
                "shoulder_l1_r1=%d\nshoulder_l2_r2=%d\n"
                "remember_resume=%d\nauto_preview_seconds=%d\n"
                "b_second_press_exit=%d\n"
                "start_aspect_shortcut=%d\na_pause_shortcut=%d\n"
                "screen_overlay=%s\n",
                kp.subtitle_size, kp.subtitle_position, mc_srt_delay_ms,
                mc_audio_delay_ms, kp.auto_next, kp.browser_previews,
                kp.shoulder_l1_r1, kp.shoulder_l2_r2,
                kp.remember_resume, kp.auto_preview_seconds,
                kp.b_second_press_exit, kp.start_aspect_shortcut,
                kp.a_pause_shortcut, kp.screen_overlay) < 0) okay = 0;
    if (fflush(fp)) okay = 0;
    if (fclose(fp)) okay = 0;
    if (!okay) { unlink(temporary); return; }
    if (rename(temporary, kp.settings_file)) unlink(temporary);
}


/* Optional screen bezel: compact opaque/blended spans, no per-frame full copy.
 * Transparent pixels are skipped, opaque pixels are copied. */
typedef struct { uint16_t y,x,len; uint8_t opaque; } mc_so_span;
static struct {
    uint32_t *pixels;
    mc_so_span *spans;
    unsigned count;
    int active;
} mc_so;
static void mc_so_free(void){
    av_freep(&mc_so.pixels);
    av_freep(&mc_so.spans);
    mc_so.count=0;mc_so.active=0;
}
static int mc_so_add(unsigned *cap,int y,int x,int len,int opaque){
    if(mc_so.count==*cap){
        unsigned next=*cap?*cap*2:1024;
        mc_so_span *grown=av_realloc_array(mc_so.spans,next,sizeof(*grown));
        if(!grown)return -1;
        mc_so.spans=grown;*cap=next;
    }
    mc_so.spans[mc_so.count++]=(mc_so_span){y,x,len,opaque};
    return 0;
}
static void mc_so_load(const char *path){
    SDL_Surface *loaded,*argb;unsigned cap=0;
    if(!path || !*path)return;
    /* Only 8-bit, non-interlaced 640x480 RGBA PNGs can overlay a frame. */
    FILE *fp=fopen(path,"rb");
    unsigned char header[29];
    if(!fp)return;
    int valid=fread(header,1,sizeof(header),fp)==sizeof(header) &&
        !memcmp(header,"\x89PNG\r\n\x1a\n",8) &&
        header[16]==0 && header[17]==0 && header[18]==2 && header[19]==128 &&
        header[20]==0 && header[21]==0 && header[22]==1 && header[23]==224 &&
        header[24]==8 && header[25]==6 &&
        header[26]==0 && header[27]==0 && header[28]==0;
    fclose(fp);
    if(!valid){fprintf(stderr,"MiniCinema overlay: requires 640x480 RGBA PNG\n");return;}
    loaded=IMG_Load(path);
    if(!loaded){fprintf(stderr,"MiniCinema overlay: cannot load %s\n",path);return;}
    argb=SDL_ConvertSurfaceFormat(loaded,SDL_PIXELFORMAT_ARGB8888,0);
    SDL_FreeSurface(loaded);
    if(!argb)return;
    if(argb->w!=KP_W || argb->h!=KP_H){
        fprintf(stderr,"MiniCinema overlay: requires 640x480: %s\n",path);
        SDL_FreeSurface(argb);return;
    }
    mc_so.pixels=av_malloc((size_t)KP_W*KP_H*sizeof(uint32_t));
    if(!mc_so.pixels)goto fail;
    if(SDL_LockSurface(argb)<0)goto fail;
    for(int y=0;y<KP_H;y++){
        const uint32_t *row=(const uint32_t*)((const uint8_t*)argb->pixels+(size_t)y*argb->pitch);
        uint32_t *out=mc_so.pixels+(size_t)y*KP_W;
        for(int x=0;x<KP_W;){
            uint32_t alpha=row[x]>>24;
            int start=x,opaque=alpha==255;
            if(!alpha){x++;continue;}
            while(x<KP_W){
                uint32_t c=row[x],a=c>>24;
                if(!a || (a==255)!=opaque)break;
                if(opaque)out[x]=c;
                else{
                    uint32_t red=((c>>16)&255)*a/255;
                    uint32_t green=((c>>8)&255)*a/255;
                    uint32_t blue=(c&255)*a/255;
                    out[x]=((255-a)<<24)|(red<<16)|(green<<8)|blue;
                }
                x++;
            }
            if(mc_so_add(&cap,y,start,x-start,opaque)<0){
                SDL_UnlockSurface(argb);goto fail;
            }
        }
    }
    SDL_UnlockSurface(argb);
    SDL_FreeSurface(argb);
    mc_so.active=mc_so.count>0;
    fprintf(stderr,"MiniCinema overlay: %u pixel runs\n",mc_so.count);
    return;
fail:
    SDL_FreeSurface(argb);mc_so_free();
}
static inline uint32_t mc_so_blend(uint32_t d,uint32_t src){
    uint32_t inv=src>>24;
    uint32_t rb=(d&0x00ff00ffu)*inv+0x00800080u;
    uint32_t g=(d&0x0000ff00u)*inv+0x00008000u;
    rb=((rb+((rb>>8)&0x00ff00ffu))>>8)&0x00ff00ffu;
    g=((g+((g>>8)&0x0000ff00u))>>8)&0x0000ff00u;
    return 0xff000000u|((src&0x00ffffffu)+rb+g);
}
static void mc_so_draw(uint8_t *pixels,int pitch){
    for(unsigned i=0;i<mc_so.count;i++){
        const mc_so_span *s=&mc_so.spans[i];
        uint32_t *dst=(uint32_t*)(pixels+(size_t)s->y*pitch)+s->x;
        const uint32_t *src=mc_so.pixels+(size_t)s->y*KP_W+s->x;
        if(s->opaque)memcpy(dst,src,(size_t)s->len*4);
        else for(unsigned n=0;n<s->len;n++)dst[n]=mc_so_blend(dst[n],src[n]);
    }
}
/* Small, sorted list loaded when the player options menu opens. */
static void mc_overlay_scan(void)
{
    DIR *dir = opendir("/mnt/SDCARD/App/MiniCinema/overlays");
    struct dirent *entry;
    kp.overlay_count = 0;
    kp.overlay_scanned = 1;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        unsigned i;
        if (!mc_overlay_filename_valid(entry->d_name)) continue;
        {
            struct stat file_stat;
            char path[256];
            if (snprintf(path, sizeof(path),
                         "/mnt/SDCARD/App/MiniCinema/overlays/%s",
                         entry->d_name) >= (int)sizeof(path) ||
                stat(path, &file_stat) || !S_ISREG(file_stat.st_mode)) continue;
        }
        if (kp.overlay_count == FF_ARRAY_ELEMS(kp.overlay_names)) break;
        i = kp.overlay_count++;
        while (i > 0 && strcasecmp(kp.overlay_names[i - 1], entry->d_name) > 0) {
            av_strlcpy(kp.overlay_names[i], kp.overlay_names[i - 1],
                       sizeof(kp.overlay_names[i]));
            i--;
        }
        av_strlcpy(kp.overlay_names[i], entry->d_name,
                   sizeof(kp.overlay_names[i]));
    }
    closedir(dir);
}

static void kp_reupload_shown_frame(VideoState *is){
    Frame *vp;
    if(!is || is->video_stream<0)return;
    vp=frame_queue_peek_last(&is->pictq);
    if(vp && vp->frame)vp->uploaded=0;
    is->force_refresh=1;
}

static void kp_init(VideoState *is)
{
    const char *kind;
    const char *start;
    int size;
    if (kp.initialized)
        return;
    memset(&kp, 0, sizeof(kp));
    kp.battery = -1;
    kp.power_fd = -1;
    kp.menu_focus_cache = -1;
    kp.pause_activity_at = SDL_GetTicks();
    kp.backlight_observed = -1;
    kp.wake_ignore_key = SDLK_UNKNOWN;
    kp.help_ignore_release = SDLK_UNKNOWN;
    mc_srt_load(getenv("MC_SRT_FILE"));
    kp_copy_env(kp.settings_file, sizeof(kp.settings_file), "MC_SETTINGS_FILE");
    mc_settings_load();
    kp_copy_env(kp.duration_file, sizeof(kp.duration_file), "VC_DURATION_FILE");
    kp_copy_env(kp.preview_file, sizeof(kp.preview_file), "MC_PREVIEW_FILE");
    kp.preview_exists = kp.preview_file[0] &&
                        access(kp.preview_file, F_OK) == 0;
    kp_copy_env(kp.position_file, sizeof(kp.position_file), "VC_POSITION_FILE");
    kp_copy_env(kp.checkpoint_file, sizeof(kp.checkpoint_file), "VC_CHECKPOINT_FILE");
    kp_copy_env(kp.seek_end_file, sizeof(kp.seek_end_file), "MC_SEEK_END_FILE");
    kp_copy_env(kp.screenshot_file, sizeof(kp.screenshot_file), "VC_SCREENSHOT_FILE");
    kp_copy_env(kp.artwork_file, sizeof(kp.artwork_file), "VC_ARTWORK_FILE");
    kp_copy_env(kp.media_title, sizeof(kp.media_title), "VC_MEDIA_TITLE");
    kp_copy_env(kp.font_path, sizeof(kp.font_path), "VC_OSD_FONT");
    {
        const char *deadline = getenv("MC_SLEEP_DEADLINE");
        if (deadline && *deadline) {
            char *end;
            int64_t value = strtoll(deadline, &end, 10);
            if (!*end && value > 0) kp.sleep_deadline = value;
        }
    }
    kind = getenv("VC_MEDIA_KIND");
    kp.audio_only = kind && !strcmp(kind, "audio");
    /* Optional overlay is read once from the validated launcher path. */
    {
        if (!kp.audio_only) {
            char file[256];
            if (mc_overlay_filename_valid(kp.screen_overlay) &&
                snprintf(file, sizeof(file),
                         "/mnt/SDCARD/App/MiniCinema/overlays/%s",
                         kp.screen_overlay) < (int)sizeof(file))
                mc_so_load(file);
        }
    }
    kp.low_battery_last_check = SDL_GetTicks() - 30000;
    start = getenv("VC_START_SECONDS");
    kp.position = start ? strtol(start, NULL, 10) : 0;
    /* Only new, unresumed plays without artwork qualify. Never overwrite PNGs. */
    kp.auto_preview_pending = kp.auto_preview_seconds >= 0 && kp.position == 0 &&
                              kp.preview_file[0] && access(kp.preview_file, F_OK) != 0;
    size = getenv("VC_OSD_FONT_SIZE") ? atoi(getenv("VC_OSD_FONT_SIZE")) : 24;
    if (size < 12 || size > 48)
        size = 24;
    {
        int ttf_ok = TTF_Init() == 0;
        const char *font_used = kp.font_path;
        if (!ttf_ok) {
            fprintf(stderr, "MiniCinema: SDL_ttf initialization failed: %s\n", SDL_GetError());
        } else {
            static const char *fallback_fonts[] = {
                "/mnt/SDCARD/miyoo/app/Helvetica-Neue-2.ttf",
                "/mnt/SDCARD/miyoo/app/wqy-microhei.ttc",
                "/customer/app/wqy-microhei.ttc"
            };
            if (kp.font_path[0])
                kp.font_small = TTF_OpenFont(kp.font_path, size);
            for (unsigned f = 0; !kp.font_small &&
                                  f < FF_ARRAY_ELEMS(fallback_fonts); ++f) {
                kp.font_small = TTF_OpenFont(fallback_fonts[f], size);
                if (kp.font_small) font_used = fallback_fonts[f];
            }
            if (kp.font_small) {
                av_strlcpy(kp.srt_font_path, font_used, sizeof(kp.srt_font_path));
                kp.srt_font = kp.font_small;
                /* SRT/embedded text may use a separate manually specified TTF. */
                if (kp.subtitle_font_setting[0]) {
                    TTF_Font *custom = TTF_OpenFont(kp.subtitle_font_setting,
                                                    kp.subtitle_size == 0 ? 20 :
                                                    kp.subtitle_size == 2 ? 29 : 24);
                    if (custom) {
                        kp.srt_font = custom;
                        av_strlcpy(kp.srt_font_path, kp.subtitle_font_setting,
                                   sizeof(kp.srt_font_path));
                    } else {
                        fprintf(stderr, "MiniCinema: subtitle font unavailable; using default\n");
                    }
                }
                if (kp.srt_font == kp.font_small && kp.subtitle_size != 1) {
                    int subtitle_pixels = kp.subtitle_size == 0 ? 20 : 29;
                    TTF_Font *saved_font = TTF_OpenFont(font_used, subtitle_pixels);
                    if (saved_font) kp.srt_font = saved_font;
                }
                kp.font_title = TTF_OpenFont(font_used, FFMIN(size + 4, 48));
            }
            if (!kp.font_small)
                fprintf(stderr, "MiniCinema: no usable OSD/SRT font: %s\n", SDL_GetError());
        }
        if (mc_cue_count)
            fprintf(stderr, "MiniCinema SRT: %u loaded cues; font %s (%s)\n",
                    mc_cue_count, kp.font_small ? "ready" : "MISSING",
                    font_used[0] ? font_used : "none");
    }
    /* The power key is also read by keymon. Do not grab the device. */
    kp.power_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    /* O_CLOEXEC is not exposed by this FFmpeg/Zig C feature profile. */
    if (kp.power_fd >= 0)
        fcntl(kp.power_fd, F_SETFD, FD_CLOEXEC);
    if (kp.power_fd < 0)
        fprintf(stderr, "MiniCinema: power key monitor unavailable: %s\n", strerror(errno));
    kp.last_save = SDL_GetTicks();
    /* For video, arm the automatic OSD only after the first clean frame has
     * actually been presented. Drawing and re-locking the texture before
     * that first presentation is unreliable in the Miyoo Mini SDL backend. */
    kp.startup_osd_pending = !kp.audio_only;
    /* Don't show a partially initialized duration or A/V clock at startup. */
    kp.clock_ready = kp.audio_only;
    kp.clock_ready_after = 0;
    kp_touch(KP_MEDIA_PLAYING, 1);
    kp_set_stay_awake(is && !is->paused);
    kp_update_status(is, 1);
    kp.initialized = 1;
}

static void kp_save_position(VideoState *is, int checkpoint)
{
    double clock = is ? get_master_clock(is) : NAN;
    if (!isnan(clock) && clock >= 0 &&
        (kp.clock_ready || kp.audio_only))
        kp.position = (long)clock;
    if (kp.remember_resume) {
        kp_write_number(kp.position_file, kp.position);
        if (checkpoint)
            kp_write_checkpoint(kp.position);
    }
}

/* PWM dimming is local to paused playback and uses the raw duty-cycle value.
 * Never change another process's new brightness setting back to an old value. */
static int kp_backlight_write(long value)
{
    FILE *fp = fopen(KP_BACKLIGHT, "w");
    int okay;
    if (!fp) return 0;
    okay = fprintf(fp, "%ld\n", value) > 0;
    if (fclose(fp)) okay = 0;
    return okay;
}

static void kp_backlight_restore(void)
{
    if (kp.backlight_stage) {
        long current = kp_read_number(KP_BACKLIGHT);
        if ((current < 0 || current == kp.backlight_written) && kp.backlight_saved > 0)
            kp_backlight_write(kp.backlight_saved);
    }
    kp.backlight_stage = 0;
    unlink(KP_BACKLIGHT_SAVED);
    kp.backlight_observed = kp_read_number(KP_BACKLIGHT);
    kp.pause_activity_at = SDL_GetTicks();
}

static void kp_backlight_tick(VideoState *is, uint32_t now)
{
    long current;
    /* Suspend belongs to keymon: do not run the paused auto-dimmer
     * while its POWER transition owns the screen. */
    if (kp.power_blanked) return;
    if (!is->paused) {
        if (kp.backlight_stage) kp_backlight_restore();
        return;
    }
    if (!kp.pause_dim_seconds) {
        if (kp.backlight_stage) kp_backlight_restore();
        return;
    }
    if (!kp.pause_activity_at) kp.pause_activity_at = now;
    current = kp_read_number(KP_BACKLIGHT);
    if (!kp.backlight_stage && kp.backlight_observed >= 0 &&
        current >= 0 && current != kp.backlight_observed) {
        /* Keymon changed brightness for a suspend/wake or brightness hotkey. */
        kp.pause_activity_at = now;
    }
    kp.backlight_observed = current;
    if (kp.backlight_stage && current >= 0 && current != kp.backlight_written) {
        /* Brightness hotkey, keymon suspend/wake or another writer won. */
        kp.backlight_stage = 0;
        unlink(KP_BACKLIGHT_SAVED);
        kp.pause_activity_at = now;
        return;
    }
    if (kp.backlight_stage == 0 && now - kp.pause_activity_at >= (uint32_t)kp.pause_dim_seconds * 1000 && current > 3) {
        kp.backlight_saved = current;
        /* Persist the recovery value before changing hardware brightness. */
        kp_write_number(KP_BACKLIGHT_SAVED, current);
        if (kp_backlight_write(3)) {
            kp.backlight_written = 3;
            kp.backlight_stage = 1;
        } else unlink(KP_BACKLIGHT_SAVED);
    } else if (kp.backlight_stage == 1 && now - kp.pause_activity_at >= (uint32_t)(kp.pause_dim_seconds + 15) * 1000) {
        if (kp_backlight_write(0)) {
            kp.backlight_written = 0;
            kp.backlight_stage = 2;
        }
    }
}

/* A short POWER press must release the sleep inhibitor before keymon handles
 * the release event. The same checkpoint path is used by B and POWER. */
static void kp_pause(VideoState *is)
{
    if (!is || is->paused) return;
    kp_save_position(is, 1);
    toggle_pause(is);
    kp_touch(KP_MEDIA_PLAYING, 0);
    kp_set_stay_awake(0);
    kp.pause_activity_at = SDL_GetTicks();
    kp.backlight_observed = kp_read_number(KP_BACKLIGHT);
    kp.osd_hidden = 0;
    kp.progress_until = kp.pause_activity_at + mc_osd_ms();
    is->force_refresh = 1;
}

/* Resume without requiring a second A press after the automatic dim/off.
 * Both ordinary resume and the wake-up resume use exactly this path. */
static void kp_resume(VideoState *is)
{
    if (!is || !is->paused) return;
    kp_backlight_restore();
    toggle_pause(is);
    kp_touch(KP_MEDIA_PLAYING, 1);
    kp_set_stay_awake(1);
    kp.osd_hidden = 0;
    kp.progress_until = SDL_GetTicks() + mc_osd_ms();
    is->force_refresh = 1;
}

/* Timed sleep uses the existing owned PWM recovery mechanism, not a
 * synthetic POWER event (keymon owns real suspend). A wakes and resumes in
 * one press, other keys wake only. Do not run the timer again this session. */
static void kp_sleep_timer_fire(VideoState *is)
{
    long current;
    if (!is) return;
    kp.sleep_timer_fired = 1;
    kp_pause(is); /* checkpoint, release stay_awake, pause audio */
    kp.progress_until = kp.seek_notice_until = 0;
    kp.notice[0] = 0;
    kp.osd_hidden = 1;
    if (kp.backlight_stage) kp_backlight_restore();
    current = kp_read_number(KP_BACKLIGHT);
    if (current > 0) {
        kp.backlight_saved = current;
        kp_write_number(KP_BACKLIGHT_SAVED, current);
        if (kp_backlight_write(0)) {
            kp.backlight_written = 0;
            kp.backlight_stage = 2;
        } else unlink(KP_BACKLIGHT_SAVED);
    }
    is->force_refresh = 1;
}

/* POWER is owned by RedOnion keymon. We draw an immediate black frame,
 * pause/checkpoint, and never write PWM during keymon's sleep transition.
 * Keymon can SIGSTOP the entire player and SIGCONT it only after wake. The
 * SIGCONT handler sets a flag which the main loop handles before reading
 * any queued POWER input, preventing the wake press from re-pausing playback.
 * The existing POWER/PWM fallbacks cover configurations that never SIGSTOP. */
static int kp_power_screen_blank(void)
{
    return kp.power_blanked;
}

/* A menu can still be on the other framebuffer page even after an ordinary
 * redraw. Present the restored picture twice at a paused UI/power boundary,
 * not during ordinary playback. Keep keymon in charge of PWM and suspend. */
static void kp_present_restored_frame(VideoState *is)
{
    if (!is || !renderer || !is->width || kp.power_blanked) return;
    if (!kp.audio_only && (!is->video_st || !is->pictq.rindex_shown)) return;
    kp_reupload_shown_frame(is);
    video_display(is);
    video_display(is);
    is->force_refresh = 1;
}

static void kp_present_sleep_black(void)
{
    if (!renderer) return;
    /* Clear both presentation pages before checkpoint I/O or SIGSTOP.
     * A single clear may leave the previous menu on the alternate page. */
    for (int i = 0; i < 2; i++) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
}

static void kp_power_woke(VideoState *is)
{
    int resume = kp.power_resume_on_wake;
    kp.power_blanked = 0;
    kp.power_resume_on_wake = 0;
    kp.power_was_dark = 0;
    if (resume) kp_resume(is);
    else kp.pause_activity_at = SDL_GetTicks();
    /* Do not flash an old pause/seek OSD over the first frame on wake. */
    kp.progress_until = 0;
    kp.seek_notice_until = 0;
    kp.notice[0] = '\0';
    /* A paused texture may still contain the previous options menu. Always
     * upload the actual shown video frame again before presenting after wake.
     * This is a one-time redraw, not a per-frame copy. */
    kp_reupload_shown_frame(is);
    kp_present_restored_frame(is);
    is->force_refresh = 1;
}

static void kp_power_poll(VideoState *is)
{
    struct input_event ev;
    ssize_t n = -1;
    uint32_t now = SDL_GetTicks();
    int continued = mc_cont_seen != 0;
    if (continued) {
        /* keymon has resumed this process after SIGSTOP. Both the sleep and
         * wake POWER events may be queued in our independent event fd; drain
         * them instead of applying a 1.2s embargo to *future* real presses.
         * In particular, the next sleep cycle must start with fresh state. */
        mc_cont_seen = 0;
        kp.power_ignore_until = now + 300; /* only a late duplicate press */
        if (kp.power_blanked) kp_power_woke(is);
        else if (is) {
            /* The sleep press itself can be missed. SIGCONT still needs a
             * fresh presentation, not a previously decorated frame. */
            kp.progress_until = 0;
            kp.seek_notice_until = 0;
            kp.notice[0] = '\0';
            kp_reupload_shown_frame(is);
            kp_present_restored_frame(is);
            is->force_refresh = 1;
        }
        if (kp.power_fd >= 0) {
            while ((n = read(kp.power_fd, &ev, sizeof(ev))) == sizeof(ev))
                ; /* discard events already queued while we were stopped */
            if (n == 0) { close(kp.power_fd); kp.power_fd = -1; }
        }
    }
    if (kp.power_fd >= 0) {
        while ((n = read(kp.power_fd, &ev, sizeof(ev))) == sizeof(ev)) {
            int64_t wall_now;
            /* RedOnion/keymon owns MENU+volume brightness shortcuts. SDL may
             * only deliver MENU, so notice the volume event on our existing
             * independent evdev fd; it must not become a browser/exit tap. */
            if (ev.type == EV_KEY && ev.value != 0 && kp.menu_down &&
                (ev.code == KEY_VOLUMEUP || ev.code == KEY_VOLUMEDOWN ||
                 ev.code == KEY_UP || ev.code == KEY_DOWN)) {
                kp.menu_used = 1;
                kp.menu_pending = 0;
            }
            if (ev.type != EV_KEY || ev.code != KEY_POWER || ev.value != 1)
                continue;
            now = SDL_GetTicks();
            if (kp.power_ignore_until &&
                !SDL_TICKS_PASSED(now, kp.power_ignore_until))
                continue;
            kp.power_ignore_until = 0;
            wall_now = av_gettime();
            /* Power release events can be lost. Filter only true double
             * presses, not entire subsequent suspend cycles. */
            if (kp.power_last_press_wall &&
                wall_now >= kp.power_last_press_wall &&
                wall_now - kp.power_last_press_wall < 300000)
                continue;
            kp.power_last_press_wall = wall_now;
            if (kp.power_blanked) {
                kp_power_woke(is);
                kp.power_ignore_until = now + 300;
            } else {
                /* Always re-arm this intent for each cycle, before pausing.
                 * Never inherit the previous wake's already-cleared flag. */
                kp.power_resume_on_wake = is && !is->paused;
                kp.power_blanked = 1;
                kp.power_was_dark = 0;
                kp.power_clock_offset = wall_now - av_gettime_relative();
                /* keymon can stop us on POWER release before the event loop
                 * gets another refresh. Blank before checkpoint writes. */
                kp_present_sleep_black();
                kp_pause(is);
                kp.progress_until = 0;
                kp.seek_notice_until = 0;
                kp.notice[0] = '\0';
                if (is) is->force_refresh = 1;
            }
        }
        if (n == 0) { close(kp.power_fd); kp.power_fd = -1; }
    }
    if (kp.power_blanked) {
        long brightness = kp_read_number(KP_BACKLIGHT);
        int64_t offset = av_gettime() - av_gettime_relative();
        if (offset - kp.power_clock_offset > 750000) {
            kp_power_woke(is);
        } else if (brightness == 0) {
            kp.power_was_dark = 1;
        } else if (brightness > 0 && kp.power_was_dark) {
            kp_power_woke(is);
        }
    }
}

static void mc_leave(VideoState *is, int exit_app);

static void mc_stats_tick(VideoState *is, uint32_t now);
static void kp_sleep_timer_fire(VideoState *is);
static void kp_tick(VideoState *is)
{
    uint32_t now;
    kp_init(is);
    now = SDL_GetTicks();
    kp_power_poll(is);
    kp_backlight_tick(is, now);
    /* Deferred redraw after a paused, overlay-baked frame. In the outer
     * event loop this runs BEFORE video_refresh, so the refresh isn't lost. */
    if (kp.so_reupload) {
        kp.so_reupload = 0;
        kp_reupload_shown_frame(is);
    }
    mc_stats_tick(is, now);
    /* FFplay briefly unpauses to step one frame. That is not a user resume:
     * keep the preexisting sleep-inhibitor state instead of toggling it. */
    if (kp.frame_stepping) {
        if (is->paused) kp.frame_stepping = 0;
        kp.paused = 1;
    } else {
        if (kp.paused != is->paused) kp_set_stay_awake(!is->paused);
        kp.paused = is->paused;
    }
    if (kp.sleep_deadline > 0 && !kp.sleep_timer_fired && !kp.power_blanked &&
        av_gettime() / 1000000 >= kp.sleep_deadline)
        kp_sleep_timer_fire(is);
    if(kp.low_battery_notice && !kp.low_battery_shown && !kp.audio_only &&
       !is->paused && !kp.power_blanked &&
       now-kp.low_battery_last_check>=30000){
        kp.low_battery_last_check=now;
        int percent=kp_battery_percentage();
        if(percent>=0 && percent<=kp.low_battery_notice){
            kp.battery=percent;
            kp.low_battery_shown=1;
            kp.low_battery_until=now+mc_osd_ms();
            is->force_refresh=1;
        }
    }
    if(kp.low_battery_until && SDL_TICKS_PASSED(now,kp.low_battery_until)){
        kp.low_battery_until=0;
        is->force_refresh=1;
    }
    /* Brightness changes may arrive slightly after MENU release because
     * keymon is a separate process. Do not treat its shortcut as MENU exit. */
    if ((kp.menu_down || kp.menu_pending) && kp.menu_brightness_start >= 0) {
        long brightness = kp_read_number(KP_BACKLIGHT);
        if (brightness >= 0 && brightness != kp.menu_brightness_start) {
            kp.menu_used = 1;
            kp.menu_pending = 0;
        }
    }
    /* Defer a plain MENU tap. Never execute it while a second MENU press
     * is being held (which might become MENU+VOL). */
    if (kp.menu_pending && !kp.menu_down && now - kp.menu_released >= 320)
        mc_leave(is, 0);
    kp_update_status(is, 0);
    if (!kp.clock_ready && kp.clock_ready_after &&
        SDL_TICKS_PASSED(now, kp.clock_ready_after) && kp.duration > 0) {
        double clock = get_master_clock(is);
        if (!isnan(clock) && clock >= 0 && clock <= (double)kp.duration + 5) {
            kp.clock_ready = 1;
            /* Clock readiness is independent of the startup OSD setting.
             * Off must never schedule a delayed automatic OSD. */
            is->force_refresh = 1;
        }
    }
    if (kp.menu_feedback_until && SDL_TICKS_PASSED(now, kp.menu_feedback_until)) {
        kp.menu_feedback_until = 0;
        kp.menu_feedback[0] = '\0';
        is->force_refresh = 1;
    }
    if (kp.progress_until && SDL_TICKS_PASSED(now, kp.progress_until)) {
        kp.progress_until = 0;
        if (is->paused && !kp.menu_open) kp_reupload_shown_frame(is);
        is->force_refresh = 1;
    }
    if (kp.seek_notice_until && SDL_TICKS_PASSED(now, kp.seek_notice_until)) {
        /* A playing video naturally replaces the expired notice on its next
         * frame. A paused frame does not, so explicitly redraw it cleanly. */
        kp.seek_notice_until = 0;
        kp.notice[0] = '\0';
        if (is->paused && !kp.menu_open) kp_reupload_shown_frame(is);
        is->force_refresh = 1;
    }
    if (now - kp.last_save >= 1000) {
        kp_save_position(is, (now / 15000) != (kp.last_save / 15000));
        kp.last_save = now;
    }
}

static SDL_Surface *kp_text(TTF_Font *font, const char *text, SDL_Color color)
{
    SDL_Color black = {0, 0, 0, 255};
    SDL_Surface *shadow, *face, *result;
    SDL_Rect destination;
    int outline = 2;
    if (!font || !text || !text[0])
        return NULL;
    TTF_SetFontOutline(font, outline);
    shadow = TTF_RenderUTF8_Blended(font, text, black);
    TTF_SetFontOutline(font, 0);
    face = TTF_RenderUTF8_Blended(font, text, color);
    if (!shadow || !face) {
        if (shadow) SDL_FreeSurface(shadow);
        if (face) SDL_FreeSurface(face);
        return NULL;
    }
    result = SDL_CreateRGBSurfaceWithFormat(0, shadow->w, shadow->h, 32,
                                            SDL_PIXELFORMAT_ARGB8888);
    if (result) {
        SDL_FillRect(result, NULL, SDL_MapRGBA(result->format, 0, 0, 0, 0));
        SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
        SDL_SetSurfaceBlendMode(face, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(shadow, NULL, result, NULL);
        destination.x = outline;
        destination.y = outline;
        destination.w = face->w;
        destination.h = face->h;
        SDL_BlitSurface(face, NULL, result, &destination);
        SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(shadow);
    SDL_FreeSurface(face);
    return result;
}

static void kp_blit(SDL_Surface *target, SDL_Surface *source, int x, int y)
{
    SDL_Rect rect;
    if (!target || !source)
        return;
    rect.x = x;
    rect.y = y;
    rect.w = source->w;
    rect.h = source->h;
    SDL_BlitSurface(source, NULL, target, &rect);
}

/* Find the actual visible ink within our ARGB8888 text surface. SDL_ttf
 * surfaces include font ascent/descent spacing that is not visually balanced;
 * using surface->h to center seek text leaves it looking vertically offset.
 * We only do this for the small transient seek/aspect notification. */
static SDL_Rect kp_text_ink_bounds(SDL_Surface *text)
{
    SDL_Rect result = {0, 0, 0, 0};
    int left, top, right, bottom;
    if (!text || !text->pixels || !text->format ||
        text->format->format != SDL_PIXELFORMAT_ARGB8888)
        return result;
    if (SDL_MUSTLOCK(text) && SDL_LockSurface(text) < 0)
        return result;
    left = text->w;
    top = text->h;
    right = -1;
    bottom = -1;
    for (int y = 0; y < text->h; ++y) {
        const uint32_t *row = (const uint32_t *)((const uint8_t *)text->pixels +
                                                 (size_t)y * text->pitch);
        for (int x = 0; x < text->w; ++x) {
            if ((row[x] >> 24) >= 32) {
                if (x < left) left = x;
                if (x > right) right = x;
                if (y < top) top = y;
                if (y > bottom) bottom = y;
            }
        }
    }
    if (SDL_MUSTLOCK(text))
        SDL_UnlockSurface(text);
    if (right >= left && bottom >= top) {
        result.x = left;
        result.y = top;
        result.w = right - left + 1;
        result.h = bottom - top + 1;
    }
    return result;
}

static SDL_Surface *kp_cached_text(SDL_Surface **cache, char *cached,
                                   size_t cached_size, TTF_Font *font,
                                   const char *text, SDL_Color color)
{
    if (*cache && !strcmp(cached, text))
        return *cache;
    if (*cache) {
        SDL_FreeSurface(*cache);
        *cache = NULL;
    }
    av_strlcpy(cached, text, cached_size);
    *cache = kp_text(font, text, color);
    return *cache;
}

static int kp_text_width(TTF_Font *font, const char *text)
{
    int width = 0;
    if (!font || !text || TTF_SizeUTF8(font, text, &width, NULL) < 0)
        return 0;
    return width;
}

/* Return the longest UTF-8 prefix that fits, preferring a natural line
 * break. A hyphen stays at the end of the first line; whitespace does not. */
static size_t kp_title_break(TTF_Font *font, const char *text, int max_width,
                             int *keep_separator)
{
    char candidate[512];
    size_t length = strlen(text), offset = 0, fit = 0, natural = 0;
    int natural_keep = 0;
    while (offset < length && offset + 1 < sizeof(candidate)) {
        size_t next = offset + 1;
        while (next < length &&
               (((unsigned char)text[next] & 0xc0) == 0x80))
            next++;
        memcpy(candidate, text, next);
        candidate[next] = '\0';
        if (kp_text_width(font, candidate) > max_width)
            break;
        fit = next;
        if (text[offset] == ' ' || text[offset] == '\t') {
            natural = offset;
            natural_keep = 0;
        } else if (text[offset] == '-' || text[offset] == '_' ||
                   text[offset] == '.') {
            natural = next;
            natural_keep = 1;
        }
        offset = next;
    }
    if (natural > 0) {
        *keep_separator = natural_keep;
        return natural;
    }
    *keep_separator = 1;
    return fit;
}

static void kp_ellipsize(TTF_Font *font, const char *text, int max_width,
                         char *out, size_t out_size)
{
    char candidate[512];
    size_t length;
    av_strlcpy(out, text, out_size);
    if (kp_text_width(font, out) <= max_width)
        return;
    while ((length = strlen(out)) > 0) {
        do {
            length--;
        } while (length > 0 &&
                 (((unsigned char)out[length] & 0xc0) == 0x80));
        out[length] = '\0';
        snprintf(candidate, sizeof(candidate), "%s...", out);
        if (kp_text_width(font, candidate) <= max_width) {
            av_strlcpy(out, candidate, out_size);
            return;
        }
    }
    av_strlcpy(out, "...", out_size);
}

static void kp_wrap_header_title(
    char lines[KP_HEADER_TITLE_LINES][512], int max_width)
{
    const char *remainder = kp.media_title;
    size_t split;
    int i, keep_separator;
    for (i = 0; i < KP_HEADER_TITLE_LINES; i++)
        lines[i][0] = '\0';
    if (!kp.media_title[0] || !kp.font_small)
        return;
    for (i = 0; i < KP_HEADER_TITLE_LINES && *remainder; i++) {
        while (*remainder == ' ' || *remainder == '\t')
            remainder++;
        if (!*remainder)
            break;
        if (kp_text_width(kp.font_small, remainder) <= max_width) {
            av_strlcpy(lines[i], remainder, sizeof(lines[i]));
            break;
        }
        if (i == KP_HEADER_TITLE_LINES - 1) {
            kp_ellipsize(kp.font_small, remainder, max_width,
                         lines[i], sizeof(lines[i]));
            break;
        }
        keep_separator = 0;
        split = kp_title_break(kp.font_small, remainder, max_width,
                               &keep_separator);
        if (!split) {
            kp_ellipsize(kp.font_small, remainder, max_width,
                         lines[i], sizeof(lines[i]));
            break;
        }
        memcpy(lines[i], remainder, split);
        lines[i][split] = '\0';
        remainder += split;
        if (!keep_separator)
            while (*remainder == ' ' || *remainder == '\t')
                remainder++;
    }
}

static void kp_draw_progress_title(SDL_Surface *surface)
{
    SDL_Color white = {224, 232, 241, 255};
    char title[256];
    SDL_Surface *label;
    if(!kp.media_title[0])return;
    if(!kp.font_small)return;
    kp_ellipsize(kp.font_small,kp.media_title,540,title,sizeof(title));
    label=kp_cached_text(&kp.text_title,kp.cached_title,sizeof(kp.cached_title),
                         kp.font_small,title,white);
    if(label)kp_blit(surface,label,25,395);
}

static void kp_format_time(long seconds, int remaining, char *out, size_t size)
{
    long hours, minutes;
    seconds = FFMAX(seconds, 0);
    hours = seconds / 3600;
    minutes = (seconds % 3600) / 60;
    seconds %= 60;
    if (hours > 0)
        snprintf(out, size, "%s%ld:%02ld:%02ld", remaining ? "-" : "", hours, minutes, seconds);
    else
        snprintf(out, size, "%s%ld:%02ld", remaining ? "-" : "", minutes, seconds);
}

static void kp_draw_progress(SDL_Surface *surface)
{
    SDL_Color soft={214, 224, 234, 255};
    char elapsed[32],remaining[32];
    SDL_Rect track,fill;
    long duration=FFMAX(kp.duration,1);
    long position=av_clip64(kp.position,0,duration);
    kp_format_time(position,0,elapsed,sizeof(elapsed));
    kp_format_time(duration-position,1,remaining,sizeof(remaining));
    /* A restrained translucent overlay, not the bright Kids Mode HUD. */
    mc_dim_rect(surface,0,378,KP_W,KP_H-378,30);
    track.x=24;track.y=432;track.w=KP_W-48;track.h=3;
    SDL_FillRect(surface,&track,SDL_MapRGB(surface->format,73,83,97));
    fill=track;
    fill.w=(int)((int64_t)track.w*position/duration);
    if(fill.w>0)SDL_FillRect(surface,&fill,SDL_MapRGB(surface->format,235,95,99));
    /* A small marker remains legible at both the start and end of the track. */
    {
        int cx=track.x+FFMIN(track.w-1,fill.w);
        Uint32 edge=SDL_MapRGB(surface->format,20,24,32);
        Uint32 dot=SDL_MapRGB(surface->format,248,146,150);
        for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++){
            int squared=dx*dx+dy*dy;
            if(squared<=16){
                SDL_Rect pixel={cx+dx,track.y+1+dy,1,1};
                SDL_FillRect(surface,&pixel,squared>=10?edge:dot);
            }
        }
    }
    SDL_Surface *left=kp_cached_text(&kp.text_elapsed,kp.cached_elapsed,
                                    sizeof(kp.cached_elapsed),kp.font_small,elapsed,soft);
    SDL_Surface *right=kp_cached_text(&kp.text_remaining,kp.cached_remaining,
                                     sizeof(kp.cached_remaining),kp.font_small,remaining,soft);
    if(left)kp_blit(surface,left,24,442);
    if(right)kp_blit(surface,right,KP_W-24-right->w,442);
}

/* Browser and player share the same DejaVu bitmap font for the battery pill. */
static SDL_Surface *mc_menu_cached(SDL_Surface **cache, char *previous,
                                   size_t capacity, const char *text, SDL_Color ink);
static void kp_draw_battery(SDL_Surface *surface)
{
    int low = kp.low_battery_until &&
              !SDL_TICKS_PASSED(SDL_GetTicks(),kp.low_battery_until);
    const int x = low ? KP_W-190 : KP_W-126, y = 12, w = low?178:114, h = 38;
    const int icon_w = 30, gap = 10;
    SDL_Color white = {244, 246, 250, 255};
    SDL_Surface *label;
    SDL_Rect area, ink = {0, 0, 0, 0};
    char text[16];
    Uint32 frame = SDL_MapRGB(surface->format, 138, 152, 170);
    Uint32 panel = SDL_MapRGB(surface->format, 31, 40, 54);
    Uint32 green = SDL_MapRGB(surface->format, 123, 206, 166);
    Uint32 red = SDL_MapRGB(surface->format, 251, 86, 89);
    int inside = 0, content_x, icon_y;
    if (kp.battery >= 0) {
        inside = kp.battery * 21 / 100;
        if (kp.battery > 0 && inside == 0) inside = 1;
        if (low) snprintf(text, sizeof(text), "LOW %d%%", kp.battery);
        else snprintf(text, sizeof(text), "%d%%", kp.battery);
    } else {
        av_strlcpy(text, "--%", sizeof(text));
    }
    label = mc_menu_cached(&kp.text_battery, kp.cached_battery,
                           sizeof(kp.cached_battery), text,
                           low?(SDL_Color){251,86,89,255}:white);
    if (label) ink = kp_text_ink_bounds(label);
    /* Center the whole icon + text group, not just the text vertically. */
    content_x = x + (w - icon_w - gap - ink.w) / 2;
    icon_y = y + (h - 16) / 2;
    area = (SDL_Rect){x, y, w, h};
    SDL_FillRect(surface, &area, panel);
    area = (SDL_Rect){content_x, icon_y, 27, 16};
    SDL_FillRect(surface, &area, frame);
    area = (SDL_Rect){content_x + 2, icon_y + 2, 23, 12};
    SDL_FillRect(surface, &area, panel);
    area = (SDL_Rect){content_x + 27, icon_y + 5, 3, 6};
    SDL_FillRect(surface, &area, frame);
    if (kp.battery >= 0) {
        area = (SDL_Rect){content_x + 3, icon_y + 3, inside, 10};
        SDL_FillRect(surface, &area, kp.battery <= 15 ? red : green);
    }
    if (label && ink.w && ink.h) {
        SDL_Rect dst = {content_x + icon_w + gap,
                        y + (h - ink.h) / 2, ink.w, ink.h};
        SDL_BlitSurface(label, &ink, surface, &dst);
    }
}

/* ---- Live stats ---------------------------------------------------------
 * Diagnostic panel (top-left). Values are sampled every 500 ms from data the
 * player already has; only /proc/stat and cpufreq (RAM-backed) are read.
 * Text surfaces are re-rendered only when a line actually changes. */
static int mc_stats_read_cpu(unsigned long long *busy, unsigned long long *total)
{
    unsigned long long v[8] = {0};
    FILE *fp = fopen("/proc/stat", "r");
    int ok;
    if (!fp) return 0;
    ok = fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) >= 4;
    fclose(fp);
    if (!ok) return 0;
    *total = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
    *busy = *total - v[3] - v[4];            /* minus idle and iowait */
    return 1;
}
static int mc_stats_cpu_mhz(void)
{
    long khz = 0;
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (!fp) return -1;
    if (fscanf(fp, "%ld", &khz) != 1) khz = -1000;
    fclose(fp);
    return (int)(khz / 1000);
}
static void mc_stats_bytes(char *out, size_t cap, int bytes)
{
    if (bytes >= 1024 * 1024) snprintf(out, cap, "%.1fM", bytes / 1048576.0);
    else snprintf(out, cap, "%dK", bytes / 1024);
}
static void mc_stats_update(VideoState *is, uint32_t now)
{
    char (*line)[64] = kp.stats_line;
    const char *perf = getenv("MC_PERF_MODE");
    const char *scaler = getenv("MC_VIDEO_SCALER");
    int drops = is->frame_drops_early + is->frame_drops_late;
    unsigned long long busy, total;
    char vq[16], aq[16];
    int mhz;

    for (int i = 0; i < MC_STATS_LINES; i++) line[i][0] = 0;
    if (is->video_st) {
        AVCodecParameters *par = is->video_st->codecpar;
        AVRational fr = av_guess_frame_rate(is->ic, is->video_st, NULL);
        int lowres = is->viddec.avctx ? is->viddec.avctx->lowres : 0;
        snprintf(line[0], 64, "%dx%d %s %.2ffps%s", par->width, par->height,
                 avcodec_get_name(par->codec_id), fr.num && fr.den ? av_q2d(fr) : 0.0,
                 lowres ? (lowres == 1 ? " lowres/2" : " lowres/4") : "");
    } else {
        av_strlcpy(line[0], "Audio only", 64);
    }
    snprintf(line[1], 64, "%s / %s", perf && *perf ? perf : "balanced",
             scaler && *scaler ? scaler : "bicubic");

    /* Drops per second over the last sample window. */
    if (kp.stats_last_ticks && now > kp.stats_last_ticks)
        kp.stats_drop_rate = (int)((drops - kp.stats_last_drops) * 1000LL /
                                   (now - kp.stats_last_ticks));
    kp.stats_last_drops = drops;
    kp.stats_last_ticks = now;
    snprintf(line[2], 64, "Dropped %d  (%d/s)", drops, kp.stats_drop_rate);

    if (is->audio_st && is->video_st) {
        double diff = get_clock(&is->audclk) - get_clock(&is->vidclk);
        if (isnan(diff)) av_strlcpy(line[3], "A-V  --", 64);
        else snprintf(line[3], 64, "A-V %+.3fs", diff);
    } else {
        av_strlcpy(line[3], "A-V  n/a", 64);
    }
    mc_stats_bytes(vq, sizeof(vq), is->videoq.size);
    mc_stats_bytes(aq, sizeof(aq), is->audioq.size);
    snprintf(line[4], 64, "Buffer V %s  A %s", vq, aq);
    snprintf(line[6], 64, "Overlay: %.32s", mc_so.active ? kp.screen_overlay : "Off");

    if (mc_stats_read_cpu(&busy, &total)) {
        if (kp.stats_cpu_total && total > kp.stats_cpu_total)
            kp.stats_cpu_percent = (int)((busy - kp.stats_cpu_busy) * 100 /
                                         (total - kp.stats_cpu_total));
        kp.stats_cpu_busy = busy;
        kp.stats_cpu_total = total;
    }
    mhz = mc_stats_cpu_mhz();
    if (mhz > 0) snprintf(line[5], 64, "CPU %d%% @ %d MHz", kp.stats_cpu_percent, mhz);
    else snprintf(line[5], 64, "CPU %d%%", kp.stats_cpu_percent);
}
static void mc_stats_tick(VideoState *is, uint32_t now)
{
    if (!kp.stats_on || kp.power_blanked || !SDL_TICKS_PASSED(now, kp.stats_next)) return;
    kp.stats_next = now + 500;
    mc_stats_update(is, now);
    for (int i = 0; i < MC_STATS_LINES; i++)
        if (strcmp(kp.stats_line[i], kp.stats_cached[i])) {
            /* Playing video redraws on the next frame anyway; paused video
             * needs a refresh to show the new numbers. */
            if (is->paused && !kp.menu_open && !kp.help_open)
                kp_reupload_shown_frame(is);
            break;
        }
}
static void mc_stats_set(VideoState *is, int on)
{
    kp.stats_on = on;
    kp.stats_next = 0;               /* sample immediately */
    kp.stats_last_ticks = 0;
    kp.stats_cpu_total = 0;          /* first CPU % appears after 500 ms */
    kp.stats_cpu_percent = 0;
    if (!on)
        for (int i = 0; i < MC_STATS_LINES; i++)
            if (kp.stats_text[i]) {
                SDL_FreeSurface(kp.stats_text[i]);
                kp.stats_text[i] = NULL;
                kp.stats_cached[i][0] = 0;
            }
    kp_reupload_shown_frame(is);
}
static void mc_stats_draw(SDL_Surface *surface)
{
    const SDL_Color ink = {235, 239, 245, 255};
    const int x = 12, y = 12, pad = 10, step = UI_SMALL_H + 4;
    SDL_Surface *rows[MC_STATS_LINES];
    int width = 0, count = 0;
    for (int i = 0; i < MC_STATS_LINES; i++) {
        rows[i] = NULL;
        if (!kp.stats_line[i][0]) continue;
        rows[i] = mc_menu_cached(&kp.stats_text[i], kp.stats_cached[i],
                                 sizeof(kp.stats_cached[i]), kp.stats_line[i], ink);
        if (rows[i]) { width = FFMAX(width, rows[i]->w); count++; }
    }
    if (!count) return;
    mc_dim_rect(surface, x, y, width + pad * 2, count * step + pad * 2 - 4, 29);
    for (int i = 0, row = 0; i < MC_STATS_LINES; i++)
        if (rows[i]) kp_blit(surface, rows[i], x + pad, y + pad + row++ * step);
}

static void kp_notice(VideoState *is, const char *message);
static SDL_Surface *mc_help_load(void){
    FILE *fp=fopen("/mnt/SDCARD/App/MiniCinema/help/controls.png","rb");
    unsigned char h[29];long len;
    SDL_Surface *original,*converted;
    if(!fp)return NULL;
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return NULL;}
    len=ftell(fp);
    rewind(fp);
    if(len<=0 || len>2*1024*1024 || fread(h,1,sizeof(h),fp)!=sizeof(h)){
        fclose(fp);return NULL;
    }
    fclose(fp);
    if(memcmp(h,"\x89PNG\r\n\x1a\n",8) ||
       h[16]!=0 || h[17]!=0 || h[18]!=2 || h[19]!=128 ||
       h[20]!=0 || h[21]!=0 || h[22]!=1 || h[23]!=224 ||
       h[24]!=8 || (h[25]!=2 && h[25]!=6) ||
       h[26]!=0 || h[27]!=0 || h[28]!=0)return NULL;
    original=IMG_Load("/mnt/SDCARD/App/MiniCinema/help/controls.png");
    if(!original)return NULL;
    if(original->w!=KP_W || original->h!=KP_H){SDL_FreeSurface(original);return NULL;}
    converted=SDL_ConvertSurfaceFormat(original,SDL_PIXELFORMAT_ARGB8888,0);
    SDL_FreeSurface(original);
    if(converted)SDL_SetSurfaceBlendMode(converted,SDL_BLENDMODE_BLEND);
    return converted;
}
static void mc_help_open(VideoState *is){
    SDL_Surface *img=mc_help_load();
    if(!img){kp_notice(is,"Help image not found");return;}
    kp.help_image=img;
    kp.help_resume_after=!is->paused;
    if(kp.help_resume_after)kp_pause(is);
    kp.pause_activity_at=SDL_GetTicks();
    kp.help_open=1;
    kp_reupload_shown_frame(is);
}
static void mc_help_close(VideoState *is){
    kp.help_open=0;
    if(kp.help_image){SDL_FreeSurface(kp.help_image);kp.help_image=NULL;}
    if(kp.help_resume_after && is->paused)kp_resume(is);
    kp.help_resume_after=0;
    kp.pause_activity_at=SDL_GetTicks();
    kp_reupload_shown_frame(is);
}
static void kp_draw_options(SDL_Surface *surface, VideoState *is);
static int kp_capture_preview(int replace, int level);

static int kp_overlay_visible(VideoState *is)
{
    uint32_t now = SDL_GetTicks();
    return kp.menu_open || kp.help_open || kp.stats_on ||
           (kp.low_battery_until && !SDL_TICKS_PASSED(now,kp.low_battery_until)) ||
           mc_srt_active(is)>=0 || mc_embedded_active(is) ||
           (!kp.osd_hidden && (kp.audio_only ||
                                 now < kp.progress_until || now < kp.seek_notice_until)) ||
           kp.battery_held;
}

static void kp_draw_overlay(SDL_Surface *surface, VideoState *is)
{
    SDL_Color white = {235, 239, 245, 255};
    uint32_t now = SDL_GetTicks();
    if(kp.help_open){
        SDL_Rect area={0,0,KP_W,KP_H};
        SDL_FillRect(surface,&area,SDL_MapRGB(surface->format,10,14,21));
        if(kp.help_image)SDL_BlitSurface(kp.help_image,NULL,surface,NULL);
        return;
    }
    /* Explicit pause/seek/progress requests remain visible regardless of the
     * startup setting, including before the clock has settled. */
    if (!kp.menu_open && !kp.osd_hidden && (kp.audio_only ||
        now < kp.progress_until || now < kp.seek_notice_until)) {
        kp_draw_progress(surface);
        if (!kp.audio_only) kp_draw_progress_title(surface);
    }
    if (!kp.menu_open) mc_srt_draw(surface,is);
    if (kp.stats_on && !kp.menu_open &&
        !(kp.notice_seek && !kp.seek_forward && now < kp.seek_notice_until))
        mc_stats_draw(surface);
    if (kp.menu_open) kp_draw_options(surface, is);
    if(kp.battery_held || (kp.low_battery_until &&
        !SDL_TICKS_PASSED(now,kp.low_battery_until)))
        kp_draw_battery(surface);
    if(is->muted && !kp.menu_open && !kp.osd_hidden &&
       (kp.audio_only || now<kp.progress_until)){
        SDL_Surface *muted=mc_menu_cached(&kp.text_muted,kp.cached_muted,
            sizeof(kp.cached_muted),"MUTED",(SDL_Color){251,86,89,255});
        if(muted)kp_blit(surface,muted,470,443);
    }
    /* Draw the transient seek/aspect/subtitle notice above other OSD items. */
    if (!kp.menu_open && !kp.osd_hidden && kp.notice[0] && now < kp.seek_notice_until) {
        SDL_Surface *notice = kp_cached_text(
            &kp.text_notice, kp.cached_notice, sizeof(kp.cached_notice),
            kp.font_small, kp.notice, white);
        if (notice) {
            SDL_Rect ink = kp_text_ink_bounds(notice);
            if (ink.w > 0 && ink.h > 0) {
                /* Center the visible pixels, not the SDL_ttf line box. The
                 * latter has asymmetric empty space around many glyphs. */
                const int margin = 12;
                const int padding_x = 12;
                const int panel_y = 12;
                const int panel_h = FFMAX(38, ink.h + 12);
                const int panel_w = ink.w + padding_x * 2;
                const int panel_x = kp.notice_seek && !kp.seek_forward ?
                                    margin : KP_W - margin - panel_w;
                SDL_Rect dst = {panel_x + padding_x,
                                panel_y + (panel_h - ink.h) / 2,
                                ink.w, ink.h};
                mc_dim_rect(surface, panel_x, panel_y, panel_w, panel_h, 29);
                SDL_BlitSurface(notice, &ink, surface, &dst);
            }
        }
    }

}

/* Copy the clean, freshly uploaded pixels while the texture is already
 * locked for composition. This avoids an extra lock and full-frame restore
 * when the OSD/subtitle is drawn on a newly decoded frame. */
static void kp_capture_clean_pixels(const void *pixels, int pitch)
{
    size_t size = (size_t)KP_W * KP_H * 4;
    int y;
    if (!pixels) return;
    if (kp.clean_capacity < size) {
        uint8_t *grown = av_realloc(kp.clean_pixels, size);
        if (!grown) { kp.clean_ready = 0; return; }
        kp.clean_pixels = grown;
        kp.clean_capacity = size;
    }
    for (y = 0; y < KP_H; y++)
        memcpy(kp.clean_pixels + y * KP_W * 4,
               (const uint8_t *)pixels + y * pitch, KP_W * 4);
    kp.clean_pitch = KP_W * 4;
    kp.clean_ready = 1;
}

/* A freshly uploaded decoder texture is already clean. Only copy its
 * pixels if an overlay will actually be drawn or a still frame needs an
 * untouched backing image. Do not copy every unobstructed movie frame. */
static void kp_capture_uploaded_texture(SDL_Texture *texture)
{
    static int playback_frame_seen;
    (void)texture;
    kp.frame_fresh=1;
    if (!playback_frame_seen) {
        const char *marker = getenv("MC_FRAME_OK_FILE");
        if (marker && marker[0]) kp_touch(marker, 1);
        playback_frame_seen = 1;
    }
}

/* Called immediately after SDL_RenderPresent(). Do not lock that texture
 * again while the custom renderer may still be presenting it. Merely arm the
 * OSD here; the next decoded frame will be uploaded, captured and decorated
 * in the normal order. */
static void kp_after_present(VideoState *is)
{
    if (!kp.initialized || kp.audio_only || !kp.startup_osd_pending)
        return;
    kp.startup_osd_pending = 0;
    kp.clock_ready_after = SDL_GetTicks() + 1800;
    /* On: show as soon as the first frame is presented (times may settle).
     * Off: no automatic progress OSD; A/pause/seek can still show it. */
    kp.progress_until = kp.startup_osd_immediate ? SDL_GetTicks() + mc_osd_ms() : 0;
    if (is)
        is->force_refresh = 1;
}

/* The clean decoder frame is still in pictq. If the old texture has a
 * decoration but no clean backup, upload before composition, not after
 * drawing has started. In particular, never present a stale options menu. */
static int kp_texture_needs_clean_upload(VideoState *is)
{
    return !kp.clean_ready && (kp.so_baked || kp.overlay_drawn) &&
           (kp_overlay_visible(is) || kp.overlay_drawn || kp.so_baked != mc_so.active);
}

static int kp_compose_video(VideoState *is, SDL_Texture *texture)
{
    void *pixels=NULL;
    int pitch=0,y,bpp=0;
    int visible,fresh,capture_due=0;
    int so=mc_so.active;
    Uint32 rmask,gmask,bmask,amask;
    SDL_Surface *surface;
    /* No event/power polling after upload: a wake or sleep must not change
     * the frame state halfway through its presentation. */
    visible=kp_overlay_visible(is);
    fresh=kp.frame_fresh;
    kp.frame_fresh=0;
    if(fresh){
        kp.overlay_drawn=0;
        kp.so_baked=0;
        kp.so_reupload=0;
        if(kp.auto_preview_pending && !is->paused){
            double pos=get_master_clock(is);
            capture_due=!isnan(pos) && pos>=kp.auto_preview_seconds;
        }
        if(!visible && !capture_due && !kp.x_preview_pending && !so){
            kp.clean_ready=0;return 1;
        }
        kp.clean_ready=0;
    } else {
        if(!visible && !kp.overlay_drawn && kp.so_baked==so)return 1;
        if(!kp.clean_ready && (kp.so_baked || kp.overlay_drawn)){
            /* Do not schedule this refresh inside video_display(): FFplay
             * clears force_refresh on its return, losing the request while
             * paused. The next outer-loop tick schedules a clean upload. */
            kp.so_reupload=1;
            return 0; /* Never present or snapshot a stale decorated texture. */
        }
    }
    if(!texture || SDL_LockTexture(texture,NULL,&pixels,&pitch)<0){
        kp.so_reupload=1;
        return 0;
    }
    if(fresh){
        if(visible || capture_due || kp.x_preview_pending){
            kp_capture_clean_pixels(pixels,pitch);
            if(kp.x_preview_pending && kp.clean_ready){
                int result;
                kp.x_preview_pending=0;
                result=kp_capture_preview(1,3);
                if(result==1)kp.preview_exists=1;
                kp_notice(is,result==1?"Preview saved":"Preview unavailable");
            }
            if(capture_due && kp.clean_ready){
                kp.auto_preview_pending=0;
                if(kp_capture_preview(0,1)==1)kp.preview_exists=1;
            }
        }
    } else if(kp.clean_ready){
        for(y=0;y<KP_H;y++)
            memcpy((uint8_t*)pixels+y*pitch,
                   kp.clean_pixels+y*kp.clean_pitch,KP_W*4);
    } else if(visible){
        kp_capture_clean_pixels(pixels,pitch);
    }
    if(so){mc_so_draw((uint8_t*)pixels,pitch);kp.so_baked=1;}
    else kp.so_baked=0;
    if(visible && kp.clean_ready &&
       SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ARGB8888,&bpp,
                                   &rmask,&gmask,&bmask,&amask)){
        surface=SDL_CreateRGBSurfaceFrom(pixels,KP_W,KP_H,bpp,pitch,
                                         rmask,gmask,bmask,amask);
        if(surface){kp_draw_overlay(surface,is);SDL_FreeSurface(surface);}
    }
    kp.overlay_drawn=visible;
    if(!visible)kp.clean_ready=0;
    SDL_UnlockTexture(texture);
    return 1;
}

static void kp_draw_scaled_artwork(SDL_Surface *target)
{
    SDL_Rect src, dst;
    int max_w = 300, max_h = 300;
    if (!kp.artwork && kp.artwork_file[0])
        kp.artwork = IMG_Load(kp.artwork_file);
    if (!kp.artwork)
        return;
    src.x = src.y = 0;
    src.w = kp.artwork->w;
    src.h = kp.artwork->h;
    dst.w = max_w;
    dst.h = (int)((int64_t)kp.artwork->h * max_w / FFMAX(kp.artwork->w, 1));
    if (dst.h > max_h) {
        dst.h = max_h;
        dst.w = (int)((int64_t)kp.artwork->w * max_h / FFMAX(kp.artwork->h, 1));
    }
    dst.x = (KP_W - dst.w) / 2;
    dst.y = 45 + (max_h - dst.h) / 2;
    SDL_BlitScaled(kp.artwork, &src, target, &dst);
}

static void kp_audio_display(VideoState *is)
{
    SDL_Color white = {255, 255, 255, 255};
    int title_w = 0;
    if (kp.power_blanked) return;
    if (!kp.audio_canvas) {
        kp.audio_canvas = SDL_CreateRGBSurfaceWithFormat(0, KP_W, KP_H, 32, SDL_PIXELFORMAT_ARGB8888);
        kp.audio_base = SDL_CreateRGBSurfaceWithFormat(0, KP_W, KP_H, 32,
                                                       SDL_PIXELFORMAT_ARGB8888);
        kp.audio_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, KP_W, KP_H);
        if (kp.audio_base) {
            SDL_FillRect(kp.audio_base, NULL,
                         SDL_MapRGB(kp.audio_base->format, 0, 0, 0));
            kp_draw_scaled_artwork(kp.audio_base);
            {
                SDL_Surface *title = kp_cached_text(
                    &kp.text_title, kp.cached_title, sizeof(kp.cached_title),
                    kp.font_title, kp.media_title, white);
                if (title) {
                    title_w = title->w;
                    kp_blit(kp.audio_base, title,
                            FFMAX((KP_W - title_w) / 2, 8), 360);
                }
            }
        }
    }
    if (!kp.audio_canvas || !kp.audio_base || !kp.audio_texture)
        return;
    SDL_BlitSurface(kp.audio_base, NULL, kp.audio_canvas, NULL);
    kp_draw_overlay(kp.audio_canvas, is);
    SDL_UpdateTexture(kp.audio_texture, NULL, kp.audio_canvas->pixels, kp.audio_canvas->pitch);
    SDL_RenderCopy(renderer, kp.audio_texture, NULL, NULL);
}

/* FFplay's video thread already watches vfilter_idx and safely rebuilds its
 * filter graph after an index change.  Every configured mode produces an
 * actual 640x480 texture: this Miyoo SDL2 backend cannot scale in RenderCopy.
 * A short announcement uses the same top-right pill as seek notifications. */
static void kp_cycle_aspect(VideoState *is)
{
#if CONFIG_AVFILTER
    static const char *names[] = { "Fit", "Fill", "Stretch", "Original" };
    unsigned next;
    if (!is || is->video_stream < 0 || nb_vfilters != FF_ARRAY_ELEMS(names))
        return;
    next = ((unsigned)is->vfilter_idx + 1) % FF_ARRAY_ELEMS(names);
    kp.notice_seek = 0;
    kp.osd_hidden = 0;
    is->vfilter_idx = (int)next;
    if (kp.menu_open) {
        snprintf(kp.menu_feedback, sizeof(kp.menu_feedback), "Aspect: %s", names[next]);
        kp.menu_feedback_until = SDL_GetTicks() + mc_osd_ms();
        kp.seek_notice_until = 0;
        kp.progress_until = 0;
    } else {
        snprintf(kp.notice, sizeof(kp.notice), "Aspect: %s", names[next]);
        kp.seek_notice_until = SDL_GetTicks() + mc_osd_ms();
        kp.progress_until = SDL_GetTicks() + mc_osd_ms();
    }
    is->force_refresh = 1;
#endif
}

static void kp_set_notice(int forward, long step)
{
    kp.osd_hidden = 0;
    kp.seek_forward = forward;
    kp.notice_seek = 1;
    snprintf(kp.notice, sizeof(kp.notice), "%c%ld%s", forward ? '+' : '-',
             step >= 60 ? step / 60 : step, step >= 60 ? "m" : "s");
    kp.seek_notice_until = kp.progress_until = SDL_GetTicks() + mc_osd_ms();
}

static void kp_seek(VideoState *is, long delta, long label)
{
    double pos = get_master_clock(is);
    if (isnan(pos))
        pos = (double)is->seek_pos / AV_TIME_BASE;
    pos = FFMAX(pos + delta, 0);
    if (kp.duration > 0 && pos >= kp.duration - 2) {
        /* Leave a few seconds of decodable video. A seek to the end must
         * not mark a video DONE or trigger automatic episode advancement. */
        pos = FFMAX(kp.duration - 4, 0);
        if(kp.seek_end_file[0])kp_touch(kp.seek_end_file,1);
    } else if(kp.seek_end_file[0]) {
        kp_touch(kp.seek_end_file,0);
    }
    stream_seek(is, (int64_t)(pos * AV_TIME_BASE), delta * AV_TIME_BASE, 0);
    kp.position = pos;
    if (kp.remember_resume) {
        kp_write_number(kp.position_file, kp.position);
        kp_write_checkpoint(kp.position);
    }
    kp_set_notice(delta >= 0, label);
}

/* UP/DOWN: tap for five minutes; hold 650 ms then repeatedly jump
 * ten minutes every 700 ms until released. Never change episodes. */
static void kp_held_vertical_seek(VideoState *is)
{
    uint32_t now = SDL_GetTicks();
    if (kp.vertical_key && !kp.menu_open &&
        now - kp.vertical_at >= 650 &&
        (!kp.vertical_done || now - kp.vertical_repeat_at >= 700)) {
        int key = kp.vertical_key;
        kp.vertical_done = 1;
        kp.vertical_repeat_at = now;
        kp_seek(is, key == SDLK_UP ? 600 : -600, 600);
        is->force_refresh = 1;
    }
}

static void kp_progressive_seek(VideoState *is)
{
    uint32_t now, held;
    long step;
    if (!kp.seek_key)
        return;
    now = SDL_GetTicks();
    if (kp.seek_last && now - kp.seek_last < 450)
        return;
    held = now - kp.seek_at;
    step = held < 1400 ? 10 : held < 3200 ? 30 : 60;
    kp_seek(is, kp.seek_forward ? step : -step, step);
    kp.seek_last = now;
}

/* Lossless 320x240 preview. The Sub filter and fast zlib shrink SD writes.
 * Capture samples clean video pixels only, never OSD/subtitle artwork. */
static uint32_t mc_png_crc(uint32_t crc, const uint8_t *bytes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (crc & 1u ? 0xedb88320u : 0u);
    }
    return crc;
}
static void mc_put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);p[3] = (uint8_t)v;
}
static int mc_write_all(int fd, const uint8_t *data, size_t n)
{
    while (n) {
        ssize_t wrote = write(fd, data, n);
        if (wrote <= 0) return -1;
        data += wrote;
        n -= (size_t)wrote;
    }
    return 0;
}
static int mc_png_chunk(int fd, const char type[4], const uint8_t *data, uint32_t n)
{
    uint8_t be[4];
    uint32_t crc = mc_png_crc(0xffffffffu, (const uint8_t *)type, 4);
    if (data && n) crc = mc_png_crc(crc, data, n);
    mc_put_be32(be, n);
    if (mc_write_all(fd, be, 4) < 0 ||
        mc_write_all(fd, (const uint8_t *)type, 4) < 0 ||
        (n && mc_write_all(fd, data, n) < 0)) return -1;
    mc_put_be32(be, crc ^ 0xffffffffu);
    return mc_write_all(fd, be, 4);
}
typedef int (*mc_compress2_fn)(unsigned char *, unsigned long *,
                                const unsigned char *, unsigned long, int);
typedef unsigned long (*mc_bound_fn)(unsigned long);

/* Portable zlib stream using stored blocks when libz is unavailable. */
static uint8_t *mc_store_deflate(const uint8_t *raw, unsigned long n,
                                 unsigned long *out_size)
{
    unsigned long blocks = (n + 65534u) / 65535u;
    unsigned long size = 2 + n + blocks * 5 + 4, off = 0, out = 2;
    uint8_t *z = av_malloc(size);
    uint32_t a = 1, b = 0;
    if (!z) return NULL;
    z[0] = 0x78; z[1] = 0x01;
    while (off < n) {
        unsigned count = FFMIN(65535u, n - off);
        z[out++] = off + count == n ? 1 : 0;
        z[out++] = (uint8_t)count; z[out++] = (uint8_t)(count >> 8);
        z[out++] = (uint8_t)~count; z[out++] = (uint8_t)(~count >> 8);
        memcpy(z + out, raw + off, count);
        out += count; off += count;
    }
    for (unsigned long i = 0; i < n; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    mc_put_be32(z + out, (b << 16) | a);
    *out_size = out + 4;
    return z;
}

/* zlib is normally present with SDL2_ttf; avoid an added hard dependency.
 * Only attempt the dynamic load once, and fall back to stored blocks. */
static uint8_t *mc_deflate(const uint8_t *raw, unsigned long n, int level,
                           unsigned long *out_size)
{
    static void *zlib;
    static int attempted;
    static mc_compress2_fn compress2_fn;
    static mc_bound_fn bound_fn;
    if (!attempted) {
        attempted = 1;
        zlib = dlopen("libz.so.1", RTLD_NOW | RTLD_LOCAL);
        if (zlib) {
            compress2_fn = (mc_compress2_fn)dlsym(zlib, "compress2");
            bound_fn = (mc_bound_fn)dlsym(zlib, "compressBound");
        }
    }
    if (compress2_fn && bound_fn) {
        unsigned long cap = bound_fn(n);
        uint8_t *z = av_malloc(cap);
        if (z && compress2_fn(z, &cap, raw, n, level) == 0) {
            *out_size = cap;
            return z;
        }
        av_free(z);
    }
    return mc_store_deflate(raw, n, out_size);
}

/* replace=0 refuses an existing image, replace=1 atomically replaces it.
 * Automatic capture uses level 1; manual capture uses level 3. */
static int kp_capture_preview(int replace, int level)
{
    const unsigned width = 320, height = 240;
    const unsigned long raw_size = height * (1 + width * 3);
    static const uint8_t signature[] = {137,80,78,71,13,10,26,10};
    uint8_t *raw = NULL, *z = NULL, ihdr[13] = {0};
    unsigned long z_size = 0;
    char parent[1024], temporary[1056];
    int fd = -1, okay = 0;
    if (!kp.clean_ready || !kp.preview_file[0] ||
        strlen(kp.preview_file) >= sizeof(parent)) return 0;
    if (!replace && access(kp.preview_file, F_OK) == 0) return 2;
    av_strlcpy(parent, kp.preview_file, sizeof(parent));
    char *slash = strrchr(parent, '/');
    if (!slash) return 0;
    *slash = 0;
    if (mkdir(parent, 0755) < 0 && errno != EEXIST) return 0;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", kp.preview_file) >=
        (int)sizeof(temporary)) return 0;
    raw = av_malloc(raw_size);
    if (!raw) goto end_capture;
    unsigned long k = 0;
    for (unsigned y = 0; y < height; y++) {
        uint8_t pr = 0, pg = 0, pb = 0;
        raw[k++] = 1; /* PNG Sub row filter */
        for (unsigned x = 0; x < width; x++) {
            const uint8_t *pixel = kp.clean_pixels + (y * 2 * KP_W + x * 2) * 4;
            uint8_t r = pixel[2], g = pixel[1], b = pixel[0];
            raw[k++] = (uint8_t)(r - pr);
            raw[k++] = (uint8_t)(g - pg);
            raw[k++] = (uint8_t)(b - pb);
            pr = r; pg = g; pb = b;
        }
    }
    z = mc_deflate(raw, raw_size, level, &z_size);
    if (!z) goto end_capture;
    mc_put_be32(ihdr, width);
    mc_put_be32(ihdr + 4, height);
    ihdr[8] = 8; ihdr[9] = 2; /* 8-bit RGB */
    unlink(temporary); /* Remove a stale interrupted capture. */
    fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) goto end_capture;
    if (mc_write_all(fd, signature, sizeof(signature)) == 0 &&
        mc_png_chunk(fd, "IHDR", ihdr, sizeof(ihdr)) == 0 &&
        mc_png_chunk(fd, "IDAT", z, (uint32_t)z_size) == 0 &&
        mc_png_chunk(fd, "IEND", NULL, 0) == 0) {
        int closed = close(fd);
        fd = -1;
        /* Even in the unlikely event of another writer racing us, automatic
         * capture must not overwrite an existing preview. */
        if (closed == 0 && (replace || access(kp.preview_file, F_OK) != 0))
            okay = rename(temporary, kp.preview_file) == 0;
    }
end_capture:
    if (fd >= 0) close(fd);
    if (!okay) unlink(temporary);
    av_free(raw); av_free(z);
    return okay;
}

static int kp_save_screenshot(void)
{
    int bpp = 0;
    Uint32 rmask, gmask, bmask, amask;
    SDL_Surface *surface;
    int result;
    if (!kp.clean_ready || !kp.screenshot_file[0] ||
        !SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ARGB8888, &bpp,
                                    &rmask, &gmask, &bmask, &amask))
        return 0;
    surface = SDL_CreateRGBSurfaceFrom(kp.clean_pixels, KP_W, KP_H, bpp,
                                       kp.clean_pitch, rmask, gmask, bmask, amask);
    if (!surface)
        return 0;
    result = SDL_SaveBMP(surface, kp.screenshot_file) == 0;
    SDL_FreeSurface(surface);
    return result;
}

/* Player main menu plus a dedicated subtitle page; one raw ID per action. */
#define MC_OPT_COUNT 20
#define MC_OPT_CHAPTERS 19
#define MC_OPT_STATS 18
#define MC_OPT_OVERLAY 17
#define MC_OPT_HELP 16
#define MC_OPT_VIDEO_PAGE 12
#define MC_OPT_ASPECT 13
#define MC_OPT_SUBTITLE_PAGE 14
#define MC_OPT_BACK 15
static const char *mc_option_name(int index)
{
    static const char *options[] = {
        "Audio track", "Audio delay", "Subtitles", "Subtitle track",
        "Subtitle delay", "Subtitle size", "Subtitle position", "Auto next video",
        "Previous video", "Next video", "Restart video", "Capture preview", "Video actions",
        "Aspect ratio", "Subtitles", "Back to playback", "Controls help",
        "Screen overlay", "Live stats", "Chapters"
    };
    if (index == 11 && kp.preview_exists) return "Replace preview";
    return (unsigned)index < FF_ARRAY_ELEMS(options) ? options[index] : "";
}

static void kp_notice(VideoState *is, const char *message)
{
    kp.osd_hidden = 0;
    kp.notice_seek = 0;
    if (kp.menu_open) {
        av_strlcpy(kp.menu_feedback, message, sizeof(kp.menu_feedback));
        kp.menu_feedback_until = SDL_GetTicks() + mc_osd_ms();
    } else {
        av_strlcpy(kp.notice, message, sizeof(kp.notice));
        kp.seek_notice_until = kp.progress_until = SDL_GetTicks() + mc_osd_ms();
        }
    is->force_refresh = 1;
}

static void mc_refresh_srt_font(VideoState *is)
{
    int font_size = kp.subtitle_size == 0 ? 20 : kp.subtitle_size == 2 ? 29 : 24;
    TTF_Font *new_font = NULL;
    if (kp.srt_font_path[0])
        new_font = TTF_OpenFont(kp.srt_font_path, font_size);
    if (new_font) {
        if (kp.srt_font && kp.srt_font != kp.font_small)
            TTF_CloseFont(kp.srt_font);
        kp.srt_font = new_font;
    }
    mc_srt_invalidate();
    is->force_refresh = 1;
}

static int mc_player_neighbor(VideoState *is, int next)
{
    const char *available = getenv(next ? "MC_HAS_NEXT" : "MC_HAS_PREV");
    const char *marker = getenv(next ? "MC_NEXT_FILE" : "MC_PREV_FILE");
    if (!available || strcmp(available, "1") || !marker || !marker[0]) {
        kp_notice(is, next ? "No next video" : "No previous video");
        return 0;
    }
    kp_save_position(is, 1);
    kp_touch(marker, 1);
    do_exit(is);
    return 1;
}

static void mc_audio_label(VideoState *is, char *dst, size_t cap)
{
    int ordinal = 0, total = 0;
    const char *lang = NULL;
    if (is->ic) {
        for (unsigned i = 0; i < is->ic->nb_streams; ++i) {
            AVStream *st = is->ic->streams[i];
            if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                total++;
                if ((int)i == is->audio_stream) {
                    ordinal = total;
                    AVDictionaryEntry *entry = av_dict_get(st->metadata, "language", NULL, 0);
                    if (entry) lang = entry->value;
                }
            }
        }
    }
    if (!ordinal) snprintf(dst, cap, "Audio: N/A");
    else if (lang && strcmp(lang, "und") && strcmp(lang, "undefined"))
        snprintf(dst, cap, "Audio: %d/%d %.6s", ordinal, total, lang);
    else snprintf(dst, cap, "Audio: %d/%d", ordinal, total);
}

/* Overlay names are shown as values, not hidden behind a tiny fixed limit. */
static void mc_overlay_value(const char *name, char *out, size_t cap)
{
    size_t n = 0, limit = 345, width = 0;
    if (!name || !*name) name = "Off";
    while (name[n] && n + 4 < cap) {
        unsigned char ch = (unsigned char)name[n];
        unsigned advance = ch >= 32 && ch <= 126 ? ui_small_advance[ch-32] :
                           ui_small_advance['?'-32];
        if (width + advance + (name[n+1] ? 3 * ui_small_advance['.'-32] : 0) > limit)
            break;
        width += advance;
        out[n] = name[n];
        n++;
    }
    if (name[n]) { out[n++]='.'; out[n++]='.'; out[n++]='.'; }
    out[n] = 0;
}

static void mc_overlay_cycle(VideoState *is, int direction)
{
    int selected = -1;
    char file[256];
    for (unsigned i=0; i<kp.overlay_count; ++i)
        if (!strcmp(kp.screen_overlay,kp.overlay_names[i])) selected=(int)i;
    selected += direction > 0 ? 1 : -1;
    if (selected < -1) selected=(int)kp.overlay_count-1;
    if (selected >= (int)kp.overlay_count) selected=-1;
    if (selected < 0) kp.screen_overlay[0]=0;
    else av_strlcpy(kp.screen_overlay,kp.overlay_names[selected],
                    sizeof(kp.screen_overlay));
    mc_settings_save();
    /* Rendering remains available for every decoder performance mode. */
    mc_so_free();
    if (!kp.audio_only &&
        mc_overlay_filename_valid(kp.screen_overlay) &&
        snprintf(file,sizeof(file),"/mnt/SDCARD/App/MiniCinema/overlays/%s",
                 kp.screen_overlay)<(int)sizeof(file)) mc_so_load(file);
    kp.clean_ready=0;
    kp_reupload_shown_frame(is);
    if (!kp.screen_overlay[0]) kp_notice(is,"Overlay: Off");
    else kp_notice(is,mc_so.active?"Overlay selected":"Overlay unavailable");
}

static void mc_modify_option(VideoState *is, int index, int direction)
{
    char message[48];
    switch (index) {
    case 0: {
        int before = is->audio_stream;
        if (is->ic && is->audio_stream >= 0)
            stream_cycle_channel(is, AVMEDIA_TYPE_AUDIO);
        mc_audio_label(is, message, sizeof(message));
        if (before == is->audio_stream)
            av_strlcpy(message, "Audio: unchanged", sizeof(message));
        kp_notice(is, message);
        break;
    }
    case 1:
        mc_audio_delay_ms = av_clip(mc_audio_delay_ms + direction * 100, -2000, 2000);
        snprintf(message, sizeof(message), "Audio delay: %+.1fs", mc_audio_delay_ms / 1000.0);
        mc_settings_save();
        kp_notice(is, message);
        break;
    case 2:
        mc_srt_enabled = !mc_srt_enabled;
        kp_notice(is, mc_srt_enabled ? "Subtitles: On" : "Subtitles: Off");
        break;
    case 3:
        if (!is->ic) { kp_notice(is, "No subtitle track"); break; }
        if (mc_cue_count && !mc_srt_source_embedded && is->subtitle_stream >= 0) {
            mc_srt_source_embedded = 1;
        } else {
            stream_cycle_channel(is, AVMEDIA_TYPE_SUBTITLE);
            mc_srt_source_embedded = is->subtitle_stream >= 0;
        }
        mc_srt_invalidate();
        kp_notice(is, mc_srt_source_embedded ? "Embedded subtitles" :
                  mc_cue_count ? "External SRT" : "Subtitle track: Off");
        break;
    case 4:
        mc_srt_delay_ms = av_clip(mc_srt_delay_ms + direction * 100, -15000, 15000);
        snprintf(message, sizeof(message), "Delay: %+.1fs", mc_srt_delay_ms / 1000.0);
        mc_settings_save();
        kp_notice(is, message);
        break;
    case 5:
        kp.subtitle_size = (kp.subtitle_size + (direction > 0 ? 1 : 2)) % 3;
        mc_refresh_srt_font(is);
        mc_settings_save();
        kp_notice(is, kp.subtitle_size == 0 ? "Text: Small" :
                  kp.subtitle_size == 1 ? "Text: Normal" : "Text: Large");
        break;
    case 6:
        kp.subtitle_position = (kp.subtitle_position + (direction > 0 ? 1 : 2)) % 3;
        mc_settings_save();
        kp_notice(is, kp.subtitle_position == 0 ? "Text: Low" :
                  kp.subtitle_position == 1 ? "Text: Raised" : "Text: High");
        break;
    case 7:
        kp.auto_next = !kp.auto_next;
        mc_settings_save();
        kp_notice(is, kp.auto_next ? "Auto next: On" : "Auto next: Off");
        break;
    case 8:
        mc_player_neighbor(is, 0);
        break;
    case 9:
        mc_player_neighbor(is, 1);
        break;
    case 10:
        if (kp.confirm_action != 10) {
            kp.confirm_action = 10;
            is->force_refresh = 1;
            break;
        }
        kp.confirm_action = 0;
        kp.confirm_from_x = 0;
        stream_seek(is, 0, 0, 0);
        kp.position = 0;
        if (kp.remember_resume) {
            kp_write_checkpoint(0);
            kp_write_number(kp.position_file, 0);
        }
        if (is->paused) toggle_pause(is);
        kp.menu_open = 0;
        kp_notice(is, "Restarted");
        break;
    case 11: {
        int replacing = kp.preview_exists;
        int result;
        if (replacing && kp.confirm_action != 11) {
            kp.confirm_action = 11;
            is->force_refresh = 1;
            break;
        }
        kp.confirm_action = 0;
        result = kp_capture_preview(replacing, 3);
        if (result == 1 || result == 2) kp.preview_exists = 1;
        kp_notice(is, result == 1 ? (replacing ? "Preview replaced" : "Preview saved") :
                  result == 2 ? "Preview exists" : "Preview unavailable");
        break;
    }
    case MC_OPT_CHAPTERS:
        if (is->ic && is->ic->nb_chapters) {
            kp.chapter_return_index = kp.menu_index;
            kp.menu_page = 3;
            kp.menu_index = 0;
            kp.menu_focus_cache = -1;
        }
        break;
    case MC_OPT_HELP:
        mc_help_open(is);
        break;
    case MC_OPT_STATS:
        mc_stats_set(is, !kp.stats_on);
        break;
    case MC_OPT_OVERLAY:
        mc_overlay_cycle(is, direction);
        break;
    case MC_OPT_ASPECT:
        kp_cycle_aspect(is); /* Current video only: never save default aspect. */
        break;
    case MC_OPT_SUBTITLE_PAGE:
        kp.menu_main_index = kp.menu_index;
        kp.menu_page = 1;
        kp.menu_index = 0;
        kp.menu_focus_cache = -1;
        break;
    case MC_OPT_VIDEO_PAGE:
        kp.menu_main_index = kp.menu_index;
        kp.menu_page = 2;
        kp.menu_index = 0;
        kp.menu_focus_cache = -1;
        break;
    case MC_OPT_BACK:
        kp.menu_page = 0;
        kp.menu_index = kp.menu_main_index;
        kp.menu_focus_cache = -1;
        break;
    default:
        kp.menu_open = 0;
        kp.progress_until = 0;
        is->force_refresh = 1;
        break;
    }
}

/* Use the browser's pre-rasterized DejaVu Sans 24px alpha masks.
 * No font file or new runtime dependency is required. */
static SDL_Surface *mc_menu_bitmap(const char *str, SDL_Color ink)
{
    unsigned width=0, count=0;
    SDL_Surface *result;
    if (!str || !str[0]) return NULL;
    for(const unsigned char *p=(const unsigned char *)str;*p;p++) {
        unsigned code=*p >= 32 && *p <= 126 ? *p-32 : '?' - 32;
        width+=ui_small_advance[code];
        count++;
    }
    if(width==0 || width>640 || count>96)return NULL;
    result=SDL_CreateRGBSurfaceWithFormat(0,width,UI_SMALL_H,32,SDL_PIXELFORMAT_ARGB8888);
    if(!result)return NULL;
    SDL_FillRect(result,NULL,SDL_MapRGBA(result->format,0,0,0,0));
    unsigned x=0;
    for(const unsigned char *p=(const unsigned char *)str;*p;p++) {
        unsigned code=*p >= 32 && *p <= 126 ? *p-32 : '?' - 32;
        for(unsigned y=0;y<UI_SMALL_H;y++){
            uint32_t *dst=(uint32_t *)((uint8_t *)result->pixels+y*result->pitch);
            for(unsigned gx=0;gx<UI_SMALL_W && x+gx<width;gx++){
                unsigned alpha=ui_small_glyph[code][y*UI_SMALL_W+gx];
                if(alpha)dst[x+gx]=SDL_MapRGBA(result->format,ink.r,ink.g,ink.b,alpha);
            }
        }
        x+=ui_small_advance[code];
    }
    SDL_SetSurfaceBlendMode(result,SDL_BLENDMODE_BLEND);
    return result;
}
static SDL_Surface *mc_menu_cached(SDL_Surface **cache,char *previous,size_t capacity,
                                    const char *text, SDL_Color ink)
{
    if(!*cache || strcmp(previous,text)){
        if(*cache)SDL_FreeSurface(*cache);
        *cache=mc_menu_bitmap(text,ink);
        av_strlcpy(previous,text,capacity);
    }
    return *cache;
}
/* Each three-space-separated hint is KEY ACTION. Color the key green,
 * matching the browser's footer; cache the composed surface between frames. */
static SDL_Surface *mc_menu_hint_surface(const char *line)
{
    SDL_Color green={123,206,166,255}, muted={172,186,207,255};
    SDL_Surface *surface, *word;
    unsigned width = 0, x = 0;
    const char *p;
    if (!line || !*line) return NULL;
    for (p=line; *p; ++p) {
        if (*p >= 32 && *p <= 126)
            width += ui_small_advance[*p - 32];
    }
    if (width < 1 || width > 640) return NULL;
    surface=SDL_CreateRGBSurfaceWithFormat(0,width,UI_SMALL_H,32,SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return NULL;
    SDL_FillRect(surface,NULL,SDL_MapRGBA(surface->format,0,0,0,0));
    p=line;
    while (*p) {
        unsigned spaces=0;
        int key=1;
        while (*p==' ') {spaces++;p++;}
        x+=spaces*ui_small_advance[' '-32];
        if (spaces==1 || spaces==2) key=0;
        while (*p) {
            char token[100];
            unsigned n=0;
            SDL_Rect dst;
            while (*p && *p!=' ' && n<sizeof(token)-1) token[n++]=*p++;
            token[n]=0;
            word=mc_menu_bitmap(token,key?green:muted);
            if (word) {
                dst=(SDL_Rect){(int)x,0,word->w,word->h};
                SDL_BlitSurface(word,NULL,surface,&dst);
                x+=word->w;
                SDL_FreeSurface(word);
            }
            spaces=0;
            while (*p==' ') {spaces++;p++;}
            x+=spaces*ui_small_advance[' '-32];
            if (spaces>=3) break;
            key=0;
        }
    }
    SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_BLEND);
    return surface;
}
static SDL_Surface *mc_menu_cached_hint(SDL_Surface **cache, char *previous,
                                        size_t capacity, const char *line)
{
    if (!*cache || strcmp(previous,line)) {
        if (*cache) SDL_FreeSurface(*cache);
        *cache=mc_menu_hint_surface(line);
        av_strlcpy(previous,line,capacity);
    }
    return *cache;
}

/* Only show options that can do something for this file. The raw option
 * indexes remain stable; kp.menu_index selects a *visible* row. */
static int mc_audio_stream_count(VideoState *is)
{
    int count = 0;
    if (!is || !is->ic) return 0;
    for (unsigned i = 0; i < is->ic->nb_streams; i++) {
        AVCodecParameters *codec = is->ic->streams[i]->codecpar;
        if (codec && codec->codec_type == AVMEDIA_TYPE_AUDIO) count++;
    }
    return count;
}

static int mc_has_text_subtitles(VideoState *is)
{
    if (mc_cue_count) return 1; /* Valid external SRT cues. */
    if (!is || !is->ic) return 0;
    for (unsigned i = 0; i < is->ic->nb_streams; i++) {
        AVCodecParameters *codec = is->ic->streams[i]->codecpar;
        if (!codec || codec->codec_type != AVMEDIA_TYPE_SUBTITLE) continue;
        /* The bundled FFmpeg build supports these text subtitle decoders.
         * Bitmap subtitle tracks are deliberately not offered. */
        switch (codec->codec_id) {
        case AV_CODEC_ID_ASS:
        case AV_CODEC_ID_SUBRIP:
        case AV_CODEC_ID_MOV_TEXT:
        case AV_CODEC_ID_WEBVTT:
            return 1;
        default:
            break;
        }
    }
    return 0;
}

/* Only show the relevant actions on the main page. */
static const int mc_main_order[] = {
    0, 1, MC_OPT_SUBTITLE_PAGE, MC_OPT_ASPECT, 7,
    MC_OPT_OVERLAY, MC_OPT_HELP, MC_OPT_VIDEO_PAGE
};
static const int mc_subtitle_order[] = { 2, 3, 4, 5, 6, MC_OPT_BACK };
static const int mc_video_order[] = { 8, 9, MC_OPT_CHAPTERS, 10, 11, MC_OPT_STATS, MC_OPT_BACK };

static void mc_option_order(const int **order, int *length)
{
    if (kp.menu_page == 3) { *order = mc_video_order; *length = 0; }
    else if (kp.menu_page == 1) {
        *order = mc_subtitle_order;
        *length = FF_ARRAY_ELEMS(mc_subtitle_order);
    } else if (kp.menu_page == 2) {
        *order = mc_video_order;
        *length = FF_ARRAY_ELEMS(mc_video_order);
    } else {
        *order = mc_main_order;
        *length = FF_ARRAY_ELEMS(mc_main_order);
    }
}

static int mc_option_visible(VideoState *is, int raw)
{
    if (raw == MC_OPT_CHAPTERS) return is && is->ic && is->ic->nb_chapters > 0;
    if (raw == 0) return mc_audio_stream_count(is) > 1;
    if (raw == MC_OPT_SUBTITLE_PAGE || (raw >= 2 && raw <= 6))
        return mc_has_text_subtitles(is);
    if (raw == MC_OPT_ASPECT || raw == MC_OPT_OVERLAY)
        return is && is->video_stream >= 0;
    return 1;
}

static int mc_visible_option_count(VideoState *is)
{
    const int *order;
    int length, count = 0;
    mc_option_order(&order, &length);
    for (int i = 0; i < length; i++)
        if (mc_option_visible(is, order[i])) count++;
    return count;
}

static int mc_visible_option(VideoState *is, int row)
{
    const int *order;
    int length;
    mc_option_order(&order, &length);
    for (int i = 0; i < length; i++) {
        if (!mc_option_visible(is, order[i])) continue;
        if (row-- == 0) return order[i];
    }
    return -1;
}

static void kp_draw_options(SDL_Surface *surface, VideoState *is)
{
    const int left=43, top=26, width=554, height=428;
    SDL_Color white={238,243,250,255};
    SDL_Color muted={172,186,207,255};
    SDL_Color accent={253,132,140,255};
    SDL_Surface *heading,*footer;
    SDL_Rect rect;
    char line[96];

    if(kp.confirm_action){
        SDL_Surface *title,*info,*hint;
        mc_dim_rect(surface,0,0,KP_W,KP_H,23);
        rect=(SDL_Rect){101,171,438,138};
        SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,9,14,22));
        rect=(SDL_Rect){105,175,430,130};
        SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,25,34,47));
        rect=(SDL_Rect){105,175,430,3};
        SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,247,107,120));
        title=mc_menu_cached(&kp.text_menu_title,kp.cached_menu_title,
                             sizeof(kp.cached_menu_title),
                             kp.confirm_action == 11 ? "REPLACE PREVIEW?" : "RESTART VIDEO?",white);
        info=mc_menu_cached(&kp.text_menu_footer,kp.cached_menu_footer,
                            sizeof(kp.cached_menu_footer),
                            kp.confirm_action == 11 ? "Overwrite the saved preview?" :
                            "Play from the beginning?",muted);
        hint=mc_menu_cached_hint(&kp.text_menu_hint,kp.cached_menu_hint,
                                 sizeof(kp.cached_menu_hint),
                                 kp.confirm_action == 11 ? "A REPLACE    B CANCEL" :
                                 "A RESTART    B CANCEL");
        if(title)kp_blit(surface,title,(KP_W-title->w)/2,192);
        if(info)kp_blit(surface,info,(KP_W-info->w)/2,224);
        if(hint)kp_blit(surface,hint,(KP_W-hint->w)/2,264);
        return;
    }
    mc_dim_rect(surface,0,0,KP_W,KP_H,23);
    rect=(SDL_Rect){left-3,top-3,width+6,height+6};
    SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,9,14,22));
    rect=(SDL_Rect){left,top,width,height};
    SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,25,34,47));
    rect=(SDL_Rect){left,top,width,3};
    SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,247,107,120));
    heading=mc_menu_cached(&kp.text_menu_title,kp.cached_menu_title,
                sizeof(kp.cached_menu_title),
                kp.menu_page == 1 ? "SUBTITLES" :
                kp.menu_page == 2 ? "VIDEO ACTIONS" :
                kp.menu_page == 3 ? "CHAPTERS" : "PLAYBACK OPTIONS",white);
    if(heading)kp_blit(surface,heading,left+25,top+13);
    footer=mc_menu_cached_hint(&kp.text_menu_footer,kp.cached_menu_footer,
                sizeof(kp.cached_menu_footer),
                kp.menu_page ? "B BACK" : "");
    if(footer)kp_blit(surface,footer,left+width-25-footer->w,top+13);
    rect=(SDL_Rect){left+23,top+46,width-46,1};
    SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,58,72,92));

    if (kp.menu_page == 3) {
        unsigned total = is->ic ? is->ic->nb_chapters : 0;
        unsigned first = kp.menu_index > 6 ? kp.menu_index - 6 : 0;
        for (unsigned row = 0; row < 7 && first + row < total; ++row) {
            unsigned n = first + row;
            AVChapter *ch = is->ic->chapters[n];
            AVDictionaryEntry *t = av_dict_get(ch->metadata, "title", NULL, 0);
            int seconds = (int)(ch->start * av_q2d(ch->time_base));
            char label[96];
            SDL_Surface *line_surface;
            int y = top + 62 + (int)row * 43;
            snprintf(label, sizeof(label), "%u  %02d:%02d  %.42s", n + 1,
                     seconds / 60, seconds % 60,
                     t && t->value && t->value[0] ? t->value : "Chapter");
            if ((int)n == kp.menu_index) {
                rect=(SDL_Rect){left+13,y-2,width-26,40};
                SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,47,63,82));
                rect=(SDL_Rect){left+13,y+2,3,31};
                SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,247,107,120));
            }
            line_surface=mc_menu_bitmap(label,(int)n==kp.menu_index?white:muted);
            if (line_surface) {
                SDL_Rect src={0,0,FFMIN(line_surface->w,width-60),line_surface->h};
                SDL_Rect dst={left+29,y+6,src.w,src.h};
                SDL_BlitSurface(line_surface,&src,surface,&dst);
                SDL_FreeSurface(line_surface);
            }
        }
        rect=(SDL_Rect){left+23,top+391,width-46,1};
        SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,58,72,92));
        snprintf(line,sizeof(line),"%d/%u   A PLAY   B BACK",kp.menu_index+1,total);
        footer=mc_menu_cached_hint(&kp.text_menu_hint,kp.cached_menu_hint,
                                   sizeof(kp.cached_menu_hint),line);
        if (footer)kp_blit(surface,footer,left+(width-footer->w)/2,top+396);
        return;
    }
    {
    int available = mc_visible_option_count(is);
    if (kp.menu_index >= available) kp.menu_index = available - 1;
    if (kp.menu_index < 0) kp.menu_index = 0;
    }
    if(kp.menu_focus_cache!=kp.menu_index){
        for(int i=0;i<MC_OPT_COUNT;i++){
            if(kp.menu_rows[i]){SDL_FreeSurface(kp.menu_rows[i]);kp.menu_rows[i]=NULL;}
            if(kp.menu_values[i]){SDL_FreeSurface(kp.menu_values[i]);kp.menu_values[i]=NULL;}
            kp.menu_row_cache[i][0]=kp.menu_value_cache[i][0]=0;
        }
        kp.menu_focus_cache=kp.menu_index;
    }
    for(int row=0;row<mc_visible_option_count(is);row++){
        int index=mc_visible_option(is,row);
        int row_step = mc_visible_option_count(is)>7 ? 39 : 43;
        int y=top+63+row*row_step;
        int selected=row==kp.menu_index;
        char value[64]="";
        SDL_Surface *name_surface,*value_surface;
        if(selected){
            /* The bitmap font has a top ink inset; lift the highlight to
             * center it visually on the visible glyphs. */
            rect=(SDL_Rect){left+13,y-2,width-26,row_step-3};
            SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,47,63,82));
            rect=(SDL_Rect){left+13,y+2,3,row_step-11};
            SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,247,107,120));
        }
        switch(index){
        case 0:{
            char audio[48];
            mc_audio_label(is,audio,sizeof(audio));
            av_strlcpy(value,audio+7,sizeof(value));
            break;
        }
        case 1:snprintf(value,sizeof(value),"%+.1fs",mc_audio_delay_ms/1000.0);break;
        case 2:av_strlcpy(value,mc_srt_enabled?"On":"Off",sizeof(value));break;
        case 3:av_strlcpy(value,mc_srt_source_embedded?"Embedded":
                     mc_cue_count?"External":
                     is->subtitle_stream>=0?"Embedded":"Off",sizeof(value));break;
        case 4:snprintf(value,sizeof(value),"%+.1fs",mc_srt_delay_ms/1000.0);break;
        case 5:av_strlcpy(value,kp.subtitle_size==0?"Small":
                      kp.subtitle_size==1?"Normal":"Large",sizeof(value));break;
        case 6:av_strlcpy(value,kp.subtitle_position==0?"Low":
                      kp.subtitle_position==1?"Raised":"High",sizeof(value));break;
        case 7:av_strlcpy(value,kp.auto_next?"On":"Off",sizeof(value));break;
        case MC_OPT_STATS:av_strlcpy(value,kp.stats_on?"On":"Off",sizeof(value));break;
        case MC_OPT_OVERLAY:
            mc_overlay_value(kp.screen_overlay, value, sizeof(value));
            break;
        case MC_OPT_ASPECT:
            av_strlcpy(value, is->vfilter_idx == 0 ? "Fit" :
                     is->vfilter_idx == 1 ? "Fill" :
                     is->vfilter_idx == 2 ? "Stretch" : "Original",sizeof(value));
            break;
        case MC_OPT_SUBTITLE_PAGE:
        case MC_OPT_VIDEO_PAGE:
        case MC_OPT_CHAPTERS:
        case MC_OPT_HELP:av_strlcpy(value,">",sizeof(value));break;
        default:break;
        }
        name_surface=mc_menu_cached(&kp.menu_rows[row],kp.menu_row_cache[row],
                     sizeof(kp.menu_row_cache[row]),mc_option_name(index),
                     selected?white:muted);
        value_surface=mc_menu_cached(&kp.menu_values[row],kp.menu_value_cache[row],
                     sizeof(kp.menu_value_cache[row]),value,selected?accent:white);
        if(name_surface)kp_blit(surface,name_surface,left+29,y+(row_step-UI_SMALL_H)/2);
        if(value_surface)kp_blit(surface,value_surface,
                              left+width-29-value_surface->w,y+(row_step-UI_SMALL_H)/2);
    }
    rect=(SDL_Rect){left+23,top+391,width-46,1};
    SDL_FillRect(surface,&rect,SDL_MapRGB(surface->format,58,72,92));
    if(kp.menu_feedback[0] && SDL_GetTicks()<kp.menu_feedback_until)
        av_strlcpy(line,kp.menu_feedback,sizeof(line));
    else
        av_strlcpy(line,kp.menu_page ?
                   "UP/DOWN MOVE   A SELECT   B BACK" :
                   "UP/DOWN MOVE   LEFT/RIGHT CHANGE   B CLOSE",sizeof(line));
    footer=mc_menu_cached_hint(&kp.text_menu_hint,kp.cached_menu_hint,
                               sizeof(kp.cached_menu_hint),line);
    if(footer)kp_blit(surface,footer,left+(width-footer->w)/2,top+396);
}

/* Display the last-five-seconds auto-next decision after natural EOF,
 * while FFplay still owns the SDL renderer. No browser process is launched
 * merely to draw this notice, and cancellation is a marker for the launcher.
 * Return 1 on B/MENU cancellation, 0 after timeout. */
static int mc_next_countdown(void)
{
    uint32_t end = SDL_GetTicks() + 5000;
    SDL_Event event;
    int last_seconds = -1;
    const char *name = getenv("MC_NEXT_TITLE");
    char label[512];
    if (!renderer || !kp.font_small) return 0;
    if (!name || !*name) name = "Next video";
    kp_ellipsize(kp.font_small, name, 465, label, sizeof(label));
    while (!mc_term_requested && (int32_t)(end - SDL_GetTicks()) > 0) {
        int remaining = ((int32_t)(end - SDL_GetTicks()) + 999) / 1000;
        if (remaining != last_seconds) {
            char title[560], hint[64];
            SDL_Color fg = {238,243,250,255};
            SDL_Color soft = {123,206,166,255};
            SDL_Surface *line1, *line2;
            SDL_Texture *tex;
            SDL_Rect dest;
            last_seconds = remaining;
            snprintf(title, sizeof(title), "Next: %s in %d s", label, remaining);
            snprintf(hint, sizeof(hint), "B to cancel");
            SDL_SetRenderDrawColor(renderer, 14, 20, 29, 255);
            SDL_RenderClear(renderer);
            line1 = TTF_RenderUTF8_Blended(kp.font_small, title, fg);
            line2 = TTF_RenderUTF8_Blended(kp.font_small, hint, soft);
            if (line1) {
                tex = SDL_CreateTextureFromSurface(renderer, line1);
                dest = (SDL_Rect){(KP_W - line1->w)/2, 205, line1->w, line1->h};
                if (tex) { SDL_RenderCopy(renderer, tex, NULL, &dest); SDL_DestroyTexture(tex); }
                SDL_FreeSurface(line1);
            }
            if (line2) {
                tex = SDL_CreateTextureFromSurface(renderer, line2);
                dest = (SDL_Rect){(KP_W - line2->w)/2, 252, line2->w, line2->h};
                if (tex) { SDL_RenderCopy(renderer, tex, NULL, &dest); SDL_DestroyTexture(tex); }
                SDL_FreeSurface(line2);
            }
            SDL_RenderPresent(renderer);
        }
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) return 1;
            if (event.type == SDL_KEYDOWN && !event.key.repeat &&
                (event.key.keysym.sym == SDLK_LCTRL ||
                 event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.sym == SDLK_HOME)) return 1;
        }
        SDL_Delay(30);
    }
    return 0;
}

static void mc_countdown_on_eof(VideoState *is)
{
    const char *has_next = getenv("MC_HAS_NEXT");
    const char *frame = getenv("MC_FRAME_OK_FILE");
    const char *seek_end = getenv("MC_SEEK_END_FILE");
    const char *cancel = getenv("MC_AUTO_NEXT_CANCEL_FILE");
    if (!is || !is->eof || !kp.auto_next ||
        (getenv("MC_SLEEP_END_VIDEO") && !strcmp(getenv("MC_SLEEP_END_VIDEO"), "1")) ||
        !has_next || strcmp(has_next, "1") ||
        !frame || access(frame, F_OK) != 0 ||
        (seek_end && access(seek_end, F_OK) == 0)) return;
    if (mc_next_countdown() && cancel && cancel[0]) kp_touch(cancel, 1);
}

/* A plays / reveals progress; B pauses on first press and returns on second.
 * MENU: tap returns to browser, double-tap exits RedOnion (no hold exit).
 * START+SELECT remains the direct emergency exit. */
static void mc_leave(VideoState *is, int exit_app)
{
    const char *marker = getenv(exit_app ? "MC_EXIT_APP_FILE" : "MC_BACK_BROWSER_FILE");
    kp_backlight_restore();
    kp_save_position(is, 1);
    kp_set_stay_awake(0);
    if (marker && marker[0]) kp_touch(marker, 1);
    /* Explicit exit only: checkpoints and handoff marker are already saved.
     * Bound slow decoder/SDL teardown on troubled SD cards. */
    mc_arm_exit_watchdog();
    do_exit(is);
}

/* Skip non-text subtitle tracks when cycling with X. */
static int mc_x_cycle_text_subtitle(VideoState *is,int from_first){
    if(!is || !is->ic)return 0;
    int previous=is->subtitle_stream;
    int start=from_first?-1:previous;
    int wanted=-1;
    for(int i=start+1;i<(int)is->ic->nb_streams;i++){
        AVCodecParameters *par=is->ic->streams[i]->codecpar;
        if(!par || par->codec_type!=AVMEDIA_TYPE_SUBTITLE ||
           !avcodec_find_decoder(par->codec_id))continue;
        switch(par->codec_id){
        case AV_CODEC_ID_ASS:case AV_CODEC_ID_SUBRIP:
        case AV_CODEC_ID_MOV_TEXT:case AV_CODEC_ID_WEBVTT:
            wanted=i;break;
        default:break;
        }
        if(wanted>=0)break;
    }
    if(previous>=0 && previous!=wanted)stream_component_close(is,previous);
    if(wanted<0){is->last_subtitle_stream=-1;return 0;}
    if(wanted==previous)return 1;
    if(stream_component_open(is,wanted)<0){
        is->last_subtitle_stream=-1;
        return 0;
    }
    return is->subtitle_stream==wanted;
}
static void mc_x_subtitles(VideoState *is){
    if(!mc_has_text_subtitles(is)){kp_notice(is,"No subtitles");return;}
    if(!mc_srt_enabled){
        mc_srt_enabled=1;
        if(mc_cue_count)mc_srt_source_embedded=0;
        else if(mc_x_cycle_text_subtitle(is,1))mc_srt_source_embedded=1;
        else mc_srt_enabled=0;
    } else if(!mc_srt_source_embedded){
        if(mc_x_cycle_text_subtitle(is,1))mc_srt_source_embedded=1;
        else mc_srt_enabled=0;
    } else {
        if(!mc_x_cycle_text_subtitle(is,0)){
            mc_srt_enabled=0;mc_srt_source_embedded=0;
        }
    }
    mc_srt_invalidate();
    if(!mc_srt_enabled)kp_notice(is,"Subtitles: Off");
    else if(!mc_srt_source_embedded)kp_notice(is,"Subtitles: External");
    else {
        AVDictionaryEntry *lang=NULL;
        if(is->subtitle_st)lang=av_dict_get(is->subtitle_st->metadata,"language",NULL,0);
        char title[48];
        snprintf(title,sizeof(title),"Subtitles: %.24s",
                 lang && lang->value[0] && strcmp(lang->value,"und")?lang->value:"Embedded");
        kp_notice(is,title);
    }
}
static void mc_x_action(VideoState *is){
    switch(kp.x_action){
    case 1:mc_modify_option(is,0,1);break;
    case 2:mc_x_subtitles(is);break;
    case 3:
        if(!mc_so.count){kp_notice(is,"No screen overlay");break;}
        mc_so.active=!mc_so.active;
        kp_reupload_shown_frame(is);
        kp_notice(is,mc_so.active?"Overlay: On":"Overlay: Off");
        break;
    case 4:
        toggle_mute(is);
        kp_notice(is,is->muted?"Sound: Muted":"Sound: On");
        break;
    case 5: /* Off -> each available overlay -> Off. No per-frame directory IO. */
        if (!kp.overlay_scanned) mc_overlay_scan();
        if (!kp.overlay_count) {
            kp_notice(is, "No screen overlays");
            break;
        }
        mc_overlay_cycle(is, 1);
        break;
    case 6:
        mc_stats_set(is, !kp.stats_on);
        if (!kp.stats_on) kp_notice(is, "Live stats: Off");
        break;
    case 7: /* Close temporary OSD, but keep subtitles and user-held battery. */
        kp.osd_hidden = 1;
        kp.progress_until = kp.seek_notice_until = 0;
        kp.notice[0] = 0;
        kp_reupload_shown_frame(is);
        break;
    case 8: /* Restart: same confirmation, but B cancels straight to playback. */
        kp_backlight_restore();
        mc_overlay_scan();
        kp.progress_until = kp.seek_notice_until = 0;
        kp.notice[0] = kp.menu_feedback[0] = 0;
        kp.confirm_from_x = 1;
        kp.menu_open = 1;
        kp.menu_page = 2;
        kp.menu_index = 2;
        kp.menu_main_index = 0;
        kp.confirm_action = 10;
        kp.menu_focus_cache = -1;
        is->force_refresh = 1;
        break;
    case 9:mc_player_neighbor(is, 1);break;
    case 10: {
        int future = 0;
        if (is->ic) {
            double current = get_master_clock(is);
            for (unsigned i = 0; i < is->ic->nb_chapters && !isnan(current); i++) {
                AVChapter *c = is->ic->chapters[i];
                if (av_compare_ts((int64_t)(current * AV_TIME_BASE), AV_TIME_BASE_Q,
                                  c->start, c->time_base) < 0) { future = 1; break; }
            }
        }
        if (future) { seek_chapter(is, 1); kp_notice(is, "Next chapter"); }
        else kp_notice(is, is->ic && is->ic->nb_chapters ? "Last chapter" : "No chapters");
        break;
    }
    case 11:mc_leave(is, 0);break;
    case 12:
        if(is->paused && is->video_stream >= 0){
            kp.frame_stepping = 1;
            step_to_next_frame(is);
            kp_notice(is, "Next frame");
        }else kp_notice(is, "Pause to step frame");
        break;
    case 13:{
        /* User explicitly chose overwrite: save a clean frame, not the OSD or overlay. */
        if(kp.clean_ready){
            int result=kp_capture_preview(1,3);
            if(result==1)kp.preview_exists=1;
            kp_notice(is,result==1?"Preview saved":"Preview unavailable");
        }else if(is->video_stream>=0){
            kp.x_preview_pending=1;
            kp_reupload_shown_frame(is);
        }else kp_notice(is,"Preview unavailable");
        break;
    }
    default:break;
    }
}
static int kp_handle_key(VideoState *is, SDL_KeyboardEvent *key)
{
    SDL_Keycode sym = key->keysym.sym;
    int down = key->state == SDL_PRESSED;
    uint32_t now = SDL_GetTicks();
    static int start_down, select_down, select_used, start_used;
    kp_init(is);
    /* The waking POWER event can be swallowed by keymon. Never lock all
     * controls behind the black frame: a fresh A/B/other SDL key-down also
     * recovers after keymon has returned control to the player. Consume the
     * triggering key so it cannot seek, exit or change settings on wake. */
    if (kp.power_blanked) {
        if (down && !key->repeat) {
            kp_power_woke(is);
            /* When POWER wake was missed and A is the recovery key, the
             * first A must resume an already-paused video, not be swallowed. */
            if (sym == SDLK_SPACE && is->paused && !kp.menu_open)
                kp_resume(is);
            kp.wake_ignore_key = sym;
            kp.wake_ignore_release = 1;
        }
        return 1;
    }
    if(kp.help_ignore_release==sym && !down){
        kp.help_ignore_release=SDLK_UNKNOWN;
        return 1;
    }
    if(kp.help_open){
        if(down && !key->repeat && (sym==SDLK_LCTRL || sym==SDLK_RCTRL)){
            kp.help_ignore_release=sym;
            mc_help_close(is);
        }
        return 1;
    }
    if (kp.wake_ignore_release && sym == kp.wake_ignore_key) {
        if (!down) { kp.wake_ignore_release = 0; kp.wake_ignore_key = SDLK_UNKNOWN; }
        return 1;
    }
    if (down && is->paused && kp.backlight_stage) {
        /* A wakes the screen AND resumes in a single press when no menu is
         * open. Other buttons only wake it, including their release. */
        if (sym == SDLK_SPACE && !kp.menu_open) kp_resume(is);
        else kp_backlight_restore();
        kp.wake_ignore_key = sym;
        kp.wake_ignore_release = 1;
        is->force_refresh = 1;
        return 1;
    }
    if (down && is->paused) kp.pause_activity_at = now;
    if (down && key->repeat) return 1;
    if (sym == SDLK_ESCAPE || sym == SDLK_HOME) {
        if (down && !kp.menu_down) {
            kp.menu_double_candidate = kp.menu_pending &&
                now - kp.menu_released < 320;
            kp.menu_down = 1;
            kp.menu_used = 0;
            kp.menu_brightness_start = kp_read_number(KP_BACKLIGHT);
        } else if (!down && kp.menu_down) {
            /* Drain any MENU+VOL input not yet seen by the outer tick. */
            kp_power_poll(is);
            kp.menu_down = 0;
            if (kp.menu_brightness_start >= 0) {
                long brightness = kp_read_number(KP_BACKLIGHT);
                if (brightness >= 0 && brightness != kp.menu_brightness_start)
                    kp.menu_used = 1;
            }
            if (kp.menu_used) kp.menu_pending = 0;
            else if (kp.menu_double_candidate) mc_leave(is, 1);
            else {
                kp.menu_pending = 1;
                kp.menu_released = now;
            }
            kp.menu_double_candidate = 0;
        }
        return 1;
    }
    /* Other button while MENU is held: a system-wide shortcut, not an exit. */
    if (down && kp.menu_down) {
        kp.menu_used = 1;
        kp.menu_pending = 0;
    }

    if (sym == SDLK_RETURN || sym == SDLK_RCTRL) {
        if (sym == SDLK_RETURN) {
            if (down) { start_down = 1; start_used = select_down; }
            else {
                if (start_down && !select_down && !start_used && !kp.menu_open &&
                    kp.start_aspect_shortcut)
                    kp_cycle_aspect(is);
                start_down = 0;
            }
        } else {
            if (down) { select_down = 1; select_used = start_down; }
            else {
                if (select_down && !start_down && !select_used) {
                    kp.menu_open = !kp.menu_open;
                                    if (kp.menu_open) kp_backlight_restore();
                    else if (is->paused)
                        /* Closing the menu must remove its pixels even if
                         * POWER suspends before another ordinary refresh. */
                        kp_reupload_shown_frame(is);
                    kp.confirm_action = 0;
                    kp.confirm_from_x = 0;
                    if (kp.menu_open) {
                        mc_overlay_scan();
                        kp.menu_page = 0;
                        kp.menu_index = 0;
                        kp.menu_focus_cache = -1;
                    }
                    kp.vertical_key = 0;
                    kp.progress_until = 0;
                    kp.seek_notice_until = 0;
                    kp.notice[0] = '\0';
                    kp.menu_feedback[0] = '\0';
                    kp.menu_feedback_until = 0;
                    if (mc_so.active && !kp.clean_ready)
                        kp_reupload_shown_frame(is);
                    is->force_refresh = 1;
                }
                select_down = 0;
            }
        }
        if (start_down && select_down) {
            start_used = select_used = 1;
            mc_leave(is, 1);
        }
        return 1;
    }

    if (sym == SDLK_LEFT || sym == SDLK_RIGHT) {
        if (!down) {
            if (kp.seek_key == sym) kp.seek_key = 0;
            return 1;
        }
    }
    if (sym == SDLK_UP || sym == SDLK_DOWN) {
        if (!kp.menu_open) {
            if (down) kp_backlight_restore();
            if (down && !kp.vertical_key) {
                kp.vertical_key = sym;
                kp.vertical_at = now;
                kp.vertical_repeat_at = now;
                kp.vertical_done = 0;
            } else if (!down && kp.vertical_key == sym) {
                if (!kp.vertical_done)
                    kp_seek(is, sym == SDLK_UP ?
                            (now - kp.vertical_at >= 650 ? 600 : 300) :
                            (now - kp.vertical_at >= 650 ? -600 : -300),
                            now - kp.vertical_at >= 650 ? 600 : 300);
                kp.vertical_key = 0;
                is->force_refresh = 1;
            }
            return 1;
        }
    }
    if (sym == SDLK_LALT) {
        if (down) kp.battery = kp_battery_percentage();
        kp.battery_held = down;
        is->force_refresh = 1;
        return 1;
    }
    if (!down) return 1;
    if (sym==SDLK_LSHIFT) {
        if(!kp.menu_open && !kp.confirm_action)mc_x_action(is);
        is->force_refresh=1;
        return 1;
    }

    if (kp.menu_open) {
        if (kp.menu_page == 3) {
            int total = is->ic ? (int)is->ic->nb_chapters : 0;
            if (sym == SDLK_LCTRL || sym == SDLK_LEFT) {
                kp.menu_page = 2;
                kp.menu_index = kp.chapter_return_index;
                kp.menu_focus_cache = -1;
            } else if (total > 0 && sym == SDLK_UP)
                kp.menu_index = (kp.menu_index + total - 1) % total;
            else if (total > 0 && sym == SDLK_DOWN)
                kp.menu_index = (kp.menu_index + 1) % total;
            else if (total > 0 && (sym == SDLK_SPACE || sym == SDLK_RIGHT)) {
                AVChapter *c = is->ic->chapters[kp.menu_index];
                int64_t start = av_rescale_q(c->start, c->time_base, AV_TIME_BASE_Q);
                stream_seek(is, start, 0, 0);
                kp.menu_page = 0;
                kp.menu_open = 0;
                kp.progress_until = 0;
                kp_notice(is, "Chapter selected");
            }
            is->force_refresh = 1;
            return 1;
        }
        if (kp.confirm_action) {
            if (sym == SDLK_LCTRL) {
                kp.confirm_action = 0;
                if (kp.confirm_from_x) {
                    kp.menu_open = 0;
                    kp.confirm_from_x = 0;
                    kp.menu_page = 0;
                    kp.progress_until = 0;
                }
            }
            else if (sym == SDLK_SPACE) mc_modify_option(is, kp.confirm_action, 1);
            is->force_refresh = 1;
            return 1;
        }
        int count = mc_visible_option_count(is);
        int option;
        if (kp.menu_index >= count) kp.menu_index = count - 1;
        if (kp.menu_index < 0) kp.menu_index = 0;
        option = mc_visible_option(is, kp.menu_index);
        if (sym == SDLK_UP) kp.menu_index = (kp.menu_index + count - 1) % count;
        else if (sym == SDLK_DOWN) kp.menu_index = (kp.menu_index + 1) % count;
        else if (sym == SDLK_LCTRL) {
            if (kp.menu_page) {
                kp.menu_page = 0;
                kp.menu_index = kp.menu_main_index;
                kp.menu_focus_cache = -1;
            } else {
                kp.menu_open = 0;
                if (is->paused) kp_reupload_shown_frame(is);
            }
        } else if (sym == SDLK_LEFT && option == MC_OPT_BACK) {
            mc_modify_option(is, MC_OPT_BACK, 1);
        } else if (sym == SDLK_SPACE ||
                   (sym == SDLK_RIGHT && (option == MC_OPT_SUBTITLE_PAGE ||
                                          option == MC_OPT_VIDEO_PAGE ||
                                          option == MC_OPT_CHAPTERS ||
                                          option == MC_OPT_HELP ||
                                          option == MC_OPT_BACK)) ||
                   ((sym == SDLK_LEFT || sym == SDLK_RIGHT) &&
                    ((option >= 0 && option <= 7) || option == MC_OPT_ASPECT ||
                     option == MC_OPT_OVERLAY)))
            mc_modify_option(is, option, sym == SDLK_LEFT ? -1 : 1);
        is->force_refresh = 1;
        return 1;
    }

    if (sym == SDLK_SPACE) {
        kp.osd_hidden = 0;
        if (is->paused) kp_resume(is);
        else if (kp.a_pause_shortcut) kp_pause(is);
        kp.progress_until = now + mc_osd_ms();
    } else if (sym == SDLK_LCTRL) {
        kp.osd_hidden = 0;
        if (is->paused) {
            /* With the second-B exit option enabled,
             * the next B exits regardless of the time since pausing.
             * Otherwise B reveals progress without resuming playback. */
            if (kp.b_second_press_exit) {
                mc_leave(is, 0);
                return 1;
            }
            kp_backlight_restore();
            kp.progress_until = now + mc_osd_ms();
            kp_reupload_shown_frame(is);
        } else {
            kp_pause(is);
            if (mc_so.active && !kp.clean_ready)
                kp_reupload_shown_frame(is);
        }
    } else if (sym == SDLK_LEFT || sym == SDLK_RIGHT) {
        kp_backlight_restore();
            kp.seek_key = sym;
        kp.seek_forward = (sym == SDLK_RIGHT);
        kp.seek_at = kp.seek_last = now;
        kp_seek(is, sym == SDLK_RIGHT ? 10 : -10, 10);
    } else if ((sym == SDLK_e || sym == SDLK_t) && kp.shoulder_l1_r1) {
        kp_backlight_restore();
        /* Optional chapter keys, disabled by default to avoid accidental skips. */
        if (is->ic && is->ic->nb_chapters > 1) {
            seek_chapter(is, sym == SDLK_t ? 1 : -1);
            kp_notice(is, sym == SDLK_t ? "Next chapter" : "Previous chapter");
        } else {
            kp_notice(is, "No chapters");
        }
    } else if ((sym == SDLK_TAB || sym == SDLK_PAGEDOWN) && kp.shoulder_l2_r2) {
        kp_backlight_restore();
        mc_player_neighbor(is, 0);
    } else if ((sym == SDLK_BACKSPACE || sym == SDLK_PAGEUP) && kp.shoulder_l2_r2) {
        kp_backlight_restore();
        mc_player_neighbor(is, 1);
    }
    is->force_refresh = 1;
    return 1;
}

static void kp_shutdown(VideoState *is)
{
    int i;
    if (!kp.initialized)
        return;
    kp_backlight_restore();
    if (kp.power_fd >= 0) { close(kp.power_fd); kp.power_fd = -1; }
    kp_save_position(is, 1);
    kp_touch(KP_MEDIA_PLAYING, 0);
    kp_set_stay_awake(0);
    mc_srt_shutdown();
    if (kp.audio_texture) SDL_DestroyTexture(kp.audio_texture);
    if (kp.audio_canvas) SDL_FreeSurface(kp.audio_canvas);
    if (kp.audio_base) SDL_FreeSurface(kp.audio_base);
    if (kp.artwork) SDL_FreeSurface(kp.artwork);
    if (kp.text_elapsed) SDL_FreeSurface(kp.text_elapsed);
    if (kp.text_remaining) SDL_FreeSurface(kp.text_remaining);
    if (kp.text_battery) SDL_FreeSurface(kp.text_battery);
    if (kp.text_muted) SDL_FreeSurface(kp.text_muted);
    for (int i = 0; i < MC_STATS_LINES; i++)
        if (kp.stats_text[i]) SDL_FreeSurface(kp.stats_text[i]);
    if (kp.help_image) SDL_FreeSurface(kp.help_image);
    mc_so_free();
    if (kp.text_notice) SDL_FreeSurface(kp.text_notice);
    if (kp.text_title) SDL_FreeSurface(kp.text_title);
    if (kp.text_menu_title) SDL_FreeSurface(kp.text_menu_title);
    if (kp.text_menu_footer) SDL_FreeSurface(kp.text_menu_footer);
    if (kp.text_menu_hint) SDL_FreeSurface(kp.text_menu_hint);
    for (i = 0; i < KP_HEADER_TITLE_LINES; i++)
        if (kp.text_header_title[i]) SDL_FreeSurface(kp.text_header_title[i]);
    for (int i = 0; i < MC_OPT_COUNT; ++i) {
        if (kp.menu_rows[i]) SDL_FreeSurface(kp.menu_rows[i]);
        if (kp.menu_values[i]) SDL_FreeSurface(kp.menu_values[i]);
    }
    if (kp.srt_font && kp.srt_font != kp.font_small) TTF_CloseFont(kp.srt_font);
    if (kp.font_small) TTF_CloseFont(kp.font_small);
    if (kp.font_title) TTF_CloseFont(kp.font_title);
    TTF_Quit();
    av_freep(&kp.clean_pixels);
    kp.initialized = 0;
}
