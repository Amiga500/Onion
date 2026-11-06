/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_LIST_VIDEO_H
#define MAINUI_LIST_VIDEO_H
#include "platform/video.h"
#include <SDL.h>
#include <stdbool.h>

/* A ROM list's video preview: once a game has stayed selected for
 * .romListVideoPreview's delay, its media/videos/<name>.mp4 plays silently
 * in place of its cover, fitted to the cover's 250x360 area, or over the
 * whole screen, fitted to it, and stops as soon as the selection or the
 * screen changes. One video at a time; its decoder thread runs only while it
 * plays. UI thread only. */
typedef struct {
    char key[4096 * 3]; /* the selection it was looked up for, "" for none */
    MainUIVideo *video;
    SDL_Surface *frame; /* shown, in place of the cover or fullscreen; owned by the video */
    Uint32 shown_at;    /* when it started */
    bool snapshot;      /* frames of the instant it started, for snapshots */
    bool changed;       /* the frame, or whether there is one, changed */
    bool waiting;       /* the selection waits for start_at */
    Uint32 start_at;
    bool fullscreen;    /* over the whole screen, not the cover */
    bool dismissed;     /* closed by a key: not again for this selection */
    bool swallow;       /* B or unbound X closed it: repeat ignored until release */
    SDLKey swallow_key; /* physical key which dismissed the fullscreen video */
} MainUIListVideo;

/* Play path (NULL for a game without a video: nothing plays) for the
 * selection key, from Onion's FFmpeg in library_dir, in place of the cover or
 * fullscreen. A snapshot waits for the first frame and keeps it. */
void mainui_list_video_start(MainUIListVideo *preview, const char *key, const char *path,
                             const char *library_dir, const SDL_Surface *screen, Uint32 now,
                             bool fullscreen, bool snapshot);
/* A fullscreen video shown now: drawn instead of the list. */
bool mainui_list_video_fullscreen(const MainUIListVideo *preview);
/* A key closed the video: it is not played again until another game is
 * selected, also after sleep. */
void mainui_list_video_dismiss(MainUIListVideo *preview);
/* Something covers or leaves the list, such as the context menu: the video
 * stops, and plays again afterwards unless it was dismissed, a dismissal
 * kept until another game is selected. */
void mainui_list_video_hide(MainUIListVideo *preview);
/* The device sleeps: as mainui_list_video_hide(). */
void mainui_list_video_sleep(MainUIListVideo *preview);
/* Take the frame due at now. A video that cannot play stops, showing the
 * cover again, and is not tried again for the same selection. */
void mainui_list_video_advance(MainUIListVideo *preview, Uint32 now);
/* Milliseconds until a frame is due or the waiting selection starts its
 * video, 0 when now, -1 for neither. */
int mainui_list_video_due(const MainUIListVideo *preview, Uint32 now);
/* Stop the video and forget the selection, and any dismissal. Safe when
 * stopped. */
void mainui_list_video_stop(MainUIListVideo *preview);
#endif
