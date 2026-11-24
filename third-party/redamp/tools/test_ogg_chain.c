#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"
int main(int argc,char **argv){
 if(argc<2) return 2;
 dec_info info;
 decoder*d=dec_open(argv[1],&info);
 if(!d){fprintf(stderr,"open fail\n");return 1;}
 float buf[2048*2];long n,total=0;double e=0;
 while(total<info.samplerate*4 && (n=dec_read(d,buf,2048))>0) {
  for(int i=0;i<n*info.channels;i++) e+=buf[i]*buf[i];
  total+=n;
 }
 char title[160];dec_radio_title(d,title,sizeof title);
 printf("CHAIN total=%ld sr=%d ch=%d energy=%.3f title=%s\n",total,info.samplerate,info.channels,e,title);
 dec_close(d);
 return total>info.samplerate*3.3 && e>0.1 && strstr(title,"Second") ? 0:1;
}
