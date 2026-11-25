#include "utf8_text.h"
#include <string.h>
#include "utf8_fold.inc"

int text_next(const char **p,uint32_t *cp){
    const unsigned char *s=(const unsigned char *)*p;
    unsigned a=s[0],n;uint32_t v,min;
    if(!a){*cp=0;return 0;}
    if(a<0x80){*cp=a;(*p)++;return 1;}
    if(a>=0xc2&&a<=0xdf){n=2;v=a&31;min=0x80;}
    else if(a>=0xe0&&a<=0xef){n=3;v=a&15;min=0x800;}
    else if(a>=0xf0&&a<=0xf4){n=4;v=a&7;min=0x10000;}
    else goto invalid;
    for(unsigned i=1;i<n;i++){
        if((s[i]&0xc0)!=0x80)goto invalid;
        v=(v<<6)|(s[i]&63);
    }
    if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))goto invalid;
    *p+=n;*cp=v;return 1;
invalid:
    (*p)++;*cp=0xfffd;return -1;
}
int text_count(const char *s){
    int n=0,r;uint32_t cp;
    while((r=text_next(&s,&cp))!=0){if(r<0)return -1;n++;}
    return n;
}
void text_erase(char *s){
    char *end=s+strlen(s);if(end==s)return;
    do{end--;}while(end>s&&((unsigned char)*end&0xc0)==0x80);
    *end=0;
}
int text_append(char *s,size_t cap,int max_chars,const char *key){
    int n=text_count(s),k=text_count(key);size_t a=strlen(s),b=strlen(key);
    if(n<0||k<0||n+k>max_chars||a+b>=cap)return 0;
    memcpy(s+a,key,b+1);return 1;
}
static int fold(uint32_t cp,uint32_t out[3]){
    size_t lo=0,hi=sizeof fold_ranges/sizeof *fold_ranges;
    while(lo<hi){size_t m=(lo+hi)/2;if(cp<fold_ranges[m].first)hi=m;
        else if(cp>fold_ranges[m].last)lo=m+1;
        else {if((cp-fold_ranges[m].first)%fold_ranges[m].step==0){out[0]=cp+fold_ranges[m].delta;return 1;}break;}}
    lo=0;hi=sizeof fold_multi/sizeof *fold_multi;
    while(lo<hi){size_t m=(lo+hi)/2;if(cp<fold_multi[m].cp)hi=m;
        else if(cp>fold_multi[m].cp)lo=m+1;
        else {memcpy(out,fold_multi[m].out,sizeof fold_multi[m].out);return out[2]?3:2;}}
    out[0]=cp;return 1;
}
typedef struct {const char *p;uint32_t pending[3];int next,count;} fold_iter;
static uint32_t folded_next(fold_iter *it){
    if(it->next<it->count)return it->pending[it->next++];
    uint32_t cp;if(!text_next(&it->p,&cp))return 0;
    it->count=fold(cp,it->pending);it->next=1;return it->pending[0];
}
int text_casecmp(const char *a,const char *b){
    fold_iter x={.p=a},y={.p=b};
    for(;;){uint32_t u=folded_next(&x),v=folded_next(&y);if(u!=v)return u<v?-1:1;if(!u)return 0;}
}
int text_contains(const char *text,const char *query){
    if(!*query)return 1;
    if(!text)return 0;
    /* Start at each folded character, including expansions such as ß -> ss. */
    fold_iter start={.p=text};
    for(;;){fold_iter x=start,y={.p=query};uint32_t v;
        while((v=folded_next(&y))&&folded_next(&x)==v){}
        if(!v)return 1;
        if(!folded_next(&start))return 0;
    }
}
