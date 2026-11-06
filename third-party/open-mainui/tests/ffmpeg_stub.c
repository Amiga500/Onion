/* SPDX-License-Identifier: GPL-3.0-only
 * A stand-in for the FFmpeg 2.4 libraries Onion ships, for host tests of the
 * media gallery's video playback. The tests link it under FFmpeg's sonames.
 * A "video" is a text file:
 *
 *   STUB <width> <height> <frames> <fps> [audio<rate>] [lag<ms>] [nopts]
 *        [slow] [noseek] [nocodec] [nosoundcodec] [audiofirst] [lie]
 *        [nodims] [cover] [extra] [gop<frames>] [packet<bytes>]
 *        [waitopen] [waitprobe] [waitread]
 *
 * Frame i decodes one frame late, as H.264 decoders hold frames, and
 * converts to the colour (i, 255 - i, 100) (i below 256). With audio<rate>
 * a sound stream of 16-bit stereo at that rate, in chunks of 1024 samples,
 * is interleaved by time, each chunk lag ms after the pictures of its time
 * (as some muxers store sound in blocks); every sample of chunk j is j + 1, and resampling
 * keeps the values. A seek lands on the frame at or before the time asked
 * for. The sound is stream 1, or stream 0 before the pictures with
 * audiofirst. The codec context tells the size before decoding, or 320x240
 * with lie, so only the decoded frames show it, or nothing with nodims.
 * cover adds cover art (an attached picture, its size unknown), extra a
 * second video stream of 4000x3000. Probing reports, as an error, decoding
 * any video stream not stated to be within 1280x720, cover art included,
 * and decoding takes each picture's buffer through the codec context's
 * get_buffer2 at the size the stream really has, reporting as an error a
 * buffer taken for a picture over 1280x720 (rounded up to 16-pixel blocks).
 * grow<n> makes pictures from the nth on 4000x3000.
 * gop<n> makes seeks land on a keyframe, every n frames: the one at or
 * before the time asked for with AVSEEK_FLAG_BACKWARD, else the one at or
 * after it, and fail when there is none;
 * packet<bytes> makes each picture's packet that large. Pictures must be
 * decoded in the order they were read until the decoder is flushed, or an
 * error is reported. waitopen, waitprobe and waitread hold the open, the
 * probe or the first read for 3 s unless the interrupt callback stops them.
 * Anything else fails to open. Built with -DSTUB_WITHOUT_SCALE it lacks
 * sws_scale. Every allocation is counted, and the count left at unload is
 * printed; errors are printed with "[ffstub]". */
#define _POSIX_C_SOURCE 200809L
#include "platform/ffmpeg_abi.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    CHUNK = 1024
};

static int live;

static void *allocate(size_t size)
{
    void *memory = calloc(1, size);
    live += memory != NULL;
    return memory;
}

static void release(void *memory)
{
    live -= memory != NULL;
    free(memory);
}

__attribute__((destructor)) static void report(void)
{
    if (live) {
        fprintf(stderr, "[ffstub] %d allocations left\n", live);
    }
}

typedef struct {
    MainUIAVFrame frame;
    int64_t best;
    int rate, channels;
    uint8_t luma[16];
    int16_t samples[CHUNK * 2];
    uint8_t *planes[1];
} StubFrame;

typedef struct {
    MainUIAVCodecContext context; /* first: the stream's codec points here */
    int width, height, frames, fps, rate, lag, pending, expect, gop, packet, grow;
    int nopts, slow, noseek, nocodec, nosoundcodec, opened, sound, audiofirst;
} StubCodec;

typedef struct {
    MainUIAVFormatContext context;
    MainUIAVStream stream, sound_stream, third;
    MainUIAVStream *streams[3];
    StubCodec codec, sound_codec;
    MainUIAVCodecContext third_codec; /* cover art or a second video */
    int third_width, third_height;    /* as decoded */
    int next, next_sound, chunks;
    bool wait_probe, wait_read;
    uint8_t packet[16];
    uint8_t *big; /* packet<bytes> */
} StubInput;

/* Hold for up to 3 s, unless the interrupt callback stops it first. */
static bool interrupted_wait(MainUIAVFormatContext *context)
{
    for (int i = 0; i < 300; i++) {
        MainUIAVIOInterruptCB *check = &context->interrupt_callback;
        if (check->callback && check->callback(check->opaque)) {
            return true;
        }
        nanosleep(&(struct timespec){0, 10 * 1000 * 1000}, NULL);
    }
    return false;
}

#define STUB_AVERROR_EXIT (-1414092869)

int avcodec_default_get_buffer2(void *context, void *frame, int flags)
{
    (void)context;
    (void)frame;
    (void)flags;
    return 0;
}

/* A decoder taking a picture's buffer, as FFmpeg's do, through get_buffer2
 * when set. */
static int take_picture(MainUIAVCodecContext *context, int width, int height)
{
    MainUIAVFrame frame = {.width = width, .height = height};
    int result = context->get_buffer2 ? context->get_buffer2(context, &frame, 0) : 0;
    if (result >= 0 && (width > 1296 || height > 736)) {
        fprintf(stderr, "[ffstub] took a buffer for a %dx%d picture\n", width, height);
    }
    return result;
}

MainUIAVFormatContext *avformat_alloc_context(void)
{
    StubInput *input = allocate(sizeof *input);
    return input ? &input->context : NULL;
}

typedef struct {
    int format, width, height;
} StubScaler;

typedef struct {
    int in_rate, out_rate;
} StubResampler;

static const int decoder, sound_decoder;

void av_register_all(void) {}

void av_log_set_level(int level)
{
    (void)level;
}

int avformat_open_input(MainUIAVFormatContext **out, const char *path, void *format, void **options)
{
    (void)format;
    (void)options;
    char text[256] = "";
    FILE *file = fopen(path, "rb");
    size_t n = file ? fread(text, 1, sizeof text - 1, file) : 0;
    if (file) {
        fclose(file);
    }
    text[n] = 0;
    StubCodec codec = {.pending = -1, .expect = -1};
    if (sscanf(text, "STUB %d %d %d %d", &codec.width, &codec.height, &codec.frames, &codec.fps) !=
            4 ||
        codec.fps <= 0 || (strstr(text, " waitopen") && *out && interrupted_wait(*out))) {
        release(*out);
        *out = NULL;
        return strstr(text, " waitopen") ? STUB_AVERROR_EXIT : -1;
    }
    const char *audio = strstr(text, " audio");
    codec.rate = audio ? atoi(audio + 6) : 0;
    const char *lag = strstr(text, " lag");
    codec.lag = lag ? atoi(lag + 4) : 0;
    codec.nopts = strstr(text, " nopts") != NULL;
    codec.slow = strstr(text, " slow") != NULL;
    codec.noseek = strstr(text, " noseek") != NULL;
    codec.nocodec = strstr(text, " nocodec") != NULL;
    codec.nosoundcodec = strstr(text, " nosoundcodec") != NULL;
    codec.audiofirst = codec.rate > 0 && strstr(text, " audiofirst") != NULL;
    const char *gop = strstr(text, " gop");
    codec.gop = gop ? atoi(gop + 4) : 0;
    const char *packet = strstr(text, " packet");
    codec.packet = packet ? atoi(packet + 7) : 0;
    const char *grow = strstr(text, " grow");
    codec.grow = grow ? atoi(grow + 5) : 0;
    bool lie = strstr(text, " lie") != NULL, nodims = strstr(text, " nodims") != NULL;
    codec.context.codec_type = MAINUI_AVMEDIA_TYPE_VIDEO;
    codec.context.codec_id = 28; /* H.264 */
    codec.context.width = nodims ? 0 : lie ? 320 : codec.width;
    codec.context.height = nodims ? 0 : lie ? 240 : codec.height;
    StubInput *input = *out ? (StubInput *)*out : allocate(sizeof *input);
    if (!input) {
        return -1;
    }
    MainUIAVIOInterruptCB check = input->context.interrupt_callback;
    *input = (StubInput){.context.interrupt_callback = check};
    input->wait_probe = strstr(text, " waitprobe") != NULL;
    input->wait_read = strstr(text, " waitread") != NULL;
    if (codec.packet > (int)sizeof input->packet) {
        input->big = allocate((size_t)codec.packet);
        if (!input->big) {
            release(input);
            *out = NULL;
            return -1;
        }
    }
    input->codec = codec;
    input->stream.codec = &input->codec.context;
    input->stream.time_base = (MainUIAVRational){1, 90000};
    input->streams[0] = &input->stream;
    input->context.nb_streams = 1;
    if (codec.rate > 0) {
        input->sound_codec = codec;
        input->sound_codec.sound = 1;
        input->sound_codec.context.codec_type = MAINUI_AVMEDIA_TYPE_AUDIO;
        input->sound_codec.context.codec_id = 86018; /* AAC */
        input->sound_codec.context.width = input->sound_codec.context.height = 0;
        input->stream.index = codec.audiofirst;
        input->sound_stream.index = !codec.audiofirst;
        input->sound_stream.codec = &input->sound_codec.context;
        input->sound_stream.time_base = (MainUIAVRational){1, codec.rate};
        input->streams[input->sound_stream.index] = &input->sound_stream;
        input->streams[input->stream.index] = &input->stream;
        input->context.nb_streams = 2;
        /* As long as the pictures. */
        input->chunks = (int)((int64_t)codec.frames * codec.rate / codec.fps / CHUNK);
    }
    bool cover = strstr(text, " cover") != NULL, extra = strstr(text, " extra") != NULL;
    if (cover || extra) {
        input->third_codec.codec_type = MAINUI_AVMEDIA_TYPE_VIDEO;
        input->third_codec.codec_id = cover ? 7 : 28; /* MJPEG or H.264 */
        input->third_codec.width = cover ? 0 : 4000;
        input->third_codec.height = cover ? 0 : 3000;
        input->third_width = cover ? 3000 : 4000;
        input->third_height = 3000;
        input->third.disposition = cover ? MAINUI_AV_DISPOSITION_ATTACHED_PIC : 0;
        input->third.index = (int)input->context.nb_streams;
        input->third.codec = &input->third_codec;
        input->third.time_base = (MainUIAVRational){1, 90000};
        input->streams[input->context.nb_streams++] = &input->third;
    }
    input->context.streams = input->streams;
    *out = &input->context;
    return 0;
}

/* As FFmpeg's, which decodes streams whose size is not known yet. */
int avformat_find_stream_info(MainUIAVFormatContext *context, void **options)
{
    (void)options;
    StubInput *input = (StubInput *)context;
    if (input->wait_probe && interrupted_wait(context)) {
        return STUB_AVERROR_EXIT;
    }
    for (unsigned i = 0; i < context->nb_streams; i++) {
        const MainUIAVCodecContext *codec = context->streams[i]->codec;
        if (codec->codec_type == MAINUI_AVMEDIA_TYPE_VIDEO &&
            codec->codec_id != MAINUI_AV_CODEC_ID_NONE &&
            (codec->width <= 0 || codec->height <= 0 || codec->width > 1280 ||
             codec->height > 720)) {
            fprintf(stderr, "[ffstub] probing decoded stream %u, %dx%d\n", i, codec->width,
                    codec->height);
        }
    }
    /* It decodes a picture of each video stream, at the stream's own size. */
    for (unsigned i = 0; i < context->nb_streams; i++) {
        MainUIAVCodecContext *codec = (MainUIAVCodecContext *)context->streams[i]->codec;
        if (codec->codec_type != MAINUI_AVMEDIA_TYPE_VIDEO ||
            codec->codec_id == MAINUI_AV_CODEC_ID_NONE) {
            continue;
        }
        bool main = codec == &input->codec.context;
        take_picture(codec, main ? input->codec.width : input->third_width,
                     main ? input->codec.height : input->third_height);
    }
    return 0;
}

int av_find_best_stream(MainUIAVFormatContext *context, int type, int wanted, int related,
                        void **found, int flags)
{
    (void)wanted;
    (void)related;
    (void)flags;
    StubInput *input = (StubInput *)context;
    if (type == MAINUI_AVMEDIA_TYPE_VIDEO) {
        *found = (void *)&decoder;
        return input->stream.index;
    }
    if (type == MAINUI_AVMEDIA_TYPE_AUDIO && context->nb_streams > 1) {
        *found = (void *)&sound_decoder;
        return input->sound_stream.index;
    }
    return -1381258232; /* AVERROR_STREAM_NOT_FOUND */
}

void av_init_packet(MainUIAVPacket *packet)
{
    packet->pts = packet->dts = MAINUI_AV_NOPTS_VALUE;
    packet->stream_index = 0;
    packet->flags = 0;
}

void av_free_packet(MainUIAVPacket *packet)
{
    packet->data = NULL;
    packet->size = 0;
}

int av_read_frame(MainUIAVFormatContext *context, MainUIAVPacket *packet)
{
    StubInput *input = (StubInput *)context;
    StubCodec *codec = &input->codec;
    if (input->wait_read) {
        input->wait_read = false;
        if (interrupted_wait(context)) {
            return STUB_AVERROR_EXIT;
        }
    }
    bool picture = input->next < codec->frames;
    bool sound = input->next_sound < input->chunks;
    /* By time: the earlier of the next picture and the next sound, the
     * sound counted lag ms later. */
    if (picture && sound &&
        (int64_t)input->next_sound * CHUNK * 1000 / codec->rate + codec->lag >=
            (int64_t)input->next * 1000 / codec->fps) {
        sound = false;
    }
    if (!picture && !sound) {
        return -541478725; /* AVERROR_EOF */
    }
    int index = sound ? input->next_sound++ : input->next++;
    bool big = !sound && input->big;
    packet->data = big ? input->big : input->packet;
    packet->size = big ? codec->packet : (int)sizeof input->packet;
    memcpy(packet->data, &index, sizeof index);
    packet->stream_index = sound ? input->sound_stream.index : input->stream.index;
    packet->pts = sound ? (int64_t)index * CHUNK : (int64_t)index * 90000 / codec->fps;
    return 0;
}

int av_seek_frame(MainUIAVFormatContext *context, int stream, int64_t timestamp, int flags)
{
    (void)stream;
    StubInput *input = (StubInput *)context;
    StubCodec *codec = &input->codec;
    if (codec->noseek) {
        return -1;
    }
    int64_t frame = timestamp * codec->fps / 90000;
    /* To the keyframe at or before it, or with no flag, at or after it:
     * none there is an error. */
    if (codec->gop > 0 && frame > 0) {
        bool backward = flags & MAINUI_AVSEEK_FLAG_BACKWARD;
        frame = (frame + (backward ? 0 : codec->gop - 1)) / codec->gop * codec->gop;
        if (frame >= codec->frames) {
            return -1;
        }
    }
    input->next = frame < 0 ? 0 : frame > codec->frames ? codec->frames : (int)frame;
    input->next_sound =
        codec->rate ? (int)((int64_t)input->next * codec->rate / codec->fps / CHUNK) : 0;
    return 0;
}

void avformat_close_input(MainUIAVFormatContext **context)
{
    release(((StubInput *)*context)->big);
    release(*context);
    *context = NULL;
}

int avcodec_open2(void *context, const void *codec, void **options)
{
    (void)options;
    StubCodec *stub = context;
    if (codec != (stub->sound ? (const void *)&sound_decoder : (const void *)&decoder) ||
        (stub->sound ? stub->nosoundcodec : stub->nocodec)) {
        return -1;
    }
    stub->opened++;
    return 0;
}

int avcodec_close(void *context)
{
    StubCodec *stub = context;
    stub->opened--;
    if (stub->opened) {
        fprintf(stderr, "[ffstub] codec closed %d times too few\n", stub->opened);
    }
    return 0;
}

void avcodec_flush_buffers(void *context)
{
    ((StubCodec *)context)->pending = -1;
    ((StubCodec *)context)->expect = -1;
}

int avcodec_decode_video2(void *context, MainUIAVFrame *frame, int *got,
                          const MainUIAVPacket *packet)
{
    StubCodec *codec = context;
    *got = 0;
    int out = codec->pending;
    if (packet && packet->size > 0) {
        memcpy(&codec->pending, packet->data, sizeof codec->pending);
        /* Pictures depend on the ones before: decode order is read order. */
        if (codec->expect >= 0 && codec->pending != codec->expect) {
            fprintf(stderr, "[ffstub] picture %d decoded where %d was due\n", codec->pending,
                    codec->expect);
        }
        codec->expect = codec->pending + 1;
    }
    else {
        codec->pending = -1;
    }
    if (out < 0) {
        return packet ? packet->size : 0;
    }
    if (codec->slow) {
        nanosleep(&(struct timespec){0, 30 * 1000 * 1000}, NULL);
    }
    int width = codec->grow && out >= codec->grow ? 4000 : codec->width;
    int height = codec->grow && out >= codec->grow ? 3000 : codec->height;
    int taken = take_picture(&codec->context, width, height);
    if (taken < 0) {
        return taken;
    }
    StubFrame *stub = (StubFrame *)frame;
    stub->luma[0] = (uint8_t)out;
    frame->data[0] = stub->luma;
    frame->linesize[0] = width;
    frame->width = width;
    frame->height = height;
    frame->format = 0; /* YUV420P */
    stub->best = codec->nopts ? MAINUI_AV_NOPTS_VALUE : (int64_t)out * 90000 / codec->fps;
    *got = 1;
    return packet ? packet->size : 0;
}

int avcodec_decode_audio4(void *context, MainUIAVFrame *frame, int *got,
                          const MainUIAVPacket *packet)
{
    StubCodec *codec = context;
    StubFrame *stub = (StubFrame *)frame;
    int chunk = 0;
    memcpy(&chunk, packet->data, sizeof chunk);
    for (int i = 0; i < CHUNK * 2; i++) {
        stub->samples[i] = (int16_t)(chunk + 1);
    }
    stub->planes[0] = (uint8_t *)stub->samples;
    frame->extended_data = stub->planes;
    frame->nb_samples = CHUNK;
    frame->format = MAINUI_AV_SAMPLE_FMT_S16;
    stub->rate = codec->rate;
    stub->channels = 2;
    stub->best = codec->nopts ? MAINUI_AV_NOPTS_VALUE : (int64_t)chunk * CHUNK;
    *got = 1;
    return packet->size;
}

MainUIAVFrame *av_frame_alloc(void)
{
    StubFrame *frame = allocate(sizeof *frame);
    return frame ? &frame->frame : NULL;
}

void av_frame_free(MainUIAVFrame **frame)
{
    release(*frame);
    *frame = NULL;
}

int64_t av_frame_get_best_effort_timestamp(const MainUIAVFrame *frame)
{
    return ((const StubFrame *)frame)->best;
}

int av_frame_get_sample_rate(const MainUIAVFrame *frame)
{
    return ((const StubFrame *)frame)->rate;
}

int av_frame_get_channels(const MainUIAVFrame *frame)
{
    return ((const StubFrame *)frame)->channels;
}

int64_t av_frame_get_channel_layout(const MainUIAVFrame *frame)
{
    (void)frame;
    return 0; /* unknown: the default layout is asked for */
}

int64_t av_get_default_channel_layout(int channels)
{
    return channels == 2 ? MAINUI_AV_CH_LAYOUT_STEREO : 0;
}

void *swr_alloc_set_opts(void *context, int64_t out_layout, int out_format, int out_rate,
                         int64_t in_layout, int in_format, int in_rate, int log_offset, void *log)
{
    (void)log_offset;
    (void)log;
    if (out_layout != MAINUI_AV_CH_LAYOUT_STEREO || in_layout != MAINUI_AV_CH_LAYOUT_STEREO ||
        out_format != MAINUI_AV_SAMPLE_FMT_S16 || in_format != MAINUI_AV_SAMPLE_FMT_S16) {
        return NULL;
    }
    StubResampler *resampler = context ? context : allocate(sizeof *resampler);
    if (resampler) {
        *resampler = (StubResampler){in_rate, out_rate};
    }
    return resampler;
}

int swr_init(void *context)
{
    StubResampler *resampler = context;
    return resampler->in_rate > 0 && resampler->out_rate > 0 ? 0 : -1;
}

/* Nearest sample: the values carry over at any rate. */
int swr_convert(void *context, uint8_t **out, int room, const uint8_t **in, int count)
{
    StubResampler *resampler = context;
    int made = (int)((int64_t)count * resampler->out_rate / resampler->in_rate);
    made = made < room ? made : room;
    const int16_t *from = (const int16_t *)in[0];
    int16_t *to = (int16_t *)out[0];
    for (int i = 0; i < made; i++) {
        int source = (int)((int64_t)i * resampler->in_rate / resampler->out_rate);
        to[2 * i] = from[2 * source];
        to[2 * i + 1] = from[2 * source + 1];
    }
    return made;
}

void swr_free(void **context)
{
    release(*context);
    *context = NULL;
}

void *sws_getCachedContext(void *context, int src_w, int src_h, int src_format, int dst_w,
                           int dst_h, int dst_format, int flags, void *src_filter, void *dst_filter,
                           const double *param)
{
    (void)src_w;
    (void)src_h;
    (void)src_format;
    (void)flags;
    (void)src_filter;
    (void)dst_filter;
    (void)param;
    if (dst_format < MAINUI_AV_PIX_FMT_ARGB || dst_format > MAINUI_AV_PIX_FMT_BGRA) {
        return NULL;
    }
    StubScaler *scaler = context ? context : allocate(sizeof *scaler);
    if (scaler) {
        *scaler = (StubScaler){dst_format, dst_w, dst_h};
    }
    return scaler;
}

#ifndef STUB_WITHOUT_SCALE
int sws_scale(void *context, const uint8_t *const source[], const int source_pitch[], int y,
              int height, uint8_t *const target[], const int target_pitch[])
{
    (void)source_pitch;
    (void)y;
    StubScaler *scaler = context;
    int index = source[0][0];
    uint8_t r = (uint8_t)index, g = (uint8_t)(255 - index), b = 100;
    uint8_t pixel[4];
    switch (scaler->format) {
    case MAINUI_AV_PIX_FMT_BGRA:
        memcpy(pixel, (uint8_t[4]){b, g, r, 255}, 4);
        break;
    case MAINUI_AV_PIX_FMT_RGBA:
        memcpy(pixel, (uint8_t[4]){r, g, b, 255}, 4);
        break;
    case MAINUI_AV_PIX_FMT_ABGR:
        memcpy(pixel, (uint8_t[4]){255, b, g, r}, 4);
        break;
    default:
        memcpy(pixel, (uint8_t[4]){255, r, g, b}, 4);
        break;
    }
    for (int row = 0; row < scaler->height; row++) {
        for (int x = 0; x < scaler->width; x++) {
            memcpy(target[0] + row * target_pitch[0] + x * 4, pixel, 4);
        }
    }
    return height;
}
#endif

void sws_freeContext(void *context)
{
    release(context);
}
