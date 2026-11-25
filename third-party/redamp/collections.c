/* Main-thread saved M3U collections, independent of the live playback queue. */
#include "collections.h"
#include "utf8_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
void col_clear(collection *c){
    for(int i=0;i<c->count;i++){free(c->items[i].path);free(c->items[i].title);}
    free(c->items);memset(c,0,sizeof *c);
}
int col_find(const collection *c,const char *path){
    if(path)for(int i=0;i<c->count;i++)if(!strcmp(c->items[i].path,path))return i;
    return -1;
}
int col_add(collection *c,const char *path,const char *title){
    if(!path||!path[0]||strlen(path)>2046||strpbrk(path,"\r\n")||c->count>=COL_MAX)return 0;
    if(c->count==c->cap){int cap=c->cap?c->cap*2:32;if(cap>COL_MAX)cap=COL_MAX;
        col_track *p=realloc(c->items,cap*sizeof *p);if(!p)return 0;c->items=p;c->cap=cap;}
    char *p=strdup(path),*t=strdup(title?title:"");
    if(!p||!t){free(p);free(t);return 0;}
    t[strcspn(t,"\r\n")]=0;c->items[c->count++]=(col_track){p,t};return 1;
}
void col_remove(collection *c,int row){
    if(row<0||row>=c->count)return;
    free(c->items[row].path);free(c->items[row].title);
    memmove(c->items+row,c->items+row+1,(--c->count-row)*sizeof *c->items);
}
int col_read(collection *c,const char *file){
    FILE *f=fopen(file,"r");if(!f)return 0;
    collection next={0};char line[4096],title[512]="";int ok=1;
    while(fgets(line,sizeof line,f)){
        if(!strchr(line,'\n')&&!feof(f)){ok=0;break;}
        line[strcspn(line,"\r\n")]=0;
        if(!strncmp(line,"#EXTINF:",8)){char *p=strchr(line,',');snprintf(title,sizeof title,"%.511s",p?p+1:"");continue;}
        if(!line[0]||line[0]=='#')continue;
        if(line[0]!='/'&&strncmp(line,"http://",7)&&strncmp(line,"https://",8)){ok=0;break;}
        if(!col_add(&next,line,title)){ok=0;break;}title[0]=0;
    }
    if(ferror(f))ok=0;
    fclose(f);if(ok){col_clear(c);*c=next;}else col_clear(&next);return ok;
}
int col_write(const collection *c,const char *file){
    char tmp[1200];if(snprintf(tmp,sizeof tmp,"%s.tmp.XXXXXX",file)>=(int)sizeof tmp)return 0;
    int fd=mkstemp(tmp);if(fd<0)return 0;FILE *f=fdopen(fd,"w");
    if(!f){close(fd);unlink(tmp);return 0;}
    int ok=fputs("#EXTM3U\n",f)>=0;
    for(int i=0;ok&&i<c->count;i++){
        const col_track *t=c->items+i;
        if(t->title[0]&&fprintf(f,"#EXTINF:-1,%s\n",t->title)<0)ok=0;
        if(fprintf(f,"%s\n",t->path)<0)ok=0;
    }
    if(fflush(f)||fsync(fd))ok=0;
    if(fclose(f))ok=0;
    if(ok&&rename(tmp,file)==0)return 1;
    unlink(tmp);return 0;
}
int col_valid_name(const char *name){
    size_t n=strlen(name);int chars=text_count(name);
    if(!n||n>COL_NAME||chars<1||chars>COL_NAME_CHARS||name[0]==' '||name[0]=='.'||
       name[n-1]==' '||name[n-1]=='.')return 0;
    const char *p=name;uint32_t cp;
    while(text_next(&p,&cp)>0){
        if(cp<32||(cp>=127&&cp<=159)||(cp<128&&strchr("<>:\"/\\|?*",(int)cp)))return 0;
    }
    /* FAT/DOS reserved basenames also remain reserved with extensions. */
    size_t stem=strcspn(name,".");while(stem&&name[stem-1]==' ')stem--;
    char base[COL_NAME+1];memcpy(base,name,stem);base[stem]=0;
    if(!strcasecmp(base,"CON")||!strcasecmp(base,"PRN")||!strcasecmp(base,"AUX")||
       !strcasecmp(base,"NUL")||!strcasecmp(base,"CONIN$")||!strcasecmp(base,"CONOUT$"))return 0;
    if(stem>=4&&(!strncasecmp(base,"COM",3)||!strncasecmp(base,"LPT",3))&&
       ((stem==4&&base[3]>='0'&&base[3]<='9')||!strcmp(base+3,"¹")||
        !strcmp(base+3,"²")||!strcmp(base+3,"³")))return 0;
    return 1;
}
static int cmp(const void *a,const void *b){return text_casecmp(a,b);}
int col_list(const char *dir,char names[COL_NAMES][COL_NAME+1]){
    DIR *d=opendir(dir);if(!d)return 0;int n=0;struct dirent *e;
    while(n<COL_NAMES&&(e=readdir(d))){size_t len=strlen(e->d_name);
        if(len<5||len>COL_NAME+4||strcasecmp(e->d_name+len-4,".m3u"))continue;
        char name[COL_NAME+1];memcpy(name,e->d_name,len-4);name[len-4]=0;
        if(!col_valid_name(name))continue;
        char p[1200];struct stat st;snprintf(p,sizeof p,"%s/%s",dir,e->d_name);
        if(lstat(p,&st)||!S_ISREG(st.st_mode))continue;
        /* RedAmp writes lowercase .m3u extensions. */
        if(strcmp(e->d_name+len-4,".m3u"))continue;
        strcpy(names[n++],name);
    }
    closedir(d);qsort(names,n,sizeof names[0],cmp);return n;
}
