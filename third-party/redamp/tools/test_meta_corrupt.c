#include <stdio.h>
#include <string.h>
#include <assert.h>
/* Include file directly to exercise the formerly static Vorbis parser using
 * large untrusted uint32 values, including on 64-bit desktop builds. */
#include "../meta.c"
int main(void) {
    track_meta m={0};
    unsigned char buf[80]={0};
    /* Vendor-length used to become negative on 32-bit long. */
    buf[3]=0x80;
    vorbis_comments(&m,buf,sizeof buf);
    assert(!m.title[0]);
    memset(buf,0,sizeof buf);
    buf[0]=1; /* vendor length 1 */
    buf[5]=1; /* one comment */
    buf[9]=11; /* length of TITLE=Hello */
    memcpy(buf+13,"TITLE=Hello",11);
    vorbis_comments(&m,buf,sizeof buf);
    assert(!strcmp(m.title,"Hello"));
    memset(buf,0,sizeof buf);
    buf[0]=1;buf[5]=1;
    buf[12]=0x80; /* invalid massive comment size */
    vorbis_comments(&m,buf,sizeof buf);
    assert(!strcmp(m.title,"Hello"));
    puts("Corrupt/large Vorbis comment lengths: PASS");
    return 0;
}
