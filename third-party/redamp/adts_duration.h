#ifndef REDAMP_ADTS_DURATION_H
#define REDAMP_ADTS_DURATION_H
#include <stdint.h>
/* Count samples in a local ADTS AAC stream instead of trusting FFmpeg's bitrate
 * estimate. Returns duration in milliseconds, or zero if the stream is not
 * a sufficiently complete ADTS file. Safe for the metadata worker as well as
 * the decoder worker; makes no calls to FFmpeg. */
int64_t adts_duration_ms(const char *path);
/* Playback must not block for seconds on a large uncached podcast: reuse
 * scanner results from the background library worker or return unknown. */
int64_t adts_duration_playback_ms(const char *path);
#endif
