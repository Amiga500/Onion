/* Playback smoke test against a local, deterministic HTTP radio server. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "decode.h"
int main(int argc,char **argv) {
    if (argc<2) return 2;
    for (int j=1;j<argc;j++) {
        dec_info info;decoder *d=dec_open(argv[j],&info);
        if (!d) {fprintf(stderr,"OPEN FAIL %s\n",argv[j]);return 1;}
        float buf[4096*2];int n,total=0;
        double energy=0;
        while (total<info.samplerate*2 && (n=dec_read(d,buf,4096))>0) {
            for (int i=0;i<n*info.channels;i++) energy+=buf[i]*buf[i];
            total+=n;
        }
        char name[160]; dec_radio_title(d,name,sizeof name);
        printf("RADIO %s sr=%d ch=%d frames=%d energy=%g title=%s\n",argv[j],info.samplerate,info.channels,total,energy,name);
        if (strstr(argv[j],"/icy") && strcmp(name,"RedAmp Test Track")) {
            fprintf(stderr,"ICY title mismatch: %s\n",name);
            dec_close(d); return 1;
        }
        dec_close(d);
        if (total<info.samplerate || energy<0.01) return 1;
    }
    return 0;
}
