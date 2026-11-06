/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/list_video.h"
#include <stdio.h>
#include <string.h>

/* The cover area a video is fitted to (see mainui_preview_draw()). */
enum {
    COVER_WIDTH = 250,
    COVER_HEIGHT = 360
};

static void close_video(MainUIListVideo *preview)
{
    mainui_video_close(preview->video);
    preview->video = NULL;
    if (preview->frame) {
        preview->frame = NULL;
        preview->changed = true;
    }
}

void mainui_list_video_stop(MainUIListVideo *preview)
{
    close_video(preview);
    preview->key[0] = 0;
    preview->waiting = false;
    preview->dismissed = false;
}

void mainui_list_video_dismiss(MainUIListVideo *preview)
{
    close_video(preview);
    preview->dismissed = true;
}

void mainui_list_video_hide(MainUIListVideo *preview)
{
    if (preview->dismissed) {
        close_video(preview);
    }
    else {
        mainui_list_video_stop(preview);
    }
}

void mainui_list_video_sleep(MainUIListVideo *preview)
{
    mainui_list_video_hide(preview);
}

bool mainui_list_video_fullscreen(const MainUIListVideo *preview)
{
    return preview->fullscreen && preview->frame;
}

void mainui_list_video_start(MainUIListVideo *preview, const char *key, const char *path,
                             const char *library_dir, const SDL_Surface *screen, Uint32 now,
                             bool fullscreen, bool snapshot)
{
    mainui_list_video_stop(preview);
    snprintf(preview->key, sizeof preview->key, "%s", key);
    preview->fullscreen = fullscreen;
    if (!path) {
        return;
    }
    preview->video =
        mainui_video_open(path, library_dir, screen, fullscreen ? screen->w : COVER_WIDTH,
                          fullscreen ? screen->h : COVER_HEIGHT, 0);
    preview->shown_at = now;
    preview->snapshot = snapshot;
    if (preview->video && snapshot) {
        mainui_video_wait_first(preview->video, 5000);
    }
    mainui_list_video_advance(preview, now);
}

void mainui_list_video_advance(MainUIListVideo *preview, Uint32 now)
{
    if (!preview->video) {
        return;
    }
    SDL_Surface *frame =
        mainui_video_frame(preview->video, preview->snapshot ? preview->shown_at : now);
    if (*mainui_video_error(preview->video)) {
        /* The cover stays; the reason is in the log. */
        mainui_video_close(preview->video);
        preview->video = NULL;
        frame = NULL;
    }
    if (frame != preview->frame) {
        preview->frame = frame;
        preview->changed = true;
    }
}

int mainui_list_video_due(const MainUIListVideo *preview, Uint32 now)
{
    if (preview->video) {
        return mainui_video_due(preview->video, now);
    }
    if (preview->waiting) {
        Sint32 until = (Sint32)(preview->start_at - now);
        return until > 0 ? (int)until : 0;
    }
    return -1;
}
