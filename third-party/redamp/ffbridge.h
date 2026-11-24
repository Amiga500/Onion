#ifndef REDAMP_FFBRIDGE_H
#define REDAMP_FFBRIDGE_H
#include "ffaudio.h"
#include <stdatomic.h>
/* ABI shared between RedAmp and its privately compiled FFmpeg bridge.
 * FFmpeg structs do not cross this boundary. */
void *ffbridge_open_tls(const char *,int,const atomic_int *,ffaudio_info *,char *,size_t,int);
int ffbridge_ready(void);
void *ffbridge_open(const char *,int,const atomic_int *,ffaudio_info *,char *,size_t);
int ffbridge_read(void *,float *,int);
int64_t ffbridge_seek(void *,int64_t);
void ffbridge_title(void *,char *,size_t);
void ffbridge_close(void *);
int ffbridge_metadata(const char *,track_meta *);
#endif
