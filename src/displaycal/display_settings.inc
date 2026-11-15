/* DisplayCal v0.8.1: MainUI-compatible display control page.
 * Intentionally never uses procfs csc, disp_init, or MI_DISP_InitDev.
 * Uses the documented 24-byte MI_DISP_LcdParam_t and MainUI conversion formulae.
 */
#define DSET_COUNT 5
#define DSET_BRIGHT 0
#define DSET_LUMA 1
#define DSET_HUE 2
#define DSET_SAT 3
#define DSET_CONTRAST 4
static const char *dkey[DSET_COUNT]={"brightness","lumination","hue","saturation","contrast"};
static const char *dtitle[DSET_COUNT]={"Brightness","Lumination","Hue","Saturation","Contrast"};
static const int ddefault[DSET_COUNT]={7,10,10,10,10};
static const int dmax[DSET_COUNT]={10,20,20,20,20};
static int dvalue[DSET_COUNT]={7,10,10,10,10};
static int doriginal[DSET_COUNT]={7,10,10,10,10};
static int dchanged=0,dstatus=0,dloaded=0,dlast_mi_rc=0;
/* 1 once a Display-page write (PWM or LCD) has succeeded this session. */
static int dapplied=0;
static int dcolor_ready=0,dmi_attempted=0,dmi_init=0;
static void *dlib_sys=0,*dlib_disp=0;
static int (*d_get_lcd)(int,void*)=0;
static int (*d_set_lcd)(int,void*)=0;
static int (*d_sys_exit)(void)=0;
/* Struct layout: 5 x uint32 CSC + uint32 sharpness (24 bytes, documented ABI).
 * The pad is spare room only: if a firmware's libmi_disp writes a larger
 * structure, it lands here instead of overwriting the caller's stack.
 * Never copy this struct by value (freestanding build has no memcpy).
 */
struct disp_lcd_param {u32 matrix,luma,contrast,hue,saturation,sharpness; u32 pad[8];};
static const char *djson_path="/mnt/SDCARD/system.json";
static const char *djson_tmp="/mnt/SDCARD/system.json.displaycal-tmp";
static char djson[65536],djnew[66000];
static int digit(char c){return c>='0'&&c<='9';}
static int dsep(char c){return c==0||c==' '||c=='\t'||c=='\r'||c=='\n';}
static int dmatch(const char *a,const char *b,int n){int i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
/* Find only top-level JSON number values. Ignore strings and nested keys.
 * Returns 1 for a plain integer value (span in *a..*b), 2 if the key exists
 * at top level but its value is not a plain integer (e.g. 7.0 or "7"),
 * 0 if absent, -1 if malformed.
 */
static int dfind_top(const char *s,int len,const char *key,int *a,int *b){
    int i,depth=0,ks=(int)slen(key),string=0,esc=0;
    for(i=0;i<len;i++){
        char c=s[i];
        if(string){if(esc){esc=0;continue;}if(c=='\\'){esc=1;continue;}if(c=='"')string=0;continue;}
        if(c=='"'){
            if(depth==1 && i+ks+2<len && dmatch(s+i+1,key,ks) && s[i+ks+1]=='"'){
                int j=i+ks+2;
                while(j<len && dsep(s[j]))j++;
                if(j<len && s[j]==':'){
                    j++;while(j<len && dsep(s[j]))j++;
                    int start=j;
                    if(j<len && s[j]=='-')j++;
                    while(j<len && digit(s[j]))j++;
                    if(j>start && j<len && (dsep(s[j])||s[j]==','||s[j]=='}')){*a=start;*b=j;return 1;}
                    return 2;
                }
            }
            string=1;continue;
        }
        if(c=='{')depth++;
        if(c=='}')depth--;
        if(depth<0)return -1;
    }
    return 0;
}
static int parse_int(const char *s,int a,int b){int n=0,i=a,neg=0;if(s[i]=='-'){neg=1;i++;}for(;i<b;i++)n=n*10+(s[i]-'0');return neg?-n:n;}
static int dload_json(void){
    int n=read_small(djson_path,djson,sizeof(djson));
    if(n<=0 || n==(int)sizeof(djson)-1) return -1;
    int i;
    for(i=0;i<DSET_COUNT;i++){
        int a=0,b=0,k=dfind_top(djson,n,dkey[i],&a,&b);
        if(k==1){int v=parse_int(djson,a,b);if(v>=0 && v<=dmax[i])dvalue[i]=v;}
        doriginal[i]=dvalue[i];
    }
    dloaded=1;return 0;
}
/* Kernel PWM sysfs on Mini uses percent values, as MainUI does. */
static int dset_brightness(int value){
    static const int pct[]={3,4,6,9,12,17,25,35,50,70,100};
    const char *path="/sys/devices/soc0/soc/1f003400.pwm/pwm/pwmchip0/pwm0/duty_cycle";
    long fd=xopen(path,O_WRONLY,0);char out[12];int n=0;long wrote;
    if(fd<0)fd=xopen("/sys/class/pwm/pwmchip0/pwm0/duty_cycle",O_WRONLY,0);
    if(fd<0)return -1;
    n=append_num(out,n,pct[value]);out[n++]='\n';
    wrote=xwrite((int)fd,out,(u32)n);xclose((int)fd);
    return wrote==n?0:-2;
}
/* Stream the whole proc file (like the launcher's grep), not just the first
 * 8 KB. Keep the last 8 bytes of each chunk so a match spanning two reads
 * is still found.
 */
static int dproc_csc_available(void){
    char info[4096];int keep=0,found=0,i;long fd,r;
    fd=xopen("/proc/mi_modules/mi_disp/mi_disp0",O_RDONLY,0);
    if(fd<0)return 0;
    for(;;){
        r=xread((int)fd,info+keep,(u32)(sizeof(info)-keep));
        if(r<=0)break;
        int n=keep+(int)r;
        for(i=0;i+9<=n;i++)if(dmatch(info+i,"CscMatrix",9)){found=1;break;}
        if(found)break;
        keep=n<8?n:8;
        for(i=0;i<keep;i++)info[i]=info[n-keep+i];
    }
    xclose((int)fd);
    return found;
}
static int dinit_mi(void){
    void *common,*pt;
    if(dmi_attempted)return dcolor_ready?0:-1;
    dmi_attempted=1;
    if(!dproc_csc_available()){dstatus=-10;return -1;}
    pt=dlopen("libpthread.so.0",2|0x100);
    if(!pt)pt=dlopen("/lib/libpthread.so.0",2|0x100);
    (void)pt;
    common=dlopen("/config/lib/libmi_common.so",2|0x100);
    if(!common)common=dlopen("libmi_common.so",2|0x100);
    if(!common){dstatus=-11;return -1;}
    dlib_sys=dlopen("/config/lib/libmi_sys.so",2|0x100);
    if(!dlib_sys)dlib_sys=dlopen("libmi_sys.so",2|0x100);
    if(!dlib_sys){dstatus=-11;return -1;}
    dlib_disp=dlopen("/config/lib/libmi_disp.so",2|0x100);
    if(!dlib_disp)dlib_disp=dlopen("libmi_disp.so",2|0x100);
    if(!dlib_disp){dstatus=-11;return -1;}
    d_get_lcd=(int(*)(int,void*))dlsym(dlib_disp,"MI_DISP_GetLcdParam");
    d_set_lcd=(int(*)(int,void*))dlsym(dlib_disp,"MI_DISP_SetLcdParam");
    int (*sys_init)(void)=(int(*)(void))dlsym(dlib_sys,"MI_SYS_Init");
    d_sys_exit=(int(*)(void))dlsym(dlib_sys,"MI_SYS_Exit");
    if(!d_get_lcd||!d_set_lcd||!sys_init){dstatus=-11;return -1;}
    int rc=sys_init();if(rc){dlast_mi_rc=rc;dstatus=-12;return -1;}
    dmi_init=1;
    {struct disp_lcd_param probe;
     dlast_mi_rc=d_get_lcd(0,&probe);}
    if(dlast_mi_rc){dstatus=-13;return -1;}
    dcolor_ready=1;dstatus=0;return 0;
}
static int dapply_lcd(void){
    if(dinit_mi()<0)return -1;
    if(!dproc_csc_available()){dstatus=-10;return -1;}
    struct disp_lcd_param p;
    dlast_mi_rc=d_get_lcd(0,&p);
    if(dlast_mi_rc){dstatus=-13;return -1;}
    p.luma=(u32)(dvalue[DSET_LUMA]+35);
    p.hue=(u32)(dvalue[DSET_HUE]*5);
    p.saturation=(u32)(dvalue[DSET_SAT]*5);
    p.contrast=(u32)((dvalue[DSET_CONTRAST]+15)*2);
    dlast_mi_rc=d_set_lcd(0,&p);
    if(dlast_mi_rc){dstatus=-14;return -1;}
    dapplied=1;dstatus=0;return 0;
}
static int dapply_change(int idx){
    if(idx==DSET_BRIGHT){int rc=dset_brightness(dvalue[0]);dstatus=rc?-15:0;if(!rc)dapplied=1;return rc;}
    return dapply_lcd();
}
/* Atomically patch five top-level integer fields, preserving all other JSON.
 * Fail closed for malformed/oversized documents instead of corrupting them.
 */
static int dsave_json(void){
    int n=read_small(djson_path,djson,sizeof(djson)),i,pos=0,seen[5]={0},depth=0,quote=0,esc=0,close=-1;
    if(n<=1 || n==(int)sizeof(djson)-1)return -1;
    for(i=0;i<n;i++){
        char c=djson[i];
        if(quote){if(esc)esc=0;else if(c=='\\')esc=1;else if(c=='"')quote=0;continue;}
        if(c=='"'){quote=1;continue;}
        if(c=='{')depth++;
        if(c=='}'){depth--;if(depth==0)close=i;if(depth<0)return -2;}
    }
    if(quote||depth!=0||close<0)return -2;
    /* Sorted replacement spans; all keys are expected in RedOnion system.json. */
    int a[5],b[5];
    for(i=0;i<5;i++){
        int k=dfind_top(djson,n,dkey[i],&a[i],&b[i]);
        if(k==1)seen[i]=1;
        /* Key present with a non-integer value: refuse rather than append a
         * duplicate key that most JSON readers would ignore. */
        else if(k!=0)return -2;
    }
    for(i=0;i<n;){
        int j,matched=-1;
        for(j=0;j<5;j++)if(seen[j]&&a[j]==i){matched=j;break;}
        if(matched>=0){
            if(pos>=(int)sizeof(djnew)-20)return -3;
            pos=append_num(djnew,pos,dvalue[matched]);i=b[matched];
        }else if(i==close){
            int h=i-1,m;
            while(h>=0 && dsep(djson[h])) h--;
            int has=(h>=0 && djson[h]!='{');
            for(m=0;m<5;m++)if(!seen[m]){
                const char *name=dkey[m];int z;
                if(pos>=(int)sizeof(djnew)-80)return -3;
                if(has)djnew[pos++]=',';
                djnew[pos++]='"';for(z=0;name[z];z++)djnew[pos++]=name[z];
                djnew[pos++]='"';djnew[pos++]=':';
                pos=append_num(djnew,pos,dvalue[m]);has=1;
            }
            djnew[pos++]=djson[i++];
        }else{
            if(pos>=(int)sizeof(djnew)-80)return -3;
            djnew[pos++]=djson[i++];
        }
    }
    long fd=xopen(djson_tmp,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0)return -4;
    long wr=xwrite((int)fd,djnew,(u32)pos);
    if(wr==pos)sc1(118,fd);
    xclose((int)fd);
    if(wr!=pos){sc1(10,(long)djson_tmp);return -5;} /* unlink leftover */
    if(sc2(38,(long)djson_tmp,(long)djson_path)<0){sc1(10,(long)djson_tmp);return -6;}
    /* keymon's public refresh marker: forces a reload on next key event.
     * Full immediate shared-memory synchronization needs MainUI's ABI. */
    fd=xopen("/tmp/settings_changed",O_WRONLY|O_CREAT,0644);
    if(fd>=0)xclose((int)fd);
    return 0;
}
static void dcancel_hw(void){
    int i,needs_csc=0,needs_pwm=0;
    for(i=0;i<DSET_COUNT;i++){
        if(dvalue[i]!=doriginal[i]){
            if(i==DSET_BRIGHT)needs_pwm=1;else needs_csc=1;
            dvalue[i]=doriginal[i];
        }
    }
    if(needs_pwm)dset_brightness(dvalue[DSET_BRIGHT]);
    if(needs_csc && dcolor_ready)dapply_lcd();
}
