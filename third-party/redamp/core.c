// Shared by the player screen and the background service: settings and
// playlist navigation (shuffle, repeat, next/previous).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "core.h"
#include "audio.h"
#include "playlist.h"

settings S = { .volume = 80, .play_standby = 1, .vis_mode = 1, .skin_smooth = 0, .list_size = 1, .cover_player = 1, .cover_library = 1, .game_mix_volume = 70, .cur_index = -1 };
char app_dir[512], data_dir[512], root_dir[512];
int current = -1;
static int *shuffle_order, shuffle_n;

void path_join(char *out, int n, const char *a, const char *b) { snprintf(out, n, "%s/%s", a, b); }

/* Remove only stale, regular mkstemp artifacts; don't race a live writer or
 * follow symlinks in a writable SD-card directory. */
static void clean_stale_settings(void)
{
    DIR *d = opendir(data_dir);
    if (!d) return;
    time_t t = time(NULL);
    struct dirent *ent;
    while ((ent = readdir(d))) {
        const char *pre = "settings.ini.tmp.";
        size_t plen = strlen(pre);
        if (strncmp(ent->d_name, pre, plen) || strlen(ent->d_name) != plen + 6) continue;
        int valid = 1;
        for (const char *s = ent->d_name + plen; *s; s++)
            if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') ||
                  (*s >= 'A' && *s <= 'Z'))) valid = 0;
        if (!valid) continue;
        struct stat st;
        if (fstatat(dirfd(d), ent->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
            S_ISREG(st.st_mode) && t > st.st_mtime && t - st.st_mtime > 3600)
            (void)unlinkat(dirfd(d), ent->d_name, 0);
    }
    closedir(d);
}

void settings_load(void)
{
    S.insecure_https = 0;
    S.play_standby = 1;
    clean_stale_settings();
    char p[1024];
    path_join(p, sizeof p, data_dir, "settings.ini");
    FILE *f = fopen(p, "r");
    if (!f) return;
    char line[1200];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "volume")) S.volume = atoi(v);
        else if (!strcmp(k, "balance")) S.balance = atoi(v);
        else if (!strcmp(k, "eq_on")) S.eq_on = atoi(v);
        else if (!strcmp(k, "eq_pre")) S.eq_pre = (float)atof(v);
        else if (!strcmp(k, "eq")) {
            const char *p2 = v;
            for (int i = 0; i < EQ_BANDS && *p2; i++) {
                S.eq[i] = strtof(p2, (char **)&p2);
                while (*p2 == ',' || *p2 == ' ') p2++;
            }
        }
        else if (!strcmp(k, "eq_preset")) snprintf(S.eq_preset, sizeof S.eq_preset, "%s", v);
        else if (!strcmp(k, "shuffle")) S.shuffle = atoi(v);
        else if (!strcmp(k, "repeat")) S.repeat = atoi(v);
        else if (!strcmp(k, "vis_mode")) S.vis_mode = atoi(v);
        else if (!strcmp(k, "vis_preset")) S.vis_preset = atoi(v);
        else if (!strcmp(k, "remaining")) S.remaining = atoi(v);
        else if (!strcmp(k, "skin_smooth")) S.skin_smooth = atoi(v);
        else if (!strcmp(k, "list_text_size")) S.list_size = atoi(v);
        else if (!strcmp(k, "play_standby")) S.play_standby = strcmp(v,"0") != 0;
        else if (!strcmp(k, "insecure_https")) S.insecure_https = !strcmp(v,"1");
        else if (!strcmp(k, "cover_fetch")) S.cover_fetch = atoi(v);
        else if (!strcmp(k, "cover_save_to_folder")) S.cover_save_to_folder = atoi(v);
        else if (!strcmp(k, "cover_player")) S.cover_player = atoi(v);
        else if (!strcmp(k, "cover_library")) S.cover_library = atoi(v);
        else if (!strcmp(k, "playlist_expanded")) S.playlist_expanded = atoi(v);
        else if (!strcmp(k, "last_view")) S.last_view = atoi(v);
        else if (!strcmp(k, "skin")) snprintf(S.skin, sizeof S.skin, "%s", v);
        else if (!strcmp(k, "last_dir")) snprintf(S.last_dir, sizeof S.last_dir, "%s", v);
        else if (!strcmp(k, "cur_index")) S.cur_index = atoi(v);
        else if (!strcmp(k, "cur_pos_ms")) S.cur_pos_ms = atoi(v);
        else if (!strcmp(k, "game_music")) S.game_music = atoi(v);
        else if (!strcmp(k, "game_mix_volume")) S.game_mix_volume = atoi(v);
        else if (!strcmp(k, "game_hotkeys")) S.game_hotkeys = atoi(v);
        else if (!strcmp(k, "resume_playing")) S.resume_playing = atoi(v);
    }
    fclose(f);
    // A damaged settings file must never drive the audio engine with wild
    // volumes, NaNs, or out-of-range visualizer indices.
    if (S.volume < 0) S.volume = 0;
    if (S.volume > 100) S.volume = 100;
    if (S.balance < -100) S.balance = -100;
    if (S.balance > 100) S.balance = 100;
    S.eq_on = !!S.eq_on;
    if (!isfinite(S.eq_pre)) S.eq_pre = 0;
    if (S.eq_pre < -12) S.eq_pre = -12;
    if (S.eq_pre > 12) S.eq_pre = 12;
    for (int i = 0; i < EQ_BANDS; i++) {
        if (!isfinite(S.eq[i])) S.eq[i] = 0;
        if (S.eq[i] < -12) S.eq[i] = -12;
        if (S.eq[i] > 12) S.eq[i] = 12;
    }
    S.shuffle = !!S.shuffle;
    if (S.repeat < 0 || S.repeat > 2) S.repeat = 0;
    if (S.vis_mode < 0 || S.vis_mode > 2) S.vis_mode = 1;
    if (S.vis_preset < 0 || S.vis_preset >= 5) S.vis_preset = 0;
    S.remaining = !!S.remaining;
    S.skin_smooth = !!S.skin_smooth;
    if (S.list_size < 0 || S.list_size > 2) S.list_size = 1;
    S.cover_fetch = !!S.cover_fetch;
    S.cover_save_to_folder = !!S.cover_save_to_folder;
    S.cover_player = !!S.cover_player;
    S.cover_library = !!S.cover_library;
    S.playlist_expanded = !!S.playlist_expanded;
    S.last_view = !!S.last_view;
    S.game_music = !!S.game_music;
    if (S.game_mix_volume < 0) S.game_mix_volume = 0;
    if (S.game_mix_volume > 100) S.game_mix_volume = 100;
    S.game_hotkeys = !!S.game_hotkeys;
    // RedAmp requires a separate consent marker: a copied/stale INI cannot
    // enable shared-system-library replacement by itself.
    char consent[1024];
    path_join(consent, sizeof consent, data_dir, "game_music.consent");
    if (access(consent, F_OK) != 0) S.game_music = 0;
    S.resume_playing = !!S.resume_playing;
    if (S.cur_index < -1) S.cur_index = -1;
    if (S.cur_pos_ms < 0) S.cur_pos_ms = 0;
}

/* Save atomically, and do not replace a known-good INI after an SD write
 * error. mkstemp avoids competing writers reusing the same .tmp name. */
void settings_save(void)
{
    char p[1024], tmp[1100];
    path_join(p, sizeof p, data_dir, "settings.ini");
    if (snprintf(tmp, sizeof tmp, "%s.tmp.XXXXXX", p) >= (int)sizeof tmp) return;
    int fd = mkstemp(tmp);
    if (fd < 0) { perror("[settings] mkstemp"); return; }
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); unlink(tmp); return; }
    int ok = fprintf(f, "volume=%d\nbalance=%d\neq_on=%d\neq_pre=%.1f\neq=", S.volume, S.balance, S.eq_on, S.eq_pre) >= 0;
    for (int i = 0; i < EQ_BANDS; i++)
        if (fprintf(f, "%s%.1f", i ? "," : "", S.eq[i]) < 0) ok = 0;
    if (fprintf(f, "\neq_preset=%s\nshuffle=%d\nrepeat=%d\nvis_mode=%d\nvis_preset=%d\nremaining=%d\nskin_smooth=%d\nlist_text_size=%d\ninsecure_https=%d\nplay_standby=%d\ncover_fetch=%d\ncover_save_to_folder=%d\ncover_player=%d\ncover_library=%d\nplaylist_expanded=%d\nlast_view=%d\nskin=%s\nlast_dir=%s\ncur_index=%d\ncur_pos_ms=%d\ngame_music=%d\ngame_mix_volume=%d\ngame_hotkeys=%d\nresume_playing=%d\n",
                S.eq_preset, S.shuffle, S.repeat, S.vis_mode, S.vis_preset, S.remaining, S.skin_smooth, S.list_size, S.insecure_https, S.play_standby, S.cover_fetch, S.cover_save_to_folder,
                S.cover_player, S.cover_library, S.playlist_expanded, S.last_view, S.skin, S.last_dir, S.cur_index, S.cur_pos_ms, S.game_music, S.game_mix_volume, S.game_hotkeys, S.resume_playing) < 0) ok = 0;
    if (ferror(f) || fflush(f) != 0 || fsync(fd) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (ok && rename(tmp, p) == 0) return;
    fprintf(stderr, "[settings] save failed, preserving old settings: %s\n", strerror(errno));
    unlink(tmp);
}


void core_build_shuffle(void)
{
    int n = pl_count();
    free(shuffle_order);
    shuffle_order = NULL;
    int *order = malloc(sizeof(int) * (n ? n : 1));
    if (!order) { shuffle_n = 0; S.shuffle = 0; return; }
    shuffle_order = order;
    shuffle_n = n;
    for (int i = 0; i < n; i++) shuffle_order[i] = i;
    for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1); int t = shuffle_order[i]; shuffle_order[i] = shuffle_order[j]; shuffle_order[j] = t; }
    for (int i = 0; i < n; i++) if (shuffle_order[i] == current) { shuffle_order[i] = shuffle_order[0]; shuffle_order[0] = current; break; }
}

int core_play_index(int i, int start_ms, int paused)
{
    const char *p = pl_path(i);
    if (!p) return 0;
    current = i;
    audio_play_file(p, start_ms, paused);
    S.cur_index = i;
    if (S.shuffle && shuffle_n != pl_count()) core_build_shuffle();
    return 1;
}

int core_next_index(int dir)
{
    int n = pl_count();
    if (n == 0) return -1;
    if (S.shuffle) {
        if (shuffle_n != n) core_build_shuffle();
        if (!shuffle_order) return current + dir >= n ? (S.repeat ? 0 : -1) :
                                   current + dir < 0 ? 0 : current + dir;
        int k = 0;
        for (int i = 0; i < n; i++) if (shuffle_order[i] == current) { k = i; break; }
        k += dir;
        if (k >= n) { if (!S.repeat) return -1; core_build_shuffle(); k = shuffle_order[0] == current && n > 1 ? 1 : 0; }
        if (k < 0) k = 0;
        return shuffle_order[k];
    }
    int k = current + dir;
    if (k >= n) return S.repeat ? 0 : -1;
    if (k < 0) return S.repeat ? n - 1 : 0;
    return k;
}

void core_skip(int dir)
{
    int k = core_next_index(dir);
    if (dir < 0 && audio_pos_ms() > 3000 && current >= 0) k = current;   // "previous" restarts first
    if (k < 0) return;
    core_play_index(k, 0, 0);
}

void core_toggle_pause(void)
{
    int st = audio_state();
    if (st == A_PLAYING) audio_set_paused(1);
    else if (st == A_PAUSED) audio_set_paused(0);
    else if (pl_count()) core_play_index(current >= 0 ? current : 0, 0, 0);
}

void core_apply_audio(void)
{
    audio_set_volume(S.volume);
    audio_set_balance(S.balance);
    audio_set_eq(S.eq_on, S.eq_pre, S.eq);
}

// handles a finished track; returns 1 if something new started
int core_track_finished(void)
{
    if (S.repeat == 2 && current >= 0) { core_play_index(current, 0, 0); return 1; }
    int k = core_next_index(1);
    if (k >= 0) { core_play_index(k, 0, 0); return 1; }
    S.cur_pos_ms = 0;
    return 0;
}
