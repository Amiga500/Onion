#ifndef REDAMP_OPUS_OGG_H
#define REDAMP_OPUS_OGG_H
#include <stdint.h>
#include "meta.h"
typedef struct opus_local opus_local;
struct radio_stream;
opus_local *opus_local_open_radio(struct radio_stream *r,int *channels);
opus_local *opus_local_open(const char *path, int *channels, int64_t *frames);
int opus_local_read(opus_local *o, float *dst, int frames);
int64_t opus_local_seek(opus_local *o, int64_t frame);
void opus_local_close(opus_local *o);
/* Reads tags/duration without requiring libopus, for the playlist worker. */
int opus_local_metadata(const char *path, track_meta *meta);
#endif
