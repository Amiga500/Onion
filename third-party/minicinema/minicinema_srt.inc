/* MiniCinema external SRT parser and video subtitle renderer.
 * Copyright (C) 2026 MiniCinema contributors, GPL-3.0-or-later.
 * Text cues use SDL_ttf and the FFplay audio clock. No extra subtitle codec
 * dependency, private to the player; no changes to the video file.
 */
#define MC_SRT_MAX_CUES 5000
#define MC_SRT_MAX_LINES 3
#define MC_SRT_LINE_CAP 512
#define MC_SRT_RENDER_LINES 6
typedef struct McSrtCue {
    long start_ms, end_ms;
    unsigned char count;
    char line[MC_SRT_MAX_LINES][MC_SRT_LINE_CAP];
} McSrtCue;
static McSrtCue *mc_cues;
static unsigned mc_cue_count, mc_cue_capacity;
static int mc_srt_announced;
static int mc_srt_delay_ms = 0;
static int mc_srt_enabled = 1;
static int mc_srt_source_embedded = 0; /* Reset for every new player process. */
/* Allocate only as many subtitles as the file needs: large 5000-cue arrays
 * should not consume Miyoo memory before an SRT is actually opened. */
static int mc_srt_append(const McSrtCue *cue) {
    if (!cue->count) return 0;
    if (mc_cue_count >= MC_SRT_MAX_CUES) return -1;
    if (mc_cue_count == mc_cue_capacity) {
        unsigned next = mc_cue_capacity ? mc_cue_capacity * 2 : 64;
        if (next > MC_SRT_MAX_CUES) next = MC_SRT_MAX_CUES;
        McSrtCue *grown = av_realloc_array(mc_cues, next, sizeof(*mc_cues));
        if (!grown) return -1;
        mc_cues = grown;
        mc_cue_capacity = next;
    }
    mc_cues[mc_cue_count++] = *cue;
    return 0;
}
static int mc_sub_last=-2;
static SDL_Surface *mc_sub_rendered[MC_SRT_RENDER_LINES];

static int mc_srt_clock(const char *s, long *out, const char **tail) {
    char *end;
    long hours,minutes,seconds,ms;
    while(*s==' '||*s=='\t')s++;
    if(*s<'0'||*s>'9')return -1;
    hours=strtol(s,&end,10);
    if(*end!=':')return -1;
    s = end + 1;
    minutes = strtol(s, &end, 10);
    if(end == s || *end != ':') return -1;
    s = end + 1;
    seconds = strtol(s, &end, 10);
    if(end == s || (*end != ',' && *end != '.')) return -1;
    s = end + 1;
    ms = strtol(s, &end, 10);
    if(end == s || end - s > 3 || (*end && *end != ' ' &&
                                 *end != '\r' && *end != '\n' && *end != '\t'))
        return -1;
    if(end - s == 1) ms *= 100;
    else if(end - s == 2) ms *= 10;
    if(hours>99||minutes>59||seconds>59||ms>999||hours<0||minutes<0||seconds<0||ms<0)
        return -1;
    *out=3600000L*hours+60000L*minutes+1000L*seconds+ms;
    if(tail)*tail=end;
    return 0;
}
static int mc_srt_times(const char *line,long *start,long *end) {
    const char *marker=strstr(line,"-->");
    if(!marker || mc_srt_clock(line,start,NULL)<0)return -1;
    if(mc_srt_clock(marker+3,end,NULL)<0 || *end<=*start)return -1;
    return 0;
}
/* Validate the whole SRT once. If it isn't UTF-8, treat it as the
 * Windows-1252 encoding commonly used by older European subtitle files. */
static int mc_srt_is_utf8(FILE *fp)
{
    unsigned needed=0,code=0,minimum=0;
    int input,valid=1;
    rewind(fp);
    while((input=fgetc(fp))!=EOF){
        unsigned char c=(unsigned char)input;
        if(!needed){
            if(c<0x80)continue;
            if(c>=0xc2 && c<=0xdf){needed=1;code=c&31;minimum=0x80;}
            else if(c>=0xe0 && c<=0xef){needed=2;code=c&15;minimum=0x800;}
            else if(c>=0xf0 && c<=0xf4){needed=3;code=c&7;minimum=0x10000;}
            else{valid=0;break;}
        }else{
            if((c&0xc0)!=0x80){valid=0;break;}
            code=(code<<6)|(c&63);
            if(--needed==0 && (code<minimum || code>0x10ffff ||
                (code>=0xd800 && code<=0xdfff))){valid=0;break;}
        }
    }
    if(needed)valid=0;
    rewind(fp);
    return valid;
}
static unsigned mc_cp1252_unicode(unsigned c)
{
    static const unsigned short special[32]={
        0x20ac,0x0081,0x201a,0x0192,0x201e,0x2026,0x2020,0x2021,
        0x02c6,0x2030,0x0160,0x2039,0x0152,0x008d,0x017d,0x008f,
        0x0090,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
        0x02dc,0x2122,0x0161,0x203a,0x0153,0x009d,0x017e,0x0178};
    return c>=0x80&&c<=0x9f ? special[c-0x80] : c;
}
static void mc_cp1252_convert(char *dst,size_t cap,const char *src)
{
    size_t n=0;
    while(*src && n+4<cap){
        unsigned c=mc_cp1252_unicode((unsigned char)*src++);
        if(c<0x80)dst[n++]=(char)c;
        else if(c<0x800){dst[n++]=(char)(0xc0|(c>>6));dst[n++]=(char)(0x80|(c&63));}
        else {dst[n++]=(char)(0xe0|(c>>12));
              dst[n++]=(char)(0x80|((c>>6)&63));dst[n++]=(char)(0x80|(c&63));}
    }
    dst[n]=0;
}
static unsigned mc_utf8_char_bytes(const char *s)
{
    unsigned c=(unsigned char)*s;
    unsigned count=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
    for(unsigned i=1;i<count;i++)if(!s[i] || ((unsigned char)s[i]&0xc0)!=0x80)
        return 1;
    return count;
}
static void mc_srt_clean_line(char *dst,const char *src)
{
    unsigned i=0;
    int tag=0;
    while(*src && i<MC_SRT_LINE_CAP-1){
        unsigned char c=(unsigned char)*src;
        unsigned count=mc_utf8_char_bytes(src);
        if(c=='<'){tag=1;src++;continue;}
        if(c=='>'){if(tag)tag=0;src++;continue;}
        if(tag){src+=count;continue;}
        if(c=='{' && src[1]=='\\'){
            while(*src && *src!='}')src++;
            if(*src=='}')src++;
            continue;
        }
        if(c=='\r' || c=='\n')break;
        if(c<32 && c!='\t'){src++;continue;}
        if(i+count>=MC_SRT_LINE_CAP)break; /* Never split a UTF-8 codepoint. */
        for(unsigned j=0;j<count;j++)dst[i++]=*src++;
    }
    while(i && (dst[i-1]==' ' || dst[i-1]=='\t'))i--;
    dst[i]=0;
}
static int mc_srt_is_index_line(const char *s)
{
    unsigned count=0;
    while(*s==' ' || *s=='\t')s++;
    while(*s>='0' && *s<='9'){count++;s++;}
    while(*s==' ' || *s=='\t' || *s=='\r' || *s=='\n')s++;
    return count>0 && !*s;
}
static void mc_srt_load(const char *path) {
    FILE *fp;
    char line[2048];
    char converted[4096];
    int utf8;
    McSrtCue cue;
    int active=0, pending_number=0;
    char pending_line[MC_SRT_LINE_CAP];
    mc_cue_count=0;
    mc_srt_announced = 0;
    mc_srt_enabled = 1;
    mc_srt_source_embedded = 0;
    if (!path || !path[0]) return;
    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "MiniCinema SRT: cannot open '%s': %s\n",
                path, strerror(errno));
        return;
    }
    utf8=mc_srt_is_utf8(fp);
    if(!utf8)fprintf(stderr,"MiniCinema SRT: converting Windows-1252 to UTF-8\n");
    memset(&cue,0,sizeof(cue));
    while(fgets(line,sizeof(line),fp)){
        long st,en;
        char *p=line;
        if(!utf8){mc_cp1252_convert(converted,sizeof(converted),line);p=converted;}
        if((unsigned char)p[0]==0xef&&(unsigned char)p[1]==0xbb&&
           (unsigned char)p[2]==0xbf)p+=3;
        if(mc_srt_times(p,&st,&en)==0){
            /* A pending numeric line is a cue number only when the next
             * line is a timestamp. Otherwise it is genuine dialogue. */
            pending_number=0;
            if (active && mc_srt_append(&cue) < 0) break;
            memset(&cue,0,sizeof(cue));
            cue.start_ms=st;cue.end_ms=en;active=1;
            continue;
        }
        if(active){
            char cleaned[MC_SRT_LINE_CAP];
            mc_srt_clean_line(cleaned,p);
            if(pending_number){
                if(cue.count<MC_SRT_MAX_LINES)
                    av_strlcpy(cue.line[cue.count++],pending_line,MC_SRT_LINE_CAP);
                pending_number=0;
            }
            if(!cleaned[0]){
                if (mc_srt_append(&cue) < 0) break;
                memset(&cue,0,sizeof(cue));active=0;
            }else if(cue.count && mc_srt_is_index_line(cleaned)){
                /* Hold a possible index until the following line. Digit-only
                 * dialogue such as "2001" must not be discarded. */
                av_strlcpy(pending_line,cleaned,sizeof(pending_line));
                pending_number=1;
            }else if(cue.count<MC_SRT_MAX_LINES){
                av_strlcpy(cue.line[cue.count++],cleaned,MC_SRT_LINE_CAP);
            }
        }
        if(mc_cue_count>=MC_SRT_MAX_CUES)break;
    }
    if (active && mc_cue_count < MC_SRT_MAX_CUES){
        if(pending_number && cue.count<MC_SRT_MAX_LINES)
            av_strlcpy(cue.line[cue.count++],pending_line,MC_SRT_LINE_CAP);
        mc_srt_append(&cue);
    }
    if (ferror(fp))
        fprintf(stderr, "MiniCinema SRT: read error for '%s'\n", path);
    fclose(fp);
    fprintf(stderr, "MiniCinema SRT: parsed %u cue(s) from '%s'%s\n",
            mc_cue_count, path,
            mc_cue_count >= MC_SRT_MAX_CUES ? " (cue limit reached)" : "");
}
static int mc_srt_active(VideoState *is){
    double clock=get_master_clock(is);
    long ms;
    if(!mc_srt_enabled || !mc_cue_count || mc_srt_source_embedded)return -1;
    if(isnan(clock))clock=(double)kp.position;
    else if(get_master_sync_type(is)==AV_SYNC_AUDIO_MASTER)
        clock -= mc_audio_delay_ms/1000.0;
    if(clock<0)return -1;
    ms=(long)(clock*1000) - mc_srt_delay_ms;
    for(unsigned i=0;i<mc_cue_count;i++){
        if(ms>=mc_cues[i].start_ms && ms<mc_cues[i].end_ms)return (int)i;
    }
    return -1;
}
static void mc_dim_rect(SDL_Surface *s,int x,int y,int w,int h,int keep){
    /* ARGB8888 texture surface; keep% of original video under this panel. */
    if(!s||s->format->BytesPerPixel!=4)return;
    int right=FFMIN(x+w,s->w),bottom=FFMIN(y+h,s->h);
    if(x<0)x=0;if(y<0)y=0;
    for(int yy=y;yy<bottom;yy++){
        uint32_t *p=(uint32_t*)((uint8_t*)s->pixels+yy*s->pitch)+x;
        for(int xx=x;xx<right;xx++,p++){
            uint32_t src=*p;
            uint32_t r=((src>>16)&255)*(unsigned)keep/100;
            uint32_t g=((src>>8)&255)*(unsigned)keep/100;
            uint32_t b=(src&255)*(unsigned)keep/100;
            *p=(src&0xff000000)|(r<<16)|(g<<8)|b;
        }
    }
}
static McSrtCue mc_embedded_cue;
/* FFmpeg delivers MKV/MP4 text subtitle rects as ASS events.  We support
 * plain text and hard line breaks, not advanced ASS positioning or styling. */
static void mc_embedded_parse(McSrtCue *cue, const AVSubtitle *sub)
{
    memset(cue, 0, sizeof(*cue));
    if (!sub) return;
    for (unsigned i = 0; i < sub->num_rects && cue->count < MC_SRT_MAX_LINES; ++i) {
        const AVSubtitleRect *rect = sub->rects[i];
        if (!rect) continue;
        const char *text = rect->text;
        if (!text && rect->ass) {
            text = rect->ass;
            int commas = 8;
            if (!strncmp(text, "Dialogue:", 9)) commas = 9;
            while (*text && commas) {
                if (*text++ == ',') commas--;
            }
            if (commas) continue;
        }
        if (!text) continue;
        int in_tag = 0;
        while (*text && cue->count < MC_SRT_MAX_LINES) {
            char *dest = cue->line[cue->count];
            unsigned used = 0;
            while (*text && used < MC_SRT_LINE_CAP - 1) {
                char c = *text++;
                if (c == '{') {in_tag=1;continue;}
                if (c == '}' && in_tag) {in_tag=0;continue;}
                if (in_tag) continue;
                if (c == '\\' && (*text == 'N' || *text == 'n')) {
                    text++;
                    break;
                }
                if (c == '\\' && *text == 'h') {text++;c=' ';}
                if (c == '\r') continue;
                if (c == '\n') break;
                if ((unsigned char)c < 32 && c != '\t') continue;
                if ((unsigned char)c >= 128) {
                    /* Restore the initial byte consumed above, then append
                     * a complete UTF-8 codepoint or none at all. */
                    --text;
                    unsigned bytes=mc_utf8_char_bytes(text);
                    if (used+bytes>=MC_SRT_LINE_CAP) break;
                    for (unsigned k=0;k<bytes;k++)dest[used++]=*text++;
                } else {
                    dest[used++]=c;
                }
            }
            while (used && dest[used-1] == ' ') used--;
            dest[used] = 0;
            if (used) cue->count++;
            if (!*text) break;
        }
    }
}
static int mc_embedded_active(VideoState *is)
{
    if (!mc_srt_enabled || (mc_cue_count && !mc_srt_source_embedded) || !is->subtitle_st ||
        frame_queue_nb_remaining(&is->subpq) <= 0) return 0;
    Frame *sp = frame_queue_peek(&is->subpq);
    if (sp->serial != is->subtitleq.serial || sp->sub.format != 1) return 0;
    double now = get_master_clock(is);
    if (isnan(now)) return 0;
    if (get_master_sync_type(is) == AV_SYNC_AUDIO_MASTER)
        now -= mc_audio_delay_ms / 1000.0;
    now -= mc_srt_delay_ms / 1000.0;
    return now >= sp->pts + sp->sub.start_display_time / 1000.0 &&
           now < sp->pts + sp->sub.end_display_time / 1000.0;
}
typedef struct McSubtitleLayout {
    unsigned count;
    char line[MC_SRT_RENDER_LINES][MC_SRT_LINE_CAP];
} McSubtitleLayout;
static McSubtitleLayout mc_layout;

static void mc_srt_invalidate(void){
    mc_sub_last=-2;
    for(int i=0;i<MC_SRT_RENDER_LINES;i++){
        if(mc_sub_rendered[i])SDL_FreeSurface(mc_sub_rendered[i]);
        mc_sub_rendered[i]=NULL;
    }
}
/* Soft-wrap text using glyph width, not bytes. Never slice a multi-byte
 * character. Limit to six visible lines so subtitles stay above the OSD. */
static void mc_subtitle_wrap(const McSrtCue *cue)
{
    memset(&mc_layout,0,sizeof(mc_layout));
    if(!cue)return;
    for(unsigned row=0;row<cue->count && mc_layout.count<MC_SRT_RENDER_LINES;row++){
        const char *p=cue->line[row];
        while(*p && mc_layout.count<MC_SRT_RENDER_LINES){
            char *dst=mc_layout.line[mc_layout.count];
            unsigned used=0,break_at=0;
            const char *after_break=NULL;
            while(*p && used+4<MC_SRT_LINE_CAP){
                unsigned count=mc_utf8_char_bytes(p);
                unsigned previous=used;
                if(used+count>=MC_SRT_LINE_CAP)break;
                for(unsigned i=0;i<count;i++)dst[used++]=p[i];
                dst[used]=0;
                int width=0,height=0;
                if(kp.srt_font && TTF_SizeUTF8(kp.srt_font,dst,&width,&height)<0)
                    width=0;
                if(width>KP_W-62 && previous){
                    if(after_break){used=break_at;p=after_break;}
                    else used=previous;
                    dst[used]=0;
                    break;
                }
                p+=count;
                if(dst[used-1]==' ' || dst[used-1]=='\t'){
                    break_at=used-1;after_break=p;
                }
            }
            while(used && (dst[used-1]==' ' || dst[used-1]=='\t'))used--;
            dst[used]=0;
            if(used)mc_layout.count++;
            while(*p==' ' || *p=='\t')p++;
            if(!used && *p){
                /* Skip an unrenderable byte to avoid a non-advancing loop. */
                p+=mc_utf8_char_bytes(p);
            }
        }
    }
}
static void mc_srt_draw(SDL_Surface *surface, VideoState *is){
    int active=mc_srt_active(is);
    const McSrtCue *cue=NULL;
    int changed=active!=mc_sub_last;
    if(active>=0){
        cue=&mc_cues[active];
    }else if(mc_embedded_active(is)){
        McSrtCue fresh;
        Frame *sp=frame_queue_peek(&is->subpq);
        mc_embedded_parse(&fresh,&sp->sub);
        if(fresh.count){
            if(mc_sub_last!=-3 || fresh.count!=mc_embedded_cue.count ||
               memcmp(fresh.line,mc_embedded_cue.line,sizeof(fresh.line))){
                mc_embedded_cue=fresh;
                changed=1;
            }
            active=-3;
            cue=&mc_embedded_cue;
        }
    }
    if(active!=mc_sub_last)changed=1;
    if(changed){
        mc_srt_invalidate();
        mc_sub_last=active;
        mc_subtitle_wrap(cue);
        if(cue && kp.srt_font){
            SDL_Color white={246,247,250,255};
            for(unsigned i=0;i<mc_layout.count;i++)
                mc_sub_rendered[i]=TTF_RenderUTF8_Blended(kp.srt_font,
                                           mc_layout.line[i],white);
        }
    }
    if(!cue || !mc_layout.count)return;
    if(!mc_srt_announced){
        mc_srt_announced=1;
        fprintf(stderr,"MiniCinema subtitles: %s active, font=%s\n",
                active==-3?"embedded text":"external SRT",
                kp.srt_font?"ready":"MISSING");
    }
    int osd=(is->paused||SDL_GetTicks()<kp.progress_until||
             SDL_GetTicks()<kp.seek_notice_until);
    int n=(int)mc_layout.count;
    int maxline=0,step=kp.subtitle_size==0?26:kp.subtitle_size==2?35:30;
    for(int i=0;i<n;i++)if(mc_sub_rendered[i] && mc_sub_rendered[i]->w>maxline)
        maxline=mc_sub_rendered[i]->w;
    if(maxline>KP_W-50)maxline=KP_W-50;
    int bx=(KP_W-maxline)/2-12;
    int by=(osd?366:452)-n*step-12-kp.subtitle_position*18;
    mc_dim_rect(surface,bx,by,maxline+24,n*step+17,36);
    for(int i=0;i<n;i++){
        SDL_Surface *line=mc_sub_rendered[i];
        if(!line)continue;
        SDL_Rect dst={(KP_W-line->w)/2,by+8+i*step,0,0};
        SDL_BlitSurface(line,NULL,surface,&dst);
    }
}
static void mc_srt_shutdown(void){
    for(int i=0;i<MC_SRT_RENDER_LINES;i++){
        if(mc_sub_rendered[i])SDL_FreeSurface(mc_sub_rendered[i]);
        mc_sub_rendered[i]=NULL;
    }
    mc_sub_last=-2;
    av_freep(&mc_cues);
    mc_cue_count=mc_cue_capacity=0;
    mc_srt_announced=0;
}
