#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ffaudio.h"
#include "netclock.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}}while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);
    ffaudio_info info={0};char err[192]={0};
    ffaudio *f=ffaudio_open("/tmp/track.m4a",0,&info,err,sizeof err);
    if(!strcmp(argv[1],"missing")) {
        CHECK(!f);CHECK(strstr(err,"FFmpeg"));
        puts("Missing FFmpeg bridge fails closed: PASS");
        return 0;
    }
    CHECK(f);CHECK(info.samplerate==48000 && info.channels==2 && info.frames==48000);
    float sample[1024*2];
    CHECK(ffaudio_read(f,sample,1024)==1024);
    CHECK(sample[0]==0.25f && sample[2047]==0.25f);
    char title[100]={0};ffaudio_title(f,title,sizeof title);
    CHECK(!strcmp(title,"Mock Radio"));
    CHECK(ffaudio_seek(f,1200)==1200);
    ffaudio_cancel_active();
    CHECK(ffaudio_read(f,sample,1024)==0);
    ffaudio_close(f);
    track_meta meta={0};CHECK(ffaudio_metadata("/tmp/track.m4a",&meta));
    CHECK(!strcmp(meta.album,"Mock album"));
    f=ffaudio_open("https://example.invalid/radio.m3u8",1,&info,err,sizeof err);
    CHECK(f);ffaudio_close(f);
    netclock_set_insecure(1);
    setenv("REDAMP_TEST_EXPECT_INSECURE","1",1);
    f=ffaudio_open("https://example.invalid/radio.m3u8",1,&info,err,sizeof err);
    if(!strcmp(argv[1],"legacy")){CHECK(!f);CHECK(strstr(err,"updated FFmpeg"));}
    else {CHECK(f);ffaudio_close(f);}
    f=ffaudio_open("/tmp/track.m4a",0,&info,err,sizeof err);CHECK(f);ffaudio_close(f);
    puts("FFmpeg bridge loader, playback, cancel, seek, metadata and TLS policy: PASS");
    return 0;
}
