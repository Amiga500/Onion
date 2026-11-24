#ifndef REDAMP_RADIO_H
#define REDAMP_RADIO_H
#include <stddef.h>
typedef struct radio_stream radio_stream;
int radio_is_url(const char *url);
radio_stream *radio_open(const char *url);
size_t radio_read(radio_stream *r,void *dst,size_t bytes);
int radio_probe(radio_stream *r,unsigned char *out,size_t n);
int radio_is_ffmpeg_stream(radio_stream *r); /* AAC/HLS HTTP Content-Type hint */
void radio_close(radio_stream *r);
/* Can be called from another thread to interrupt pending network reads. */
void radio_abort_active(void);
int radio_take_network_error(void); /* 1 DNS, 2 connect, 3 invalid clock, 4 TLS verification, 5 timeout, 6 server/stream error */
void radio_title(radio_stream *r,char *out,size_t cap);
void radio_set_title(radio_stream *r,const char *title);
#endif
