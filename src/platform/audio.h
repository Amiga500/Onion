/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_AUDIO_H
#define MAINUI_AUDIO_H
#include <SDL.h>
#include <stdbool.h>

/* SDL_mixer is loaded at runtime so host previews do not require its optional
 * decoder DLLs. All functions run on the SDL/UI thread; close before SDL_Quit.
 * Open returns false on unavailable audio, with no partially owned resources. */
/* `chunk`: sample frames per mixer buffer, as Mix_OpenAudio takes. */
bool mainui_audio_open(const char *theme, const char *fallback, int volume, int chunk);
void mainui_audio_volume(int volume); /* Onion menu volume: 0..20. */
void mainui_audio_change(void);       /* One navigation sample; music keeps looping. */
void mainui_audio_close(void);
bool mainui_audio_available(void);
void mainui_audio_pause(bool paused);
/* The mixer's sample rate when a video's sound can replace the music (the
 * mixer is open, 16-bit stereo, with a music hook), else 0. */
int mainui_audio_rate(void);
/* Play fill's sound in place of the theme music, which pauses until
 * unhooked; navigation sounds still play over it. fill runs on SDL's audio
 * thread and writes length bytes of 16-bit stereo at mainui_audio_rate().
 * While the device sleeps the mixer gets silence instead. */
bool mainui_audio_hook(void (*fill)(void *, Uint8 *, int), void *context);
/* After this returns, fill no longer runs. Safe when nothing is hooked. */
void mainui_audio_unhook(void);
/* Diagnostics: change samples requested since start, counted even without
 * audio, and the menu volume (0..20, or -1 before any) requested before the
 * latest one. Lets tests check sound order without a mixer. */
unsigned mainui_audio_change_requests(int *volume);
#endif
