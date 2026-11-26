#ifndef CORE_H
#define CORE_H
#include "audio.h"
typedef struct {
    int volume, balance;
    int eq_on;
    float eq_pre, eq[EQ_BANDS];
    char eq_preset[40];
    int shuffle, repeat;          // repeat: 0 off, 1 all, 2 one
    int vis_mode;                 // main window: 1 spectrum, 2 scope, 0 off
    int vis_preset;               // full screen
    int remaining;
    int skin_smooth;             // 1 bilinear scaling, 0 classic sharp pixels
    int list_size;              // 0 small, 1 medium (default), 2 large
    int play_standby;            // 1 keep audio running with screen off (default)
    int insecure_https;          // explicit opt-in; default verified TLS
    int cover_fetch, cover_save_to_folder, cover_player, cover_library;
    int playlist_expanded;       // 1 playlist-only view
    int destination, classic_player; /* Route B destination, optional Winamp appearance */
    int last_view;               // 0 player, 1 library; restored on next launch
    char skin[512];
    char last_dir[1024];
    int cur_index, cur_pos_ms;
    int game_music;               // keep playing in games after quitting
    int game_mix_volume;          // extra 0..100% attenuation while mixing
    int game_hotkeys;             // advanced opt-in; avoid emulator/Onion clashes
    int resume_playing;           // the background service was playing when it stopped
} settings;

extern settings S;
extern char app_dir[512], data_dir[512], root_dir[512];
extern int current;     // playlist index being played

void path_join(char *out, int n, const char *a, const char *b);
void settings_load(void);
void settings_save(void);
void core_build_shuffle(void);
void core_shuffle_next(int index);
int core_play_index(int i, int start_ms, int paused);
int core_next_index(int dir);
void core_skip(int dir);
void core_toggle_pause(void);
void core_apply_audio(void);
int core_track_finished(void);

#endif
