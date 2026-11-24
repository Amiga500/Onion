#include "adts_duration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s (line %d)\n",#x,__LINE__);exit(1);}}while(0)
static void frame(FILE *f, unsigned n) {
    /* 44100 Hz, two-channel, no CRC, one AAC raw block, 100-byte frame. */
    const unsigned char head[]={0xff,0xf1,0x50,0x80,0x0c,0x9f,0xfc};
    CHECK(fwrite(head,1,7,f)==7);
    unsigned char b[93]={0};b[0]=(unsigned char)n;
    CHECK(fwrite(b,1,sizeof b,f)==sizeof b);
}
int main(void) {
    char path[]="/tmp/redamp-adts-XXXXXX";
    int fd=mkstemp(path);CHECK(fd>=0);
    FILE *f=fdopen(fd,"wb");CHECK(f);
    for(unsigned i=0;i<200;i++)frame(f,i);
    CHECK(!fclose(f));
    int64_t duration=adts_duration_ms(path);
    CHECK(duration>=4643 && duration<=4645);
    f=fopen(path,"wb");CHECK(f);CHECK(fputs("not an AAC",f)>=0);CHECK(!fclose(f));
    CHECK(adts_duration_ms(path)==0);
    unlink(path);
    puts("ADTS duration, frame count and malformed-file handling: PASS");
    return 0;
}
