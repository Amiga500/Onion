/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_VIDEO_H
#define MAINUI_VIDEO_H
#include <SDL.h>
#include <stdbool.h>

/* Playback of one video file for the media gallery, with Onion's
 * FFmpeg loaded by dlopen() on the decoder thread, so nothing of it is
 * needed to start MainUI and nothing is loaded until a video is shown.
 * The thread exists only while the video is open. UI thread only. */
typedef struct MainUIVideo MainUIVideo;

/* The SDL_USEREVENT code the decoder posts for a decoded frame or an error. */
#define MAINUI_VIDEO_CODE 3

/* Videos larger than this are not played (decoder memory on the Mini): every
 * video stream must state a size within it before FFmpeg probes the file,
 * which can decode, and a decoder is refused the buffer for a larger picture,
 * while probing too; cover art in the file is not decoded. */
#define MAINUI_VIDEO_MAX_WIDTH 1280
#define MAINUI_VIDEO_MAX_HEIGHT 720

/* Start playing path, scaled to fit max_width x max_height in the pixel
 * format of screen (32 bits per pixel). FFmpeg is looked for by name, then
 * in library_dir (Onion's .tmp_update/lib/parasyte; NULL for none). With a
 * sound_rate, the file's sound is decoded as 16-bit stereo at that rate, for
 * mainui_video_sound(); 0 plays it silently. NULL only when no thread or
 * memory could be had; a file or library problem shows as failed later. */
MainUIVideo *mainui_video_open(const char *path, const char *library_dir, const SDL_Surface *screen,
                               int max_width, int max_height, int sound_rate);
/* The frame to show at now: the newest one due, or the one shown before.
 * NULL before the first frame. Owned by the video; valid until the next call
 * or close. Frames are timed from the first one shown; the video repeats. */
SDL_Surface *mainui_video_frame(MainUIVideo *video, Uint32 now);
/* Why the video cannot play ("" while it can): libraries missing, the file
 * unusable or too large. Once set, no frame follows. */
const char *mainui_video_error(MainUIVideo *video);
/* Milliseconds until a decoded frame is due, 0 when one is due now, or -1
 * when none is waiting: the decoder posts an SDL_USEREVENT for each frame. */
int mainui_video_due(MainUIVideo *video, Uint32 now);
/* For snapshots: wait up to timeout_ms for the first frame or an error. */
void mainui_video_wait_first(MainUIVideo *video, Uint32 timeout_ms);
/* Pause at now, or go on from where it was paused: the picture and the
 * sound stop, and the decoder waits with the next frame. */
void mainui_video_pause(MainUIVideo *video, bool paused, Uint32 now);
bool mainui_video_paused(MainUIVideo *video);
/* While the device sleeps: the decoder thread waits, reading and decoding
 * nothing, and the picture and sound stand still; then it goes on from
 * there, still paused if it was. */
void mainui_video_hold(MainUIVideo *video, bool held, Uint32 now);
/* Jump about delta_ms (5000 for 5 s forward, say) from the frame shown, or
 * from the start before one shows, to a keyframe, shown at once: forward
 * the first at or after that time, back the last at or before it, not
 * before the start. With no keyframe after it the video starts again. Seeks
 * asked for before one took effect add up. Paused, it stays paused. */
void mainui_video_seek(MainUIVideo *video, int delta_ms);
/* The video's sound for the next length bytes, for SDL_mixer's music hook
 * (any thread), or silence while paused, before the first frame or without
 * sound. It starts where the picture is, with 200 ms held, then plays on
 * without gaps and the picture follows it, within 50 ms; it is dropped to
 * catch up only when 250 ms behind, as after the decoder fell behind. */
void mainui_video_sound(void *video, Uint8 *stream, int length);
/* Stop the decoder and free everything: thread, FFmpeg contexts, frames,
 * sound and libraries. The decoder stops at its next check: FFmpeg's reads
 * and probing check through an interrupt, and every step between them. What
 * is under way is waited for: a library being loaded, a decode call, or a
 * read blocked on the card until it returns. Unhook mainui_video_sound()
 * first. NULL is allowed. */
void mainui_video_close(MainUIVideo *video);
#endif
