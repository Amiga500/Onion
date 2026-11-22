/*
 * DisplayCal v0.8.4 for Onion
 * Freestanding ARM/Linux utility: direct framebuffer + libz PNG decode.
 * Uses /dev/fb0 + /dev/input/event* and the working SigmaStar DISP proc node.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

enum {
    SYS_exit=1, SYS_read=3, SYS_write=4, SYS_open=5, SYS_close=6, SYS_access=33,
    SYS_mkdir=39, SYS_ioctl=54, SYS_munmap=91, SYS_nanosleep=162, SYS_mmap2=192, SYS_clock_gettime=263
};

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_NONBLOCK 04000
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define PROT_READ 1
#define PROT_WRITE 2
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602

static inline long sc1(long n,long a) {
    register long r7 asm("r7")=n; register long r0 asm("r0")=a;
    asm volatile("svc 0" : "+r"(r0) : "r"(r7) : "memory"); return r0;
}
static inline long sc2(long n,long a,long b) {
    register long r7 asm("r7")=n; register long r0 asm("r0")=a; register long r1 asm("r1")=b;
    asm volatile("svc 0" : "+r"(r0) : "r"(r1),"r"(r7) : "memory"); return r0;
}
static inline long sc3(long n,long a,long b,long c) {
    register long r7 asm("r7")=n; register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c;
    asm volatile("svc 0" : "+r"(r0) : "r"(r1),"r"(r2),"r"(r7) : "memory"); return r0;
}
static inline long sc6(long n,long a,long b,long c,long d,long e,long f) {
    register long r7 asm("r7")=n; register long r0 asm("r0")=a; register long r1 asm("r1")=b; register long r2 asm("r2")=c;
    register long r3 asm("r3")=d; register long r4 asm("r4")=e; register long r5 asm("r5")=f;
    asm volatile("svc 0" : "+r"(r0) : "r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5),"r"(r7) : "memory"); return r0;
}

static long xopen(const char *p,int fl,int mode){return sc3(SYS_open,(long)p,fl,mode);} 
static long xread(int fd,void *p,u32 n){return sc3(SYS_read,fd,(long)p,n);} 
static long xwrite(int fd,const void *p,u32 n){return sc3(SYS_write,fd,(long)p,n);} 
static long xclose(int fd){return sc1(SYS_close,fd);} 
static long xmkdir(const char *p,int mode){return sc2(SYS_mkdir,(long)p,mode);} 
static long xioctl(int fd,u32 req,void *p){return sc3(SYS_ioctl,fd,req,(long)p);} 
static void *xmmap(void *addr,u32 len,int prot,int flags,int fd,u32 off_pages){return (void*)sc6(SYS_mmap2,(long)addr,len,prot,flags,fd,off_pages);} 
static void xmunmap(void *p,u32 n){sc2(SYS_munmap,(long)p,n);} 
static void xsleep_ms(u32 ms){ struct {u32 sec,nsec;} ts; ts.sec=0; ts.nsec=ms*1000000u; sc2(SYS_nanosleep,(long)&ts,0); }
static u32 udiv32(u32 n,u32 d){u32 q=0,bit=1,dd=d;if(!d)return 0xffffffffu;while(dd<=0x7fffffffu && (dd<<1)<=n){dd<<=1;bit<<=1;}while(bit){if(n>=dd){n-=dd;q|=bit;}dd>>=1;bit>>=1;}return q;}
static void xexit(int code){sc1(SYS_exit,code); for(;;){} }

static u32 slen(const char *s){u32 n=0; while(s[n]) n++; return n;}
static void puts2(const char *s){xwrite(1,s,slen(s));}

struct fb_bitfield {u32 offset,length,msb_right;};
struct fb_var_screeninfo {
    u32 xres,yres,xres_virtual,yres_virtual,xoffset,yoffset,bits_per_pixel,grayscale;
    struct fb_bitfield red,green,blue,transp;
    u32 nonstd,activate,height,width,accel_flags,pixclock,left_margin,right_margin,upper_margin,lower_margin,hsync_len,vsync_len,sync,vmode,rotate,colorspace,reserved[4];
};
struct fb_fix_screeninfo {
    char id[16]; u32 smem_start,smem_len,type,type_aux,visual; u16 xpanstep,ypanstep,ywrapstep; u32 line_length; u32 mmio_start,mmio_len,accel; u16 capabilities,reserved[2];
};
struct input_event32 {u32 sec,usec; u16 type,code; s32 value;};

static volatile u8 *fb; /* Current visible page, not necessarily mmap base. */
static volatile u8 *fb_map; /* Actual mmap base for munmap. */
static u8 *canvas,*previous; /* Off-screen canvas and previous frame. */
static int presented=0;
static u32 W,H,BPP,STRIDE,MAPLEN,BUFLEN;
static struct fb_var_screeninfo vi;

static u32 chan(u8 v,u32 len){
    if(len==8) return v;
    if(len==6) return udiv32((u32)v*63u + 127u,255u);
    if(len==5) return udiv32((u32)v*31u + 127u,255u);
    if(len==4) return udiv32((u32)v*15u + 127u,255u);
    if(len==0) return 0;
    return ((u32)v >> (8-len));
}
static u32 px(u8 r,u8 g,u8 b){
    return (chan(r,vi.red.length)<<vi.red.offset) | (chan(g,vi.green.length)<<vi.green.offset) | (chan(b,vi.blue.length)<<vi.blue.offset);
}
static void pset(int x,int y,u32 c){
    if(x<0||y<0||(u32)x>=W||(u32)y>=H) return;
    /* Original Miyoo Mini framebuffer is presented 180 degrees from raw fb coordinates. */
    x=(int)W-1-x; y=(int)H-1-y;
    u8 *p=canvas + (u32)y*STRIDE + (u32)x*(BPP/8u);
    if(BPP==16){ *(u16*)p=(u16)c; }
    else if(BPP==32){ *(u32*)p=c; }
    else if(BPP==24){ p[0]=(u8)c; p[1]=(u8)(c>>8); p[2]=(u8)(c>>16); }
}
/* Clipped rectangle fill in RAM, not directly to visible video memory. */
static void rect(int x,int y,int w,int h,u32 c){
    int x0=x,y0=y,x1=x+w,y1=y+h,yy,xx;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)W)x1=(int)W;
    if(y1>(int)H)y1=(int)H;
    if(x0>=x1||y0>=y1)return;
    for(yy=y0;yy<y1;yy++){
        u8 *row=canvas+(u32)((int)H-1-yy)*STRIDE+(u32)((int)W-x1)*(BPP/8u);
        int n=x1-x0;
        if(BPP==16){u16 *p=(u16*)row;for(xx=0;xx<n;xx++)p[xx]=(u16)c;}
        else if(BPP==32){u32 *p=(u32*)row;for(xx=0;xx<n;xx++)p[xx]=c;}
        else {for(xx=0;xx<n;xx++){u8 *p=row+xx*3;p[0]=(u8)c;p[1]=(u8)(c>>8);p[2]=(u8)(c>>16);}}
    }
}
/* Incremental present. Never clear the *visible* framebuffer on UI redraw.
 * This keeps reference images and black levels steady while moving the cursor.
 */
static void present(void){
    u32 y,x;
    for(y=0;y<H;y++){
        u8 *src=canvas+y*STRIDE,*old=previous+y*STRIDE;
        volatile u8 *out=fb+y*STRIDE;
        if(BPP==16){
            u16 *s=(u16*)src,*p=(u16*)old;
            volatile u16 *d=(volatile u16*)out;
            for(x=0;x<W;x++)if(!presented||s[x]!=p[x]){d[x]=s[x];p[x]=s[x];}
        }else if(BPP==32){
            u32 *s=(u32*)src,*p=(u32*)old;
            volatile u32 *d=(volatile u32*)out;
            for(x=0;x<W;x++)if(!presented||s[x]!=p[x]){d[x]=s[x];p[x]=s[x];}
        }else{
            for(x=0;x<W*3u;x++)if(!presented||src[x]!=old[x]){out[x]=src[x];old[x]=src[x];}
        }
    }
    presented=1;
} 

/* compact 5x7 font: digits + selected uppercase ASCII used by UI. */
struct glyph {char ch; u8 r[7];};
static const struct glyph F[] = {
{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},{'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
{'I',{14,4,4,4,4,4,14}},{'J',{7,2,2,2,2,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
{'+',{0,4,4,31,4,4,0}},{'-',{0,0,0,31,0,0,0}},{'/',{1,2,2,4,8,8,16}},{':',{0,4,4,0,4,4,0}},{'.',{0,0,0,0,0,12,12}}
};
static const u8 *glyph(char c){u32 i; if(c>='a'&&c<='z') c-=32; for(i=0;i<sizeof(F)/sizeof(F[0]);i++) if(F[i].ch==c) return F[i].r; return 0;}
static void chr5(int x,int y,char c,int s,u32 col){const u8 *g=glyph(c);int yy,xx; if(c==' ') return; if(!g) return; for(yy=0;yy<7;yy++) for(xx=0;xx<5;xx++) if(g[yy]&(1u<<(4-xx))) rect(x+xx*s,y+yy*s,s,s,col);} 
static void text5(int x,int y,const char *s,int scale,u32 col){while(*s){chr5(x,y,*s,scale,col);x+=6*scale;s++;}}

static int append_num(char *b,int p,int v){
    if(v>=100){int h=0,t=0;while(v>=100){v-=100;h++;}while(v>=10){v-=10;t++;}b[p++]='0'+h;b[p++]='0'+t;b[p++]='0'+v;}
    else if(v>=10){int t=0;while(v>=10){v-=10;t++;}b[p++]='0'+t;b[p++]='0'+v;}
    else b[p++]='0'+v;
    return p;
}

static int isserialchar(char c){return (c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'||c=='_';}
static int read_small(const char *path,char *buf,int cap){
    long fd=xopen(path,O_RDONLY,0); int n=0; if(fd<0)return -1;
    while(n<cap-1){long r=xread((int)fd,buf+n,(u32)(cap-1-n)); if(r<=0)break; n+=(int)r;}
    xclose((int)fd); buf[n]=0; return n;
}
static int allzero_id(const char *s){
    int i=0,seen=0;
    while(s[i]){
        char c=s[i++];
        if(c=='0') {seen=1; continue;}
        if(c=='-'||c=='_'||c==' '||c=='\r'||c=='\n'||c=='\t') continue;
        return 0;
    }
    return seen;
}
static int get_serial(char *out,int cap){
    char b[128]; int n=read_small("/tmp/displaycal-device-id",b,sizeof(b)); int i,k=0;
    if(n>0){
        for(i=0;i<n&&k<cap-1;i++){
            if(isserialchar(b[i])) out[k++]=b[i];
            else if(k) break;
        }
        out[k]=0;
        if(k>0 && !allzero_id(out)) return k;
    }
    out[0]='d';out[1]='e';out[2]='f';out[3]='a';out[4]='u';out[5]='l';out[6]='t';out[7]=0;
    return 7;
}
static int cfg_path(char *out,int cap){
    const char *pre="/mnt/SDCARD/.tmp_update/config/display/cal-"; const char *suf=".cfg"; char serial[80]; int p=0,i,n;
    n=get_serial(serial,sizeof(serial)); if(n<=0)return -1;
    for(i=0;pre[i]&&p<cap-1;i++)out[p++]=pre[i];
    for(i=0;serial[i]&&p<cap-1;i++)out[p++]=serial[i];
    for(i=0;suf[i]&&p<cap-1;i++)out[p++]=suf[i];
    if(p>=cap-1)return -2;
    out[p]=0;
    return p;
}
static int parse_cfg(const char *path,int *rr,int *gg,int *bb,int *ww){
    char bfr[256]; int n=read_small(path,bfr,sizeof(bfr)); int i=0,got=0;
    if(n<=0)return -1;
    while(i<n){char c=bfr[i++]; if((c=='R'||c=='G'||c=='B'||c=='W')&&i<n&&bfr[i]=='='){
        int v=0,d=0,sign=1; i++;
        if(c=='W' && i<n && (bfr[i]=='-'||bfr[i]=='+')) {if(bfr[i]=='-')sign=-1; i++;}
        while(i<n&&bfr[i]>='0'&&bfr[i]<='9'&&d<4){v=v*10+(bfr[i]-'0');i++;d++;}
        if(d && (i==n || bfr[i]<'0'||bfr[i]>'9')) {
            if(c=='W'){if(v<=100)*ww=v*sign;}
            else if(v<=255){if(c=='R')*rr=v; else if(c=='G')*gg=v; else *bb=v; got++;}
        }
    }}
    return got>=3?0:-2;
}
/* Preserve the actual kernel return for diagnosing post-MainUI failures.
 * Raw ARM syscalls return -errno, unlike libc which returns -1 and sets errno.
 */
static int rgb_error_stage=0, rgb_csc_missing=0;
static long rgb_error_sysrc=0;
static int apply_rgb(int r,int g,int b){
    const char *node="/proc/mi_modules/mi_disp/mi_disp0"; char s[64]; int p=0; long fd,n;
    const char *pre="colortemp 0 0 0 0 "; u32 i;
    for(i=0;pre[i];i++) s[p++]=pre[i];
    p=append_num(s,p,b); s[p++]=' '; p=append_num(s,p,g); s[p++]=' '; p=append_num(s,p,r); s[p++]='\n';
    rgb_error_stage=0;rgb_error_sysrc=0;
    fd=xopen(node,O_WRONLY,0);
    if(fd<0){rgb_error_stage=1;rgb_error_sysrc=fd;return -1;}
    n=xwrite((int)fd,s,p);
    xclose((int)fd);
    if(n!=p){rgb_error_stage=2;rgb_error_sysrc=n;return -2;}
    return 0;
}
static int is_neutral_profile(int r,int g,int b,int w){
    return r==128 && g==128 && b==128 && w==0;
}
static int save_cfg(int r,int g,int b,int w){
    char path[192], tmp[200],s[64];int p=0,i=0;long fd,n;
    if(cfg_path(path,sizeof(path))<0)return -3;
    /* A fully neutral profile means calibration disabled. Remove the file
     * rather than rewriting 128/128/128 at every boot. ENOENT is fine.
     */
    if(is_neutral_profile(r,g,b,w)){
        long rc=sc1(10,(long)path); /* ARM unlink */
        return rc==0 || rc==-2 ? 0 : -6;
    }
    xmkdir("/mnt/SDCARD/.tmp_update/config/display",0755);
    for(;path[i]&&i<(int)sizeof(tmp)-5;i++) tmp[i]=path[i];
    if(i>=(int)sizeof(tmp)-5)return -4;
    tmp[i++]='.';tmp[i++]='t';tmp[i++]='m';tmp[i++]='p';tmp[i]=0;
    s[p++]='R';s[p++]='=';p=append_num(s,p,r);s[p++]='\n';
    s[p++]='G';s[p++]='=';p=append_num(s,p,g);s[p++]='\n';
    s[p++]='B';s[p++]='=';p=append_num(s,p,b);s[p++]='\n';
    s[p++]='W';s[p++]='=';if(w<0){s[p++]='-';w=-w;}p=append_num(s,p,w);s[p++]='\n';
    fd=xopen(tmp,O_WRONLY|O_CREAT|O_TRUNC,0644);if(fd<0)return -1;
    n=xwrite((int)fd,s,p);
    if(n==p)sc1(118,fd); /* fsync syscall on ARM */
    xclose((int)fd);
    if(n!=p){sc1(10,(long)tmp);return -2;} /* unlink leftover temp */
    if(sc2(38,(long)tmp,(long)path)<0){sc1(10,(long)tmp);return -5;} /* rename */
    return 0;
}

/* Lightweight PNG reader for the installed Onion topbar graphic.
 * Supports non-interlaced PNG grayscale / indexed / RGB / RGBA, 1/2/4/8-bit
 * where applicable. Uses system libz for IDAT inflate. The GUI works without
 * libz or an image: only the preview is omitted. No image is bundled.
 */
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
#define PNG_FILE_CAP (2u * 1024u * 1024u)
#define PNG_INFLATE_CAP (4u * 1024u * 1024u)
static u8 png_file[PNG_FILE_CAP];
static u8 png_idat[PNG_FILE_CAP];
static u8 png_data[PNG_INFLATE_CAP];
static u8 png_pal[768], png_alpha[256];
static u32 png_w, png_h, png_stride, png_bits, png_type, png_channels, palette_n_global;
static int png_ok=0;
static u32 png_be(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static int png_name(const u8 *p, const char *q){int k;for(k=0;k<4;k++)if(p[k]!=(u8)q[k])return 0;return 1;}
static int png_paeth(int a,int b,int c){int p=a+b-c,pa=p-a,pb=p-b,pc=p-c; if(pa<0)pa=-pa;if(pb<0)pb=-pb;if(pc<0)pc=-pc;return pa<=pb&&pa<=pc?a:(pb<=pc?b:c);}
static int png_load(const char *path){
    png_ok=0;
    long fd=xopen(path,O_RDONLY,0);u32 n=0,at=8,comp_len=0,palette_n=0;int got_ihdr=0,got_idat=0;
    if(fd<0)return -1;
    while(n<PNG_FILE_CAP){long r=xread((int)fd,png_file+n,PNG_FILE_CAP-n);if(r<0){xclose((int)fd);return -2;}if(!r)break;n+=(u32)r;}
    xclose((int)fd);
    if(n<33||n==PNG_FILE_CAP)return -2;
    { const u8 sig[8]={137,80,78,71,13,10,26,10};int i;for(i=0;i<8;i++)if(png_file[i]!=sig[i])return -3; }
    {int i;for(i=0;i<256;i++)png_alpha[i]=255;}
    while(at+12<=n){
        u32 len=png_be(png_file+at);const u8 *type=png_file+at+4, *data=png_file+at+8;
        if(len>n-at-12)return -4;
        if(png_name(type,"IHDR")){
            if(got_ihdr||len!=13)return -4;
            png_w=png_be(data);png_h=png_be(data+4);png_bits=data[8];png_type=data[9];
            if(data[10]!=0||data[11]!=0||data[12]!=0)return -5;
            if(!png_w||!png_h||png_w>2048||png_h>1024)return -5;
            if(png_type==0) png_channels=1;
            else if(png_type==2)png_channels=3;
            else if(png_type==3)png_channels=1;
            else if(png_type==4)png_channels=2;
            else if(png_type==6)png_channels=4;
            else return -5;
            if(png_type==2||png_type==4||png_type==6){if(png_bits!=8)return -5;}
            else if(png_bits!=1&&png_bits!=2&&png_bits!=4&&png_bits!=8)return -5;
            png_stride=(png_w*png_bits*png_channels+7)/8;
            if((png_stride+1)*(png_h)>PNG_INFLATE_CAP)return -5;
            got_ihdr=1;
        }else if(png_name(type,"PLTE")){
            if(len>768||len%3)return -6;
            palette_n=len/3;
            {u32 i;for(i=0;i<len;i++)png_pal[i]=data[i];}
        }else if(png_name(type,"tRNS")){
            if(png_type==3){u32 i;for(i=0;i<len&&i<256;i++)png_alpha[i]=data[i];}
        }else if(png_name(type,"IDAT")){
            if(comp_len+len>PNG_FILE_CAP)return -7;
            {u32 i;for(i=0;i<len;i++)png_idat[comp_len+i]=data[i];}
            comp_len+=len;got_idat=1;
        }else if(png_name(type,"IEND")){break;}
        at+=len+12;
    }
    if(!got_ihdr||!got_idat||(png_type==3&&!palette_n))return -8;
    palette_n_global=palette_n;
    void *hz=dlopen("libz.so.1",2);
    if(!hz)hz=dlopen("/lib/libz.so.1",2);
    if(!hz)hz=dlopen("/mnt/SDCARD/.tmp_update/lib/libz.so.1",2);
    if(!hz)return -9;
    typedef int (*uncompress_fn)(u8 *,unsigned long *,const u8 *,unsigned long);
    uncompress_fn inflate=(uncompress_fn)dlsym(hz,"uncompress");
    if(!inflate)return -10;
    unsigned long rawlen=(unsigned long)(png_stride+1)*png_h;
    if(inflate(png_data,&rawlen,png_idat,comp_len)!=0 || rawlen!=(unsigned long)(png_stride+1)*png_h)return -11;
    /* Defilter in place, moving rows down by one filter byte per scanline.
       Traverse forward so previous unfiltered row has already been moved. */
    u32 y,x,bpp=(png_bits*png_channels+7)/8;
    if(!bpp)bpp=1;
    for(y=0;y<png_h;y++){
        u32 from=y*(png_stride+1),to=y*png_stride;
        u8 filter=png_data[from];
        if(filter>4)return -12;
        for(x=0;x<png_stride;x++){
            u8 raw=png_data[from+1+x];
            int left=x>=bpp?png_data[to+x-bpp]:0;
            int up=y?png_data[to-png_stride+x]:0;
            int ul=(y&&x>=bpp)?png_data[to-png_stride+x-bpp]:0;
            int predictor=0;
            if(filter==1)predictor=left;
            else if(filter==2)predictor=up;
            else if(filter==3)predictor=(left+up)/2;
            else if(filter==4)predictor=png_paeth(left,up,ul);
            png_data[to+x]=(u8)(raw+predictor);
        }
    }
    png_ok=1;
    return 0;
}
static u32 png_sample(int x,int y){
    if(!png_ok||x<0||y<0||(u32)x>=png_w||(u32)y>=png_h)return 0u;
    const u8 *p=png_data+(u32)y*png_stride;
    u32 r=0,g=0,b=0,a=255;
    if(png_type==2){p+=3*x;r=p[0];g=p[1];b=p[2];}
    else if(png_type==6){p+=4*x;r=p[0];g=p[1];b=p[2];a=p[3];}
    else if(png_type==4){p+=2*x;r=g=b=p[0];a=p[1];}
    else {u32 index;
        if(png_bits==8)index=p[x];
        else{u32 offset=(u32)x*png_bits;index=(p[offset/8]>>(8-png_bits-(offset%8)))&((1u<<png_bits)-1);}
        if(png_type==3){if(index>=palette_n_global)return 0; r=png_pal[index*3];g=png_pal[index*3+1];b=png_pal[index*3+2];a=png_alpha[index];}
        else {r=g=b=(index*255)/((1u<<png_bits)-1);}
    }
    /* Composite transparent pixels over true black for a neutral preview. */
    if(a!=255){r=(r*a+127)/255;g=(g*a+127)/255;b=(b*a+127)/255;}
    return px((u8)r,(u8)g,(u8)b);
}
static int image_idx=0;
/* Cached high-quality 3x3 downsample for the normal preview panel.
 * Regenerated on image changes, not on RGB adjustment / selection moves.
 */
#define PREVIEW_W 290
#define PREVIEW_H 280
static u32 preview_cache[PREVIEW_W * PREVIEW_H];
static int cache_image=-1, cache_dw=0, cache_dh=0;
static int ch8(u32 c,struct fb_bitfield f);
static u32 filtered_sample(int i,int j,int dw,int dh){
    u32 rr=0,gg=0,bb=0;int a,b;
    for(b=0;b<3;b++)for(a=0;a<3;a++){
        int sx=(int)udiv32((u32)(6*i+2*a+1)*png_w,(u32)6*dw);
        int sy=(int)udiv32((u32)(6*j+2*b+1)*png_h,(u32)6*dh);
        u32 q=png_sample(sx,sy);
        rr+=(u32)ch8(q,vi.red);gg+=(u32)ch8(q,vi.green);bb+=(u32)ch8(q,vi.blue);
    }
    /* Retain a little center-pixel detail without nearest-neighbor aliasing. */
    u32 mid=png_sample((int)udiv32((u32)(2*i+1)*png_w,(u32)2*dw),
                       (int)udiv32((u32)(2*j+1)*png_h,(u32)2*dh));
    return px((u8)((rr + (u32)ch8(mid,vi.red)*2+5)/11),
              (u8)((gg + (u32)ch8(mid,vi.green)*2+5)/11),
              (u8)((bb + (u32)ch8(mid,vi.blue)*2+5)/11));
}
static void png_draw_preview(int x,int y,int boxw,int boxh){
    rect(x,y,boxw,boxh,px(0,0,0));
    if(!png_ok){
        text5(x+20,y+boxh/2-8,"TOPBAR PREVIEW UNAVAILABLE",2,px(185,190,198));
        return;
    }
    /* The Onion topbar is an exact pixel/color reference: never rescale it.
     * If it is larger than the preview, crop equally from both sides instead.
     * The other calibration images remain aspect-fit in the small preview.
     */
    if(image_idx==4){
        int dw=(int)png_w,dh=(int)png_h;
        int srcx=0,srcy=0,dx=0,dy=0,i,j;
        if(dw>boxw){srcx=(dw-boxw)/2;dw=boxw;}
        else dx=(boxw-dw)/2;
        if(dh>boxh){srcy=(dh-boxh)/2;dh=boxh;}
        else dy=(boxh-dh)/2;
        for(j=0;j<dh;j++)for(i=0;i<dw;i++)
            pset(x+dx+i,y+dy+j,png_sample(srcx+i,srcy+j));
        return;
    }
    int dw=(int)png_w,dh=(int)png_h;
    if(dw>boxw){dh=(int)udiv32((u32)dh*(u32)boxw,(u32)dw);dw=boxw;}
    if(dh>boxh){dw=(int)udiv32((u32)dw*(u32)boxh,(u32)dh);dh=boxh;}
    if(dw<1||dh<1)return;
    int offset=(boxw-dw)/2,voff=(boxh-dh)/2,i,j;
    if(boxw==PREVIEW_W && boxh==PREVIEW_H){
        if(cache_image!=image_idx || cache_dw!=dw || cache_dh!=dh){
            for(j=0;j<dh;j++)for(i=0;i<dw;i++)
                preview_cache[j*PREVIEW_W+i]=filtered_sample(i,j,dw,dh);
            cache_image=image_idx;cache_dw=dw;cache_dh=dh;
        }
        for(j=0;j<dh;j++)for(i=0;i<dw;i++)
            pset(x+offset+i,y+voff+j,preview_cache[j*PREVIEW_W+i]);
    }else{
        /* Fullscreen is normally 1:1 on the Mini; never resample it needlessly. */
        for(j=0;j<dh;j++)for(i=0;i<dw;i++){
            u32 sx=udiv32((u32)i*png_w,(u32)dw);
            u32 sy=udiv32((u32)j*png_h,(u32)dh);
            pset(x+offset+i,y+voff+j,png_sample((int)sx,(int)sy));
        }
    }
}

/* Reference images include a user-provided gray chart and MIT-licensed artwork from onion-color-balance.
 * The actual theme topbar is last and loaded from the SD card.
 * SELECT: next image; hold SELECT for >= 600 ms: full screen on/off.
 */
static const char *image_paths[] = {
    "testimages/04_color_bars.png",
    "testimages/01_white_gray.png",
    "testimages/02_gray_ramp.png",
    "testimages/03_natural_colors.png",
    "/mnt/SDCARD/miyoo/app/skin/miyoo-topbar.png"
};
static const char *image_names[] = {
    "COLOR BARS", "WHITE GRAY", "GRAY RAMP", "NATURAL", "TOPBAR"
};
#define NUM_IMAGES 5
static int fullscreen=0,png_err=0;
static int r=128,g=128,b=128,warmth=0,sel=0,page=0,applied=0,status=0;
static int original_r=128,original_g=128,original_b=128,original_w=0;
static int original_has_cfg=0;
static int changed_by_gui=0,invalid_saved_cfg=0;
/* /tmp/.blfOn is Onion's active Blue Light Filter state flag. */
static int blf_on(void){
    return sc2(SYS_access,(long)"/tmp/.blfOn",0)==0;
}
#include "display_settings.inc"
static int clamp(int v){return v<0?0:(v>255?255:v);}
static int clamp_w(int v){return v<-100?-100:(v>100?100:v);}
/* Warmth does not alter the stored base RGB values.  Both warm and cool
 * settings attenuate channels instead of boosting above the user values.
 * This is an additive adjustment in *display control units*, not Kelvin.
 */
#define MIN_RGB_TOTAL 70
static int rgb_total_ok(int er,int eg,int eb){
    return er>=0 && eg>=0 && eb>=0 && er+eg+eb>=MIN_RGB_TOTAL;
}
static void effective_rgb(int *er,int *eg,int *eb){
    *er=r;*eg=g;*eb=b;
    if(warmth>0){
        *eg=clamp(g - (warmth*28+50)/100);
        *eb=clamp(b - (warmth*56+50)/100);
    }else if(warmth<0){
        int cool=-warmth;
        *er=clamp(r - (cool*56+50)/100);
        *eg=clamp(g - (cool*28+50)/100);
    }
}
/* Diagnostic is kept in tmpfs, not written to the SD card on GUI launch.
 * This remains available after returning to MainUI and costs nothing on --apply.
 */
static void print_rgb_error(void){
    char b[176]; int n=0,digitpos,i;long rc=rgb_error_sysrc,fd;
    const char *msg="DisplayCal: RGB colortemp ";
    for(i=0;msg[i];i++)b[n++]=msg[i];
    msg=rgb_error_stage==1?"open":"write";
    for(i=0;msg[i];i++)b[n++]=msg[i];
    msg=" failed (syscall_rc=";
    for(i=0;msg[i];i++)b[n++]=msg[i];
    if(rc<0){b[n++]='-';rc=-rc;}
    digitpos=n;
    do{b[n++]=(char)('0'+(rc%10));rc/=10;}while(rc&&n<(int)sizeof(b)-62);
    for(i=0;i<(n-digitpos)/2;i++){
        char tmp=b[digitpos+i];b[digitpos+i]=b[n-1-i];b[n-1-i]=tmp;
    }
    msg=rgb_csc_missing?", CscMatrix=missing)\n":", CscMatrix=present)\n";
    for(i=0;msg[i];i++)b[n++]=msg[i];
    xwrite(1,b,(u32)n);
    fd=xopen("/tmp/displaycal-last-error",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){xwrite((int)fd,b,(u32)n);xclose((int)fd);}
}
static int apply_current(void){
    int rr,gg,bb;
    if(blf_on()){status=-6;applied=0;return status;}
    effective_rgb(&rr,&gg,&bb);
    if(!rgb_total_ok(rr,gg,bb)){
        status=-7;applied=0;return status; /* No hardware write. */
    }
    status=apply_rgb(rr,gg,bb);applied=(status==0);
    if(status<0){
        rgb_csc_missing=!dproc_csc_available();
        print_rgb_error();
    }else{
        rgb_csc_missing=0;
        sc1(10,(long)"/tmp/displaycal-last-error"); /* unlink stale diagnostic */
    }
    return status;
}
static int load_image(int idx){
    if(idx<0||idx>=NUM_IMAGES)return -1;
    image_idx=idx;
    cache_image=-1;
    png_err=png_load(image_paths[idx]);
    if(png_err<0){
        puts2("DisplayCal: PNG unavailable: ");puts2(image_paths[idx]);puts2("\n");
    }
    return png_err;
}

/* RedOnion-inspired UI: 640x480, direct framebuffer.
 * RedOnion accent is exactly #EB5F7F; RGB labels remain pure primaries.
 */
#include "ui_font_data.h"
static u32 accent(void){return px(235,95,127);}
static void gradient_rect(int x,int y,int w,int h,int r0,int g0,int b0,int r1,int g1,int b1){
    int k;
    for(k=0;k<h;k++){
        int t=h>1? k*256/(h-1):0;
        u8 rr=(u8)(r0+((r1-r0)*t)/256);
        u8 gg=(u8)(g0+((g1-g0)*t)/256);
        u8 bb=(u8)(b0+((b1-b0)*t)/256);
        rect(x,y+k,w,1,px(rr,gg,bb));
    }
}
static int curve_inset(int row,int rad){
    if(row>=rad) return 0;
    int d=rad-row;
    return (d*d+rad)/(rad+1);
}
static void round_fill(int x,int y,int w,int h,int rad,u32 c){
    int j;
    if(rad>h/2)rad=h/2;
    for(j=0;j<h;j++){
        int dist=j<h/2?j:h-1-j;
        int cut=curve_inset(dist,rad);
        rect(x+cut,y+j,w-2*cut,1,c);
    }
}
static void round_stroke(int x,int y,int w,int h,int rad,u32 border,u32 center,int thick){
    round_fill(x,y,w,h,rad,border);
    round_fill(x+thick,y+thick,w-2*thick,h-2*thick,rad-thick,center);
}
static void disk(int cx,int cy,int r,u32 c){
    int xx,yy;
    for(yy=-r;yy<=r;yy++)for(xx=-r;xx<=r;xx++){
        if(xx*xx+yy*yy<=r*r) pset(cx+xx,cy+yy,c);
    }
}
/* Glyph alpha is packed 4-bit from a Lato bitmap, not an external font dependency. */
static u32 pget(int x,int y){
    if(x<0||y<0||(u32)x>=W||(u32)y>=H)return 0;
    u8 *p=canvas + (u32)((int)H-1-y)*STRIDE + (u32)((int)W-1-x)*(BPP/8u);
    if(BPP==16)return *(u16*)p;
    if(BPP==32)return *(u32*)p;
    return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16);
}
static int ch8(u32 c,struct fb_bitfield f){
    u32 v=(c>>f.offset)&((1u<<f.length)-1);
    if(f.length==8)return (int)v;
    if(f.length==6)return (int)((v<<2)|(v>>4));
    if(f.length==5)return (int)((v<<3)|(v>>2));
    if(f.length==0)return 0;
    return (int)udiv32(v*255u,((1u<<f.length)-1u));
}
static void alpha_pixel(int x,int y,u32 color,int alpha){
    if(alpha<=0)return;
    if(alpha>=15){pset(x,y,color);return;}
    u32 under=pget(x,y);
    int a=alpha,inv=16-a;
    int rr=(ch8(under,vi.red)*inv+ch8(color,vi.red)*a+8)>>4;
    int gg=(ch8(under,vi.green)*inv+ch8(color,vi.green)*a+8)>>4;
    int bb=(ch8(under,vi.blue)*inv+ch8(color,vi.blue)*a+8)>>4;
    pset(x,y,px((u8)rr,(u8)gg,(u8)bb));
}
static int ui_text_width(const char *s,int font){
    int n=0;
    for(;*s;s++){
        unsigned char c=(unsigned char)*s;
        if(c>=32&&c<=126)n+=ui_fonts[font][c-32].adv;
    }
    return n;
}
static void ui_text(int x,int y,const char *s,int font,u32 color){
    for(;*s;s++){
        unsigned char c=(unsigned char)*s;
        if(c<32||c>126) continue;
        const struct fntglyph *g=&ui_fonts[font][c-32];
        int row,col;
        for(row=0;row<g->h;row++)for(col=0;col<g->w;col++){
            u32 idx=g->off+(u32)row*g->w+(u32)col;
            u8 packed=glyph_a4[idx>>1];
            int a=(idx&1)?(packed&15):(packed>>4);
            if(a) alpha_pixel(x+g->xo+col,y+g->yo+row,color,a);
        }
        x+=g->adv;
    }
}
static void ui_text_center(int x,int w,int y,const char*s,int font,u32 color){
    ui_text(x+(w-ui_text_width(s,font))/2,y,s,font,color);
}
static void slider_draw(int row,int value){
    const int x=193,y=115+row*66,w=94,th=8;
    int knob=0;
    u32 bg=px(80,83,91),hot=accent();
    round_fill(x,y,w,th,4,bg);
    if(row==3){
        int rel=(value+100)*w/200;
        disk(x+w/2,y+4,2,px(148,153,164));
        if(rel>=w/2)rect(x+w/2,y+1,rel-w/2,6,hot);
        else rect(x+rel,y+1,w/2-rel,6,px(82,168,204));
        knob=x+rel;
    }else{
        int fill=value*w/255;
        if(fill>2)round_fill(x,y,fill,th,3,hot);
        knob=x+fill;
    }
    if(knob>x+w)knob=x+w;
    disk(knob,y+4,9,px(85,82,85));
    disk(knob,y+4,7,px(245,246,247));
}
static void channel_card(int row,int selected,int value){
    const int y=95+row*66;
    const u32 active=accent(),fg=px(242,244,247),muted=px(197,201,211);
    u32 fill=selected?px(55,35,47):px(31,33,41);
    round_stroke(16,y,294,56,11,selected?active:px(63,65,75),fill,selected?2:1);
    /* Inner value box. */
    round_stroke(99,y+10,83,37,7,selected?px(157,103,119):px(91,96,107),px(23,24,31),1);
    if(row==0)ui_text(36,y+10,"R",2,px(255,40,40));
    if(row==1)ui_text(36,y+10,"G",2,px(30,255,30));
    if(row==2)ui_text(36,y+10,"B",2,px(40,88,255));
    if(row==3)ui_text(29,y+17,"Warmth",0,muted);
    char t[20];int pos=0,txtw;
    if(value<0){t[pos++]='-';value=-value;}
    pos=append_num(t,pos,value);t[pos]=0;
    txtw=ui_text_width(t,2);
    ui_text(99+(83-txtw)/2,y+10,t,2,fg);
    slider_draw(row,row==3?warmth:(row==0?r:(row==1?g:b)));
}
static void display_slider(int row,int value){
    int x=193,y=112+row*56,width=94,maxv=dmax[row];
    int fill=value*width/maxv;
    round_fill(x,y,width,8,4,px(80,83,91));
    if(fill>2)round_fill(x,y,fill,8,4,accent());
    disk(x+fill,y+4,9,px(85,82,85));disk(x+fill,y+4,7,px(245,246,247));
}
static void display_card(int row,int selected,int value){
    int y=92+row*56;
    u32 active=accent(),fg=px(242,244,247),muted=px(197,201,211);
    u32 fill=selected?px(55,35,47):px(31,33,41);
    round_stroke(16,y,294,50,10,selected?active:px(63,65,75),fill,selected?2:1);
    /* Value box starts at x=116: longest label "Lumination" (font 0, 79 px
     * from x=27) ends at 106, leaving a 10 px gap. Right edge stays at 181. */
    round_stroke(116,y+8,65,34,7,selected?px(157,103,119):px(91,96,107),px(23,24,31),1);
    ui_text(27,y+14,dtitle[row],0,muted);
    char t[10];int p=append_num(t,0,value);t[p]=0;
    int sw=ui_text_width(t,2);
    ui_text(116+(65-sw)/2,y+8,t,2,fg);
    display_slider(row,value);
}
static void monitor_icon(int x,int y){
    u32 framecol=px(246,246,249),edge=px(105,112,125);
    round_stroke(x,y,32,26,5,edge,px(16,17,23),1);
    rect(x+4,y+5,8,15,px(238,55,63));
    rect(x+12,y+5,8,15,px(36,220,91));
    rect(x+20,y+5,8,15,px(38,99,246));
    rect(x+13,y+26,6,4,framecol);
    rect(x+8,y+30,16,2,framecol);
}
/* Uniform rounded button legends, including dual keys. */
static int footer_key(int x,int y,const char *key){
    int kw=ui_text_width(key,0)+16;
    round_stroke(x,y,kw,22,6,accent(),px(41,32,41),1);
    ui_text_center(x,kw,y+2,key,0,px(252,240,244));
    return kw;
}
static int footer_item(int x,int y,const char *key,const char *label){
    int kw=footer_key(x,y,key);
    ui_text(x+kw+7,y+2,label,0,px(225,224,230));
    return kw+7+ui_text_width(label,0);
}
static int footer_pair(int x,int y,const char *first,const char *second,const char *label){
    int kw=footer_key(x,y,first);
    int kw2=footer_key(x+kw+4,y,second);
    ui_text(x+kw+kw2+12,y+2,label,0,px(225,224,230));
    return kw+kw2+12+ui_text_width(label,0);
}
static void draw(void){
    int filter_active=blf_on();
    const u32 fg=px(245,245,247),muted=px(172,177,189),line=px(71,75,87);
    int er,eg,eb;char output[48];int p=0;
    effective_rgb(&er,&eg,&eb);
    if(fullscreen){
        rect(0,0,W,H,px(0,0,0));
        png_draw_preview(0,0,(int)W,(int)H);
        present();return;
    }
    gradient_rect(0,0,(int)W,(int)H,13,16,25,19,19,26);
    gradient_rect(0,0,(int)W,61,48,25,36,27,23,34);
    rect(0,60,W,2,accent());
    monitor_icon(19,14);
    ui_text(67,12,"DisplayCal",2,fg);
    ui_text(213,21,"v0.8.4",0,muted);
    /* Status pill reflects the CURRENT page only:
     *   RGB page:     BLF ON / RGB ERR / LIVE (colortemp written) / READY
     *   Display page: SDK ERR / LIVE (PWM or LCD write succeeded) / READY
     */
    {
        int page_err=(page==0?status:dstatus)<0;
        int page_live=(page==0?applied:dapplied);
        int blf=(page==0 && filter_active);
        u32 pill=blf?px(126,86,42):(page_err?px(113,45,48):(page_live?px(43,87,66):px(88,77,47)));
        const char *label=blf?"BLF ON":
            (page==0 && status==-7?"TOO DARK":
             (page_err?(page==0?"RGB ERR":"SDK ERR"):(page_live?"LIVE":"READY")));
        round_fill((int)W-95,17,77,26,13,pill);
        ui_text_center((int)W-95,77,20,label,0,px(234,240,237));
    }
    ui_text(20,66,page==0?"CHANNEL BALANCE":"DISPLAY SETTINGS",0,muted);
    /* Visible page navigation hint. A is available because Compare was postponed. */
    ui_text(225,66,page==0?"A:DISPLAY":"A:RGB",0,px(235,95,127));
    ui_text(326,66,"REFERENCE",0,muted);
    if(page==0){
        for(int row=0;row<4;row++)channel_card(row,row==sel,row==0?r:(row==1?g:(row==2?b:warmth)));
    }else{
        for(int row=0;row<5;row++)display_card(row,row==sel,dvalue[row]);
    }
    rect(26,377,280,1,line);
    ui_text(23,381,page==0?(filter_active?"BLF ACTIVE - DISABLE TO TUNE":
                 (status==-7?"RGB TOO DARK - MIN TOTAL 70":
                  (status<0?(rgb_csc_missing?"DISP CSC MISSING":"RGB PROC WRITE FAILED"):"OUTPUT RGB"))):
          (dstatus==-10?"DISP UNAVAILABLE":(dstatus<0?"SDK FAILED - SAVE ONLY":"MAINUI VALUES")),0,muted);
    p=0; p=append_num(output,p,er);output[p++]=' ';output[p++]='/';output[p++]=' ';
    p=append_num(output,p,eg);output[p++]=' ';output[p++]='/';output[p++]=' ';
    p=append_num(output,p,eb);output[p]=0;
    if(page==0 && !filter_active) ui_text(149,378,output,1,fg);
    /* Preview card; test images preserve aspect ratio, topbar is native-size. */
    round_stroke(320,92,304,294,10,line,px(9,10,15),2);
    png_draw_preview(327,99,290,280);
    /* Current reference image name and count. */
    round_fill(446,64,178,25,6,px(36,35,44));
    ui_text_center(446,178,67,image_names[image_idx],0,px(239,222,225));
    gradient_rect(0,412,(int)W,68,26,26,35,17,18,27);
    rect(0,407,W,2,accent());
    /* Row 1: adjustment speeds and BOTH reset shortcuts on the same line. */
    footer_pair(12,420,"LEFT","RIGHT","+/-1");
    footer_pair(190,420,"L1","R1","+/-5");
    footer_item(344,420,"X","RESET ROW");
    footer_item(482,420,"Y","RESET PAGE");
    rect(14,446,W-28,1,px(60,58,70));
    /* Row 2: independent Select tap/hold hints plus save and cancel. */
    footer_item(12,452,"SELECT","NEXT");
    footer_item(171,452,"HOLD SELECT","FULL");
    footer_item(396,452,"B","CANCEL");
    footer_item(504,452,"START","SAVE");
    present();
}

static int is_up(u16 c){return c==103;}
static int is_down(u16 c){return c==108;}
static int is_left(u16 c){return c==105;}
static int is_right(u16 c){return c==106;}
static int is_l(u16 c){return c==18;}      /* KEY_E: HW_BTN_L1 */
static int is_r(u16 c){return c==20;}      /* KEY_T: HW_BTN_R1 */
static int is_l2(u16 c){return c==15;} /* KEY_TAB, Onion HW_BTN_L2 */
static int is_r2(u16 c){return c==14;} /* KEY_BACKSPACE, Onion HW_BTN_R2 */
static int is_a(u16 c){return c==57;} /* KEY_SPACE, Onion HW_BTN_A */
static int is_select(u16 c){return c==97;} /* KEY_RIGHTCTRL */
static int is_start(u16 c){return c==28;}  /* KEY_ENTER */
static int is_b(u16 c){return c==29 || c==1;} /* KEY_LEFTCTRL / MENU=ESC */
static int is_x(u16 c){return c==42;} /* KEY_LEFTSHIFT */
static int is_y(u16 c){return c==56;} /* KEY_LEFTALT */
static u32 clock_ms(void){
    u32 t[2];
    if(sc2(SYS_clock_gettime,1,(long)t)<0)return 0;
    return t[0]*1000u + udiv32(t[1],1000000u);
}
static int due(u32 now,u32 when){return (s32)(now-when)>=0;}

static void change(int delta){
    if(page==1){
        int nv=dvalue[sel]+delta;if(nv<0)nv=0;if(nv>dmax[sel])nv=dmax[sel];
        if(nv==dvalue[sel])return;
        dvalue[sel]=nv;dchanged=1;dapply_change(sel);draw();return;
    }
    if(blf_on()){status=-6;draw();return;}
    int *v= sel==0?&r:(sel==1?&g:(sel==2?&b:&warmth));
    int value=sel==3?clamp_w(*v+delta):clamp(*v+delta);
    if(value==*v)return;
    {
        int old=*v,er,eg,eb;
        *v=value;
        effective_rgb(&er,&eg,&eb);
        if(!rgb_total_ok(er,eg,eb)){
            *v=old;status=-7;draw();return;
        }
    }
    changed_by_gui=1;apply_current();draw();
}
static void reset_selected(void){
    if(page==1){
        if(dvalue[sel]==ddefault[sel])return;
        dvalue[sel]=ddefault[sel];dchanged=1;dapply_change(sel);draw();return;
    }
    if(blf_on()){status=-6;draw();return;}
    int *v=sel==0?&r:(sel==1?&g:(sel==2?&b:&warmth));
    int value=sel==3?0:128;
    if(*v==value)return;
    {
        int old=*v,er,eg,eb;
        *v=value;
        effective_rgb(&er,&eg,&eb);
        if(!rgb_total_ok(er,eg,eb)){
            *v=old;status=-7;draw();return;
        }
    }
    changed_by_gui=1;apply_current();draw();
}
static void reset_all(void){
    if(page==1){
        int i,any=0,bri=0,colors=0;
        for(i=0;i<5;i++)if(dvalue[i]!=ddefault[i]){
            dvalue[i]=ddefault[i];any=1;if(i==0)bri=1;else colors=1;
        }
        if(!any)return;
        dchanged=1;
        if(bri)dapply_change(0);
        if(colors)dapply_change(DSET_LUMA);
        draw();return;
    }
    if(blf_on()){status=-6;draw();return;}
    if(r==128&&g==128&&b==128&&warmth==0)return;
    r=g=b=128;warmth=0;changed_by_gui=1;apply_current();draw();
}
void _start(void){
    long ffd=xopen("/dev/fb0",O_RDWR,0); if(ffd<0){puts2("DisplayCal: cannot open /dev/fb0\n");xexit(2);}
    struct fb_fix_screeninfo fi;
    if(xioctl((int)ffd,FBIOGET_VSCREENINFO,&vi)<0 || xioctl((int)ffd,FBIOGET_FSCREENINFO,&fi)<0){puts2("DisplayCal: framebuffer ioctl failed\n");xexit(3);}
    W=vi.xres; H=vi.yres; BPP=vi.bits_per_pixel; STRIDE=fi.line_length;
    if(W<620||H<460||(BPP!=16&&BPP!=24&&BPP!=32)){
        puts2("DisplayCal: unsupported framebuffer size or pixel format\n");xexit(3);
    }
    /* Only draw to the currently displayed fb page (e.g. yoffset 480/960
     * on the 640x1440 triple-buffered Mini framebuffer). Map from offset 0
     * and retain fb_map for munmap, while canvas/previous need just one page.
     * Check bounds before multiplication/addition to prevent an OOB fb write.
     */
    if(STRIDE==0 || vi.yoffset>vi.yres_virtual || H>vi.yres_virtual-vi.yoffset ||
       STRIDE>0xffffffffu/vi.yres_virtual || STRIDE>0xffffffffu/H ||
       W*(BPP/8u)>STRIDE){
        puts2("DisplayCal: invalid framebuffer page/stride\n");xexit(3);
    }
    MAPLEN=STRIDE*vi.yres_virtual;
    BUFLEN=STRIDE*H;
    if(fi.smem_len<MAPLEN || vi.yoffset*STRIDE>MAPLEN-BUFLEN){
        puts2("DisplayCal: framebuffer backing memory too small\n");xexit(3);
    }
    fb_map=(volatile u8*)xmmap(0,MAPLEN,PROT_READ|PROT_WRITE,MAP_SHARED,(int)ffd,0);
    if((u32)(long)fb_map>=0xfffff001u){puts2("DisplayCal: framebuffer mmap failed\n");xexit(4);}
    fb=fb_map+vi.yoffset*STRIDE;
    canvas=(u8*)xmmap(0,BUFLEN,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    previous=(u8*)xmmap(0,BUFLEN,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if((u32)(long)canvas>=0xfffff001u || (u32)(long)previous>=0xfffff001u){
        puts2("DisplayCal: cannot allocate off-screen framebuffer\n");xexit(4);
    }

    int ev[6],nfd=0,i; char path[]="/dev/input/event0";
    for(i=0;i<6;i++){path[16]='0'+i; long fd=xopen(path,O_RDONLY|O_NONBLOCK,0); if(fd>=0) ev[nfd++]=(int)fd;}
    if(!nfd){puts2("DisplayCal: no /dev/input/event* found\n");xexit(5);}
    {
        char cp[192]; int rr=128,gg=128,bb=128,ww=0;
        if(cfg_path(cp,sizeof(cp))>0 && parse_cfg(cp,&rr,&gg,&bb,&ww)==0){
            int er,eg,eb;
            r=rr;g=gg;b=bb;warmth=ww;
            effective_rgb(&er,&eg,&eb);
            if(!rgb_total_ok(er,eg,eb)){
                /* Corrupt/unsafe saved profile: never apply on GUI launch.
                 * Start on neutral will delete this profile; Cancel won't
                 * try to restore the unsafe values. */
                r=g=b=128;warmth=0;
                invalid_saved_cfg=1;status=-7;
            }else{
                original_r=rr;original_g=gg;original_b=bb;original_w=ww;original_has_cfg=1;
                if(!blf_on() && !is_neutral_profile(r,g,b,warmth))apply_current();
            }
        }
    }
    dinit_model_defaults();
    dload_json();
    load_image(0);
    draw();
    puts2("DisplayCal v0.8.4 running (BLF has priority).\n");

    {
        u16 held=0;
        u32 repeat_at=0;
        int select_down=0,select_long=0;
        u32 select_at=0;
        for(;;){
            for(i=0;i<nfd;i++){
                struct input_event32 e; long n;
                while((n=xread(ev[i],&e,sizeof(e)))==(long)sizeof(e)){
                    if(e.type!=1) continue;
                    if(e.value==0){
                        if(e.code==held)held=0;
                        if(is_select(e.code)&&select_down){
                            select_down=0;
                            if(!select_long){load_image((image_idx+1)%NUM_IMAGES);draw();}
                        }
                        continue;
                    }
                    if(e.value!=1)continue; /* Own repeat timer, ignore kernel EV_KEY repeat. */
                    if(is_select(e.code)){select_down=1;select_at=clock_ms();select_long=0;continue;}
                    if(is_b(e.code)){
                        if(fullscreen){fullscreen=0;draw();continue;}
                        /* Never discard the active Blue Light Filter by writing
                         * neutral on Cancel. If unchanged, don't write at all.
                         * Otherwise, restore the saved profile, or the neutral
                         * baseline only when BLF is inactive and no profile exists.
                         */
                        if(!blf_on() && changed_by_gui){
                            if(original_has_cfg){r=original_r;g=original_g;b=original_b;warmth=original_w;apply_current();}
                            else status=apply_rgb(128,128,128);
                        }
                        if(dchanged)dcancel_hw();
                        goto done;
                    }
                    if(is_start(e.code)){
                        if(dchanged){
                            if(dsave_json()<0){dstatus=-16;draw();continue;}
                            /* system.json now holds these values: make them the
                             * new baseline so a later Cancel (after an RGB save
                             * failure) does not revert hardware away from disk. */
                            {int k;for(k=0;k<DSET_COUNT;k++)doriginal[k]=dvalue[k];}
                            dchanged=0;
                        }
                        /* Only touch the RGB profile when RGB/warmth changed;
                         * otherwise Start would create a profile that --apply
                         * then writes at every boot. */
                        if(!blf_on() && (changed_by_gui || invalid_saved_cfg ||
                             (original_has_cfg && is_neutral_profile(r,g,b,warmth)))){
                            int er,eg,eb;
                            effective_rgb(&er,&eg,&eb);
                            if(!rgb_total_ok(er,eg,eb)){status=-7;draw();continue;}
                            /* An already-neutral profile should just be removed;
                             * no unnecessary colortemp write on Save. */
                            if(changed_by_gui && apply_current()<0){draw();continue;}
                            if(save_cfg(r,g,b,warmth)<0){status=-3;draw();continue;}
                        }
                        draw();xsleep_ms(150);goto done;
                    }
                    if(fullscreen)continue;
                    if(is_l2(e.code)||is_r2(e.code)||is_a(e.code)){
                        int next=is_a(e.code)?1-page:(is_l2(e.code)?0:1);
                        if(next!=page){page=next;sel=0;draw();}
                        continue;
                    }
                    if(is_up(e.code)){sel--;if(sel<0)sel=page?4:3;draw();}
                    else if(is_down(e.code)){sel++;if(sel>(page?4:3))sel=0;draw();}
                    else if(is_x(e.code))reset_selected();
                    else if(is_y(e.code))reset_all();
                    else if(is_left(e.code)||is_right(e.code)||is_l(e.code)||is_r(e.code)){
                        int delta=(is_left(e.code)?-1:(is_right(e.code)?1:(is_l(e.code)?-5:5)));
                        held=e.code;repeat_at=clock_ms()+300u;
                        change(delta);
                    }
                }
            }
            xsleep_ms(12);
            {
                u32 now=clock_ms();
                if(select_down&&!select_long&&due(now,select_at+600u)){
                    select_long=1;fullscreen=!fullscreen;draw();
                }
                if(held&&due(now,repeat_at)){
                    int delta=is_left(held)?-1:(is_right(held)?1:(is_l(held)?-5:5));
                    repeat_at=now+75u;
                    if(!fullscreen)change(delta);
                }
            }
        }
    }
done:
    /* Leave a black screen, not the last UI frame: the launcher may now run
     * disp_csc_reset.sh --repair, which restarts DISP and would otherwise
     * flicker the stale UI until the app list redraws. Clear every fb page
     * (not only the visible one) so a later pan can't show an old frame. */
    {
        volatile u32 *w=(volatile u32*)fb_map;
        u32 k,words=MAPLEN/4u;
        for(k=0;k<words;k++)w[k]=0;
        for(k=words*4u;k<MAPLEN;k++)fb_map[k]=0;
    }
    if(dmi_init && d_sys_exit) d_sys_exit();
    for(i=0;i<nfd;i++)xclose(ev[i]);
    xmunmap(canvas,BUFLEN);xmunmap(previous,BUFLEN);
    xmunmap((void*)fb_map,MAPLEN);xclose((int)ffd);xexit(0);
}
/* Compiler-generated zero-init helper for the freestanding ARM build. */
void *memset(void *p,int c,unsigned long n){
    volatile u8 *v=(volatile u8*)p;
    unsigned long i;
    for(i=0;i<n;i++)v[i]=(u8)c;
    return p;
}
