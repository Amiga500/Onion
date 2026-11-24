// Audio file decoding: MP3, FLAC, WAV, Ogg Vorbis, local Ogg Opus and MP3 radio.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "stb_vorbis.c"
#undef L
#undef C
#undef R

#include "decode.h"
#include "opus_ogg.h"
#include "radio.h"
#include "ffaudio.h"

enum { F_MP3, F_FLAC, F_WAV, F_OGG, F_OPUS, F_RADIO, F_FFMPEG };

struct decoder {
    int fmt;
    int src_ch;          // channels in the file
    int out_ch;          // 1 or 2
    char *path;
    drmp3 mp3;
    drmp3_seek_point *seek_points;
    drflac *flac;
    drwav wav;
    stb_vorbis *ogg;
    opus_local *opus;
    radio_stream *radio;
    ffaudio *ff;
    int radio_kind; /* 0 MP3, 1 Vorbis, 2 Opus */
    unsigned char *push;
    int push_n, push_cap;
    float *push_pcm;
    int push_frames, push_pos;
    int vorbis_src_ch, vorbis_sample_rate;
    float *tmp;          // for downmixing >2 channels
    int tmp_frames;
};

static const char *ext_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return "";
    return dot + 1;
}

static int format_of(const char *path)
{
    if (radio_is_url(path)) {
        /* Explicit AAC and HLS URLs bypass the MP3/Ogg curl path entirely. */
        const char *end=strpbrk(path,"?#");
        size_t n=end ? (size_t)(end-path):strlen(path);
        if ((n>=5 && !strncasecmp(path+n-5,".m3u8",5)) ||
            (n>=4 && !strncasecmp(path+n-4,".aac",4)) ||
            (n>=4 && !strncasecmp(path+n-4,".m4a",4))) return F_FFMPEG;
        return F_RADIO;
    }
    const char *e = ext_of(path);
    if (!strcasecmp(e, "mp3")) return F_MP3;
    if (!strcasecmp(e, "flac")) return F_FLAC;
    if (!strcasecmp(e, "wav")) return F_WAV;
    if (!strcasecmp(e, "ogg") || !strcasecmp(e, "oga")) return F_OGG;
    if (!strcasecmp(e, "opus")) return F_OPUS;
    if (!strcasecmp(e, "aac") || !strcasecmp(e, "m4a") || !strcasecmp(e,"alac")) return F_FFMPEG;
    return -1;
}

int dec_supported(const char *path) { return format_of(path) >= 0; }

static char decoder_err[192];
void dec_error_message(char *out,int cap) {
    if(out && cap>0) snprintf(out,(size_t)cap,"%s",decoder_err);
}
int dec_ffmpeg_source(const char *path) { return path && format_of(path)==F_FFMPEG; }

static int prefix_has_hls(const unsigned char *p,size_t n)
{
    const char h[]="#EXT-X-";
    for(size_t i=0;i+sizeof h-1<=n;i++)
        if(!memcmp(p+i,h,sizeof h-1)) return 1;
    return 0;
}
static size_t radio_read_cb(void *userdata,void *dst,size_t n)
{
    return radio_read((radio_stream *)userdata,dst,n);
}
/* Bounded push-data buffer for live Ogg Vorbis. */
static int push_more(decoder *d)
{
    if (d->push_cap-d->push_n < 8192) {
        int cap=d->push_cap ? d->push_cap*2 : 32768;
        if (cap>524288 || cap<=d->push_cap) return 0;
        unsigned char *p=realloc(d->push,(size_t)cap);
        if (!p) return 0;
        d->push=p;d->push_cap=cap;
    }
    size_t n=radio_read(d->radio,d->push+d->push_n,8192);
    d->push_n+=(int)n;
    return n>0;
}
static void push_take(decoder *d,int n)
{
    if (n<0 || n>d->push_n) n=d->push_n;
    d->push_n-=n;
    memmove(d->push,d->push+n,(size_t)d->push_n);
}
static void vorbis_radio_title(decoder *d)
{
    if (!d->radio) return;
    stb_vorbis_comment c = stb_vorbis_get_comment(d->ogg);
    char title[160] = "", artist[160] = "";
    for (int i=0;i<c.comment_list_length && i<4096;i++) {
        const char *s=c.comment_list[i];
        if (!s) continue;
        if (!strncasecmp(s,"TITLE=",6)) snprintf(title,sizeof title,"%s",s+6);
        else if (!strncasecmp(s,"ARTIST=",7)) snprintf(artist,sizeof artist,"%s",s+7);
    }
    if (title[0]) {
        char full[320];
        if (artist[0]) snprintf(full,sizeof full,"%s - %s",artist,title);
        else snprintf(full,sizeof full,"%s",title);
        radio_set_title(d->radio,full);
    }
}
static int open_vorbis_radio(decoder *d,dec_info *info)
{
    int err=0,used=0;
    if (!d->push_n && !push_more(d)) return 0;
    for (;;) {
        d->ogg=stb_vorbis_open_pushdata(d->push,d->push_n,&used,&err,NULL);
        if (d->ogg) break;
        if (err!=VORBIS_need_more_data || !push_more(d)) return 0;
    }
    push_take(d,used);
    stb_vorbis_info vi=stb_vorbis_get_info(d->ogg);
    if (vi.channels<1 || vi.channels>2) return 0;
    if (d->vorbis_sample_rate && vi.sample_rate!=d->vorbis_sample_rate) {
        fprintf(stderr,"[radio] chained Vorbis sample-rate change unsupported\n");
        return 0;
    }
    d->vorbis_src_ch=vi.channels;
    d->vorbis_sample_rate=vi.sample_rate;
    if (!d->src_ch) d->src_ch=vi.channels;
    info->samplerate=vi.sample_rate;
    if (!d->push_pcm) d->push_pcm=malloc(sizeof(float)*8192*2);
    d->push_frames=d->push_pos=0;
    if (d->push_pcm) vorbis_radio_title(d);
    return d->push_pcm!=NULL;
}
static int read_vorbis_radio(decoder *d,float *out,int frames)
{
    int done=0;
    while (done<frames) {
        if (d->push_pos<d->push_frames) {
            int n=d->push_frames-d->push_pos;
            if (n>frames-done) n=frames-done;
            for (int i=0;i<n;i++) {
                const float *p=d->push_pcm+(size_t)(d->push_pos+i)*d->vorbis_src_ch;
                float *o=out+(size_t)(done+i)*d->src_ch;
                if (d->src_ch==d->vorbis_src_ch) {
                    for (int ch=0;ch<d->src_ch;ch++) o[ch]=p[ch];
                } else if (d->src_ch==1) o[0]=(p[0]+p[1])*0.5f;
                else o[0]=o[1]=p[0];
            }
            d->push_pos+=n;done+=n;
            continue;
        }
        /* The new Ogg BOS header starts a new logical stream (track or
         * reconnect). Open fresh setup/comment headers at that boundary. */
        if (d->push_n>=27 && !memcmp(d->push,"OggS",4) && (d->push[5]&2)) {
            stb_vorbis_close(d->ogg);d->ogg=NULL;
            dec_info next={0};
            if (!open_vorbis_radio(d,&next)) break;
            continue;
        }
        int ch=0,samples=0;
        float **planar=NULL;
        int used=stb_vorbis_decode_frame_pushdata(d->ogg,d->push,d->push_n,&ch,&planar,&samples);
        if (used<0 || used>d->push_n) break;
        if (samples>8192 || (samples && ch!=d->vorbis_src_ch)) break;
        d->push_frames=samples;d->push_pos=0;
        for (int i=0;i<samples;i++)
            for (int c=0;c<ch;c++) d->push_pcm[(size_t)i*ch+c]=planar[c][i];
        push_take(d,used);
        if (!samples && !used && !push_more(d)) break;
    }
    return done;
}


decoder *dec_open(const char *path, dec_info *info)
{
    decoder_err[0]=0;
    radio_take_network_error(); /* discard errors belonging to the previous source */
    int fmt = format_of(path);
    if (fmt < 0) return NULL;
    decoder *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->fmt = fmt;
    memset(info, 0, sizeof *info);
    int ok = 0;
    switch (fmt) {
    case F_MP3:
        if (drmp3_init_file(&d->mp3, path, NULL)) {
            ok = 1;
            d->src_ch = d->mp3.channels;
            info->samplerate = d->mp3.sampleRate;
            if (d->mp3.totalPCMFrameCount != DRMP3_UINT64_MAX)
                info->frames = (int64_t)d->mp3.totalPCMFrameCount - d->mp3.delayInPCMFrames - d->mp3.paddingInPCMFrames;
        }
        break;
    case F_FLAC:
        d->flac = drflac_open_file(path, NULL);
        if (d->flac) {
            ok = 1;
            d->src_ch = d->flac->channels;
            info->samplerate = d->flac->sampleRate;
            info->frames = (int64_t)d->flac->totalPCMFrameCount;
        }
        break;
    case F_WAV:
        if (drwav_init_file(&d->wav, path, NULL)) {
            ok = 1;
            d->src_ch = d->wav.channels;
            info->samplerate = d->wav.sampleRate;
            info->frames = (int64_t)d->wav.totalPCMFrameCount;
            info->kbps = (int)((int64_t)d->wav.sampleRate * d->wav.channels * d->wav.bitsPerSample / 1000);
        }
        break;
    case F_FFMPEG: {
        ffaudio_info f={0};
        char err[192]={0};
        d->ff=ffaudio_open(path,radio_is_url(path),&f,err,sizeof err);
        if(d->ff) {
            info->samplerate=f.samplerate;info->channels=2;
            info->frames=f.frames;info->kbps=f.kbps;
            d->src_ch=2;ok=1;
        } else if(err[0]) {
            snprintf(decoder_err,sizeof decoder_err,"%s",err);
            fprintf(stderr,"[ffaudio] %s\n",err);
        }
        break;
    }
    case F_RADIO: {
        d->radio=radio_open(path);
        if (!d->radio) {snprintf(decoder_err,sizeof decoder_err,"Could not initialize radio transport");break;}
        unsigned char prefix[64];
        if (!radio_probe(d->radio,prefix,sizeof prefix)) {snprintf(decoder_err,sizeof decoder_err,"Stream ended before audio arrived");break;}
        fprintf(stderr,"[radio] probe prefix %02x %02x %02x %02x\n",prefix[0],prefix[1],prefix[2],prefix[3]);
        /* Sniff AAC ADTS and HLS even when a station URL has no extension.
         * The existing curl reader is closed before FFmpeg opens the URL. */
        if (radio_is_ffmpeg_stream(d->radio) ||
            (prefix[0]==0xff && (prefix[1]&0xf6)==0xf0) ||
            (!memcmp(prefix,"#EXTM3U",7) &&
             (prefix_has_hls(prefix,sizeof prefix)))) {
            fprintf(stderr,"[radio] switching to AAC/HLS FFmpeg decoder\n");
            radio_close(d->radio);d->radio=NULL;
            ffaudio_info f={0};char err[192]={0};
            d->ff=ffaudio_open(path,1,&f,err,sizeof err);
            if(d->ff) {
                d->fmt=F_FFMPEG;
                info->samplerate=f.samplerate;info->frames=0;
                info->kbps=f.kbps;d->src_ch=2;ok=1;
            } else if(err[0]) {
            snprintf(decoder_err,sizeof decoder_err,"%s",err);
            fprintf(stderr,"[ffaudio] %s\n",err);
        }
            break;
        }
        if (!memcmp(prefix,"OggS",4)) {
            int is_opus=0;
            for (int i=0;i+8<=(int)sizeof prefix;i++)
                if (!memcmp(prefix+i,"OpusHead",8)) { is_opus=1;break; }
            if (is_opus) {
                d->radio_kind=2;
                d->opus=opus_local_open_radio(d->radio,&d->src_ch);
                if (!d->opus) break;
                info->samplerate=48000;ok=1;
            } else {
                d->radio_kind=1;
                if (open_vorbis_radio(d,info)) ok=1;
            }
        } else {
            d->radio_kind=0;
            if (drmp3_init(&d->mp3,radio_read_cb,NULL,NULL,NULL,d->radio,NULL)) {
                d->src_ch=d->mp3.channels;
                info->samplerate=d->mp3.sampleRate;
                ok=1;
            }
        }
        if(!ok){snprintf(decoder_err,sizeof decoder_err,"Could not initialize stream decoder");
            fprintf(stderr,"[radio] %s (codec=%s)\n",decoder_err,d->radio_kind==2?"Opus":d->radio_kind==1?"Vorbis":"MP3");}
        else fprintf(stderr,"[radio] decoder ready: %s %d Hz\n",d->radio_kind==2?"Opus":d->radio_kind==1?"Vorbis":"MP3",info->samplerate);
        info->frames=0;
        break;
    }
    case F_OPUS:
        d->opus=opus_local_open(path,&d->src_ch,&info->frames);
        if (d->opus) { ok=1; info->samplerate=48000; }
        break;
    case F_OGG: {
        int err = 0;
        d->ogg = stb_vorbis_open_filename(path, &err, NULL);
        if (d->ogg) {
            stb_vorbis_info vi = stb_vorbis_get_info(d->ogg);
            ok = 1;
            d->src_ch = vi.channels;
            info->samplerate = vi.sample_rate;
            info->frames = stb_vorbis_stream_length_in_samples(d->ogg);
        }
        break;
    }
    }
    if (!ok || d->src_ch < 1 || info->samplerate < 8000) {
        dec_close(d);
        return NULL;
    }
    d->path = strdup(path);
    d->out_ch = d->src_ch >= 2 ? 2 : 1;
    info->channels = d->out_ch;
    return d;
}

static int read_raw(decoder *d, float *out, int frames)
{
    switch (d->fmt) {
    case F_MP3: return (int)drmp3_read_pcm_frames_f32(&d->mp3, frames, out);
    case F_FLAC: return (int)drflac_read_pcm_frames_f32(d->flac, frames, out);
    case F_WAV: return (int)drwav_read_pcm_frames_f32(&d->wav, frames, out);
    case F_OGG: return stb_vorbis_get_samples_float_interleaved(d->ogg, d->src_ch, out, frames * d->src_ch);
    case F_OPUS: return opus_local_read(d->opus,out,frames);
    case F_FFMPEG: return ffaudio_read(d->ff,out,frames);
    case F_RADIO:
        if (d->radio_kind==1) return read_vorbis_radio(d,out,frames);
        if (d->radio_kind==2) return opus_local_read(d->opus,out,frames);
        return (int)drmp3_read_pcm_frames_f32(&d->mp3,frames,out);
    }
    return 0;
}

int dec_read(decoder *d, float *out, int frames)
{
    if (d->src_ch <= 2) return read_raw(d, out, frames);
    // downmix surround to stereo: odd channels left, even channels right
    if (d->tmp_frames < frames) {
        free(d->tmp);
        d->tmp = malloc(sizeof(float) * frames * d->src_ch);
        d->tmp_frames = d->tmp ? frames : 0;
        if (!d->tmp) return 0;
    }
    int n = read_raw(d, d->tmp, frames);
    int c = d->src_ch;
    for (int i = 0; i < n; i++) {
        float l = 0, r = 0;
        for (int k = 0; k < c; k++) { if (k & 1) r += d->tmp[i * c + k]; else l += d->tmp[i * c + k]; }
        out[i * 2] = l / ((c + 1) / 2);
        out[i * 2 + 1] = r / (c / 2);
    }
    return n;
}

// MP3 seeking without a table decodes everything up to the target, so the
// first seek scans the file's frame headers (no decoding) to build one.
static void mp3_build_seek_table(decoder *d)
{
    drmp3 scan;
    if (!drmp3_init_file(&scan, d->path, NULL)) return;
    drmp3_uint32 n = 1024;
    drmp3_seek_point *pts = malloc(sizeof *pts * n);
    if (pts && drmp3_calculate_seek_points(&scan, &n, pts) && n > 0) {
        d->seek_points = pts;
        drmp3_bind_seek_table(&d->mp3, n, pts);
    } else {
        free(pts);
    }
    drmp3_uninit(&scan);
}

int64_t dec_seek(decoder *d, int64_t frame)
{
    if (frame < 0) frame = 0;
    switch (d->fmt) {
    case F_MP3:
        if (!d->seek_points) mp3_build_seek_table(d);
        if (!drmp3_seek_to_pcm_frame(&d->mp3, (drmp3_uint64)frame)) return -1;
        return (int64_t)d->mp3.currentPCMFrame;
    case F_FLAC:
        return drflac_seek_to_pcm_frame(d->flac, (drflac_uint64)frame) ? frame : -1;
    case F_WAV:
        return drwav_seek_to_pcm_frame(&d->wav, (drwav_uint64)frame) ? frame : -1;
    case F_OGG:
        return stb_vorbis_seek(d->ogg, (unsigned)frame) ? frame : -1;
    case F_OPUS: return opus_local_seek(d->opus,frame);
    case F_RADIO: return -1;
    case F_FFMPEG: return ffaudio_seek(d->ff,frame);
    }
    return -1;
}

void dec_close(decoder *d)
{
    if (!d) return;
    switch (d->fmt) {
    case F_MP3: if (d->mp3.onRead) drmp3_uninit(&d->mp3); break;
    case F_FLAC: if (d->flac) drflac_close(d->flac); break;
    case F_WAV: if (d->wav.onRead) drwav_uninit(&d->wav); break;
    case F_OGG: if (d->ogg) stb_vorbis_close(d->ogg); break;
    case F_OPUS: opus_local_close(d->opus); break;
    case F_FFMPEG: ffaudio_close(d->ff); break;
    case F_RADIO:
        if (d->radio_kind==0 && d->mp3.onRead) drmp3_uninit(&d->mp3);
        if (d->radio_kind==1 && d->ogg) stb_vorbis_close(d->ogg);
        if (d->radio_kind==2) opus_local_close(d->opus);
        radio_close(d->radio);
        break;
    }
    free(d->seek_points);
    free(d->tmp);
    free(d->push);
    free(d->push_pcm);
    free(d->path);
    free(d);
}

void dec_cancel_pending(void) { radio_abort_active(); ffaudio_cancel_active(); }
void dec_radio_title(decoder *d,char *out,int cap)
{
    if (cap<=0) return;
    if (!d) { *out=0;return; }
    if (d->fmt==F_FFMPEG) { ffaudio_title(d->ff,out,(size_t)cap);return; }
    if (d->fmt==F_RADIO) { radio_title(d->radio,out,(size_t)cap);return; }
    *out=0;
}
