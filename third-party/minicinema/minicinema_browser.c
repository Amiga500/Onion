/* MiniCinema's tiny Linux framebuffer video browser.
 * Copyright (C) 2026 MiniCinema contributors.
 * GPL-3.0-or-later. See App/MiniCinema/LICENSE.
 *
 * No dependency on SDL, FFmpeg or glibc. Intended as a bootstrap browser on
 * the 640x480 Miyoo Mini family until our integrated SDL2 UI is ready.
 * Static ARM EABI Linux syscalls; NOT a general-purpose Linux GUI library.
 */
#include <linux/fb.h>
#include <linux/types.h>
#include <asm/fcntl.h> /* ARM-specific O_DIRECTORY / O_NOFOLLOW values */
#include "ui_assets.h"
#include "ui_latin.h"

_Static_assert(O_DIRECTORY == 040000, "ARM O_DIRECTORY must not be O_DIRECT");

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned long usize;
#include "minicinema_png.h"
#define ROOT "/mnt/SDCARD/Media/Videos"
#define SELECT_FILE "/tmp/minicinema_selected"
#define FOLDER_FILE "/mnt/SDCARD/App/MiniCinema/data/.last-folder.txt"
#define LAST_FILE "/mnt/SDCARD/App/MiniCinema/data/.last-file.txt"
#define SCROLL_FILE "/mnt/SDCARD/App/MiniCinema/data/.last-scroll.txt"
#define ERROR_FILE "/tmp/minicinema_error"
#define PROGRESS_FILE "/mnt/SDCARD/App/MiniCinema/resume/index.tsv"
#define COMPLETED_FILE "/mnt/SDCARD/App/MiniCinema/resume/completed.tsv"
#define BAD_FILE "/mnt/SDCARD/App/MiniCinema/resume/bad.tsv"
#define PLAYLIST_FILE "/mnt/SDCARD/App/MiniCinema/data/playlist.txt"
#define SETTINGS_FILE "/mnt/SDCARD/App/MiniCinema/data/player-settings.conf"
#define HELP_PNG "/mnt/SDCARD/App/MiniCinema/help/controls.png"
#define OVERLAY_DIR "/mnt/SDCARD/App/MiniCinema/overlays"
#define MAX_OVERLAYS 32
#define SETTINGS_TEMP "/mnt/SDCARD/App/MiniCinema/data/.player-settings.browser.tmp"
#define RESTART_FILE "/tmp/minicinema_restart"
#define MAX_ITEMS 512
#define NAME_MAXLEN 255
#define PATH_CAP 1024
#define AT_FDCWD (-100)
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define POLLIN 1
#define SYS_exit 1
#define SYS_read 3
#define SYS_write 4
#define SYS_openat 322
#define SYS_close 6
#define SYS_ioctl 54
#define SYS_munmap 91
#define SYS_poll 168
#define SYS_mmap2 192
#define SYS_getdents64 217
#define SYS_unlinkat 328
#define SYS_renameat 329

static inline long call1(long n, long a) {
    register long r0 __asm__("r0")=a;
    register long r7 __asm__("r7")=n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r7) : "memory", "cc");
    return r0;
}
static inline long call3(long n, long a, long b, long c) {
    register long r0 __asm__("r0")=a;
    register long r1 __asm__("r1")=b;
    register long r2 __asm__("r2")=c;
    register long r7 __asm__("r7")=n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return r0;
}
static inline long call4(long n, long a, long b, long c, long d) {
    register long r0 __asm__("r0")=a;
    register long r1 __asm__("r1")=b;
    register long r2 __asm__("r2")=c;
    register long r3 __asm__("r3")=d;
    register long r7 __asm__("r7")=n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory", "cc");
    return r0;
}
static inline long call6(long n, long a, long b, long c,
                         long d, long e, long f) {
    register long r0 __asm__("r0")=a;
    register long r1 __asm__("r1")=b;
    register long r2 __asm__("r2")=c;
    register long r3 __asm__("r3")=d;
    register long r4 __asm__("r4")=e;
    register long r5 __asm__("r5")=f;
    register long r7 __asm__("r7")=n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3),
                     "r"(r4), "r"(r5), "r"(r7) : "memory", "cc");
    return r0;
}
static int open_ro(const char *path) {
    return (int)call4(SYS_openat, AT_FDCWD, (long)path, O_RDONLY|O_CLOEXEC, 0);
}
static int open_dir(const char *path) {
    return (int)call4(SYS_openat, AT_FDCWD, (long)path, O_RDONLY|O_DIRECTORY|O_CLOEXEC, 0);
}
static int write_file(const char *path, const char *data) {
    int fd=(int)call4(SYS_openat, AT_FDCWD, (long)path, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0644);
    usize n=0;
    if (fd<0) return -1;
    while (data[n]) ++n;
    if (call3(SYS_write,fd,(long)data,n)!=(long)n) {call1(SYS_close,fd); return -1;}
    call1(SYS_close,fd);
    return 0;
}
static int read_file(const char *path, char *dst, unsigned cap) {
    int fd=open_ro(path);
    long n;
    unsigned i;
    if (fd<0 || cap<2) return -1;
    n=call3(SYS_read,fd,(long)dst,cap-1);
    call1(SYS_close,fd);
    if (n<=0) return -1;
    dst[n]=0;
    for (i=0;i<(unsigned)n;++i) if (dst[i]=='\n' || dst[i]=='\r') {dst[i]=0; break;}
    return 0;
}
static usize str_len(const char *s) {usize n=0;while(s[n])++n;return n;}
static int str_copy(char *to, usize capacity, const char *src) {
    usize i=0;
    if (!capacity) return -1;
    while (src[i] && i+1<capacity) {to[i]=src[i];++i;}
    to[i]=0;
    return src[i]?-1:0;
}
static u8 fold(u8 c){return c>='A'&&c<='Z'?c+32:c;}
static int ci_cmp(const char *a,const char *b){
    while (*a && *b && fold((u8)*a)==fold((u8)*b)){++a;++b;}
    return (int)fold((u8)*a)-(int)fold((u8)*b);
}
/* Natural ASCII ordering: Episode 2 before Episode 10, without integer overflow. */
static int natural_cmp(const char *a, const char *b){
    while (*a && *b) {
        if (*a>='0' && *a<='9' && *b>='0' && *b<='9') {
            const char *ae=a,*be=b,*an=a,*bn=b;
            while(*ae>='0' && *ae<='9')++ae;
            while(*be>='0' && *be<='9')++be;
            while(an<ae-1 && *an=='0')++an;
            while(bn<be-1 && *bn=='0')++bn;
            usize al=(usize)(ae-an),bl=(usize)(be-bn);
            if(al!=bl)return al<bl?-1:1;
            for(usize i=0;i<al;i++)if(an[i]!=bn[i])return an[i]<bn[i]?-1:1;
            /* Equal numeric value: use fewer leading zeroes first. */
            if(ae-a!=be-b)return ae-a<be-b?-1:1;
            a=ae;b=be;continue;
        }
        if(fold((u8)*a)!=fold((u8)*b))return (int)fold((u8)*a)-(int)fold((u8)*b);
        ++a;++b;
    }
    return (int)fold((u8)*a)-(int)fold((u8)*b);
}
static int str_equal(const char *a,const char *b){
    while(*a&&*b&&*a==*b){++a;++b;}return *a==*b;
}
static int path_under_root(const char *p){
    usize i=0;
    while(ROOT[i] && ROOT[i]==p[i])++i;
    return ROOT[i]==0 && (p[i]==0||p[i]=='/');
}
/* Direct stderr diagnostics (no libc): the launcher captures these in
 * App/MiniCinema/reports/last-play.log only with RedOnion logging enabled.
 * Kernel errors are returned as -errno. */
static void diag_puts(const char *s){
    call3(SYS_write,2,(long)s,str_len(s));
}
static void scan_error(const char *operation,long result,const char *path){
    char digits[12];unsigned n=0;
    unsigned err=(unsigned)(result<0?-result:result);
    diag_puts("MiniCinema browser: ");
    diag_puts(operation);
    diag_puts(" failed: ");
    diag_puts(path);
    diag_puts(" (errno=");
    do{digits[n++]=(char)('0'+err%10);err/=10;}while(err&&n<sizeof(digits));
    while(n){char c=digits[--n];call3(SYS_write,2,(long)&c,1);}
    diag_puts(")\n");
}
static int append_path(char *out,usize cap,const char *dir,const char *name){
    usize i=0, j=0;
    while(dir[i]&&i+1<cap){out[i]=dir[i];++i;}
    if(dir[i] || i+1>=cap) return -1;
    out[i++]='/';
    while(name[j]&&i+1<cap)out[i++]=name[j++];
    out[i]=0;
    return name[j]?-1:0;
}
static int has_video_ext(const char *name){
    usize n=str_len(name),i;
    const char *ext;
    if (n<4) return 0;
    for (i=n;i>0;--i) if(name[i-1]=='.') break;
    if(i==0) return 0;
    ext=name+i;
    return ci_cmp(ext,"mp4")==0||ci_cmp(ext,"mkv")==0||ci_cmp(ext,"avi")==0||
           ci_cmp(ext,"mov")==0||ci_cmp(ext,"m4v")==0||ci_cmp(ext,"webm")==0||
           ci_cmp(ext,"mpg")==0||ci_cmp(ext,"mpeg")==0||ci_cmp(ext,"wmv")==0||
           ci_cmp(ext,"flv")==0||ci_cmp(ext,"ts")==0||ci_cmp(ext,"3gp")==0||
           ci_cmp(ext,"ogv")==0;
}
struct item {char name[256];u8 is_dir;u8 resumable;u8 percent;u8 done;u8 bad;};
static struct item items[MAX_ITEMS];
static unsigned nitems, cursor, scrollpos;
#define ROWS_PER_PAGE 8
static char cwd[PATH_CAP], filepath[PATH_CAP];
static int omitted;
static int show_error;
static char browser_error[96];
struct linux_dirent64 {unsigned long long ino;long long off;u16 reclen;u8 type;char name[];};
/* Battery is only sampled when Y is pressed. Onion/RedOnion publishes the
 * preferred percentage in /tmp/percBat. Some devices expose a sysfs battery. */
static int percent_text(const char *text){
    unsigned value=0,digits=0;
    while(*text==' '||*text=='\t')text++;
    while(*text>='0' && *text<='9'){
        value=value*10u+(unsigned)(*text++-'0');
        if(++digits>3 || value>100)return -1;
    }
    while(*text==' '||*text=='\t')text++;
    return digits && !*text ? (int)value : -1;
}
static int read_battery_level(void){
    char buf[64];
    if(read_file("/tmp/percBat",buf,sizeof(buf))==0){
        int n=percent_text(buf);
        if(n>=0)return n;
    }
    int fd=open_dir("/sys/class/power_supply");
    if(fd<0)return -1;
    char entries[2048];
    long count;
    while((count=call3(SYS_getdents64,fd,(long)entries,sizeof(entries)))>0){
        int pos=0;
        while(pos<count){
            struct linux_dirent64 *e=(struct linux_dirent64*)(entries+pos);
            char base[256],path[320],type[64];
            int n;
            if(e->reclen<20 || e->reclen>count-pos){
                call1(SYS_close,fd);return -1;
            }
            pos+=e->reclen;
            if(e->name[0]=='.')continue;
            if(append_path(base,sizeof(base),"/sys/class/power_supply",e->name)<0 ||
               append_path(path,sizeof(path),base,"type")<0 ||
               read_file(path,type,sizeof(type))<0 || ci_cmp(type,"Battery")!=0 ||
               append_path(path,sizeof(path),base,"capacity")<0 ||
               read_file(path,buf,sizeof(buf))<0)continue;
            n=percent_text(buf);
            if(n>=0){call1(SYS_close,fd);return n;}
        }
    }
    call1(SYS_close,fd);
    return -1;
}
static void uint_to_text(unsigned value,char *out);
static int battery_visible, battery_value=-1;
static int restart_confirm;
static int preview_index=-1;
static int preview_ready=-1;
static int previews_enabled=1;
/* Settings: -1 root; 0 playback, 1 controls, 2 audio, 3 subtitles,
 * 4 browser, 5 power & battery; Help is a root-only action. */
static int settings_open,settings_page=-1,settings_selection;
static int opt_b_second_exit=1;
static int opt_auto_next,opt_sub_delay,opt_sub_size=1,opt_sub_position;
static int opt_audio_delay,opt_auto_preview=-1,opt_video_scaler=1,opt_default_aspect;
static int opt_performance_mode; /* Quality=0, Balanced=1 (default), Speed=2. */
static int opt_pause_dim_seconds=30, opt_night_mode;
static int opt_sleep_timer_minutes; /* -1: end of video; 0: off */
static int opt_audio_language, opt_subtitle_language;
static int opt_low_battery_notice, opt_x_action, opt_osd_seconds=2, opt_subtitles_enabled=1;
static char opt_screen_overlay[64];
static char overlay_names[MAX_OVERLAYS][64];
static unsigned overlay_count;
static int help_open, help_ready;
static const char *x_action_names[] = {
    "Off", "Audio track", "Subtitles", "Screen overlay", "Mute",
    "Cycle overlays", "Live stats", "Close OSD", "Restart video",
    "Next video", "Next chapter", "Exit player", "Next frame", "Take preview"
};
#define MC_X_ACTION_COUNT (sizeof(x_action_names)/sizeof(x_action_names[0]))
/* Auto first; English, Spanish, French, German; remaining names A-Z. */
static const char *mc_languages[]={"Auto","English","Spanish","French","German",
    "Arabic","Chinese","Danish","Dutch","Finnish","Italian","Japanese",
    "Norwegian","Polish","Portuguese","Swedish","Turkish"};
static const char *mc_language_codes[]={"auto","eng","spa","fra","deu",
    "ara","zho","dan","nld","fin","ita","jpn","nor","pol","por","swe","tur"};
#define MC_LANG_COUNT (sizeof(mc_language_codes)/sizeof(mc_language_codes[0]))
static int mc_language_index(const char *name){
    for(unsigned i=0;i<MC_LANG_COUNT;i++)if(str_equal(name,mc_language_codes[i]))return (int)i;
    if(str_equal(name,"dut"))return 8; /* Dutch */
    if(str_equal(name,"chi"))return 6; /* Chinese */
    return 0;
}
static void copy_overlay_name(char *dest,const char *src){
    usize i=0;
    while(src[i] && i<63){dest[i]=src[i];i++;}
    dest[i]=0;
}
static int is_png_file(const char *n){
    usize len=str_len(n);
    return len>4 && n[len-4]=='.' && fold((u8)n[len-3])=='p' &&
           fold((u8)n[len-2])=='n' && fold((u8)n[len-1])=='g';
}
static void load_overlay_names(void){
    char buf[4096];int fd=open_dir(OVERLAY_DIR);long n;
    overlay_count=0;
    if(fd<0)return;
    while((n=call3(SYS_getdents64,fd,(long)buf,sizeof(buf)))>0){
        int off=0;
        while(off<n){
            struct linux_dirent64 *e=(struct linux_dirent64*)(buf+off);
            if(e->reclen<20 || e->reclen>n-off)goto end;
            off+=e->reclen;
            if(e->name[0]=='.' || (e->type!=8 && e->type!=0) ||
               !is_png_file(e->name) || str_len(e->name)>=64)continue;
            if(overlay_count>=MAX_OVERLAYS)goto end;
            unsigned i=overlay_count++;
            while(i>0 && ci_cmp(overlay_names[i-1],e->name)>0){
                copy_overlay_name(overlay_names[i],overlay_names[i-1]);i--;
            }
            copy_overlay_name(overlay_names[i],e->name);
        }
    }
end:
    call1(SYS_close,fd);
}
static int opt_startup_osd_immediate;
static int opt_start_aspect_shortcut=1, opt_a_pause_shortcut;

static int remember_resume=1;
static int shoulder_l1_r1=0, shoulder_l2_r2=0;
#define FOLDER_CACHE_SIZE 32
static struct {char path[PATH_CAP];unsigned count;int valid;} folder_cache[FOLDER_CACHE_SIZE];
static unsigned folder_cache_next;
static unsigned count_folder_videos(const char *path){
    char buf[4096], child[PATH_CAP];long n;unsigned total=0;
    int fd;
    for(unsigned i=0;i<FOLDER_CACHE_SIZE;i++)
        if(folder_cache[i].valid && str_equal(folder_cache[i].path,path))
            return folder_cache[i].count;
    fd=open_dir(path);
    if(fd<0)return 0;
    while((n=call3(SYS_getdents64,fd,(long)buf,sizeof(buf)))>0){
        long off=0;
        while(off<n){
            struct linux_dirent64 *e=(struct linux_dirent64*)(buf+off);
            if(e->reclen<20 || e->reclen>n-off)goto done;
            off+=e->reclen;
            if(e->name[0]=='.' || !has_video_ext(e->name) || e->type==4)continue;
            if(e->type==0){
                if(append_path(child,sizeof(child),path,e->name)<0)continue;
                int check=open_dir(child);
                if(check>=0){call1(SYS_close,check);continue;}
            }
            total++;
        }
    }
done:
    call1(SYS_close,fd);
    unsigned index=folder_cache_next++%FOLDER_CACHE_SIZE;
    folder_cache[index].valid=str_copy(folder_cache[index].path,PATH_CAP,path)==0;
    folder_cache[index].count=total;
    return total;
}

static int scan_dir(void){
    int fd=open_dir(cwd), r;
    char buf[4096];
    nitems=cursor=scrollpos=0;omitted=0;preview_index=-1;preview_ready=-1;
    if(fd<0){scan_error("open directory",fd,cwd);return -1;}
    while((r=(int)call3(SYS_getdents64,fd,(long)buf,sizeof(buf)))>0){
        int p=0;
        while(p<r){
            struct linux_dirent64 *e=(struct linux_dirent64*)(buf+p);
            int dir=0;
            if(e->reclen<20 || e->reclen>r-p){
                diag_puts("MiniCinema browser: invalid directory entry\n");
                call1(SYS_close,fd);
                return -1;
            }
            p+=e->reclen;
            if(e->name[0]=='.') continue; /* hides dotfiles and traversal */
            if(str_len(e->name)>NAME_MAXLEN) continue;
            if(e->type==4) dir=1;
            else if(e->type==8) dir=0;
            else {
                int f;
                if(append_path(filepath,sizeof(filepath),cwd,e->name)<0)continue;
                f=open_dir(filepath);
                if(f>=0){dir=1;call1(SYS_close,f);}
            }
            if(dir && ci_cmp(e->name,"Imgs")==0)continue;
            if(!dir&&!has_video_ext(e->name))continue;
            if(nitems>=MAX_ITEMS){omitted=1;continue;}
            str_copy(items[nitems].name,sizeof(items[nitems].name),e->name);
            items[nitems].is_dir=dir;
            items[nitems].resumable=0;
            items[nitems].percent=0;
            items[nitems].done=0;
            nitems++;
        }
    }
    if(r<0)scan_error("read directory",r,cwd);
    call1(SYS_close,fd);
    if(r<0)return -1;
    /* Stable, case-insensitive directory-first insertion sort. */
    for(unsigned i=1;i<nitems;++i){
        struct item tmp=items[i];
        unsigned j=i;
        while(j>0 && ((items[j-1].is_dir<tmp.is_dir)||
              (items[j-1].is_dir==tmp.is_dir && natural_cmp(items[j-1].name,tmp.name)>0))){
            items[j]=items[j-1];--j;
        }
        items[j]=tmp;
    }
    return 0;
}
/* Both history indexes may grow beyond 64 KiB. Stream line-by-line rather
 * than silently ignoring newer entries. Skip overlong/corrupt records. */
static void apply_history_line(char *line, int completed){
    char *tab=0,*tab2=0;
    unsigned seconds=0,duration=0;
    if(completed == 0){
        for(char *q=line;*q;q++)if(*q=='\t'){
            if(!tab)tab=q;else if(!tab2)tab2=q;
        }
        if(!tab)return;
        *tab++=0;
        if(tab2)*tab2++=0;
        for(char *q=tab;*q>='0'&&*q<='9';q++){
            if(seconds>100000000)return;
            seconds=seconds*10+(unsigned)(*q-'0');
        }
        if(tab2)for(char *q=tab2;*q>='0'&&*q<='9';q++){
            if(duration>100000000)break;
            duration=duration*10+(unsigned)(*q-'0');
        }
        if(!seconds)return;
    }
    for(unsigned i=0;i<nitems;i++){
        char full[PATH_CAP];
        if(items[i].is_dir || append_path(full,sizeof(full),cwd,items[i].name)<0 ||
           !str_equal(line,full))continue;
        if(completed == 1) items[i].done=1;
        else if(completed == 2) items[i].bad=1;
        else {
            items[i].resumable=1;
            if(duration){
                u32 pct=seconds<=1000000 && duration<=1000000 ?
                        seconds*100u/duration : 0;
                items[i].percent=(u8)(pct>99?99:pct);
                if(!items[i].percent)items[i].percent=1;
            }
        }
        break;
    }
}
static void scan_history(const char *path,int completed){
    char readbuf[4096],line[PATH_CAP+48];
    unsigned used=0;int overflow=0;
    int fd=open_ro(path);
    if(fd<0)return;
    for(;;){
        long n=call3(SYS_read,fd,(long)readbuf,sizeof(readbuf));
        if(n<=0)break;
        for(long k=0;k<n;k++){
            char c=readbuf[k];
            if(c=='\n'){
                if(!overflow && used){line[used]=0;apply_history_line(line,completed);}
                used=0;overflow=0;
            }else if(!overflow){
                if(used+1<sizeof(line))line[used++]=c;
                else overflow=1;
            }
        }
    }
    if(!overflow && used){line[used]=0;apply_history_line(line,completed);}
    call1(SYS_close,fd);
}
static void load_resume_marks(void){
    for(unsigned i=0;i<nitems;i++){
        items[i].resumable=0;items[i].percent=0;items[i].done=0;items[i].bad=0;
    }
    if(remember_resume){
        scan_history(PROGRESS_FILE,0);
        scan_history(COMPLETED_FILE,1);
        /* A new saved position takes precedence over an older END record,
         * including if the completion index could not be updated. */
        for(unsigned i=0;i<nitems;i++)if(items[i].resumable)items[i].done=0;
    }
    scan_history(BAD_FILE,2);
}
/* The browser has no libc. Read the small global preferences file using
 * direct syscalls; reject oversized files instead of silently losing keys. */
static int parse_setting_number(const char *str,int *out){
    int negative=0;unsigned n=0,digits=0;
    if(*str=='-'){negative=1;str++;}
    for(;*str;str++){
        if(*str=='\r' || *str=='\n')break;
        if(*str<'0' || *str>'9' || n>1000000)return 0;
        n=n*10+(unsigned)(*str-'0');digits++;
    }
    if(!digits)return 0;
    *out=negative?-(int)n:(int)n;
    return 1;
}
static void load_browser_settings(void){
    char data[8192];unsigned used=0;
    int fd=open_ro(SETTINGS_FILE);
    previews_enabled=1;remember_resume=1;
    shoulder_l1_r1=shoulder_l2_r2=0;
    opt_auto_next=opt_sub_delay=opt_sub_position=opt_audio_delay=0;
    opt_sub_size=1;opt_auto_preview=-1;opt_video_scaler=1;opt_default_aspect=0;
    opt_performance_mode=1;
    opt_pause_dim_seconds=30;opt_night_mode=0;
    opt_audio_language=opt_subtitle_language=0;
    opt_low_battery_notice=0;opt_x_action=0;opt_osd_seconds=2;
    opt_sleep_timer_minutes=0;
    opt_subtitles_enabled=1;opt_screen_overlay[0]=0;
    opt_startup_osd_immediate=0;
    opt_start_aspect_shortcut=1;opt_a_pause_shortcut=0;
    opt_b_second_exit=1;
    if(fd<0)return;
    while(used+1<sizeof(data)){
        long n=call3(SYS_read,fd,(long)data+used,sizeof(data)-1-used);
        if(n<=0)break;
        used+=(unsigned)n;
    }
    call1(SYS_close,fd);data[used]=0;
    char *line=data;
    while(*line){
        char *end=line;
        while(*end && *end!='\n')end++;
        char saved=*end;*end=0;
        if(end>line && end[-1]=='\r')end[-1]=0;
        char *sep=line;
        while(*sep && *sep!='=')sep++;
        if(*sep){
            *sep++=0;
            int value;
            if(str_equal(line,"video_scaler")){
                if(str_equal(sep,"bicubic"))opt_video_scaler=0;
                else if(str_equal(sep,"bilinear"))opt_video_scaler=1;
                else if(str_equal(sep,"fast_bilinear"))opt_video_scaler=2;
            } else if(str_equal(line,"performance_mode")){
                if(str_equal(sep,"quality"))opt_performance_mode=0;
                else if(str_equal(sep,"balanced"))opt_performance_mode=1;
                else if(str_equal(sep,"speed"))opt_performance_mode=2;
            } else if(str_equal(line,"screen_overlay")){
                usize len=str_len(sep);int valid=len<sizeof(opt_screen_overlay) &&
                    (len==0 || (sep[0]!='.' && is_png_file(sep)));
                for(usize i=0;i<len && valid;i++)if(sep[i]=='/' || sep[i]=='\\')valid=0;
                if(valid)copy_overlay_name(opt_screen_overlay,sep);
            } else if(str_equal(line,"preferred_audio_language")){
                opt_audio_language=mc_language_index(sep);
            } else if(str_equal(line,"preferred_subtitle_language")){
                opt_subtitle_language=mc_language_index(sep);
            } else if(parse_setting_number(sep,&value)){
                if(str_equal(line,"remember_resume") && (value==0 || value==1))remember_resume=value;
                else if(str_equal(line,"browser_previews") && (value==0 || value==1))previews_enabled=value;
                else if(str_equal(line,"shoulder_l1_r1") && (value==0 || value==1))shoulder_l1_r1=value;
                else if(str_equal(line,"shoulder_l2_r2") && (value==0 || value==1))shoulder_l2_r2=value;
                else if(str_equal(line,"auto_next") && (value==0 || value==1))opt_auto_next=value;
                else if(str_equal(line,"audio_delay_ms") && value>=-2000 && value<=2000)opt_audio_delay=value;
                else if(str_equal(line,"subtitle_delay_ms") && value>=-15000 && value<=15000)opt_sub_delay=value;
                else if(str_equal(line,"subtitle_size") && value>=0 && value<=2)opt_sub_size=value;
                else if(str_equal(line,"subtitle_position") && value>=0 && value<=2)opt_sub_position=value;
                else if(str_equal(line,"auto_preview_seconds") && value>=-1 && value<=3600)opt_auto_preview=value;
                else if(str_equal(line,"pause_dim_seconds") &&
                    (value==0 || value==15 || value==30 || value==60 || value==90 || value==120))
                    opt_pause_dim_seconds=value;
                else if(str_equal(line,"night_mode") && (value==0 || value==1))opt_night_mode=value;
                else if(str_equal(line,"low_battery_notice") &&
                    (value==0 || value==5 || value==10 || value==15))opt_low_battery_notice=value;
                else if(str_equal(line,"sleep_timer_minutes") &&
                    (value==0 || value==15 || value==30 || value==60 ||
                     value==90 || value==-1))opt_sleep_timer_minutes=value;
                else if(str_equal(line,"x_button_action") && value>=0 && value<(int)MC_X_ACTION_COUNT)opt_x_action=value;
                else if(str_equal(line,"osd_duration_seconds") &&
                    (value==1 || value==2 || value==3 || value==5 || value==8 || value==10))
                    opt_osd_seconds=value;
                else if(str_equal(line,"subtitles_enabled") && (value==0 || value==1))
                    opt_subtitles_enabled=value;
                else if(str_equal(line,"default_aspect_ratio") && value>=0 && value<=3)opt_default_aspect=value;
                else if(str_equal(line,"b_second_press_exit") && (value==0 || value==1))
                    opt_b_second_exit=value;
                else if(str_equal(line,"start_aspect_shortcut") && (value==0 || value==1))
                    opt_start_aspect_shortcut=value;
                else if(str_equal(line,"a_pause_shortcut") && (value==0 || value==1))
                    opt_a_pause_shortcut=value;
                else if(str_equal(line,"startup_osd_immediate") && (value==0 || value==1))
                    opt_startup_osd_immediate=value;
            }
        }
        if(!saved)break;
        line=end+1;
    }
}
/* Atomic single-key update, retaining unknown keys and manual font paths. */
static int save_browser_setting(const char *key,const char *value){
    char data[8192],output[8384];unsigned used=0,out=0;
    int fd=open_ro(SETTINGS_FILE);
    if(fd>=0){
        while(used+1<sizeof(data)){
            long n=call3(SYS_read,fd,(long)data+used,sizeof(data)-1-used);
            if(n<=0)break;
            used+=(unsigned)n;
        }
        if(used+1==sizeof(data)){
            char dummy;
            if(call3(SYS_read,fd,(long)&dummy,1)>0){call1(SYS_close,fd);return -1;}
        }
        call1(SYS_close,fd);
    }
    data[used]=0;
    usize key_len=str_len(key);
    const char *cursor=data;
    while(*cursor){
        const char *end=cursor;
        while(*end && *end!='\n')end++;
        usize length=(usize)(end-cursor);
        int matches=length>key_len && cursor[key_len]=='=';
        for(usize i=0;i<key_len && matches;i++)if(cursor[i]!=key[i])matches=0;
        if(!matches){
            if(out+length+2>=sizeof(output))return -1;
            for(usize i=0;i<length;i++)output[out++]=cursor[i];
            output[out++]='\n';
        }
        cursor=*end?end+1:end;
    }
    usize value_len=str_len(value);
    if(out+key_len+value_len+3>=sizeof(output))return -1;
    for(usize i=0;i<key_len;i++)output[out++]=key[i];
    output[out++]='=';
    for(usize i=0;i<value_len;i++)output[out++]=value[i];
    output[out++]='\n';
    int tmp=(int)call4(SYS_openat,AT_FDCWD,(long)SETTINGS_TEMP,
                      O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0644);
    if(tmp<0)return -1;
    unsigned cursor_out=0;
    while(cursor_out<out){
        long written=call3(SYS_write,tmp,(long)(output+cursor_out),out-cursor_out);
        if(written<=0){call1(SYS_close,tmp);
            call3(SYS_unlinkat,AT_FDCWD,(long)SETTINGS_TEMP,0);return -1;}
        cursor_out+=(unsigned)written;
    }
    call1(SYS_close,tmp);
    if(call4(SYS_renameat,AT_FDCWD,(long)SETTINGS_TEMP,AT_FDCWD,(long)SETTINGS_FILE)<0){
        call3(SYS_unlinkat,AT_FDCWD,(long)SETTINGS_TEMP,0);return -1;
    }
    return 0;
}
static void uint_to_text(unsigned value,char *out);
static void int_to_text(int value,char *out){
    if(value<0){*out++='-';uint_to_text((unsigned)(-value),out);}
    else uint_to_text((unsigned)value,out);
}
static void save_int_setting(const char *key,int value){
    char text[16];int_to_text(value,text);
    if(save_browser_setting(key,text)<0)diag_puts("MiniCinema: settings not saved\n");
}
static void toggle_browser_previews(void){
    previews_enabled=!previews_enabled;
    save_int_setting("browser_previews",previews_enabled);
    preview_ready=-1;
}

/* Browser owns natural sorting; write the playback sequence once on video
 * selection, so the launcher needs no sort -V or extra binary. */
static int save_playlist(void){
    int fd=(int)call4(SYS_openat,AT_FDCWD,(long)PLAYLIST_FILE,
                     O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0644);
    if(fd<0)return -1;
    for(unsigned i=0;i<nitems;i++){
        if(items[i].is_dir)continue;
        char full[PATH_CAP+2];
        if(append_path(full,sizeof(full),cwd,items[i].name)<0)continue;
        usize n=str_len(full);
        full[n++]='\n';
        if(call3(SYS_write,fd,(long)full,n)!=(long)n){
            call1(SYS_close,fd);return -1;
        }
    }
    call1(SYS_close,fd);
    return 0;
}
static int up_directory(void){
    usize n=str_len(cwd);
    if(str_equal(cwd,ROOT))return 0;
    while(n>0 && cwd[n-1]!='/')--n;
    if(n>str_len(ROOT)){cwd[n-1]=0;}else str_copy(cwd,sizeof(cwd),ROOT);
    return scan_dir();
}
static void select_visible(unsigned i){
    if(i>=nitems)return;
    cursor=i;
    if(cursor<scrollpos)scrollpos=cursor;
    if(cursor>=scrollpos+ROWS_PER_PAGE)scrollpos=cursor-(ROWS_PER_PAGE-1);
}

/* Browsing page/letter selection within the directory-first sort. */
static unsigned nav_letter(int forward) {
    int i=(int)cursor, step=forward?1:-1;
    int same_type=nitems?(int)items[cursor].is_dir:0;
    u8 letter=nitems?fold((u8)items[cursor].name[0]):0;
    if(!nitems)return cursor;
    for(i+=step;i>=0&&i<(int)nitems;i+=step){
        if((int)items[i].is_dir!=same_type || fold((u8)items[i].name[0])!=letter){
            if(!forward){
                u8 target=fold((u8)items[i].name[0]);
                int kind=items[i].is_dir;
                while(i>0 && items[i-1].is_dir==kind &&
                      fold((u8)items[i-1].name[0])==target)--i;
            }
            return (unsigned)i;
        }
    }
    return cursor;
}
static char preview_path[PATH_CAP];
static void preview_load(void){
    int fd;
    long n;
    unsigned got=0;
    char base[NAME_MAXLEN+1];
    const char *name;
    usize cut;
    if(preview_index==(int)cursor && preview_ready!=-1)return;
    preview_index=(int)cursor;
    preview_ready=0;
    if(cursor>=nitems || items[cursor].is_dir)return;
    name=items[cursor].name;
    cut=str_len(name);
    while(cut && name[cut-1]!='.')--cut;
    if(!cut)return;
    cut--;
    if(cut+5>sizeof(base))return;
    for(usize i=0;i<cut;i++)base[i]=name[i];
    base[cut]=0;
    if(append_path(preview_path,sizeof(preview_path),cwd,"Imgs")<0)return;
    if(append_path(filepath,sizeof(filepath),preview_path,base)<0)return;
    /* append .png without ever truncating a long path */
    usize len=str_len(filepath);
    if(len+5>sizeof(filepath))return;
    filepath[len++]='.';filepath[len++]='p';filepath[len++]='n';filepath[len++]='g';filepath[len]=0;
    fd=open_ro(filepath);
    if(fd<0)return;
    for(;;){
        if(got==MC_PNG_MAX_FILE){got=0;n=-1;break;}
        n=call3(SYS_read,fd,(long)(mc_png_file+got),MC_PNG_MAX_FILE-got);
        if(n<=0)break;
        got+=(unsigned)n;
    }
    call1(SYS_close,fd);
    if(n<0 || !got)return;
    if(mc_png_decode(mc_png_file,got)==0)preview_ready=1;
}
/* 32-bit framebuffer only: both original Mini and Mini Plus report it. */
static void load_controls_help(void){
    int fd=open_ro(HELP_PNG);long n;unsigned got=0;
    help_ready=0;
    if(fd<0)return;
    for(;;){
        if(got>=MC_PNG_MAX_FILE){n=-1;break;}
        n=call3(SYS_read,fd,(long)(mc_png_file+got),MC_PNG_MAX_FILE-got);
        if(n<=0)break;
        got+=(unsigned)n;
    }
    call1(SYS_close,fd);
    if(n<0 || !got)return;
    if(mc_png_decode(mc_png_file,got)==0 && mc_png.width==640 && mc_png.height==480 &&
       (mc_png.ctype==2 || mc_png.ctype==6))
        help_ready=1;
}
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static volatile u32 *fb;
static u32 canvas[640*480];
static int fbfd=-1,W,H,pitch,origin_x,origin_y;
static u32 color(int r,int g,int b){
    u32 px=0;
    if(vinfo.red.length) px|=((u32)r>>(8-vinfo.red.length))<<vinfo.red.offset;
    if(vinfo.green.length) px|=((u32)g>>(8-vinfo.green.length))<<vinfo.green.offset;
    if(vinfo.blue.length) px|=((u32)b>>(8-vinfo.blue.length))<<vinfo.blue.offset;
    if(vinfo.transp.length)px|=((1u<<vinfo.transp.length)-1)<<vinfo.transp.offset;
    return px;
}
static u32 blend(u32 bg, u32 fg, unsigned alpha){
    u32 out=0, mask;
    unsigned br,gr, n;
    if(alpha==0)return bg;
    if(alpha>=255)return fg;
#define BLEND_CHANNEL(CH) do { \
 n=vinfo.CH.length; if(n){ mask=(1u<<n)-1u; \
 br=(bg>>vinfo.CH.offset)&mask;gr=(fg>>vinfo.CH.offset)&mask; \
 out|=(((br*(255-alpha)+gr*alpha+127)/255)&mask)<<vinfo.CH.offset; } \
 } while(0)
    BLEND_CHANNEL(red);
    BLEND_CHANNEL(green);
    BLEND_CHANNEL(blue);
    BLEND_CHANNEL(transp);
#undef BLEND_CHANNEL
    return out;
}
static void rect(int x,int y,int w,int h,u32 c){
    int x0=x<0?0:x, y0=y<0?0:y;
    int x1=x+w>640?640:x+w, y1=y+h>480?480:y+h;
    if(x0>=x1 || y0>=y1)return;
    for(int yy=y0;yy<y1;yy++){
        u32 *dst=canvas+yy*640+x0;
        for(int xx=x0;xx<x1;xx++) *dst++=c;
    }
}
/* Drawing takes place only in RAM. Partial, 180-degree rotated copying to
 * fb0 happens after a completed frame, eliminating the old clear/flicker on
 * every D-pad press without changing framebuffer page state. */
static void flush_rect(int x,int y,int w,int h){
    int x0=x<0?0:x, y0=y<0?0:y;
    int x1=x+w>640?640:x+w, y1=y+h>480?480:y+h;
    if(x0>=x1||y0>=y1)return;
    for(int yy=y0;yy<y1;yy++){
        int physical_y=H-1-(yy+origin_y);
        volatile u32 *dst=fb+(vinfo.yoffset+physical_y)*pitch+vinfo.xoffset+W-1-(x0+origin_x);
        const u32 *src=canvas+yy*640+x0;
        for(int xx=x0;xx<x1;xx++) *dst--=*src++;
    }
}
static void flush_full(void){flush_rect(0,0,640,480);}
static void draw_glyph(int x,int y,const u8 *a,int w,int h,u32 c){
    for(int gy=0;gy<h;gy++){
        int py=y+gy;
        if(py<0||py>=480)continue;
        for(int gx=0;gx<w;gx++){
            int px=x+gx;
            if(px<0||px>=640)continue;
            unsigned alpha=a[gy*w+gx];
            if(alpha)canvas[py*640+px]=blend(canvas[py*640+px],c,alpha);
        }
    }
}
/* Decode one well-formed UTF-8 character; unsupported Unicode becomes
 * one '?' glyph instead of one question mark for every UTF-8 byte. */
static unsigned utf8_codepoint(const char **text){
    const u8 *p=(const u8 *)*text;
    unsigned v,n;
    if(p[0]<0x80){(*text)++;return p[0];}
    if(p[0]>=0xc2 && p[0]<=0xdf){v=p[0]&31;n=2;}
    else if(p[0]>=0xe0 && p[0]<=0xef){v=p[0]&15;n=3;}
    else if(p[0]>=0xf0 && p[0]<=0xf4){v=p[0]&7;n=4;}
    else{(*text)++;return '?';}
    for(unsigned i=1;i<n;i++){
        if(!p[i] || (p[i]&0xc0)!=0x80){(*text)++;return '?';}
        v=(v<<6)|(p[i]&63);
    }
    *text+=n;
    if((n==2 && v<0x80)||(n==3 && v<0x800)||(n==4 && v<0x10000)||
       v>0x10ffff || (v>=0xd800 && v<=0xdfff))return '?';
    return v;
}
static unsigned char_width(unsigned ch,int face){
    if(ch>=0xc0 && ch<=0xff)
        return face==1?ui_latin_small_advance[ch-0xc0]:
               ui_latin_body_advance[ch-0xc0];
    if(ch<32||ch>126)ch='?';
    if(face==2)return ui_title_advance[ch-32];
    return face==1?ui_small_advance[ch-32]:ui_body_advance[ch-32];
}
static void text_face(int x,int y,const char *text,int maxwidth,u32 c,int face){
    int x0=x,w,h;
    if(face==2){w=UI_TITLE_W;h=UI_TITLE_H;}
    else if(face==1){w=UI_SMALL_W;h=UI_SMALL_H;}
    else{w=UI_BODY_W;h=UI_BODY_H;}
    while(*text){
        unsigned ch=utf8_codepoint(&text);
        unsigned advance=char_width(ch,face);
        const u8 *glyph;
        if(x+(int)advance>x0+maxwidth)break;
        if(ch>=0xc0 && ch<=0xff && face!=2)
            glyph=face==1?ui_latin_small_glyph[ch-0xc0]:
                           ui_latin_body_glyph[ch-0xc0];
        else{
            if(ch<32||ch>126)ch='?';
            if(face==2)glyph=ui_title_glyph[ch-32];
            else if(face==1)glyph=ui_small_glyph[ch-32];
            else glyph=ui_body_glyph[ch-32];
        }
        draw_glyph(x,y,glyph,w,h,c);
        x+=(int)advance;
    }
}
static void label(int x,int y,const char *s,int width,u32 c){
    text_face(x,y,s,width,c,0);
}
static void small(int x,int y,const char *s,int width,u32 c){
    text_face(x,y,s,width,c,1);
}
/* Keep the same actual pixel gap between each footer key and its action. */
static int small_text_width(const char *s){
    int width=0;
    while(*s)width+=char_width(utf8_codepoint(&s),1);
    return width;
}
static void draw_footer_hints(u32 keycolor,u32 textcolor){
    const char *keys[]={"A","B","X","SELECT","MENU"};
    const char *actions[]={"OPEN","BACK","RESTART","OPTIONS","EXIT"};
    int widths[5],total=0;
    int x=12;
    const int key_gap=4;
    /* Reserve the right side for a potentially wide video count. */
    const int hint_right=500;
    for(unsigned i=0;i<5;i++){
        widths[i]=small_text_width(keys[i])+key_gap+small_text_width(actions[i]);
        total+=widths[i];
    }
    /* The full hint is preferred, but keep a compact fallback if necessary. */
    if(total+4*5>hint_right-x){
        keys[3]="SEL";
        total=0;
        for(unsigned i=0;i<5;i++){
            widths[i]=small_text_width(keys[i])+key_gap+small_text_width(actions[i]);
            total+=widths[i];
        }
    }
    int gap=(hint_right-x-total)/4;
    if(gap<4)gap=4;
    for(unsigned i=0;i<5;i++){
        small(x,444,keys[i],small_text_width(keys[i])+1,keycolor);
        small(x+small_text_width(keys[i])+key_gap,444,actions[i],
              small_text_width(actions[i])+1,textcolor);
        x+=widths[i]+gap;
    }
}
static void draw_icon(int x,int y){
    for(int iy=0;iy<UI_ICON_H;iy++){
        int py=y+iy;
        if(py<0||py>=480)continue;
        for(int ix=0;ix<UI_ICON_W;ix++){
            int px=x+ix;
            if(px<0||px>=640)continue;
            const u8 *rgba=ui_icon_rgba+(iy*UI_ICON_W+ix)*4;
            if(!rgba[3])continue;
            u32 c=color(rgba[0],rgba[1],rgba[2]);
            u32 *pixel=&canvas[py*640+px];
            *pixel=blend(*pixel,c,rgba[3]);
        }
    }
}
static void preview_draw(int px,int py,int pw,int ph){
    u32 panel=color(26,33,45),muted=color(125,140,160);
    rect(px,py,pw,ph,panel);
    small(px+(pw-small_text_width("PREVIEW"))/2,py+6,"PREVIEW",pw-26,muted);
    rect(px+13,py+36,pw-26,1,color(57,68,84));
    if(previews_enabled)preview_load();
    if(previews_enabled && preview_ready==1){
        const int frame_x=px+11,frame_y=py+48,frame_w=pw-22,frame_h=ph-67;
        unsigned iw=mc_png.width,ih=mc_png.height;
        unsigned outw=(unsigned)frame_w,outh=ih*(unsigned)frame_w/iw;
        if(outh>(unsigned)frame_h){outh=(unsigned)frame_h;outw=iw*(unsigned)frame_h/ih;}
        if(outw<1 || outh<1)return;
        int x0=frame_x+(frame_w-outw)/2,y0=frame_y+(frame_h-outh)/2;
        for(unsigned y=0;y<outh;y++)for(unsigned x=0;x<outw;x++){
            u8 rgba[4];
            mc_png_pixel((unsigned)x*iw/outw,(unsigned)y*ih/outh,rgba);
            if(rgba[3]){
                u32 *dst=&canvas[(y0+y)*640+x0+x];
                u32 fg=color(rgba[0],rgba[1],rgba[2]);
                *dst=rgba[3]==255?fg:blend(*dst,fg,rgba[3]);
            }
        }
    }else{
        const char *message=!previews_enabled ? "Previews off" :
                            (nitems && !items[cursor].is_dir)?
                            "No preview":"Select a video";
        int label_w=small_text_width(message);
        /* Both empty-state labels share the exact same centered baseline. */
        small(px+(pw-label_w)/2,py+162,message,pw,muted);
    }
    rect(px,py+ph,pw,1,color(52,62,78));
}
/* Matches the player's compact top-right battery pill. */
static void draw_battery(void){
    if(!battery_visible)return;
    u32 panel=color(15,19,27),muted=color(138,152,170);
    u32 white=color(244,246,250);
    u32 fill=battery_value>=0 && battery_value<=15 ? color(251,86,89) :
             color(123,206,166);
    char digits[14];
    rect(527,23,27,16,muted);
    rect(529,25,23,12,panel);
    rect(554,28,3,6,muted);
    if(battery_value>=0){
        int inside=battery_value*21/100;
        if(battery_value>0 && inside==0)inside=1;
        rect(530,26,inside,10,fill);
        unsigned value=(unsigned)battery_value;
        char reverse[4];unsigned n=0,i=0;
        do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value);
        while(n)digits[i++]=reverse[--n];
        digits[i++]='%';digits[i]=0;
    }else{
        str_copy(digits,sizeof(digits),"--%");
    }
    /* Center visible glyph pixels on the battery icon, not the 24px font box. */
    int ink_top=UI_SMALL_H,ink_bottom=-1;
    for(const char *p=digits;*p;p++){
        unsigned code=(unsigned char)*p;
        if(code<32 || code>126)continue;
        const u8 *glyph=ui_small_glyph[code-32];
        for(int y=0;y<UI_SMALL_H;y++){
            for(int x=0;x<UI_SMALL_W;x++)if(glyph[y*UI_SMALL_W+x]>=32){
                if(y<ink_top)ink_top=y;
                if(y>ink_bottom)ink_bottom=y;
            }
        }
    }
    int ink_mid=ink_bottom>=ink_top ? (ink_top+ink_bottom)/2 : UI_SMALL_H/2;
    small(568,31-ink_mid,digits,55,white);
}
/* Browser settings use the same DejaVu bitmap font and dark selected rows
 * as the player's SELECT menu. Pages keep the text readable on 640x480. */
static int menu_row_count(void){
    return settings_page<0?7:settings_page==0?8:
           settings_page==1?6:settings_page==2?3:
           settings_page==3?5:settings_page==4?2:settings_page==5?3:0;
}
static const char *menu_row_name(int page,int row){
    static const char *sections[]={"Playback","Controls","Audio",
        "Subtitles","Browser & previews","Power & Battery","Controls help"};
    static const char *play[]={"Remember resume","Auto next video",
        "Video scaler","Performance mode","Default aspect ratio","Startup OSD",
        "OSD duration", "Screen overlay"};
    static const char *controls[]={"L1/R1 prev/next chapter", "L2/R2 prev/next video",
        "START aspect shortcut","A pause/resume shortcut","B twice exits video","X button"};
    static const char *audio[]={"Audio delay","Preferred language","Dialogue boost"};
    static const char *subs[]={"Subtitles On/Off","Subtitle delay","Subtitle size",
        "Subtitle position","Preferred language"};
    static const char *browse[]={"Show previews","Auto capture preview"};
    static const char *power[]={"Pause auto dim","Low battery notice","Sleep timer"};
    if(page<0)return sections[row];
    if(page==0)return play[row];
    if(page==1)return controls[row];
    if(page==2)return audio[row];
    if(page==3)return subs[row];
    if(page==4)return browse[row];
    if(page==5)return power[row];
    return "";
}
static const char *menu_row_value(int page,int row,char *buff){
    int n=0;
    if(page<0)return ">";
    if(page==0){
        if(row==0)return remember_resume?"On":"Off";
        if(row==1)return opt_auto_next?"On":"Off";
        if(row==2)return opt_video_scaler==0?"Bicubic (best)":
                         opt_video_scaler==1?"Bilinear (balanced)":"Fast bilinear (fastest)";
        if(row==3)return opt_performance_mode==0?"Quality (normal decode)":
                         opt_performance_mode==1?"Balanced (faster HD)":"Speed (may skip frames)";
        if(row==4)return opt_default_aspect==0?"Fit":opt_default_aspect==1?"Fill":
                         opt_default_aspect==2?"Stretch":"Original";
        if(row==5)return opt_startup_osd_immediate?"On":"Off";
        if(row==6){int_to_text(opt_osd_seconds,buff);
            usize k=str_len(buff);buff[k++]='s';buff[k]=0;return buff;}
        if(row==7)return opt_screen_overlay[0]?opt_screen_overlay:"Off";
    }else if(page==1){
        if(row==0)return shoulder_l1_r1?"On":"Off";
        if(row==1)return shoulder_l2_r2?"On":"Off";
        if(row==2)return opt_start_aspect_shortcut?"On":"Off";
        if(row==3)return opt_a_pause_shortcut?"On":"Off";
        if(row==4)return opt_b_second_exit?"On":"Off";
        if(row==5)return x_action_names[opt_x_action];
    }else if(page==2){
        if(row==0)n=opt_audio_delay;
        if(row==1)return mc_languages[opt_audio_language];
        if(row==2)return opt_night_mode?"On":"Off";
    }else if(page==3){
        if(row==0)return opt_subtitles_enabled?"On":"Off";
        if(row==1)n=opt_sub_delay;
        if(row==2)return opt_sub_size==0?"Small":opt_sub_size==1?"Normal":"Large";
        if(row==3)return opt_sub_position==0?"Low":opt_sub_position==1?"Raised":"High";
        if(row==4)return mc_languages[opt_subtitle_language];
    }else if(page==4){
        if(row==0)return previews_enabled?"On":"Off";
        if(row==1 && opt_auto_preview<0)return "Off";
        if(row==1){int_to_text(opt_auto_preview,buff);
            usize k=str_len(buff);if(k+1<24){buff[k++]='s';buff[k]=0;}return buff;}
    }else if(page==5){
        if(row==0){if(!opt_pause_dim_seconds)return "Off";
            int_to_text(opt_pause_dim_seconds,buff);
            usize k=str_len(buff);buff[k++]='s';buff[k]=0;return buff;}
        if(row==1){if(!opt_low_battery_notice)return "Off";
            int_to_text(opt_low_battery_notice,buff);
            usize k=str_len(buff);buff[k++]='%';buff[k]=0;return buff;}
        if(row==2){if(opt_sleep_timer_minutes<0)return "End of video";
            if(!opt_sleep_timer_minutes)return "Off";
            int_to_text(opt_sleep_timer_minutes,buff);
            usize k=str_len(buff);buff[k++]=' ';buff[k++]='m';buff[k++]='i';
            buff[k++]='n';buff[k]=0;return buff;}
    }
/* Display a signed decimal number of seconds without libc formatting. */
    unsigned abs=(unsigned)(n<0?-n:n);
    buff[0]=n<0?'-':'+';
    char secs[12];uint_to_text(abs/1000,secs);
    usize index=1;
    for(usize i=0;secs[i];i++)buff[index++]=secs[i];
    buff[index++]='.';
    buff[index++]=(char)('0'+(abs%1000)/100);
    buff[index++]='s';buff[index]=0;
    return buff;
}
/* Footer-style green button tokens within small popup instructions. */
static void draw_popup_hints(const char *text, int left, int width, int y){
    const char *p=text;
    int full=small_text_width(text);
    int x=left+(width-full)/2;
    u32 green=color(123,206,166), muted=color(172,186,207);
    while(*p){
        char word[64];int n=0,spaces=0;
        while(*p==' '){spaces++;p++;}
        x+=spaces*small_text_width(" ");
        if(!*p)break;
        while(*p && *p!=' ' && n<63)word[n++]=*p++;
        word[n]=0;
        int w=small_text_width(word);
        int is_key=(spaces>=3 || x==left+(width-full)/2);
        if(str_equal(word,"A") || str_equal(word,"B") ||
           str_equal(word,"RIGHT") || str_equal(word,"UP/DOWN") ||
           str_equal(word,"LEFT/RIGHT"))is_key=1;
        small(x,y,word,w+1,is_key?green:muted);
        x+=w;
    }
}
/* Fit file names to actual free row width instead of a 17-character limit. */
static const char *overlay_row_text(const char *name, int max_px, char out[64]){
    usize len=str_len(name);
    if(small_text_width(name)<=max_px)return name;
    if(max_px<=small_text_width("..."))return "...";
    usize n=0;
    while(n<len && n<sizeof(opt_screen_overlay)-4){
        out[n]=name[n];out[n+1]=0;
        if(small_text_width(out)+small_text_width("...")>max_px)break;
        n++;
    }
    out[n++]='.';out[n++]='.';out[n++]='.';out[n]=0;
    return out;
}
static void draw_settings_menu(void){
    const int left=43,top=26,width=554,height=428;
    u32 white=color(238,243,250),muted=color(172,186,207);
    u32 accent=color(253,132,140);
    rect(0,0,640,480,color(10,14,21));
    rect(left-3,top-3,width+6,height+6,color(9,14,22));
    rect(left,top,width,height,color(25,34,47));
    rect(left,top,width,3,color(247,107,120));
    const char *title=settings_page<0?"SETTINGS":
        settings_page==0?"SETTINGS  /  PLAYBACK":
        settings_page==1?"SETTINGS  /  CONTROLS":
        settings_page==2?"SETTINGS  /  AUDIO":
        settings_page==3?"SETTINGS  /  SUBTITLES":
        settings_page==4?"SETTINGS  /  BROWSER":"SETTINGS  /  POWER & BATTERY";
    small(left+25,top+13,title,width-50,white);
    rect(left+23,top+46,width-46,1,color(58,72,92));
    int count=menu_row_count();
    for(int i=0;i<count;i++){
        int row_step=count>7?39:43;
        int y=top+67+i*row_step;
        if(i==settings_selection){
            rect(left+13,y-2,width-26,row_step-4,color(47,63,82));
            rect(left+13,y+2,3,row_step-12,color(247,107,120));
        }
        char number[64], filename[64];
        const char *name=menu_row_name(settings_page,i);
        const char *value=menu_row_value(settings_page,i,number);
        if(settings_page==0 && i==7){
            int free_px=width-85-small_text_width(name)-16;
            value=overlay_row_text(value,free_px,filename);
        }
        int w=small_text_width(value);
        small(left+29,y+8,name,width-85-w,i==settings_selection?white:muted);
        small(left+width-29-w,y+8,value,w+1,i==settings_selection?accent:white);
    }
    rect(left+23,top+389,width-46,1,color(58,72,92));
    const char *hint=settings_page<0?"A OPEN   RIGHT OPEN   B CLOSE":
        "UP/DOWN MOVE   LEFT/RIGHT CHANGE   B BACK";
    draw_popup_hints(hint,left,width,top+397);
}
static void settings_change(int direction,int a_pressed){
    int row=settings_selection,now=0;const char *key=0;
    if(settings_page==0){
        switch(row){
        case 0:remember_resume=!remember_resume;key="remember_resume";now=remember_resume;
               load_resume_marks();break;
        case 1:opt_auto_next=!opt_auto_next;key="auto_next";now=opt_auto_next;break;
        case 2:opt_video_scaler=(opt_video_scaler+(direction>0?1:2))%3;
               if(save_browser_setting("video_scaler",opt_video_scaler==0?"bicubic":
                   opt_video_scaler==1?"bilinear":"fast_bilinear")<0)
                   diag_puts("MiniCinema: scaler not saved\n");
               return;
        case 3:opt_performance_mode=(opt_performance_mode+(direction>0?1:2))%3;
               if(save_browser_setting("performance_mode",opt_performance_mode==0?"quality":
                   opt_performance_mode==1?"balanced":"speed")<0)
                   diag_puts("MiniCinema: performance mode not saved\n");
               return;
        case 4:opt_default_aspect=(opt_default_aspect+(direction>0?1:3))%4;
               key="default_aspect_ratio";now=opt_default_aspect;break;
        case 5:opt_startup_osd_immediate=!opt_startup_osd_immediate;
               key="startup_osd_immediate";now=opt_startup_osd_immediate;break;
        case 6:{static const int times[]={1,2,3,5,8,10};int idx=1;
               for(int i=0;i<6;i++)if(opt_osd_seconds==times[i])idx=i;
               opt_osd_seconds=times[(idx+(direction>0?1:5))%6];
               key="osd_duration_seconds";now=opt_osd_seconds;break;}
        case 7:{int idx=-1;
               for(unsigned i=0;i<overlay_count;i++)
                   if(str_equal(opt_screen_overlay,overlay_names[i]))idx=(int)i;
               idx+=direction>0?1:-1;
               if(idx<-1)idx=(int)overlay_count-1;
               if(idx>=(int)overlay_count)idx=-1;
               if(idx<0)opt_screen_overlay[0]=0;
               else copy_overlay_name(opt_screen_overlay,overlay_names[idx]);
               if(save_browser_setting("screen_overlay",opt_screen_overlay)<0)
                   diag_puts("MiniCinema: overlay setting not saved\n");
               return;}

        }
    }else if(settings_page==1){
        switch(row){
        case 0:shoulder_l1_r1=!shoulder_l1_r1;key="shoulder_l1_r1";now=shoulder_l1_r1;break;
        case 1:shoulder_l2_r2=!shoulder_l2_r2;key="shoulder_l2_r2";now=shoulder_l2_r2;break;
        case 2:opt_start_aspect_shortcut=!opt_start_aspect_shortcut;
               key="start_aspect_shortcut";now=opt_start_aspect_shortcut;break;
        case 3:opt_a_pause_shortcut=!opt_a_pause_shortcut;
               key="a_pause_shortcut";now=opt_a_pause_shortcut;break;
        case 4:opt_b_second_exit=!opt_b_second_exit;
               key="b_second_press_exit";now=opt_b_second_exit;break;
        case 5:opt_x_action=(opt_x_action+(direction>0?1:(int)MC_X_ACTION_COUNT-1))%
                     (int)MC_X_ACTION_COUNT;
               key="x_button_action";now=opt_x_action;break;
        }
    }else if(settings_page==2){
        if(row==0){
            opt_audio_delay+=direction*100;
            if(opt_audio_delay>2000)opt_audio_delay=2000;
            if(opt_audio_delay< -2000)opt_audio_delay=-2000;
            key="audio_delay_ms";now=opt_audio_delay;
        }else if(row==1){
            opt_audio_language=(opt_audio_language+(direction>0?1:(int)MC_LANG_COUNT-1))%(int)MC_LANG_COUNT;
            if(save_browser_setting("preferred_audio_language",
                mc_language_codes[opt_audio_language])<0)
                diag_puts("MiniCinema: audio language not saved\n");
            return;
        }else if(row==2){
            opt_night_mode=!opt_night_mode;key="night_mode";now=opt_night_mode;
        }
    }else if(settings_page==3){
        switch(row){
        case 0:opt_subtitles_enabled=!opt_subtitles_enabled;
               key="subtitles_enabled";now=opt_subtitles_enabled;break;
        case 1:opt_sub_delay+=direction*100;
               if(opt_sub_delay>15000)opt_sub_delay=15000;
               if(opt_sub_delay< -15000)opt_sub_delay=-15000;
               key="subtitle_delay_ms";now=opt_sub_delay;break;
        case 2:opt_sub_size=(opt_sub_size+(direction>0?1:2))%3;
               key="subtitle_size";now=opt_sub_size;break;
        case 3:opt_sub_position=(opt_sub_position+(direction>0?1:2))%3;
               key="subtitle_position";now=opt_sub_position;break;
        case 4:opt_subtitle_language=(opt_subtitle_language+
               (direction>0?1:(int)MC_LANG_COUNT-1))%(int)MC_LANG_COUNT;
               if(save_browser_setting("preferred_subtitle_language",
                  mc_language_codes[opt_subtitle_language])<0)
                   diag_puts("MiniCinema: subtitle language not saved\n");
               return;
        }
    }else if(settings_page==4){
        if(row==0){toggle_browser_previews();return;}
        if(row==1){
            /* A always enables from Off/0 at 5s. RIGHT passes through
             * 0s first; LEFT from 0 returns to Off. */
            if(a_pressed && opt_auto_preview<=0)opt_auto_preview=5;
            else if(opt_auto_preview<0)opt_auto_preview=direction>0?0:-1;
            else if(direction<0 && opt_auto_preview==0)opt_auto_preview=-1;
            else {opt_auto_preview+=direction*5;
                  if(opt_auto_preview>3600)opt_auto_preview=3600;}
            key="auto_preview_seconds";now=opt_auto_preview;
        }
    }else if(settings_page==5){
        if(row==0){static const int times[]={0,15,30,60,90,120};int idx=2;
            for(int i=0;i<6;i++)if(opt_pause_dim_seconds==times[i])idx=i;
            opt_pause_dim_seconds=times[(idx+(direction>0?1:5))%6];
            key="pause_dim_seconds";now=opt_pause_dim_seconds;
        }else if(row==1){static const int low[]={0,5,10,15};int idx=0;
            for(int i=0;i<4;i++)if(opt_low_battery_notice==low[i])idx=i;
            opt_low_battery_notice=low[(idx+(direction>0?1:3))%4];
            key="low_battery_notice";now=opt_low_battery_notice;
        }else if(row==2){static const int times[]={0,15,30,60,90,-1};int idx=0;
            for(int i=0;i<6;i++)if(opt_sleep_timer_minutes==times[i])idx=i;
            opt_sleep_timer_minutes=times[(idx+(direction>0?1:5))%6];
            key="sleep_timer_minutes";now=opt_sleep_timer_minutes;
        }
    }
    if(key)save_int_setting(key,now);
}

static void display(void){
    if(help_open){
        rect(0,0,640,480,color(10,14,21));
        if(help_ready){
            for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){
                u8 rgba[4];mc_png_pixel(x,y,rgba);
                if(rgba[3]){
                    u32 fg=color(rgba[0],rgba[1],rgba[2]);
                    u32 *px=&canvas[y*640+x];
                    *px=rgba[3]==255?fg:blend(*px,fg,rgba[3]);
                }
            }
        }else{
            label(72,205,"Help image not found",500,color(244,246,250));
            small(75,238,"help/controls.png",480,color(172,186,207));
        }
        return;
    }
    u32 bg=color(15,19,27),panel=color(27,34,46),muted=color(128,143,161);
    u32 soft=color(198,207,218),white=color(244,246,250),red=color(251,86,89);
    u32 green=color(123,206,166);
    rect(0,0,640,480,bg);
    rect(0,0,640,3,red);
    draw_icon(19,17); /* Align icon center with the title, six pixels lower. */
    text_face(87,21,"MiniCinema",440,white,2);
    {   /* Place the smaller version after the title, lowered for visual alignment. */
        const char *title = "MiniCinema";
        int advance = 0;
        for (const char *ch = title; *ch; ++ch)
            advance += ui_title_advance[(unsigned char)*ch - 32];
        rect(88, 62, advance, 2, red);
        small(87+advance+15,35,"v1.1.41",95,muted);
    }
    rect(22,78,596,1,color(53,62,75));
    const char *folder=cwd;
    for(const char *p=cwd;*p;++p)if(*p=='/' && p[1])folder=p+1;
    small(23,89,"FOLDER",76,muted);
    small(96,89,folder,omitted?(previews_enabled?222:450):
                              (previews_enabled?298:(show_error?343:514)),white);
    if(omitted)small(previews_enabled?330:557,89,"512+",60,red);
    if(nitems==0){
        label(33,187,"No videos found",356,white);
        small(34,215,"Add videos to Media/Videos",360,soft);
    }
    for(unsigned i=scrollpos;i<nitems&&i<scrollpos+ROWS_PER_PAGE;++i){
        int y=113+(int)(i-scrollpos)*38,selected=i==cursor;
        if(selected){
            int selected_y=y+(items[i].is_dir?0:1);
            rect(21,selected_y,previews_enabled?375:597,35,panel);
            rect(21,selected_y,3,35,red);
        }
        if(items[i].is_dir){
            /* Quiet two-tone folder icon with a small tab and inset highlight. */
            u32 edge=color(129,151,177),face=color(68,98,128);
            rect(33,y+12,22,16,edge);
            rect(34,y+11,10,4,edge);
            rect(35,y+14,18,12,face);
            rect(37,y+16,14,1,color(151,174,198));
        }else{
            /* Modern play tile instead of the previous nested squares. */
            u32 frame=color(68,89,114),light=color(126,151,177);
            rect(33,y+7,22,22,frame);
            rect(34,y+8,20,20,color(32,42,56));
            rect(35,y+9,18,1,light);
            for(int h=0;h<13;h++){
                int dy=h<7?h:12-h;
                rect(41,y+12+h,3+dy,1,selected?red:color(174,188,208));
            }
        }
        int marked=items[i].bad || items[i].done || items[i].resumable;
        int status_x=previews_enabled?351:567;
        label(65,y+8,items[i].name,
              previews_enabled ? (items[i].is_dir ? 280 : marked?272:316) :
                                 (marked || items[i].is_dir ? 491 : 547),
              selected?white:soft);
        if(items[i].bad)small(status_x,y+9,"BAD",43,red);
        else if(items[i].done)small(status_x,y+9,"END",43,green);
        else if(items[i].percent){
            char pct[8];unsigned at=0;
            uint_to_text(items[i].percent,pct);
            while(pct[at])at++;
            pct[at++]='%';pct[at]=0;
            small(status_x,y+9,pct,43,selected?white:muted);
        }else if(items[i].resumable)rect(previews_enabled?384:602,y+15,5,5,red);
        if(items[i].is_dir)small(status_x,y+9,"DIR",39,muted);
    }
    if(previews_enabled) preview_draw(410,113,208,304);
    if(show_error){
        if(previews_enabled){
            rect(417,371,194,40,color(85,36,43));
            small(424,377,"Playback error",179,white);
            small(424,392,"See log if enabled",184,soft);
        }else{
            /* With the preview panel hidden, keep every video row clear. */
            rect(446,83,170,25,color(85,36,43));
            small(453,84,"Playback failed",157,white);
        }
    }
    rect(12,434,616,1,color(53,62,75));
    draw_footer_hints(green,soft);
    /* Folder count is lazy and cached; movie numbering remains unchanged. */
    if(nitems>0 && items[cursor].is_dir){
        char path[PATH_CAP], digits[12], shown[16];
        unsigned n=0;
        if(append_path(path,sizeof(path),cwd,items[cursor].name)==0){
            uint_to_text(count_folder_videos(path),digits);
            shown[n++]='(';
            for(unsigned i=0;digits[i] && n<sizeof(shown)-2;i++)shown[n++]=digits[i];
            shown[n++]=')';shown[n]=0;
            int width=small_text_width(shown);
            small(628-width,444,shown,width+1,muted);
        }
    } else if(nitems>0){
        char countbuf[22];unsigned x=0,num=0,total=0;
        char temp[10];unsigned j=0;
        for(unsigned i=0;i<nitems;i++){
            if(!items[i].is_dir){
                total++;
                if(i<=cursor)num++;
            }
        }
        do{temp[j++]=(char)('0'+num%10);num/=10;}while(num&&j<10);
        while (j) {
            countbuf[x++] = temp[--j];
        }
        countbuf[x++] = '/';
        j = 0;
        do{temp[j++]=(char)('0'+total%10);total/=10;}while(total&&j<10);
        while (j) {
            countbuf[x++] = temp[--j];
        }
        countbuf[x] = 0;
        int count_width=small_text_width(countbuf);
        small(628-count_width,444,countbuf,count_width+1,muted);
    }
    if(settings_open)draw_settings_menu();
    if(restart_confirm){
        u32 back=color(24,33,46),white2=color(244,246,250),muted2=color(174,190,211);
        rect(91,169,458,142,color(8,13,20));
        rect(95,173,450,134,back);
        rect(95,173,450,3,red);
        const char *title="RESTART VIDEO?";
        const char *message="Play this video from the beginning?";
        small(320-small_text_width(title)/2,192,title,450,white2);
        small(320-small_text_width(message)/2,224,message,450,muted2);
        draw_popup_hints("A RESTART    B CANCEL",95,450,266);
    }
    draw_battery();
}
static int init_fb(void){
    fbfd=(int)call4(SYS_openat,AT_FDCWD,(long)"/dev/fb0",O_RDWR|O_CLOEXEC,0);
    if(fbfd<0)return -1;
    if(call3(SYS_ioctl,fbfd,FBIOGET_VSCREENINFO,(long)&vinfo)<0 ||
       call3(SYS_ioctl,fbfd,FBIOGET_FSCREENINFO,(long)&finfo)<0)return -1;
    W=(int)vinfo.xres;H=(int)vinfo.yres;
    pitch=(int)finfo.line_length/4;
    if(W<640 || H<480 || vinfo.bits_per_pixel!=32 ||
       finfo.line_length<(u32)(vinfo.xoffset+W)*4 ||
       vinfo.yoffset+vinfo.yres>vinfo.yres_virtual ||
       finfo.smem_len< (vinfo.yoffset+vinfo.yres)*finfo.line_length ||
       vinfo.red.length>8 || vinfo.green.length>8 || vinfo.blue.length>8 ||
       vinfo.transp.length>8)return -1;
    long mem=call6(SYS_mmap2,0,finfo.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fbfd,0);
    if(mem<0 && mem>-4096)return -1;
    fb=(volatile u32*)mem;
    origin_x=(W-640)/2;origin_y=(H-480)/2;
    return 0;
}
struct input_event32 {int sec,usec;u16 type,code;int value;};
struct pollfd32 {int fd;short events,revents;};
static int evfds[12],evcount;
static void setup_inputs(void){
    char path[20]="/dev/input/event0";
    for(int i=0;i<12;i++){
        if(i<10)path[16]=(char)('0'+i);
        else {path[16]='1';path[17]=(char)('0'+i-10);path[18]=0;}
        int fd=open_ro(path);
        if(fd>=0)evfds[evcount++]=fd;
    }
}
/* Actions: 1 up, 2 down, 3 open, 4 back, 5 exit,
 * 6/7 pages, 8/9 letters, 10/11 endpoints,
 * 13 Y down, 14 X (restart), 15 SELECT (settings), 16 START (previews),
 * 19 Y released,
 * 17 LEFT, 18 RIGHT (settings). */
static int decode_button(unsigned code){
    switch(code){
    case 103: case 544: return 1;
    case 105: case 546: return 17;
    case 106: case 547: return 18;
    case 108: case 545: return 2;
    case 57: case 304: return 3;
    case 56: case 308: return 13; /* Y: battery OSD */
    case 42: case 307: return 14; /* X: play from beginning */
    case 29: case 305: return 4;
    case 1: case 139: case 316: return 5;
    /* These always navigate in the browser; the Settings toggles apply only
     * to player chapter/video skips, not browser letters or pages. */
    case 15: case 109: case 312: return 6; /* L2 */
    case 14: case 104: case 313: return 7; /* R2 */
    case 18: case 310: return 8; /* L1 */
    case 20: case 311: return 9; /* R1 */
    default:return 0;
    }
}
static int combo_select,combo_start,combo_latched,combo_select_used,combo_start_used,held_up,held_down;
static int next_button(void){
    struct pollfd32 fds[12];
    for(int i=0;i<evcount;i++){fds[i].fd=evfds[i];fds[i].events=POLLIN;fds[i].revents=0;}
    for(;;){
        long r=call3(SYS_poll,(long)fds,evcount,-1);
        if(r<0)return 5;
        for(int i=0;i<evcount;i++)if(fds[i].revents&POLLIN){
            struct input_event32 events[16];
            long n=call3(SYS_read,evfds[i],(long)events,sizeof(events));
            if(n<=0)continue;
            int y_action=0;
            for(unsigned k=0;k<(unsigned)n/sizeof(events[0]);k++){
                if(events[k].type!=1)continue;
                unsigned code=events[k].code;
                int value=events[k].value;
                if(code==97 || code==314 || code==28 || code==315){
                    if(value==0 || value==1){
                        int is_select=(code==97 || code==314);
                        if(is_select){
                            if(value){combo_select=1;combo_select_used=combo_start;
                                      if(combo_start)combo_start_used=1;}
                            else {
                                int trigger=!combo_select_used && !combo_start;
                                combo_select=0;combo_select_used=0;
                                if(trigger)return 15;
                            }
                        }else{
                            if(value){combo_start=1;combo_start_used=combo_select;
                                      if(combo_select)combo_select_used=1;}
                            else{
                                int trigger=!combo_start_used && !combo_select;
                                combo_start=0;combo_start_used=0;
                                if(trigger)return 16;
                            }
                        }
                        if(!combo_select && !combo_start)combo_latched=0;
                        if(combo_select && combo_start && !combo_latched){
                            combo_latched=1;combo_select_used=combo_start_used=1;return 5;
                        }
                    }
                    continue;
                }
                if(code==56 || code==308){
                    /* A down/up pair may arrive in one read; honor its last state. */
                    if(value==1)y_action=13;
                    if(value==0)y_action=19;
                    continue;
                }
                if(code==103 || code==544)held_up=value!=0;
                if(code==108 || code==545)held_down=value!=0;
                if(value==1 || (value==2 && (code==103 || code==108 ||
                                               code==544 || code==545))){
                    int action=decode_button(code);
                    if(action==6 && held_up)return 10;
                    if(action==7 && held_down)return 11;
                    if(action)return action;
                }
            }
            if(y_action)return y_action;
        }
        for(int i=0;i<evcount;i++)if(fds[i].revents&(8|16|32))return 5;
    }
}
static void uint_to_text(unsigned value,char *out){
    char tmp[12];unsigned n=0,i=0;
    do{tmp[n++]=(char)('0'+value%10);value/=10;}while(value && n<sizeof(tmp));
    while (n) {
        out[i++] = tmp[--n];
    }
    out[i] = 0;
}
static unsigned read_scroll(void){
    char text[20];unsigned n=0;
    if(read_file(SCROLL_FILE,text,sizeof(text))<0)return 0;
    for(unsigned i=0;text[i];i++){
        if(text[i]<'0'||text[i]>'9')return 0;
        n=n*10+(unsigned)(text[i]-'0');
        if(n>=MAX_ITEMS)return 0;
    }
    return n;
}
static void save_selection(const char *file){
    char num[16];
    write_file(FOLDER_FILE,cwd);
    write_file(LAST_FILE,file);
    uint_to_text(scrollpos,num);
    write_file(SCROLL_FILE,num);
}
static int run(void){
    if(str_copy(cwd,sizeof(cwd),ROOT)<0)return 3;
    char saved[PATH_CAP];
    if(read_file(FOLDER_FILE,saved,sizeof(saved))==0 && path_under_root(saved)){
        int fd=open_dir(saved);
        if(fd>=0){str_copy(cwd,sizeof(cwd),saved);call1(SYS_close,fd);}
    }
    if(scan_dir()<0)return 4;
    load_browser_settings();
    char lastfile[PATH_CAP];
    if(read_file(LAST_FILE,lastfile,sizeof(lastfile))==0 && path_under_root(lastfile)){
        for(unsigned i=0;i<nitems;i++){
            if(append_path(filepath,sizeof(filepath),cwd,items[i].name)==0 &&
               str_equal(filepath,lastfile)){select_visible(i);break;}
        }
    }
    /* Restore exact previous viewport, not just the selected filename. */
    unsigned saved_scroll=read_scroll();
    if(nitems && saved_scroll<=cursor && cursor<saved_scroll+ROWS_PER_PAGE)
        scrollpos=saved_scroll;
    load_resume_marks();
    show_error=read_file(ERROR_FILE,browser_error,sizeof(browser_error))==0;
    if(show_error)call3(SYS_unlinkat,AT_FDCWD,(long)ERROR_FILE,0);
    if(init_fb()<0)return 5;
    setup_inputs();
    if(evcount==0)return 6;
    display();
    flush_full();
    for(;;){
        unsigned oldcursor=cursor, oldscroll=scrollpos;
        int full_redraw=0;
        int action=next_button();
        if(help_open){
            if(action==4 || action==15){
                help_open=0;help_ready=0;preview_ready=-1;
                display();flush_full();
            }
            continue;
        }
        if(action==13){
            battery_value=read_battery_level();
            battery_visible=1;
            display();flush_rect(510,10,120,50);
            continue;
        }
        if(action==19){
            battery_visible=0;
            display();flush_rect(510,10,120,50);
            continue;
        }
        /* Y remains visible through navigation until the physical key is released. */
        int dismiss_battery=0;
        int restart_approved=0;
        if(restart_confirm){
            if(action==3){restart_confirm=0;restart_approved=1;action=14;}
            else if(action==4 || action==14 || action==15 || action==5){
                restart_confirm=0;
                if(action==5)return 1;
                full_redraw=1;
                action=0;
            }else{
                /* Ignore movement/other keys until the confirmation closes. */
                continue;
            }
        }
        if(!restart_approved && action==14 && nitems && !items[cursor].is_dir &&
           !settings_open){
            restart_confirm=1;
            full_redraw=1;
            action=0;
        }
        /* Dismiss one-shot error notification as soon as navigation resumes. */
        if(show_error){show_error=0;full_redraw=1;}
        if(action==5)return 1;
        if(action==16){toggle_browser_previews();full_redraw=1;}
        else if(action==15){
            settings_open=!settings_open;settings_page=-1;settings_selection=0;
            if(settings_open)load_overlay_names();
            full_redraw=1;
        }else if(settings_open){
            if(action==4){
                if(settings_page>=0){settings_selection=settings_page;settings_page=-1;}
                else settings_open=0;
            }else if(action==1){
                int count=menu_row_count();
                settings_selection=settings_selection?settings_selection-1:count-1;
            }else if(action==2){
                settings_selection=(settings_selection+1)%menu_row_count();
            }else if(action==3 || (action==18 && settings_page<0)){
                if(settings_page<0){
                    if(settings_selection==6){help_open=1;load_controls_help();}
                    else {settings_page=settings_selection;settings_selection=0;}
                }
                else settings_change(1,action==3);
            }else if(action==17 || action==18){
                if(settings_page>=0)settings_change(action==17?-1:1,0);
            }
            full_redraw=1;
        }
        else if(action==1){if(nitems)select_visible(cursor?cursor-1:nitems-1);}
        else if(action==2){if(nitems)select_visible(cursor+1<nitems?cursor+1:0);}
        else if(action==5)return 1;
        else if(action==6){if(cursor>ROWS_PER_PAGE)select_visible(cursor-ROWS_PER_PAGE);else select_visible(0);}
        else if(action==7){select_visible(cursor+ROWS_PER_PAGE<nitems?cursor+ROWS_PER_PAGE:nitems?nitems-1:0);}
        else if(action==8 || action==9){select_visible(nav_letter(action==9));}
        else if(action==10){select_visible(0);}
        else if(action==11){if(nitems)select_visible(nitems-1);}
        else if(action==4){
            if(str_equal(cwd,ROOT))continue; /* B never exits MiniCinema. */
            char previous[PATH_CAP];
            str_copy(previous,sizeof(previous),cwd);
            if(up_directory()<0)return 4;
            for(unsigned i=0;i<nitems;i++){
                if(append_path(filepath,sizeof(filepath),cwd,items[i].name)==0 &&
                   str_equal(filepath,previous)){select_visible(i);break;}
            }
            load_resume_marks();
            full_redraw=1;
        }else if((action==3 || (action==14 && restart_approved)) && nitems &&
                 (action!=14 || !items[cursor].is_dir)){
            if(append_path(filepath,sizeof(filepath),cwd,items[cursor].name)==0){
                if(items[cursor].is_dir){
                    if(str_copy(cwd,sizeof(cwd),filepath)==0){
                        if(scan_dir()<0)return 4;
                        load_resume_marks();
                        full_redraw=1;
                    }
                }else{
                    if(action==14)write_file(RESTART_FILE,"1\n");
                    else call3(SYS_unlinkat,AT_FDCWD,(long)RESTART_FILE,0);
                    char out[PATH_CAP+2];
                    if(str_copy(out,sizeof(out),filepath)==0){
                        usize n=str_len(out);
                        out[n]='\n';out[n+1]=0;
                        if(write_file(SELECT_FILE,out)==0){
                            save_selection(filepath);
                            if(save_playlist()<0)diag_puts("MiniCinema: cannot write playlist\n");
                            /* Immediate visual acknowledgement before video initialization. */
                            rect(417,368,194,43,color(44,53,67));
                            small(436,381,"Opening video...",169,color(244,246,250));
                            flush_rect(417,368,194,43);
                            return 0;
                        }
                    }
                }
            }
        }
        if(!full_redraw && oldcursor==cursor && oldscroll==scrollpos &&
           !dismiss_battery)continue;
        if(previews_enabled && (oldcursor!=cursor || full_redraw))preview_ready=-1;
        display();
        if(full_redraw)flush_full();
        else {
            /* Always redraw the preview; refresh list region only as needed. */
            int list_width=previews_enabled?375:597;
            if(oldscroll!=scrollpos)flush_rect(21,113,list_width,307);
            else {
                flush_rect(21,113+(int)(oldcursor-scrollpos)*38,list_width,37);
                flush_rect(21,113+(int)(cursor-scrollpos)*38,list_width,37);
            }
            if(previews_enabled)flush_rect(410,113,208,305);
            flush_rect(507,439,126,27);
            if(dismiss_battery)flush_rect(510,10,120,50);
        }
    }
}
void _start(void){
    int result=run();
    call1(SYS_exit,result);
    __builtin_unreachable();
}
/* No libc: keep the handful of possible ARM compiler intrinsics local. */
void *memcpy(void *d,const void *s,usize n){
    u8 *p=d;const u8 *q=s;for(usize i=0;i<n;i++)p[i]=q[i];return d;
}
void *memset(void *d,int v,usize n){
    u8 *p=d;for(usize i=0;i<n;i++)p[i]=(u8)v;return d;
}
void __aeabi_memcpy(void *d,const void *s,usize n){memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,usize n){memcpy(d,s,n);}
void __aeabi_memcpy8(void *d,const void *s,usize n){memcpy(d,s,n);}
void __aeabi_memclr(void *d,usize n){memset(d,0,n);}
void __aeabi_memclr4(void *d,usize n){memset(d,0,n);}
void __aeabi_memset(void *d,usize n,int v){memset(d,v,n);}
/* Required by freestanding ARM builds: no libgcc or target libc at link time. */
unsigned __aeabi_uidiv(unsigned numerator, unsigned denominator){
    unsigned quotient=0,remainder=0;
    if(!denominator)return 0;
    for(int bit=31;bit>=0;bit--){
        remainder=(remainder<<1)|((numerator>>(unsigned)bit)&1u);
        if(remainder>=denominator){
            remainder-=denominator; quotient|=1u<<(unsigned)bit;
        }
    }
    return quotient;
}

/* The browser also uses signed division. GCC lowers signed / and % to
 * __aeabi_idiv and __aeabi_idivmod on this ARM target. Use unsigned magnitudes
 * so the helper itself never requires another signed-division runtime symbol.
 */
int __aeabi_idiv(int numerator, int denominator){
    unsigned n=(unsigned)numerator,d=(unsigned)denominator;
    if(numerator<0)n=0u-n;
    if(denominator<0)d=0u-d;
    unsigned q=__aeabi_uidiv(n,d);
    if((numerator<0)!=(denominator<0))q=0u-q;
    return (int)q;
}

/* Signed quotient in r0 and signed remainder in r1 (ARM EABI/AAPCS).
 * The subtraction is done in registers, so even INT_MIN is safe here.
 */
__asm__(
    ".text\n"
    ".align 2\n"
    ".global __aeabi_idivmod\n"
    ".type __aeabi_idivmod, %function\n"
    "__aeabi_idivmod:\n"
    "    push {r4, r5, r6, lr}\n"
    "    mov r4, r0\n"
    "    mov r5, r1\n"
    "    bl __aeabi_idiv\n"
    "    mul r2, r0, r5\n"
    "    sub r1, r4, r2\n"
    "    pop {r4, r5, r6, pc}\n"
    ".size __aeabi_idivmod, .-__aeabi_idivmod\n"
);

/* ARM EABI expects __aeabi_uidivmod to return quotient in r0 and remainder
 * in r1. Implement both using our libc-free __aeabi_uidiv above, rather than
 * pulling in libgcc (which also defines __aeabi_uidiv and references raise).
 * Saving four registers maintains AAPCS stack alignment across the BL.
 */
__asm__(
    ".text\n"
    ".align 2\n"
    ".global __aeabi_uidivmod\n"
    ".type __aeabi_uidivmod, %function\n"
    "__aeabi_uidivmod:\n"
    "    push {r4, r5, r6, lr}\n"
    "    mov r4, r0\n"
    "    mov r5, r1\n"
    "    bl __aeabi_uidiv\n"
    "    mul r2, r0, r5\n"
    "    sub r1, r4, r2\n"
    "    pop {r4, r5, r6, pc}\n"
    ".size __aeabi_uidivmod, .-__aeabi_uidivmod\n"
);
