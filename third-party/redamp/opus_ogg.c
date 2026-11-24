/* Local Ogg Opus: bounded Ogg packet reader and libopus decoding.
 * No dependency on libopusfile: load the system/bundled libopus.so.0 at run time.
 * Supports mapping-family-0 mono/stereo files, OpusTags and sample-exact duration.
 * Seek currently decodes from the beginning for correctness (slow on long files).
 * Opus codec is BSD licensed; see docs/BUILDING.md for bundling dependencies.
 */
#include "opus_ogg.h"
#include "radio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <limits.h>
#include <dlfcn.h>

#define MAX_OPUS_PACKET (256 * 1024)
#define OGG_PAGE_DATA (255 * 255)
#define OPUS_OUT 5760

typedef void OpusDecoder;
typedef OpusDecoder *(*create_fn)(int,int,int*);
typedef void (*destroy_fn)(OpusDecoder*);
typedef int (*decode_fn)(OpusDecoder*,const unsigned char*,int,float*,int,int);
typedef struct {
    FILE *f;
    radio_stream *radio;
    unsigned serial;
    int serial_set, eof, page_segments, segment, offset, page_len;
    unsigned char laces[255], page[OGG_PAGE_DATA], packet[MAX_OPUS_PACKET];
    size_t packet_len;
    int incomplete;
} ogg_reader;

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t le64(const unsigned char *p) {
    return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}
static size_t read_bytes(ogg_reader *r,void *out,size_t n) {
    return r->radio ? radio_read(r->radio,out,n) : fread(out,1,n,r->f);
}
static int page_next(ogg_reader *r) {
    unsigned char h[27];
    for (;;) {
        if (read_bytes(r,h,27) != 27 || memcmp(h,"OggS",4) || h[4] != 0) return 0;
        int n=h[26];
        if (read_bytes(r,r->laces,(size_t)n)!=(size_t)n) return 0;
        size_t total=0;
        for (int i=0;i<n;i++) total+=r->laces[i];
        if (total > sizeof(r->page) || read_bytes(r,r->page,total)!=total) return 0;
        unsigned serial=le32(h+14);
        if (!r->serial_set) { r->serial=serial; r->serial_set=1; }
        if (serial != r->serial) {
            if (!r->radio || !(h[5] & 2)) continue;
            /* A radio station started another Ogg logical bitstream. */
            r->serial=serial;
            r->packet_len=0;
            r->incomplete=0;
        }
        if ((h[5] & 1) && r->packet_len == 0) {
            /* Seeked to a continued page: discard the first partial packet. */
            r->incomplete=1;
        }
        r->page_segments=n; r->segment=0; r->offset=0; r->page_len=(int)total;
        return 1;
    }
}
static int packet_next(ogg_reader *r, unsigned char **data, size_t *size) {
    for (;;) {
        if (r->segment >= r->page_segments) {
            if (!page_next(r)) return 0;
        }
        while (r->segment < r->page_segments) {
            int n=r->laces[r->segment++];
            if (r->offset+n > r->page_len && r->page_len) return 0;
            if (r->incomplete) { r->offset+=n; if (n < 255) r->incomplete=0; continue; }
            if (r->packet_len + (size_t)n > sizeof(r->packet)) return 0;
            memcpy(r->packet+r->packet_len,r->page+r->offset,(size_t)n);
            r->packet_len+=(size_t)n; r->offset+=n;
            if (n < 255) {
                *data=r->packet; *size=r->packet_len; r->packet_len=0;
                return 1;
            }
        }
    }
}
/* Decode the OpusHead, refusing unsupported multistream mappings. */
static int head_read(ogg_reader *r, int *channels, int *pre_skip) {
    unsigned char *data; size_t len;
    if (!packet_next(r,&data,&len) || len < 19 || memcmp(data,"OpusHead",8)) return 0;
    if (data[8] != 1 || (data[9] != 1 && data[9] != 2) || data[18] != 0) return 0;
    *channels=data[9]; *pre_skip=data[10] | (data[11]<<8);
    return 1;
}
static void tag_copy(char *dst,size_t cap,const unsigned char *str,size_t n) {
    if (n >= cap) n=cap-1;
    memcpy(dst,str,n); dst[n]=0;
}
static void tags_parse(const unsigned char *p,size_t size,track_meta *m) {
    if (size < 16 || memcmp(p,"OpusTags",8)) return;
    size_t pos=8;
    uint32_t vendor=le32(p+pos); pos+=4;
    if (vendor > size-pos || size-pos-vendor < 4) return;
    pos+=vendor;
    uint32_t count=le32(p+pos); pos+=4;
    if (count>65536) return;
    for (uint32_t i=0;i<count;i++) {
        if (size-pos < 4) return;
        uint32_t n=le32(p+pos); pos+=4;
        if (n > size-pos) return;
        const unsigned char *eq=memchr(p+pos,'=',n);
        if (eq && m) {
            size_t k=(size_t)(eq-(p+pos));
            const unsigned char *v=eq+1; size_t vl=n-k-1;
            if (k==5 && !strncasecmp((char *)p+pos,"TITLE",5)) tag_copy(m->title,sizeof m->title,v,vl);
            else if (k==6 && !strncasecmp((char *)p+pos,"ARTIST",6)) tag_copy(m->artist,sizeof m->artist,v,vl);
            else if (k==5 && !strncasecmp((char *)p+pos,"ALBUM",5)) tag_copy(m->album,sizeof m->album,v,vl);
            else if (k==11 && !strncasecmp((char *)p+pos,"ALBUMARTIST",11)) tag_copy(m->album_artist,sizeof m->album_artist,v,vl);
            else if (k==12 && !strncasecmp((char *)p+pos,"ALBUM ARTIST",12)) tag_copy(m->album_artist,sizeof m->album_artist,v,vl);
            else if (k==11 && !strncasecmp((char *)p+pos,"TRACKNUMBER",11)) {
                char t[16]={0}; if(vl>=sizeof t)vl=sizeof t-1;
                memcpy(t,v,vl);m->track_no=atoi(t);
            }
        }
        pos+=n;
    }
}
static int64_t last_granule(FILE *f, unsigned serial) {
    /* Ogg page headers + payload max about 65KB; inspect final 256KB. */
    if (fseek(f,0,SEEK_END)) return -1;
    long end=ftell(f);
    if (end <= 0) return -1;
    long start=end > 262144 ? end-262144 : 0;
    if (fseek(f,start,SEEK_SET)) return -1;
    size_t len=(size_t)(end-start);
    unsigned char *buf=malloc(len);
    if (!buf) return -1;
    if (fread(buf,1,len,f)!=len) { free(buf); return -1; }
    int64_t out=-1;
    for (size_t i=0;i+27<=len;i++) {
        if (memcmp(buf+i,"OggS",4) || buf[i+4]!=0 || le32(buf+i+14)!=serial) continue;
        size_t segments=buf[i+26];
        if (i+27+segments > len) continue;
        size_t total=0;
        for (size_t k=0;k<segments;k++) total+=buf[i+27+k];
        if (i+27+segments+total>len) continue;
        uint64_t g=le64(buf+i+6);
        if (g <= INT64_MAX) out=(int64_t)g;
    }
    free(buf);
    return out;
}
struct opus_local {
    ogg_reader ogg;
    void *library;
    OpusDecoder *decoder;
    destroy_fn destroy;
    decode_fn decode;
    int ch, output_ch, pre_skip, skip;
    int64_t length, pos;
    float pcm[OPUS_OUT*2];
    int pcm_len, pcm_pos;
};
static int decoder_setup(opus_local *o) {
    if (o->decoder && o->destroy) { o->destroy(o->decoder); o->decoder=NULL; }
    if (!o->library) o->library=dlopen("libopus.so.0",RTLD_NOW|RTLD_LOCAL);
    if (!o->library) o->library=dlopen("libopus.so",RTLD_NOW|RTLD_LOCAL);
    if (!o->library) return 0;
    create_fn create=(create_fn)dlsym(o->library,"opus_decoder_create");
    o->destroy=(destroy_fn)dlsym(o->library,"opus_decoder_destroy");
    o->decode=(decode_fn)dlsym(o->library,"opus_decode_float");
    if (!create || !o->destroy || !o->decode) return 0;
    int err=-1; o->decoder=create(48000,o->ch,&err);
    return o->decoder && err==0;
}
static int stream_init(opus_local *o,const char *path) {
    o->ogg.f=fopen(path,"rb");
    if (!o->ogg.f) return 0;
    if (!head_read(&o->ogg,&o->ch,&o->pre_skip)) return 0;
    unsigned char *pkt; size_t len;
    if (!packet_next(&o->ogg,&pkt,&len)) return 0;
    o->length=last_granule(o->ogg.f,o->ogg.serial);
    if (o->length>=0) o->length=o->length>o->pre_skip ? o->length-o->pre_skip:0;
    FILE *file=o->ogg.f;
    if (fseek(file,0,SEEK_SET)) return 0;
    memset(&o->ogg,0,sizeof o->ogg);
    o->ogg.f=file;
    if (!head_read(&o->ogg,&o->ch,&o->pre_skip) || !packet_next(&o->ogg,&pkt,&len)) return 0;
    o->skip=o->pre_skip;
    return decoder_setup(o);
}
opus_local *opus_local_open_radio(radio_stream *r,int *channels) {
    if (!r) return NULL;
    opus_local *o=calloc(1,sizeof *o);
    if (!o) return NULL;
    o->ogg.radio=r;
    unsigned char *packet; size_t n;
    o->length=-1;
    if (!head_read(&o->ogg,&o->ch,&o->pre_skip) ||
        !packet_next(&o->ogg,&packet,&n) || !decoder_setup(o)) {
        opus_local_close(o); return NULL;
    }
    o->skip=o->pre_skip;
    o->output_ch=o->ch;
    *channels=o->ch;
    return o;
}

opus_local *opus_local_open(const char *path,int *channels,int64_t *frames) {
    opus_local *o=calloc(1,sizeof *o);
    if (!o) return NULL;
    if (!stream_init(o,path)) { opus_local_close(o); return NULL; }
    o->output_ch=o->ch;
    *channels=o->ch; *frames=o->length<0 ? 0:o->length;
    return o;
}
int opus_local_read(opus_local *o,float *out,int frames) {
    if (!o || frames<=0) return 0;
    int made=0;
    while (made<frames) {
        if (o->length>=0 && o->pos>=o->length) break;
        if (o->pcm_pos>=o->pcm_len) {
            unsigned char *pkt; size_t size;
            if (!packet_next(&o->ogg,&pkt,&size)) break;
            if (size==0) continue;
            if (o->ogg.radio && size>=19 && !memcmp(pkt,"OpusHead",8)) {
                int ch=0,pre=0;
                if (pkt[8]!=1 || (pkt[9]!=1 && pkt[9]!=2) || pkt[18]!=0) break;
                ch=pkt[9];pre=pkt[10]|(pkt[11]<<8);
                o->ch=ch;o->pre_skip=pre;o->skip=pre;
                o->pcm_pos=o->pcm_len=0;
                if (!decoder_setup(o)) break;
                continue;
            }
            if (o->ogg.radio && size>=8 && !memcmp(pkt,"OpusTags",8)) {
                track_meta meta={0}; tags_parse(pkt,size,&meta);
                char title[320];
                if (meta.title[0]) {
                    if (meta.artist[0]) snprintf(title,sizeof title,"%s - %s",meta.artist,meta.title);
                    else snprintf(title,sizeof title,"%s",meta.title);
                    radio_set_title(o->ogg.radio,title);
                }
                continue;
            }
            int n=o->decode(o->decoder,pkt,(int)size,o->pcm,OPUS_OUT,0);
            if (n<=0) break;
            o->pcm_pos=0; o->pcm_len=n;
            if (o->skip>0) {
                int skip=o->skip<n?o->skip:n;
                o->pcm_pos+=skip; o->skip-=skip;
            }
            continue;
        }
        int n=o->pcm_len-o->pcm_pos;
        if (n>frames-made) n=frames-made;
        if (o->length>=0 && n>o->length-o->pos) n=(int)(o->length-o->pos);
        if (n<=0) break;
        /* Keep the initial channel count exposed to the audio engine.
         * A station can switch mono/stereo at a chained stream boundary. */
        if (o->ch==o->output_ch) {
            memcpy(out+(size_t)made*o->output_ch,o->pcm+(size_t)o->pcm_pos*o->ch,
                   (size_t)n*o->ch*sizeof(float));
        } else for (int i=0;i<n;i++) {
            const float *src=o->pcm+(size_t)(o->pcm_pos+i)*o->ch;
            float *dst=out+(size_t)(made+i)*o->output_ch;
            if (o->output_ch==1) dst[0]=(src[0]+(o->ch==2?src[1]:src[0]))*0.5f;
            else dst[0]=dst[1]=src[0];
        }
        o->pcm_pos+=n; o->pos+=n; made+=n;
    }
    return made;
}
int64_t opus_local_seek(opus_local *o,int64_t frame) {
    /* A correct but linear seek. No page-granule/index assumptions. */
    if (!o || o->ogg.radio || frame<0 || (o->length>=0 && frame>o->length)) return -1;
    if (frame < o->pos) {
        if (fseek(o->ogg.f,0,SEEK_SET)) return -1;
        FILE *file=o->ogg.f;
        memset(&o->ogg,0,sizeof o->ogg); o->ogg.f=file;
        unsigned char *pkt; size_t len; int ch,pre;
        if (!head_read(&o->ogg,&ch,&pre) || !packet_next(&o->ogg,&pkt,&len)) return -1;
        int err=-1;
        /* The decoder reset ioctl is CTL 4028; avoid depending on opus headers. */
        typedef int (*ctl_fn)(OpusDecoder *,int,...);
        ctl_fn ctl=(ctl_fn)dlsym(o->library,"opus_decoder_ctl");
        if (!ctl || ctl(o->decoder,4028)!=0) return -1;
        o->skip=pre; o->pos=0; o->pcm_len=o->pcm_pos=0;
        (void)err;
    }
    float buffer[2048*2];
    while (o->pos<frame) {
        int n=(frame-o->pos)>2048 ? 2048 : (int)(frame-o->pos);
        if (opus_local_read(o,buffer,n)!=n) return -1;
    }
    return o->pos;
}
void opus_local_close(opus_local *o) {
    if (!o) return;
    if (o->decoder && o->destroy) o->destroy(o->decoder);
    if (o->library) dlclose(o->library);
    if (o->ogg.f) fclose(o->ogg.f);
    free(o);
}
int opus_local_metadata(const char *path,track_meta *m) {
    ogg_reader r={0}; r.f=fopen(path,"rb");
    if (!r.f) return 0;
    int ch,pre,ok=head_read(&r,&ch,&pre);
    if (ok) {
        unsigned char *pkt;size_t n;
        if (packet_next(&r,&pkt,&n)) tags_parse(pkt,n,m);
        m->channels=ch; m->samplerate=48000;
        int64_t total=last_granule(r.f,r.serial);
        if (total > pre) {
            int64_t duration=(total-pre)*1000/48000;
            m->dur_ms=duration>INT_MAX?INT_MAX:(int)duration;
        }
        if (m->dur_ms>0) {
            long end=ftell(r.f);
            if (fseek(r.f,0,SEEK_END)==0) {
                long bytes=ftell(r.f);
                if (bytes>0) m->kbps=(int)((int64_t)bytes*8/m->dur_ms);
            }
            (void)end;
        }
    }
    fclose(r.f);
    return ok;
}
