/* Optional local MP3 music preview for AudioTune. SPDX-License-Identifier: MIT.
   The minimp3 decoder is CC0-1.0; see src/MP3-DECODER.txt. */
#ifndef AUDIOTUNE_MP3_H
#define AUDIOTUNE_MP3_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "minimp3.h"

#define AUDIOTUNE_MP3_LIMIT (16u * 1024u * 1024u)
typedef struct {
    uint8_t *bytes;
    size_t length, pos, audio_start;
    mp3dec_t decoder;
    mp3d_sample_t decoded[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int decoded_frames, decoded_at;
    int rate, channels;
    float prev[2], next[2], fraction;
    bool loaded;
} AudioTuneMP3;

bool audiotune_mp3_open(AudioTuneMP3 *mp3, const char *path);
void audiotune_mp3_close(AudioTuneMP3 *mp3);
void audiotune_mp3_rewind(AudioTuneMP3 *mp3);
/* Called in SDL's locked audio callback. Never reads from disk or allocates. */
bool audiotune_mp3_sample(AudioTuneMP3 *mp3,int output_rate,float *left,float *right);
#endif
