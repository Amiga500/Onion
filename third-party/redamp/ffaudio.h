#ifndef REDAMP_FFAUDIO_H
#define REDAMP_FFAUDIO_H
#include <stdint.h>
#include <stddef.h>
#include "meta.h"

typedef struct ffaudio ffaudio;
typedef struct {
    int samplerate;       /* fixed 48000 for FFmpeg decoded sources */
    int channels;         /* fixed stereo */
    int kbps;
    int64_t frames;       /* zero for live radio */
} ffaudio_info;

/* Nothing from FFmpeg's ABI is included in the player: the separate bridge
 * is built with FFmpeg 4.4.8's own headers and requires private suffixed libs. */
ffaudio *ffaudio_open(const char *path, int live, ffaudio_info *info,
                     char *error, size_t error_size);
int ffaudio_read(ffaudio *f, float *stereo, int frames);
int64_t ffaudio_seek(ffaudio *f, int64_t frame);
void ffaudio_title(ffaudio *f, char *out, size_t size);
void ffaudio_close(ffaudio *f);
void ffaudio_cancel_active(void);
int ffaudio_metadata(const char *path, track_meta *meta);
const char *ffaudio_last_error(void);
#endif
