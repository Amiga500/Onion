/* Mock-backed integration test of decode.c's new FFmpeg routing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"
#include "meta.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}}while(0)
int main(void) {
    CHECK(dec_supported("album/song.m4a"));
    CHECK(dec_supported("album/song.aac"));
    CHECK(dec_supported("http://example.invalid/radio.m3u8"));
    CHECK(dec_ffmpeg_source("album/song.m4a"));
    CHECK(!dec_ffmpeg_source("album/song.ogg"));
    dec_info info={0};
    decoder *d=dec_open("/tmp/mock.m4a",&info);
    CHECK(d);
    CHECK(info.samplerate==48000 && info.channels==2);
    float f[1024*2];
    CHECK(dec_read(d,f,1024)==1024);
    CHECK(f[0]==0.25f && f[2047]==0.25f);
    CHECK(dec_seek(d,2000)==2000);
    dec_close(d);
    puts("AAC/M4A decode.c routing and 48 kHz stereo: PASS");
    return 0;
}
