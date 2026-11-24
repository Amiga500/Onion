/* RedAmp FFmpeg 4.4.8 bridge. Compile this file ONLY with the exact FFmpeg
 * 4.4.8 headers and link against the private *_redamp.so libraries.
 * Main RedAmp never references an AV* struct or links to libav* directly. */
#define _POSIX_C_SOURCE 200809L
#include "ffbridge.h"
#include "ffpath.h"
#include "adts_duration.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>

#define REDAMP_RATE 48000
#define MAX_PENDING 65536
#define MAX_URL 1800

typedef struct {
    AVFormatContext *fmt;
    AVCodecContext *codec;
    AVPacket *packet;
    AVFrame *frame;
    SwrContext *swr;
    int stream, live, eof, flushing;
    int input_rate, input_channels, input_format;
    int64_t input_layout;
    float *pending;
    int cap, count, pos;
    char title[256];
    const atomic_int *cancel;
    int64_t deadline;
} bridge;

static void set_err(char *p,size_t n,const char *s) {
    if(p && n) snprintf(p,n,"%s",s);
}
static int interrupt_cb(void *ctx) {
    bridge *b=ctx;
    return (b->cancel && atomic_load_explicit(b->cancel,memory_order_acquire)) ||
           (b->deadline && av_gettime_relative()>b->deadline);
}
static int ca_store(char *dest,size_t cap) {
    const char *env=getenv("REDAMP_CA_FILE");
    const char *home=getenv("MA_HOME");
    if(env && env[0]=='/' && access(env,R_OK)==0) { snprintf(dest,cap,"%s",env);return 1; }
    if(home && snprintf(dest,cap,"%s/cacert.pem",home)<(int)cap && access(dest,R_OK)==0)
        return 1;
    const char *fallback[] = {"/etc/ssl/certs/ca-certificates.crt", "/etc/ssl/cert.pem"};
    for(unsigned i=0;i<sizeof(fallback)/sizeof(fallback[0]);i++)
        if(access(fallback[i],R_OK)==0) {snprintf(dest,cap,"%s",fallback[i]);return 1;}
    return 0;
}
static void refresh_title(bridge *b) {
    if(!b || !b->fmt) return;
    char title[160]={0},artist[100]={0};
    const AVDictionary *d=b->fmt->metadata;
    if(b->stream>=0 && b->stream<(int)b->fmt->nb_streams) {
        AVStream *st=b->fmt->streams[b->stream];
        AVDictionaryEntry *t=av_dict_get(st->metadata,"title",NULL,0);
        AVDictionaryEntry *a=av_dict_get(st->metadata,"artist",NULL,0);
        if(t) snprintf(title,sizeof title,"%s",t->value);
        if(a) snprintf(artist,sizeof artist,"%s",a->value);
    }
    if(!title[0]) {
        AVDictionaryEntry *t=av_dict_get(d,"title",NULL,0);
        if(t) snprintf(title,sizeof title,"%s",t->value);
    }
    if(!artist[0]) {
        AVDictionaryEntry *a=av_dict_get(d,"artist",NULL,0);
        if(a) snprintf(artist,sizeof artist,"%s",a->value);
    }
    /* HLS timed metadata and ICY packet are exposed by some protocol backends
     * through AVFormatContext metadata or the nested AVIO options. */
    if(b->fmt->pb) {
        uint8_t *icy=NULL;
        if(av_opt_get(b->fmt->pb,"icy_metadata_packet",AV_OPT_SEARCH_CHILDREN,&icy)>=0 && icy) {
            const char *key="StreamTitle='";
            char *p=strstr((char *)icy,key);
            if(p) {
                p+=strlen(key);
                char *end=strstr(p,"';");
                if(end) snprintf(title,sizeof title,"%.*s",(int)(end-p),p);
            }
            av_free(icy);
        }
    }
    if(title[0]) {
        if(artist[0]) snprintf(b->title,sizeof b->title,"%s - %s",artist,title);
        else snprintf(b->title,sizeof b->title,"%s",title);
    }
}
int ffbridge_ready(void) {
    return (avformat_version()==LIBAVFORMAT_VERSION_INT) &&
           (avcodec_version()==LIBAVCODEC_VERSION_INT) &&
           (avutil_version()==LIBAVUTIL_VERSION_INT) &&
           (swresample_version()==LIBSWRESAMPLE_VERSION_INT);
}
static void bridge_free(bridge *b) {
    if(!b) return;
    if(b->swr) swr_free(&b->swr);
    if(b->frame) av_frame_free(&b->frame);
    if(b->packet) av_packet_free(&b->packet);
    if(b->codec) avcodec_free_context(&b->codec);
    if(b->fmt) avformat_close_input(&b->fmt);
    free(b->pending);
    free(b);
}
void ffbridge_close(void *ctx) {bridge_free(ctx);}
static bridge *bridge_new(const char *path,int live,const atomic_int *cancel,
                          ffaudio_info *info,char *err,size_t errlen,int insecure) {
    if(!ffbridge_ready()) { set_err(err,errlen,"FFmpeg 4.4 ABI mismatch");return NULL; }
    if(!ffpath_allowed(path,live)) {set_err(err,errlen,"Unsupported FFmpeg input path");return NULL;}
    bridge *b=calloc(1,sizeof *b);
    if(!b) return NULL;
    b->stream=-1;b->live=live;b->cancel=cancel;
    b->fmt=avformat_alloc_context();
    if(!b->fmt) goto fail;
    b->fmt->interrupt_callback.callback=interrupt_cb;
    b->fmt->interrupt_callback.opaque=b;
    b->deadline=av_gettime_relative()+10000000;
    AVDictionary *options=NULL;
    /* Raw ADTS can need more initial analysis than an MP4 container. */
    av_dict_set(&options,"probesize",live ? "131072" : "1048576",0);
    av_dict_set(&options,"analyzeduration",live ? "2000000" : "8000000",0);
    av_dict_set(&options,"format_whitelist",live?"hls,mpegts,aac,mov,mp3,ogg":"mov,aac",0);
    if(live) {
        av_dict_set(&options,"protocol_whitelist","http,https,tcp,tls,crypto",0);
        char user_agent[96];
        snprintf(user_agent,sizeof user_agent,"RedAmp/%s (RedOnion)",
                 getenv("MA_VERSION") ? getenv("MA_VERSION") : "1.1.10");
        av_dict_set(&options,"user_agent",user_agent,0);
        av_dict_set(&options,"icy","1",0);
        av_dict_set(&options,"tls_verify",insecure?"0":"1",0);
        av_dict_set(&options,"rw_timeout","12000000",0);
        av_dict_set(&options,"reconnect","0",0); /* RedAmp owns bounded retries */
        /* Carry the explicit policy to nested HLS requests. Secure mode
         * still requires a CA bundle, including for HTTP-to-HTTPS redirects. */
        av_dict_set(&options,"redamp_tls_verify",insecure?"0":"1",0);
        if(!insecure){
            char ca[1024]={0};
            if(!ca_store(ca,sizeof ca)) {
                set_err(err,errlen,"Radio requires a trusted CA bundle");
                av_dict_free(&options);goto fail;
            }
            av_dict_set(&options,"ca_file",ca,0);
            av_dict_set(&options,"redamp_ca_file",ca,0);
        }else fprintf(stderr,"[ffaudio] insecure HTTPS explicitly enabled\n");
    } else av_dict_set(&options,"protocol_whitelist","file",0);
    AVInputFormat *force_format=NULL;
    if(!live) {
        const char *extension=strrchr(path,'.');
        if(extension && !strcasecmp(extension,".aac"))
            force_format=av_find_input_format("aac");
    }
    int open_result=avformat_open_input(&b->fmt,path,force_format,&options);
    if(open_result<0) {
        char detail[96]={0};
        av_strerror(open_result,detail,sizeof detail);
        if(err && errlen) snprintf(err,errlen,"FFmpeg open failed: %s",detail);
        av_dict_free(&options);goto fail;
    }
    av_dict_free(&options);
    b->deadline=av_gettime_relative()+12000000;
    int info_result=avformat_find_stream_info(b->fmt,NULL);
    if(info_result<0) {
        char detail[96]={0};
        av_strerror(info_result,detail,sizeof detail);
        if(err && errlen) snprintf(err,errlen,"AAC/HLS stream info: %s",detail);
        goto fail;
    }
    b->stream=av_find_best_stream(b->fmt,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);
    if(b->stream<0) {set_err(err,errlen,"No supported audio stream");goto fail;}
    AVCodecParameters *param=b->fmt->streams[b->stream]->codecpar;
    /* Never turn FFmpeg into an arbitrary codec engine via untrusted HLS. */
    if(param->codec_id!=AV_CODEC_ID_AAC && param->codec_id!=AV_CODEC_ID_AAC_LATM &&
       param->codec_id!=AV_CODEC_ID_ALAC && param->codec_id!=AV_CODEC_ID_MP3) {
        set_err(err,errlen,"This FFmpeg audio codec is disabled");goto fail;
    }
    const AVCodec *codec=avcodec_find_decoder(param->codec_id);
    if(!codec) {set_err(err,errlen,"AAC/ALAC decoder unavailable");goto fail;}
    b->codec=avcodec_alloc_context3(codec);
    if(!b->codec || avcodec_parameters_to_context(b->codec,param)<0 ||
       avcodec_open2(b->codec,codec,NULL)<0) {
        set_err(err,errlen,"FFmpeg decoder initialization failed");goto fail;
    }
    b->packet=av_packet_alloc();b->frame=av_frame_alloc();
    if(!b->packet || !b->frame) goto fail;
    info->samplerate=REDAMP_RATE;info->channels=2;
    info->kbps=param->bit_rate>0 ? (int)(param->bit_rate/1000):0;
    /* FFmpeg estimates raw ADTS duration from bitrate; some files produce
     * wildly incorrect multi-hour estimates. Count actual ADTS frames instead.
     * This runs inside the playback worker, never on the UI/render thread. */
    const char *ext=strrchr(path,'.');
    int raw_aac=!live && ext && !strcasecmp(ext,".aac");
    int64_t adts_ms=raw_aac ? adts_duration_playback_ms(path) : 0;
    info->frames=adts_ms>0 ? av_rescale(adts_ms,REDAMP_RATE,1000) :
        (!raw_aac && !live && b->fmt->duration>0 ?
         av_rescale(b->fmt->duration,REDAMP_RATE,AV_TIME_BASE):0);
    b->deadline=0;
    refresh_title(b);
    return b;
fail:
    if(err && errlen && !err[0]) set_err(err,errlen,"FFmpeg initialization failed");
    bridge_free(b);return NULL;
}
void *ffbridge_open(const char *path,int live,const atomic_int *cancel,
                    ffaudio_info *info,char *error,size_t error_len) {
    if(error && error_len) error[0]=0;
    return bridge_new(path,live,cancel,info,error,error_len,0);
}
void *ffbridge_open_tls(const char *path,int live,const atomic_int *cancel,
                        ffaudio_info *info,char *error,size_t error_len,int insecure) {
    if(error && error_len) error[0]=0;
    return bridge_new(path,live,cancel,info,error,error_len,live&&insecure);
}
static int reset_converter(bridge *b,AVFrame *f) {
    int rate=f->sample_rate, ch=f->channels, fmt=f->format;
    if(rate<8000 || rate>384000 || ch<1 || ch>8) return 0;
    int64_t layout=f->channel_layout ? (int64_t)f->channel_layout : av_get_default_channel_layout(ch);
    if(!layout) return 0;
    if(b->swr && b->input_rate==rate && b->input_channels==ch &&
       b->input_layout==layout && b->input_format==fmt) return 1;
    swr_free(&b->swr);
    b->swr=swr_alloc_set_opts(NULL,AV_CH_LAYOUT_STEREO,AV_SAMPLE_FMT_FLT,REDAMP_RATE,
            layout,(enum AVSampleFormat)fmt,rate,0,NULL);
    if(!b->swr || swr_init(b->swr)<0) return 0;
    b->input_rate=rate;b->input_channels=ch;b->input_layout=layout;b->input_format=fmt;
    return 1;
}
static int decode_frame(bridge *b) {
    AVFrame *f=b->frame;
    if(!reset_converter(b,f)) return -1;
    int bound=swr_get_out_samples(b->swr,f->nb_samples);
    if(bound<0 || bound>MAX_PENDING) return -1;
    if(bound>b->cap) {
        float *p=realloc(b->pending,(size_t)bound*2*sizeof(float));
        if(!p) return -1;
        b->pending=p;b->cap=bound;
    }
    uint8_t *dst=(uint8_t *)b->pending;
    int out=swr_convert(b->swr,&dst,b->cap,(const uint8_t **)f->extended_data,f->nb_samples);
    if(out<0 || out>bound) return -1;
    b->count=out;b->pos=0;
    return out;
}
int ffbridge_read(void *ctx,float *stereo,int frames) {
    bridge *b=ctx;
    if(!b || frames<=0 || frames>8192 || !stereo) return 0;
    int done=0, iterations=0;
    while(done<frames && iterations++<10000 && !interrupt_cb(b)) {
        if(b->pos<b->count) {
            int k=b->count-b->pos;
            if(k>frames-done) k=frames-done;
            memcpy(stereo+(size_t)done*2,b->pending+(size_t)b->pos*2,(size_t)k*2*sizeof(float));
            done+=k;b->pos+=k;
            continue;
        }
        int r=avcodec_receive_frame(b->codec,b->frame);
        if(r>=0) {
            int got=decode_frame(b);
            av_frame_unref(b->frame);
            if(got<0) break;
            continue;
        }
        if(r==AVERROR_EOF) break;
        if(r!=AVERROR(EAGAIN)) break;
        if(b->eof) {
            if(!b->flushing) {avcodec_send_packet(b->codec,NULL);b->flushing=1;continue;}
            break;
        }
        b->deadline=av_gettime_relative()+20000000;
        r=av_read_frame(b->fmt,b->packet);
        b->deadline=0;
        if(r<0) {b->eof=1;continue;}
        if(b->packet->stream_index==b->stream) {
            /* Skip isolated damaged packets and leave the decoder available. */
            avcodec_send_packet(b->codec,b->packet);
        }
        if(b->fmt->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED) {
            refresh_title(b);
            b->fmt->event_flags &= ~AVFMT_EVENT_FLAG_METADATA_UPDATED;
        }
        av_packet_unref(b->packet);
    }
    if(done && b->live) refresh_title(b);
    return done;
}
int64_t ffbridge_seek(void *ctx,int64_t frame) {
    bridge *b=ctx;
    if(!b || b->live || frame<0) return -1;
    AVStream *s=b->fmt->streams[b->stream];
    if(s->time_base.num<=0 || s->time_base.den<=0) return -1;
    int64_t pts=av_rescale_q(frame,(AVRational){1,REDAMP_RATE},s->time_base);
    if(avformat_seek_file(b->fmt,b->stream,INT64_MIN,pts,INT64_MAX,AVSEEK_FLAG_BACKWARD)<0)
        return -1;
    avcodec_flush_buffers(b->codec);
    if(b->swr) swr_close(b->swr),swr_init(b->swr);
    b->pos=b->count=0;b->eof=b->flushing=0;
    return frame;
}
void ffbridge_title(void *ctx,char *out,size_t n) {
    if(!n) return;out[0]=0;
    bridge *b=ctx;
    if(!b) return;
    snprintf(out,n,"%s",b->title);
}
int ffbridge_metadata(const char *path,track_meta *meta) {
    if(!path || !meta || !ffpath_allowed(path,0) || !ffbridge_ready()) return 0;
    bridge b={0};
    b.deadline=av_gettime_relative()+5000000;
    AVFormatContext *fmt=avformat_alloc_context();
    if(!fmt) return 0;
    fmt->interrupt_callback.callback=interrupt_cb;
    fmt->interrupt_callback.opaque=&b;
    AVDictionary *opts=NULL;
    av_dict_set(&opts,"protocol_whitelist","file",0);
    av_dict_set(&opts,"format_whitelist","mov,aac",0);
    av_dict_set(&opts,"probesize","1048576",0);
    av_dict_set(&opts,"analyzeduration","8000000",0);
    const char *extension=strrchr(path,'.');
    AVInputFormat *forced=(extension && !strcasecmp(extension,".aac")) ?
        av_find_input_format("aac") : NULL;
    if(avformat_open_input(&fmt,path,forced,&opts)<0) {av_dict_free(&opts);return 0;}
    av_dict_free(&opts);
    if(avformat_find_stream_info(fmt,NULL)<0) {avformat_close_input(&fmt);return 0;}
    int i=av_find_best_stream(fmt,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);
    if(i<0) {avformat_close_input(&fmt);return 0;}
    AVStream *s=fmt->streams[i];
    meta->samplerate=s->codecpar->sample_rate;
    meta->channels=s->codecpar->channels;
    meta->kbps=(int)(s->codecpar->bit_rate/1000);
    int raw_aac=extension && !strcasecmp(extension,".aac");
    int64_t exact_ms=raw_aac ? adts_duration_ms(path) : 0;
    /* Never expose FFmpeg's unreliable bitrate estimate for raw ADTS. */
    meta->dur_ms=exact_ms>0 ? (int)exact_ms :
                 (!raw_aac && fmt->duration>0 && fmt->duration/1000<=INT_MAX ?
                  (int)(fmt->duration/1000) : 0);
    const AVDictionary *d=s->metadata;
    const char *fields[]={"title","artist","album","album_artist","track"};
    char *targets[]={meta->title,meta->artist,meta->album,meta->album_artist,NULL};
    for(int j=0;j<5;j++) {
        AVDictionaryEntry *e=av_dict_get(d,fields[j],NULL,0);
        if(!e) e=av_dict_get(fmt->metadata,fields[j],NULL,0);
        if(e) {if(j==4) meta->track_no=atoi(e->value);
               else snprintf(targets[j],160,"%s",e->value);}
    }
    avformat_close_input(&fmt);
    return 1;
}
