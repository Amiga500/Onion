/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FFMPEG_ABI_H
#define MAINUI_FFMPEG_ABI_H
#include <stdint.h>

/* The FFmpeg that Onion ships in .tmp_update/lib/parasyte, 2.4.14
 * (libavformat 56, libavcodec 56, libavutil 54, libswscale 3, libswresample
 * 1), is loaded
 * with dlopen() only when a video plays; MainUI neither links nor ships it.
 * These are the leading fields of the structures MainUI reads, declared with
 * the types of FFmpeg 2.4.14's own headers, so the compiler lays them out as
 * FFmpeg does on every target. Within a major version FFmpeg only appends to
 * these structures. AVPacket is the caller's, so it is declared whole. */

typedef struct {
    int num, den;
} MainUIAVRational;

typedef struct MainUIAVPacket {
    void *buf;
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    int size;
    int stream_index;
    int flags;
    void *side_data;
    int side_data_elems;
    int duration;
    void (*destruct)(struct MainUIAVPacket *); /* deprecated, still in lavc 56 */
    void *priv;                                /* deprecated, still in lavc 56 */
    int64_t pos;
    int64_t convergence_duration;
} MainUIAVPacket;

typedef struct {
    const void *av_class;
    int log_level_offset;
    int codec_type; /* enum AVMediaType */
    const void *codec;
    char codec_name[32]; /* deprecated, still in lavc 56 */
    int codec_id;        /* enum AVCodecID */
    unsigned int codec_tag;
    unsigned int stream_codec_tag;
    void *priv_data;
    void *internal;
    void *opaque;
    int bit_rate;
    int bit_rate_tolerance;
    int global_quality;
    int compression_level;
    int flags;
    int flags2;
    uint8_t *extradata;
    int extradata_size;
    MainUIAVRational time_base;
    int ticks_per_frame;
    int delay;
    int width, height; /* known from the file before anything is decoded */
    /* The fields up to get_buffer2, as declared in lavc 56 (function
     * pointers MainUI does not call are declared without their types). */
    int coded_width, coded_height;
    int gop_size;
    int pix_fmt;
    int me_method;
    void (*draw_horiz_band)(void);
    void (*get_format)(void);
    int max_b_frames;
    float b_quant_factor;
    int rc_strategy;
    int b_frame_strategy;
    float b_quant_offset;
    int has_b_frames;
    int mpeg_quant;
    float i_quant_factor;
    float i_quant_offset;
    float lumi_masking;
    float temporal_cplx_masking;
    float spatial_cplx_masking;
    float p_masking;
    float dark_masking;
    int slice_count;
    int prediction_method;
    int *slice_offset;
    MainUIAVRational sample_aspect_ratio;
    int me_cmp;
    int me_sub_cmp;
    int mb_cmp;
    int ildct_cmp;
    int dia_size;
    int last_predictor_count;
    int pre_me;
    int me_pre_cmp;
    int pre_dia_size;
    int me_subpel_quality;
    int dtg_active_format;
    int me_range;
    int intra_quant_bias;
    int inter_quant_bias;
    int slice_flags;
    int xvmc_acceleration;
    int mb_decision;
    uint16_t *intra_matrix;
    uint16_t *inter_matrix;
    int scenechange_threshold;
    int noise_reduction;
    int me_threshold;
    int mb_threshold;
    int intra_dc_precision;
    int skip_top;
    int skip_bottom;
    float border_masking;
    int mb_lmin;
    int mb_lmax;
    int me_penalty_compensation;
    int bidir_refine;
    int brd_scale;
    int keyint_min;
    int refs;
    int chromaoffset;
    int scenechange_factor;
    int mv0_threshold;
    int b_sensitivity;
    int color_primaries;
    int color_trc;
    int colorspace;
    int color_range;
    int chroma_sample_location;
    int slices;
    int field_order;
    int sample_rate;
    int channels;
    int sample_fmt;
    int frame_size;
    int frame_number;
    int block_align;
    int cutoff;
    int request_channels;
    uint64_t channel_layout;
    uint64_t request_channel_layout;
    int audio_service_type;
    int request_sample_fmt;
    void (*get_buffer)(void);
    void (*release_buffer)(void);
    void (*reget_buffer)(void);
    /* Asked for every picture buffer a decoder takes, also while
     * avformat_find_stream_info() decodes: MainUI refuses one too large. */
    int (*get_buffer2)(void *context, void *frame, int flags);
} MainUIAVCodecContext;

typedef struct {
    int index;
    int id;
    MainUIAVCodecContext *codec; /* opened in place */
    void *priv_data;

    struct {
        int64_t val, num, den;
    } pts; /* deprecated AVFrac, still in lavf 56 */

    MainUIAVRational time_base;
    int64_t start_time;
    int64_t duration;
    int64_t nb_frames;
    int disposition; /* AV_DISPOSITION_*: ATTACHED_PIC for cover art */
} MainUIAVStream;

typedef struct {
    int (*callback)(void *);
    void *opaque;
} MainUIAVIOInterruptCB;

/* Allocated with avformat_alloc_context(), so interrupt_callback is set
 * before avformat_open_input() reads anything. */
typedef struct {
    const void *av_class;
    void *iformat;
    void *oformat;
    void *priv_data;
    void *pb;
    int ctx_flags;
    unsigned int nb_streams;
    MainUIAVStream **streams;
    char filename[1024];
    int64_t start_time;
    int64_t duration;
    int bit_rate;
    unsigned int packet_size;
    int max_delay;
    int flags;
    unsigned int probesize;
    int max_analyze_duration; /* deprecated, still in lavf 56 */
    const uint8_t *key;
    int keylen;
    unsigned int nb_programs;
    void **programs;
    int video_codec_id;    /* enum AVCodecID */
    int audio_codec_id;    /* enum AVCodecID */
    int subtitle_codec_id; /* enum AVCodecID */
    unsigned int max_index_size;
    unsigned int max_picture_buffer;
    unsigned int nb_chapters;
    void **chapters;
    void *metadata;
    int64_t start_time_realtime;
    int fps_probe_size;
    int error_recognition;
    /* Checked by FFmpeg's reads and its stream probing: non-zero stops them. */
    MainUIAVIOInterruptCB interrupt_callback;
} MainUIAVFormatContext;

typedef struct {
    uint8_t *data[8];
    int linesize[8];
    uint8_t **extended_data;
    int width, height;
    int nb_samples;
    int format;
} MainUIAVFrame;

enum {
    MAINUI_AVMEDIA_TYPE_VIDEO = 0,
    MAINUI_AVMEDIA_TYPE_AUDIO = 1,
    MAINUI_AV_CODEC_ID_NONE = 0,
    MAINUI_AV_DISPOSITION_ATTACHED_PIC = 0x0400,
    MAINUI_AV_SAMPLE_FMT_S16 = 1,
    MAINUI_AV_LOG_QUIET = -8,
    MAINUI_AVSEEK_FLAG_BACKWARD = 1,
    MAINUI_SWS_FAST_BILINEAR = 1,
    /* enum AVPixelFormat in libavutil 54 */
    MAINUI_AV_PIX_FMT_ARGB = 27,
    MAINUI_AV_PIX_FMT_RGBA = 28,
    MAINUI_AV_PIX_FMT_ABGR = 29,
    MAINUI_AV_PIX_FMT_BGRA = 30
};

#define MAINUI_AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#define MAINUI_AV_CH_LAYOUT_STEREO 3 /* AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT */

/* The functions looked up, with their 2.4 signatures. */
typedef struct {
    void (*av_register_all)(void);
    MainUIAVFormatContext *(*avformat_alloc_context)(void);
    int (*avformat_open_input)(MainUIAVFormatContext **, const char *, void *, void **);
    int (*avformat_find_stream_info)(MainUIAVFormatContext *, void **);
    int (*av_find_best_stream)(MainUIAVFormatContext *, int, int, int, void **, int);
    int (*av_read_frame)(MainUIAVFormatContext *, MainUIAVPacket *);
    int (*av_seek_frame)(MainUIAVFormatContext *, int, int64_t, int);
    void (*avformat_close_input)(MainUIAVFormatContext **);
    int (*avcodec_open2)(void *, const void *, void **);
    int (*avcodec_default_get_buffer2)(void *, void *, int);
    int (*avcodec_close)(void *);
    int (*avcodec_decode_video2)(void *, MainUIAVFrame *, int *, const MainUIAVPacket *);
    void (*avcodec_flush_buffers)(void *);
    void (*av_init_packet)(MainUIAVPacket *);
    void (*av_free_packet)(MainUIAVPacket *);
    MainUIAVFrame *(*av_frame_alloc)(void);
    void (*av_frame_free)(MainUIAVFrame **);
    int64_t (*av_frame_get_best_effort_timestamp)(const MainUIAVFrame *);
    void (*av_log_set_level)(int);
    void *(*sws_getCachedContext)(void *, int, int, int, int, int, int, int, void *, void *,
                                  const double *);
    int (*sws_scale)(void *, const uint8_t *const[], const int[], int, int, uint8_t *const[],
                     const int[]);
    void (*sws_freeContext)(void *);
    /* Sound: optional, a video plays silently without any of these. */
    int (*avcodec_decode_audio4)(void *, MainUIAVFrame *, int *, const MainUIAVPacket *);
    int (*av_frame_get_sample_rate)(const MainUIAVFrame *);
    int64_t (*av_frame_get_channel_layout)(const MainUIAVFrame *);
    int (*av_frame_get_channels)(const MainUIAVFrame *);
    int64_t (*av_get_default_channel_layout)(int);
    void *(*swr_alloc_set_opts)(void *, int64_t, int, int, int64_t, int, int, int, void *);
    int (*swr_init)(void *);
    int (*swr_convert)(void *, uint8_t **, int, const uint8_t **, int);
    void (*swr_free)(void **);
} MainUIFFmpeg;

#endif
