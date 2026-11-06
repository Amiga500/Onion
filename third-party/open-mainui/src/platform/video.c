/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/video.h"
#include "platform/ffmpeg_abi.h"
#include <dlfcn.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <unistd.h>
#endif

/* One frame shown and one decoded ahead: the decoder waits for a free one. */
enum {
    SLOTS = 2,
    LIBRARIES = 9,
    /* Sound decoded ahead, in seconds; the oldest is dropped beyond it.
     * Reading stops once this much is ahead, below the ring's size. */
    RING_SECONDS = 3,
    SOUND_AHEAD_MS = 1000,
    /* Playing sound that falls this far behind the picture (after the
     * decoder could not keep up) is dropped to catch up. */
    RESYNC_MS = 250,
    /* The picture follows the sound when they are this far apart. */
    FOLLOW_MS = 50,
    /* Sound starts only with this much held, so it does not run dry at
     * once (or with the whole file read). */
    LEAD_MS = 200,
    /* Compressed pictures read ahead of decoding, so sound stored after
     * its pictures in the file is read in time. */
    QUEUE_PACKETS = 512,
    QUEUE_BYTES = 4 << 20,
    /* Zeroed bytes after a packet's data, which decoders may read. */
    PACKET_PADDING = 64
};

typedef enum {
    SLOT_FREE,
    SLOT_READY,
    SLOT_SHOWN
} SlotState;

/* Onion starts MainUI with an LD_LIBRARY_PATH without .tmp_update/lib/
 * parasyte, where its FFmpeg is, so the libraries are loaded from there by
 * path, each after the ones it needs: a library's dependencies are found
 * among those already loaded. Each is tried by name first, so one already
 * loaded or on the path (zlib, bzip2 in miyoo/lib) is not loaded twice. The
 * four FFmpeg libraries must load; the others only help them to, and give
 * sound (libswresample). */
static const struct {
    const char *soname;
    int ffmpeg; /* index used by the symbol table, or -1 */
} libraries[LIBRARIES] = {
    {"libz.so.1", -1},      {"libbz2.so.1.0", -1},   {"liblzma.so.5", -1},
    {"libx264.so.152", -1}, {"libavutil.so.54", 0},  {"libswresample.so.1", 4},
    {"libswscale.so.3", 1}, {"libavcodec.so.56", 2}, {"libavformat.so.56", 3}};

static const char *const ffmpeg_names[] = {"libavutil.so.54", "libswscale.so.3", "libavcodec.so.56",
                                           "libavformat.so.56", "libswresample.so.1"};

/* One symbols[] row. clang-format 14 and 17 lay this macro out
 * differently, so it is kept as it is. */
/* clang-format off */
#define SYMBOL(library, name, sound) {#name, library, offsetof(MainUIFFmpeg, name), sound}

/* clang-format on */

static const struct {
    const char *name;
    int library;
    size_t offset;
    bool sound; /* optional: without it the video plays silently */
} symbols[] = {
    SYMBOL(0, av_frame_alloc, false),
    SYMBOL(0, av_frame_free, false),
    SYMBOL(0, av_frame_get_best_effort_timestamp, false),
    SYMBOL(0, av_log_set_level, false),
    SYMBOL(1, sws_getCachedContext, false),
    SYMBOL(1, sws_scale, false),
    SYMBOL(1, sws_freeContext, false),
    SYMBOL(2, avcodec_default_get_buffer2, false),
    SYMBOL(2, avcodec_open2, false),
    SYMBOL(2, avcodec_close, false),
    SYMBOL(2, avcodec_decode_video2, false),
    SYMBOL(2, avcodec_flush_buffers, false),
    SYMBOL(2, av_init_packet, false),
    SYMBOL(2, av_free_packet, false),
    SYMBOL(3, av_register_all, false),
    SYMBOL(3, avformat_alloc_context, false),
    SYMBOL(3, avformat_open_input, false),
    SYMBOL(3, avformat_find_stream_info, false),
    SYMBOL(3, av_find_best_stream, false),
    SYMBOL(3, av_read_frame, false),
    SYMBOL(3, av_seek_frame, false),
    SYMBOL(3, avformat_close_input, false),
    SYMBOL(2, avcodec_decode_audio4, true),
    SYMBOL(0, av_frame_get_sample_rate, true),
    SYMBOL(0, av_frame_get_channel_layout, true),
    SYMBOL(0, av_frame_get_channels, true),
    SYMBOL(0, av_get_default_channel_layout, true),
    SYMBOL(4, swr_alloc_set_opts, true),
    SYMBOL(4, swr_init, true),
    SYMBOL(4, swr_convert, true),
    SYMBOL(4, swr_free, true),

#undef SYMBOL
};

struct MainUIVideo {
    char path[4096], library_dir[4096];
    Uint32 rmask, gmask, bmask;
    int pixel_format, max_width, max_height;
    int rate; /* sound output, samples per second; 0 for none */
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_cond *changed;
    atomic_bool stop;
    atomic_bool oversized; /* a decoder asked for a picture over the limit */
    void *handles[LIBRARIES], *ffmpeg[5];
    MainUIFFmpeg ff;
    bool sound_api;
    /* Shared, under lock. Frames are created by the decoder before the first
     * one is ready and freed only on close; the decoder writes FREE ones and
     * the UI reads READY and SHOWN ones. due is a frame's time in
     * milliseconds from the start of the video, counting repeats; at its
     * position in the file. */
    SDL_Surface *frames[SLOTS];
    SlotState state[SLOTS];
    Uint32 due[SLOTS], at[SLOTS];
    bool clock_set;
    Uint32 clock;      /* ticks at which the video's time 0 was, once shown */
    bool paused, held; /* paused by the user; held while the device sleeps */
    Uint32 paused_at;  /* when it stopped, either way */
    bool seek_pending;
    int seek_ms;
    bool seek_anchor_valid; /* committed target until its first frame is presented */
    int64_t seek_anchor_ms;
    char error[160];
    /* Sound, 16-bit stereo at rate, read by the mixer's thread. start is the
     * video time, in samples, of the oldest sample held. */
    Uint8 *ring;
    size_t ring_size, ring_head, ring_fill;
    int64_t ring_start;
    /* Sound plays on without gaps once it runs; the picture follows it. The
     * last buffer the mixer took: when, where it began, and how long. */
    bool sound_running, sound_read; /* sound_read: to the end of the file */
    Uint32 heard_at;
    int64_t heard_sample;
    int heard_samples;
};

static void wake_ui(void)
{
    SDL_Event event = {.type = SDL_USEREVENT, .user.code = MAINUI_VIDEO_CODE};
    SDL_PushEvent(&event);
}

/* Only the first reason is kept; the log gets the details. */
static void fail(MainUIVideo *video, const char *shown, const char *format, ...)
{
    char detail[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(detail, sizeof detail, format, arguments);
    va_end(arguments);
    fprintf(stderr, "[video] %.300s: %s\n", video->path, detail);
    SDL_mutexP(video->lock);
    if (!*video->error) {
        snprintf(video->error, sizeof video->error, "%s", shown);
    }
    SDL_mutexV(video->lock);
    wake_ui();
}

static bool load(MainUIVideo *video)
{
    for (int i = 0; i < LIBRARIES; i++) {
        /* Closing asked for: no more libraries (each load can take a while). */
        if (atomic_load(&video->stop)) {
            return false;
        }
        void *handle = dlopen(libraries[i].soname, RTLD_NOW | RTLD_LOCAL);
        char path[4096 + 32];
        if (!handle && *video->library_dir &&
            snprintf(path, sizeof path, "%s/%s", video->library_dir, libraries[i].soname) <
                (int)sizeof path) {
            handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        }
        video->handles[i] = handle;
        if (libraries[i].ffmpeg < 0) {
            continue;
        }
        if (!handle && libraries[i].ffmpeg < 4) {
            const char *why = dlerror();
            fail(video, "Video needs Onion's FFmpeg libraries", "%s (also looked in %s)",
                 why ? why : libraries[i].soname,
                 *video->library_dir ? video->library_dir : "no folder");
            return false;
        }
        video->ffmpeg[libraries[i].ffmpeg] = handle;
    }
    video->sound_api = video->rate > 0;
    for (size_t i = 0; i < sizeof symbols / sizeof *symbols; i++) {
        void *library = video->ffmpeg[symbols[i].library];
        void *symbol = library ? dlsym(library, symbols[i].name) : NULL;
        if (!symbol && symbols[i].sound) {
            if (video->sound_api) {
                fprintf(stderr, "[video] %s has no %s: no sound\n",
                        ffmpeg_names[symbols[i].library], symbols[i].name);
            }
            video->sound_api = false;
            continue;
        }
        if (!symbol) {
            fail(video, "Video needs Onion's FFmpeg libraries", "%s has no %s",
                 ffmpeg_names[symbols[i].library], symbols[i].name);
            return false;
        }
        memcpy((char *)&video->ff + symbols[i].offset, &symbol, sizeof symbol);
    }
    return true;
}

/* Paused or held: the picture and sound stand still. */
static bool frozen(const MainUIVideo *video)
{
    return video->paused || video->held;
}

/* The video's own time at now: stopped, it stays where it stopped. */
static Uint32 moment(const MainUIVideo *video, Uint32 now)
{
    return frozen(video) ? video->paused_at : now;
}

/* Under lock. Time stands still from the first stop to the last go. */
static void set_frozen(MainUIVideo *video, bool paused, bool held, Uint32 now)
{
    bool was = frozen(video);
    video->paused = paused;
    video->held = held;
    if (!was && frozen(video)) {
        video->paused_at = now;
    }
    else if (was && !frozen(video) && video->clock_set) {
        video->clock += now - video->paused_at;
    }
    SDL_CondSignal(video->changed);
}

/* A free frame to write, or -1 once stopping or asked to seek. */
static int free_slot(MainUIVideo *video)
{
    int slot = -1;
    SDL_mutexP(video->lock);
    while (!atomic_load(&video->stop) && !video->seek_pending) {
        for (int i = 0; i < SLOTS && slot < 0; i++) {
            if (video->state[i] == SLOT_FREE) {
                slot = i;
            }
        }
        if (slot >= 0) {
            break;
        }
        SDL_CondWait(video->changed, video->lock);
    }
    SDL_mutexV(video->lock);
    return slot;
}

/* A frame's due time is already past by more than a frame: skip it. */
static bool late(MainUIVideo *video, Uint32 due, Uint32 frame_ms)
{
    SDL_mutexP(video->lock);
    bool behind = video->clock_set &&
                  (Sint32)(moment(video, SDL_GetTicks()) - (video->clock + due)) > (Sint32)frame_ms;
    SDL_mutexV(video->lock);
    return behind;
}

typedef struct {
    MainUIAVFormatContext *input;
    void *codec, *scaler;
    MainUIAVFrame *frame;
    int stream;
    MainUIAVRational base;
    bool zero_set;
    int64_t zero_ms;       /* the file's first timestamp, the video's time 0 */
    Uint32 last, frame_ms; /* last frame's due time and the spacing of frames */
    Uint32 offset;         /* added on each repeat */
    bool sought;           /* the next frame shown is the first after a seek */
    int width, height;     /* of the frames shown */
    bool any;              /* a frame was decoded */
    /* Sound, when the file has it and the libraries can give it. */
    int sound;
    void *sound_codec, *resampler;
    MainUIAVFrame *sound_frame;
    MainUIAVRational sound_base;
    int in_rate, in_channels, in_format;
    int64_t in_layout;
    int64_t next_sample; /* where sound without a timestamp goes */
    Uint8 *pcm;
    size_t pcm_size;
    /* Pictures read and not decoded yet: copies, oldest first. */
    MainUIAVPacket queue[QUEUE_PACKETS];
    int queue_head, queued;
    size_t queue_bytes;
    bool ended; /* the file is read to its end */
} Decoder;

/* A timestamp in milliseconds from the start of the file, or -1. */
static int64_t file_ms(Decoder *d, int64_t stamp, MainUIAVRational base)
{
    if (stamp == MAINUI_AV_NOPTS_VALUE || base.den <= 0 || base.num <= 0) {
        return -1;
    }
    int64_t ms = stamp * 1000 * base.num / base.den;
    if (!d->zero_set) {
        d->zero_ms = ms;
        d->zero_set = true;
    }
    ms -= d->zero_ms;
    return ms >= 0 && ms < 24 * 3600 * 1000 ? ms : -1;
}

static bool show(MainUIVideo *video, Decoder *d)
{
    MainUIFFmpeg *ff = &video->ff;
    MainUIAVFrame *frame = d->frame;
    if (frame->width <= 0 || frame->height <= 0 || frame->width > MAINUI_VIDEO_MAX_WIDTH ||
        frame->height > MAINUI_VIDEO_MAX_HEIGHT) {
        fail(video, "Video too large to play", "%dx%d is over %dx%d", frame->width, frame->height,
             MAINUI_VIDEO_MAX_WIDTH, MAINUI_VIDEO_MAX_HEIGHT);
        return false;
    }
    int64_t ms = file_ms(d, ff->av_frame_get_best_effort_timestamp(frame), d->base);
    Uint32 due = ms >= 0 ? d->offset + (Uint32)ms : d->last + d->frame_ms;
    /* Times only go forward, except to the keyframe a seek went to, shown
     * at its own time in the file. */
    if (d->sought) {
        d->sought = false;
    }
    else if (d->any && due > d->last && due - d->last < 1000) {
        d->frame_ms = due - d->last;
    }
    else if (d->any && due <= d->last) {
        due = d->last + d->frame_ms;
    }
    bool first = !d->any;
    d->any = true;
    d->last = due;
    if (first) {
        /* Fit the screen, keeping the shape. */
        int max_w = video->max_width, max_h = video->max_height;
        if ((int64_t)frame->width * max_h > (int64_t)frame->height * max_w) {
            d->width = max_w;
            d->height = (int)((int64_t)frame->height * max_w / frame->width);
        }
        else {
            d->height = max_h;
            d->width = (int)((int64_t)frame->width * max_h / frame->height);
        }
        d->width = d->width < 1 ? 1 : d->width;
        d->height = d->height < 1 ? 1 : d->height;
        for (int i = 0; i < SLOTS; i++) {
            SDL_Surface *surface =
                SDL_CreateRGBSurface(SDL_SWSURFACE, d->width, d->height, 32, video->rmask,
                                     video->gmask, video->bmask, 0);
            SDL_mutexP(video->lock);
            video->frames[i] = surface;
            SDL_mutexV(video->lock);
            if (!surface) {
                fail(video, "Not enough memory for this video", "no %dx%d frame", d->width,
                     d->height);
                return false;
            }
        }
    }
    else if (late(video, due, d->frame_ms)) {
        return true;
    }
    d->scaler = ff->sws_getCachedContext(d->scaler, frame->width, frame->height, frame->format,
                                         d->width, d->height, video->pixel_format,
                                         MAINUI_SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (!d->scaler) {
        fail(video, "Cannot play this video", "no conversion from pixel format %d", frame->format);
        return false;
    }
    int slot = free_slot(video);
    if (slot < 0) {
        return !atomic_load(&video->stop);
    }
    SDL_Surface *target = video->frames[slot];
    uint8_t *planes[4] = {target->pixels, NULL, NULL, NULL};
    int pitches[4] = {target->pitch, 0, 0, 0};
    ff->sws_scale(d->scaler, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height,
                  planes, pitches);
    SDL_mutexP(video->lock);
    video->state[slot] = SLOT_READY;
    video->due[slot] = due;
    video->at[slot] = due - d->offset;
    SDL_mutexV(video->lock);
    wake_ui();
    return true;
}

/* Decode one packet; a NULL-data packet drains the decoder's delayed frames.
 * Returns false to stop, with *got whether a frame came out. */
static bool too_large(MainUIVideo *video);

static bool decode(MainUIVideo *video, Decoder *d, MainUIAVPacket *packet, bool *got)
{
    int picture = 0;
    *got = false;
    int used = video->ff.avcodec_decode_video2(d->codec, d->frame, &picture, packet);
    if (too_large(video)) {
        return false;
    }
    if (used < 0 || !picture) {
        return true;
    }
    *got = true;
    return show(video, d);
}

/* Keep sound starting at video sample start. A jump (a seek, or a gap in
 * the file) starts over from it; beyond the ring the oldest is dropped. */
static void keep_sound(MainUIVideo *video, const Uint8 *pcm, size_t bytes, int64_t start)
{
    SDL_mutexP(video->lock);
    int64_t end = video->ring_start + (int64_t)(video->ring_fill / 4);
    int64_t jump = start - end;
    if (!video->ring_fill || jump > video->rate / 10 || jump < -video->rate / 10) {
        video->ring_head = video->ring_fill = 0;
        video->ring_start = start;
    }
    if (bytes > video->ring_size) {
        pcm += bytes - video->ring_size;
        video->ring_start += (int64_t)((bytes - video->ring_size) / 4);
        bytes = video->ring_size;
    }
    size_t over = video->ring_fill + bytes > video->ring_size
                      ? video->ring_fill + bytes - video->ring_size
                      : 0;
    video->ring_head = (video->ring_head + over) % video->ring_size;
    video->ring_fill -= over;
    video->ring_start += (int64_t)(over / 4);
    size_t tail = (video->ring_head + video->ring_fill) % video->ring_size;
    size_t first = bytes < video->ring_size - tail ? bytes : video->ring_size - tail;
    memcpy(video->ring + tail, pcm, first);
    memcpy(video->ring, pcm + first, bytes - first);
    video->ring_fill += bytes;
    SDL_mutexV(video->lock);
}

/* Stop decoding sound for the rest of the video; the picture goes on. */
static void no_sound(MainUIVideo *video, Decoder *d, const char *why)
{
    fprintf(stderr, "[video] %.300s: no sound: %s\n", video->path, why);
    d->sound = -1;
}

static void convert_sound(MainUIVideo *video, Decoder *d)
{
    MainUIFFmpeg *ff = &video->ff;
    MainUIAVFrame *frame = d->sound_frame;
    int rate = ff->av_frame_get_sample_rate(frame);
    int channels = ff->av_frame_get_channels(frame);
    int64_t layout = ff->av_frame_get_channel_layout(frame);
    if (!layout) {
        layout = ff->av_get_default_channel_layout(channels);
    }
    if (frame->nb_samples <= 0) {
        return;
    }
    /* Ordinary sound only: a frame converts to at most a second here, so
     * the conversion's buffer stays small (192 KiB at 48 kHz). */
    int64_t room = (int64_t)frame->nb_samples * video->rate / (rate > 0 ? rate : 1) + 256;
    if (rate < 1000 || rate > 384000 || channels < 1 || channels > 8 || room > video->rate + 256) {
        no_sound(video, d, "its sound is not of a kind this plays");
        return;
    }
    if (!d->resampler || rate != d->in_rate || channels != d->in_channels ||
        layout != d->in_layout || frame->format != d->in_format) {
        if (d->resampler) {
            ff->swr_free(&d->resampler);
        }
        d->resampler =
            ff->swr_alloc_set_opts(NULL, MAINUI_AV_CH_LAYOUT_STEREO, MAINUI_AV_SAMPLE_FMT_S16,
                                   video->rate, layout, frame->format, rate, 0, NULL);
        if (!d->resampler || ff->swr_init(d->resampler) < 0) {
            no_sound(video, d, "no conversion for its sound");
            return;
        }
        d->in_rate = rate;
        d->in_channels = channels;
        d->in_layout = layout;
        d->in_format = frame->format;
    }
    if ((size_t)room * 4 > d->pcm_size) {
        Uint8 *grown = realloc(d->pcm, (size_t)room * 4);
        if (!grown) {
            no_sound(video, d, "not enough memory");
            return;
        }
        d->pcm = grown;
        d->pcm_size = (size_t)room * 4;
    }
    uint8_t *out[1] = {d->pcm};
    int made = ff->swr_convert(d->resampler, out, (int)room, (const uint8_t **)frame->extended_data,
                               frame->nb_samples);
    if (made <= 0) {
        return;
    }
    int64_t ms = file_ms(d, ff->av_frame_get_best_effort_timestamp(frame), d->sound_base);
    int64_t start = ms >= 0 ? ((int64_t)d->offset + ms) * video->rate / 1000 : d->next_sample;
    d->next_sample = start + made;
    keep_sound(video, d->pcm, (size_t)made * 4, start);
}

static void decode_sound(MainUIVideo *video, Decoder *d, const MainUIAVPacket *packet)
{
    MainUIAVPacket rest = *packet;
    while (rest.size > 0 && d->sound >= 0 && !atomic_load(&video->stop)) {
        int got = 0;
        int used = video->ff.avcodec_decode_audio4(d->sound_codec, d->sound_frame, &got, &rest);
        if (used < 0) {
            return;
        }
        if (got) {
            convert_sound(video, d);
        }
        if (!used && !got) {
            return;
        }
        rest.data += used;
        rest.size -= used;
    }
}

static void open_sound(MainUIVideo *video, Decoder *d)
{
    MainUIFFmpeg *ff = &video->ff;
    void *decoder = NULL;
    d->sound = -1;
    if (!video->sound_api) {
        return;
    }
    int index =
        ff->av_find_best_stream(d->input, MAINUI_AVMEDIA_TYPE_AUDIO, -1, d->stream, &decoder, 0);
    if (index < 0 || !decoder || (unsigned)index >= d->input->nb_streams) {
        return;
    }
    MainUIAVStream *stream = d->input->streams[index];
    if (ff->avcodec_open2(stream->codec, decoder, NULL) < 0) {
        fprintf(stderr, "[video] %.300s: no sound: its decoder did not open\n", video->path);
        return;
    }
    d->sound_codec = stream->codec;
    d->sound_base = stream->time_base;
    d->sound_frame = ff->av_frame_alloc();
    d->sound = d->sound_frame ? index : -1;
}

/* Go to the keyframe at or before a position in the file, in milliseconds,
 * or with forward, at or after it: for a seek or a repeat. */
static bool go_to(MainUIVideo *video, Decoder *d, int64_t target, bool forward)
{
    MainUIFFmpeg *ff = &video->ff;
    int64_t stamp = 0;
    if (target > 0 || d->zero_ms) {
        int64_t ms = d->zero_ms + (target > 0 ? target : 0);
        stamp = d->base.num > 0 ? ms * d->base.den / ((int64_t)1000 * d->base.num) : 0;
    }
    if (ff->av_seek_frame(d->input, d->stream, stamp, forward ? 0 : MAINUI_AVSEEK_FLAG_BACKWARD) <
        0) {
        return false;
    }
    ff->avcodec_flush_buffers(d->codec);
    if (d->sound >= 0) {
        ff->avcodec_flush_buffers(d->sound_codec);
    }
    return true;
}

/* A seek asked for by the UI: from the frame shown, to a keyframe, so the
 * picture shows at once, nothing decoded to reach an exact frame. Forward
 * it is the first keyframe at or after the target, back the last one at or
 * before it, so a jump never goes the other way; with no keyframe after,
 * the video starts again. Frames and sound decoded for the old position are
 * let go. */
static void unqueue_all(Decoder *d);

static void seek(MainUIVideo *video, Decoder *d)
{
    SDL_mutexP(video->lock);
    bool asked = video->seek_pending;
    int64_t delta = video->seek_ms;
    /* A second seek before the new frame appears must use the committed
     * destination, not the still-displayed frame from the previous position. */
    int64_t origin = video->seek_anchor_valid ? video->seek_anchor_ms : 0;
    if (!video->seek_anchor_valid) {
        for (int i = 0; i < SLOTS; i++) {
            if (video->state[i] == SLOT_SHOWN) {
                origin = video->at[i];
                break;
            }
        }
    }
    if (asked) {
        video->seek_pending = false;
        video->seek_ms = 0;
    }
    SDL_mutexV(video->lock);
    if (!asked) {
        return;
    }
    int64_t target = origin + delta;
    target = target < 0 ? 0 : target;
    /* Nothing in the decoder or presentation queues changes before FFmpeg
     * accepts the seek. A failed seek can continue playing the old stream. */
    bool forward = delta > 0;
    if (!go_to(video, d, target, forward)) {
        if (!forward || !go_to(video, d, 0, false)) {
            fprintf(stderr, "[video] %.300s: cannot seek\n", video->path);
            return;
        }
        target = 0;
    }
    unqueue_all(d);
    d->ended = false;
    SDL_mutexP(video->lock);
    for (int i = 0; i < SLOTS; i++) {
        if (video->state[i] == SLOT_READY) {
            video->state[i] = SLOT_FREE;
        }
    }
    video->clock_set = false;
    video->sound_running = false;
    video->sound_read = false;
    video->ring_head = video->ring_fill = 0;
    video->seek_anchor_valid = true;
    video->seek_anchor_ms = target;
    SDL_CondSignal(video->changed);
    SDL_mutexV(video->lock);
    d->sought = true;
    /* The frame before time zero cannot have a negative unsigned stamp. */
    uint64_t first = (uint64_t)d->offset + (uint64_t)target;
    d->last = (Uint32)(first >= d->frame_ms ? first - d->frame_ms : 0);
    d->next_sample = (int64_t)first * video->rate / 1000;
}

/* A copy of a picture's packet, with its own data, to decode later. */
static bool enqueue(Decoder *d, const MainUIAVPacket *packet)
{
    if (d->queued >= QUEUE_PACKETS || packet->size <= 0 ||
        d->queue_bytes + (size_t)packet->size > QUEUE_BYTES) {
        return false;
    }
    uint8_t *data = calloc(1, (size_t)packet->size + PACKET_PADDING);
    if (!data) {
        return false;
    }
    memcpy(data, packet->data, (size_t)packet->size);
    MainUIAVPacket *copy = &d->queue[(d->queue_head + d->queued) % QUEUE_PACKETS];
    *copy = *packet;
    copy->buf = NULL;
    copy->data = data;
    copy->side_data = NULL;
    copy->side_data_elems = 0;
    copy->destruct = NULL;
    copy->priv = NULL;
    d->queued++;
    d->queue_bytes += (size_t)packet->size;
    return true;
}

static void unqueue(Decoder *d)
{
    MainUIAVPacket *oldest = &d->queue[d->queue_head];
    d->queue_bytes -= (size_t)oldest->size;
    free(oldest->data);
    oldest->data = NULL;
    d->queue_head = (d->queue_head + 1) % QUEUE_PACKETS;
    d->queued--;
}

static void unqueue_all(Decoder *d)
{
    while (d->queued) {
        unqueue(d);
    }
}

static bool seek_asked(MainUIVideo *video)
{
    SDL_mutexP(video->lock);
    bool asked = video->seek_pending;
    SDL_mutexV(video->lock);
    return asked;
}

static bool slot_free(MainUIVideo *video)
{
    SDL_mutexP(video->lock);
    bool free_one = false;
    for (int i = 0; i < SLOTS; i++) {
        free_one |= video->state[i] == SLOT_FREE;
    }
    SDL_mutexV(video->lock);
    return free_one;
}

/* Read on while pictures are short, and while sound is: so sound stored
 * after its pictures still comes before it is needed. */
static bool read_more(MainUIVideo *video, Decoder *d)
{
    if (d->ended || d->queued >= QUEUE_PACKETS || d->queue_bytes >= QUEUE_BYTES) {
        return false;
    }
    if (d->queued < 2) {
        return true;
    }
    if (d->sound < 0) {
        return false;
    }
    SDL_mutexP(video->lock);
    size_t ahead = (size_t)video->rate * 4 * SOUND_AHEAD_MS / 1000;
    bool short_of_sound = video->ring_fill < ahead;
    SDL_mutexV(video->lock);
    return short_of_sound;
}

/* The decoder thread: reads the file ahead into the picture queue and the
 * sound ring, and decodes a picture whenever a frame is free to take it. */
static void play(MainUIVideo *video, Decoder *d)
{
    MainUIFFmpeg *ff = &video->ff;
    int unplayable = 0;
    while (!atomic_load(&video->stop)) {
        /* While the device sleeps nothing is read or decoded. */
        SDL_mutexP(video->lock);
        while (video->held && !atomic_load(&video->stop)) {
            SDL_CondWait(video->changed, video->lock);
        }
        SDL_mutexV(video->lock);
        seek(video, d);
        MainUIAVPacket packet;
        bool got = false, go_on = true;
        if (d->queued && slot_free(video)) {
            go_on = decode(video, d, &d->queue[d->queue_head], &got);
            unqueue(d);
            if (!go_on) {
                return;
            }
            continue;
        }
        if (read_more(video, d)) {
            ff->av_init_packet(&packet);
            packet.data = NULL;
            packet.size = 0;
            if (ff->av_read_frame(d->input, &packet) < 0) {
                d->ended = true;
                SDL_mutexP(video->lock);
                video->sound_read = true;
                SDL_mutexV(video->lock);
                continue;
            }
            if (packet.stream_index == d->stream) {
                /* No room or memory: decode the older pictures first, in
                 * order, waiting for frames, until it fits; with none
                 * older left, decode it now. Never before an older one. */
                bool kept = enqueue(d, &packet);
                while (!kept && go_on && d->queued && !atomic_load(&video->stop) &&
                       !seek_asked(video)) {
                    go_on = decode(video, d, &d->queue[d->queue_head], &got);
                    unqueue(d);
                    kept = go_on && enqueue(d, &packet);
                }
                /* A seek asked for meanwhile drops it with the queue. */
                if (!kept && go_on && !d->queued && !seek_asked(video)) {
                    go_on = decode(video, d, &packet, &got);
                }
            }
            else if (packet.stream_index == d->sound && d->sound >= 0) {
                decode_sound(video, d, &packet);
            }
            ff->av_free_packet(&packet);
            /* Many packets and still no picture: not a video this can show. */
            if (go_on && !d->any && ++unplayable > 300 + QUEUE_PACKETS) {
                fail(video, "Cannot play this video", "no picture in many packets");
                return;
            }
            if (!go_on) {
                return;
            }
            continue;
        }
        if (d->ended && !d->queued) {
            /* The end: the frames the decoder still holds, then from the
             * start. */
            do {
                ff->av_init_packet(&packet);
                packet.data = NULL;
                packet.size = 0;
                go_on = decode(video, d, &packet, &got);
            } while (go_on && got && !atomic_load(&video->stop));
            if (!go_on) {
                return;
            }
            if (!d->any) {
                fail(video, "Cannot play this video", "no picture decoded");
                return;
            }
            if (!go_to(video, d, 0, false)) {
                fprintf(stderr, "[video] %.300s: cannot seek to the start; the last frame stays\n",
                        video->path);
                return;
            }
            d->ended = false;
            SDL_mutexP(video->lock);
            video->sound_read = false;
            SDL_mutexV(video->lock);
            d->sought = false;
            d->offset = d->last + d->frame_ms;
            d->next_sample = (int64_t)d->offset * video->rate / 1000;
            continue;
        }
        /* Nothing to do until a frame is free or sound has been played;
         * stopped, until it goes on (no wakes meanwhile). */
        SDL_mutexP(video->lock);
        if (!atomic_load(&video->stop) && !video->seek_pending) {
            if (frozen(video)) {
                SDL_CondWait(video->changed, video->lock);
            }
            else {
                SDL_CondWaitTimeout(video->changed, video->lock, 20);
            }
        }
        SDL_mutexV(video->lock);
    }
}

/* FFmpeg's reads and its stream probing ask this: closing stops them. */
static int interrupted(void *context)
{
    MainUIVideo *video = context;
    return atomic_load(&video->stop);
}

/* Every picture buffer a video decoder takes, while probing too: one over
 * the limit (the size the stream itself gives, which can differ from what
 * the file states, or change midway) is refused before it is allocated. */
static int picture_buffer(void *context, void *picture, int flags)
{
    MainUIAVCodecContext *codec = context;
    MainUIAVFrame *frame = picture;
    MainUIVideo *video = codec->opaque;
    /* Decoders round the size up to whole 16-pixel blocks. */
    if (frame->width > MAINUI_VIDEO_MAX_WIDTH + 16 ||
        frame->height > MAINUI_VIDEO_MAX_HEIGHT + 16) {
        if (!atomic_exchange(&video->oversized, true)) {
            fprintf(stderr, "[video] %.300s: refused a %dx%d picture\n", video->path, frame->width,
                    frame->height);
        }
        return -1094995529; /* AVERROR_INVALIDDATA */
    }
    return video->ff.avcodec_default_get_buffer2(context, picture, flags);
}

/* Before probing the streams, which can decode them: every video stream it
 * could decode must state its size, within the limit, and gets the picture
 * buffer guard above. Cover art (an attached picture) is left undecoded
 * instead: it is never shown here. A decoder's smaller tables, sized by the
 * stream, are not limited here. */
static bool sizes_allowed(MainUIVideo *video, MainUIAVFormatContext *input)
{
    for (unsigned i = 0; i < input->nb_streams; i++) {
        MainUIAVStream *stream = input->streams[i];
        MainUIAVCodecContext *codec = stream ? stream->codec : NULL;
        if (!codec || codec->codec_type != MAINUI_AVMEDIA_TYPE_VIDEO ||
            codec->codec_id == MAINUI_AV_CODEC_ID_NONE) {
            continue;
        }
        if (stream->disposition & MAINUI_AV_DISPOSITION_ATTACHED_PIC) {
            codec->codec_id = MAINUI_AV_CODEC_ID_NONE;
            continue;
        }
        if (codec->width <= 0 || codec->height <= 0) {
            fail(video, "Cannot play this video", "stream %u states no size", i);
            return false;
        }
        if (codec->width > MAINUI_VIDEO_MAX_WIDTH || codec->height > MAINUI_VIDEO_MAX_HEIGHT) {
            fail(video, "Video too large to play", "stream %u is %dx%d, over %dx%d", i,
                 codec->width, codec->height, MAINUI_VIDEO_MAX_WIDTH, MAINUI_VIDEO_MAX_HEIGHT);
            return false;
        }
        codec->opaque = video;
        codec->get_buffer2 = picture_buffer;
    }
    return true;
}

/* A decoder was refused a picture over the limit: the video stops. */
static bool too_large(MainUIVideo *video)
{
    if (!atomic_load(&video->oversized)) {
        return false;
    }
    fail(video, "Video too large to play", "a picture over %dx%d", MAINUI_VIDEO_MAX_WIDTH,
         MAINUI_VIDEO_MAX_HEIGHT);
    return true;
}

static int run(void *context)
{
    MainUIVideo *video = context;
    /* The silent ROM-list decoder can monopolize one core. Linux schedules
     * nice values per thread: favor the UI/marquee's frame deadlines. */
#if defined(__linux__)
    if (video->rate == 0) {
        /* glibc warns of an unused result; failing to lower it is harmless. */
        int niced = nice(5);
        (void)niced;
    }
#endif
    if (!load(video)) {
        return 0;
    }
    MainUIFFmpeg *ff = &video->ff;
    ff->av_log_set_level(MAINUI_AV_LOG_QUIET);
    ff->av_register_all();
    /* With its picture queue, too large for the thread's stack. */
    Decoder *dp = calloc(1, sizeof *dp);
    if (!dp) {
        fail(video, "Not enough memory for this video", "no decoder");
        return 0;
    }
    dp->stream = -1;
    dp->sound = -1;
    dp->frame_ms = 40;
    void *decoder = NULL;
    /* Each step can take a while on a slow card; closing stops FFmpeg's
     * reads and probing through the interrupt, and the steps between. */
    dp->input = ff->avformat_alloc_context();
    if (dp->input) {
        dp->input->interrupt_callback = (MainUIAVIOInterruptCB){interrupted, video};
    }
    if (!dp->input) {
        fail(video, "Not enough memory for this video", "no format context");
    }
    else if (ff->avformat_open_input(&dp->input, video->path, NULL, NULL) < 0) {
        dp->input = NULL; /* freed by FFmpeg */
        if (!atomic_load(&video->stop)) {
            fail(video, "Cannot play this video", "cannot open it");
        }
    }
    else if (atomic_load(&video->stop) || !sizes_allowed(video, dp->input)) {
        /* Closing, or refused: nothing more. */
    }
    else if (ff->avformat_find_stream_info(dp->input, NULL) < 0 || atomic_load(&video->stop)) {
        if (!atomic_load(&video->stop)) {
            fail(video, "Cannot play this video", "no stream information");
        }
    }
    else if (too_large(video)) {
        /* Refused while probing. */
    }
    else if ((dp->stream = ff->av_find_best_stream(dp->input, MAINUI_AVMEDIA_TYPE_VIDEO, -1, -1,
                                                   &decoder, 0)) < 0 ||
             !decoder || (unsigned)dp->stream >= dp->input->nb_streams) {
        dp->stream = -1;
        fail(video, "Cannot play this video", "no video stream it can decode");
    }
    else {
        MainUIAVStream *stream = dp->input->streams[dp->stream];
        dp->base = stream->time_base;
        int width = stream->codec->width, height = stream->codec->height;
        /* Again as probed, before this decoder opens; decoded frames, which
         * could change size midway, are checked too. */
        if (width > MAINUI_VIDEO_MAX_WIDTH || height > MAINUI_VIDEO_MAX_HEIGHT) {
            fail(video, "Video too large to play", "%dx%d is over %dx%d", width, height,
                 MAINUI_VIDEO_MAX_WIDTH, MAINUI_VIDEO_MAX_HEIGHT);
        }
        else if (ff->avcodec_open2(stream->codec, decoder, NULL) < 0) {
            fail(video, "Cannot play this video", "the decoder did not open");
        }
        else {
            dp->codec = stream->codec;
            dp->frame = ff->av_frame_alloc();
            if (dp->frame) {
                open_sound(video, dp);
                play(video, dp);
            }
            else {
                fail(video, "Not enough memory for this video", "no frame");
            }
        }
    }
    if (dp->sound_frame) {
        ff->av_frame_free(&dp->sound_frame);
    }
    if (dp->sound_codec) {
        ff->avcodec_close(dp->sound_codec);
    }
    if (dp->resampler) {
        ff->swr_free(&dp->resampler);
    }
    free(dp->pcm);
    unqueue_all(dp);
    if (dp->frame) {
        ff->av_frame_free(&dp->frame);
    }
    if (dp->codec) {
        ff->avcodec_close(dp->codec);
    }
    if (dp->input) {
        ff->avformat_close_input(&dp->input);
    }
    if (dp->scaler) {
        ff->sws_freeContext(dp->scaler);
    }
    free(dp);
    return 0;
}

static int pixel_format(const SDL_PixelFormat *format)
{
    if (SDL_BYTEORDER != SDL_LIL_ENDIAN || format->BitsPerPixel != 32) {
        return -1;
    }
    /* Little-endian bytes in memory, lowest first. */
    if (format->Rmask == 0x00ff0000 && format->Gmask == 0x0000ff00 && format->Bmask == 0x000000ff) {
        return MAINUI_AV_PIX_FMT_BGRA;
    }
    if (format->Rmask == 0x000000ff && format->Gmask == 0x0000ff00 && format->Bmask == 0x00ff0000) {
        return MAINUI_AV_PIX_FMT_RGBA;
    }
    if (format->Rmask == 0xff000000 && format->Gmask == 0x00ff0000 && format->Bmask == 0x0000ff00) {
        return MAINUI_AV_PIX_FMT_ABGR;
    }
    if (format->Rmask == 0x0000ff00 && format->Gmask == 0x00ff0000 && format->Bmask == 0xff000000) {
        return MAINUI_AV_PIX_FMT_ARGB;
    }
    return -1;
}

MainUIVideo *mainui_video_open(const char *path, const char *library_dir, const SDL_Surface *screen,
                               int max_width, int max_height, int sound_rate)
{
    if (!path || !screen || max_width < 1 || max_height < 1 ||
        strlen(path) >= sizeof((MainUIVideo *)0)->path ||
        (library_dir && strlen(library_dir) >= sizeof((MainUIVideo *)0)->library_dir)) {
        return NULL;
    }
    MainUIVideo *video = calloc(1, sizeof *video);
    if (!video) {
        return NULL;
    }
    strcpy(video->path, path);
    strcpy(video->library_dir, library_dir ? library_dir : "");
    video->rmask = screen->format->Rmask;
    video->gmask = screen->format->Gmask;
    video->bmask = screen->format->Bmask;
    video->pixel_format = pixel_format(screen->format);
    video->max_width = max_width;
    video->max_height = max_height;
    atomic_init(&video->stop, false);
    atomic_init(&video->oversized, false);
    video->lock = SDL_CreateMutex();
    video->changed = SDL_CreateCond();
    if (!video->lock || !video->changed) {
        mainui_video_close(video);
        return NULL;
    }
    /* Sound is optional: without memory for it the video is silent. */
    if (sound_rate >= 8000 && sound_rate <= 192000) {
        video->ring_size = (size_t)sound_rate * 4 * RING_SECONDS;
        video->ring = malloc(video->ring_size);
        video->rate = video->ring ? sound_rate : 0;
    }
    if (video->pixel_format < 0) {
        fail(video, "Cannot play video on this screen", "unsupported screen pixel format");
        return video;
    }
    video->thread = SDL_CreateThread(run, video);
    if (!video->thread) {
        mainui_video_close(video);
        return NULL;
    }
    return video;
}

SDL_Surface *mainui_video_frame(MainUIVideo *video, Uint32 now)
{
    if (!video) {
        return NULL;
    }
    SDL_mutexP(video->lock);
    Uint32 at = moment(video, now);
    /* While sound plays, the picture follows it: the sample playing now is
     * about where the mixer's last buffer began, a buffer before it is
     * heard, plus the time since. */
    if (video->sound_running && video->clock_set && !frozen(video) && now - video->heard_at < 500) {
        int64_t playing = video->heard_sample * 1000 / video->rate +
                          (Sint32)(now - video->heard_at) -
                          (int64_t)video->heard_samples * 1000 / video->rate;
        int64_t drift = (int64_t)(Sint32)(at - video->clock) - playing;
        /* Not before the first buffer is heard, and never past now. */
        if (playing >= 0 && (drift > FOLLOW_MS || drift < -FOLLOW_MS)) {
            video->clock += (Uint32)drift;
            if ((Sint32)(at - video->clock) < 0) {
                video->clock = at;
            }
        }
    }
    int chosen = -1;
    for (int i = 0; i < SLOTS; i++) {
        if (video->state[i] != SLOT_READY) {
            continue;
        }
        /* The first frame, also after a seek, starts the clock; then the
         * newest one due. */
        if (!video->clock_set) {
            if (chosen < 0 || video->due[i] < video->due[chosen]) {
                chosen = i;
            }
        }
        else if ((Sint32)(at - (video->clock + video->due[i])) >= 0 &&
                 (chosen < 0 || video->due[i] > video->due[chosen])) {
            chosen = i;
        }
    }
    if (chosen >= 0) {
        if (!video->clock_set) {
            video->clock = at - video->due[chosen];
            video->clock_set = true;
        }
        for (int i = 0; i < SLOTS; i++) {
            if (i != chosen &&
                (video->state[i] == SLOT_SHOWN ||
                 (video->state[i] == SLOT_READY && video->due[i] < video->due[chosen]))) {
                video->state[i] = SLOT_FREE;
            }
        }
        video->state[chosen] = SLOT_SHOWN;
        /* New picture is authoritative again for subsequent user seeks. */
        video->seek_anchor_valid = false;
        SDL_CondSignal(video->changed);
    }
    SDL_Surface *shown = NULL;
    for (int i = 0; i < SLOTS; i++) {
        if (video->state[i] == SLOT_SHOWN) {
            shown = video->frames[i];
        }
    }
    SDL_mutexV(video->lock);
    return shown;
}

const char *mainui_video_error(MainUIVideo *video)
{
    if (!video) {
        return "";
    }
    SDL_mutexP(video->lock);
    /* Written once, under the lock, and never again after: once seen set it
     * can be read after unlocking. Unset, it may be written meanwhile, so
     * "" is returned rather than the buffer. */
    const char *error = *video->error ? video->error : "";
    SDL_mutexV(video->lock);
    return error;
}

int mainui_video_due(MainUIVideo *video, Uint32 now)
{
    if (!video) {
        return -1;
    }
    int wait = -1;
    SDL_mutexP(video->lock);
    for (int i = 0; i < SLOTS && !*video->error; i++) {
        if (video->state[i] != SLOT_READY || (frozen(video) && video->clock_set)) {
            continue;
        }
        Sint32 until = video->clock_set ? (Sint32)(video->clock + video->due[i] - now) : 0;
        int ms = until < 0 ? 0 : (int)until;
        if (wait < 0 || ms < wait) {
            wait = ms;
        }
    }
    SDL_mutexV(video->lock);
    return wait;
}

void mainui_video_wait_first(MainUIVideo *video, Uint32 timeout_ms)
{
    Uint32 start = SDL_GetTicks();
    while (video && SDL_GetTicks() - start < timeout_ms) {
        SDL_mutexP(video->lock);
        /* A frame to show: the shown one, or after a seek the next one. */
        bool settled = *video->error || !video->thread;
        for (int i = 0; i < SLOTS; i++) {
            settled |= !video->seek_pending && (video->clock_set ? video->state[i] == SLOT_SHOWN
                                                                 : video->state[i] == SLOT_READY);
        }
        SDL_mutexV(video->lock);
        if (settled) {
            return;
        }
        SDL_Delay(5);
    }
}

void mainui_video_pause(MainUIVideo *video, bool paused, Uint32 now)
{
    if (!video) {
        return;
    }
    SDL_mutexP(video->lock);
    set_frozen(video, paused, video->held, now);
    SDL_mutexV(video->lock);
}

void mainui_video_hold(MainUIVideo *video, bool held, Uint32 now)
{
    if (!video) {
        return;
    }
    SDL_mutexP(video->lock);
    set_frozen(video, video->paused, held, now);
    SDL_mutexV(video->lock);
}

bool mainui_video_paused(MainUIVideo *video)
{
    if (!video) {
        return false;
    }
    SDL_mutexP(video->lock);
    bool paused = video->paused;
    SDL_mutexV(video->lock);
    return paused;
}

void mainui_video_seek(MainUIVideo *video, int delta_ms)
{
    if (!video) {
        return;
    }
    SDL_mutexP(video->lock);
    /* From the frame shown, or the start before one shows; seeks asked for
     * before one took effect add up. */
    if (!*video->error) {
        video->seek_ms = (video->seek_pending ? video->seek_ms : 0) + delta_ms;
        video->seek_pending = true;
        SDL_CondSignal(video->changed);
    }
    SDL_mutexV(video->lock);
}

/* Drop held sound from the oldest end. Under lock. */
static void drop_sound(MainUIVideo *video, int64_t samples)
{
    size_t bytes = (size_t)samples * 4;
    bytes = bytes < video->ring_fill ? bytes : video->ring_fill;
    video->ring_head = (video->ring_head + bytes) % video->ring_size;
    video->ring_fill -= bytes;
    video->ring_start += (int64_t)(bytes / 4);
}

void mainui_video_sound(void *context, Uint8 *stream, int length)
{
    MainUIVideo *video = context;
    memset(stream, 0, (size_t)length);
    SDL_mutexP(video->lock);
    if (!video->ring_fill || !video->clock_set || frozen(video) || *video->error) {
        video->sound_running = false;
        SDL_mutexV(video->lock);
        return;
    }
    /* The sample the picture is at now, by the clock frames are shown by. */
    Uint32 ticks = SDL_GetTicks();
    /* Signed: the picture can be just before its time 0 after following. */
    int64_t now = (int64_t)(Sint32)(ticks - video->clock) * video->rate / 1000;
    int64_t behind = now - video->ring_start;
    if (!video->sound_running) {
        /* Starting, after a pause, a seek or running dry: begin where the
         * picture is, waiting if the sound held is still ahead of it. */
        if (behind > 0) {
            drop_sound(video, behind);
        }
        size_t lead = (size_t)video->rate * 4 * LEAD_MS / 1000;
        if (video->ring_start > now + video->rate / 100 || !video->ring_fill ||
            (video->ring_fill < lead && !video->sound_read)) {
            SDL_mutexV(video->lock);
            return;
        }
        video->sound_running = true;
    }
    else if (behind > (int64_t)video->rate * RESYNC_MS / 1000) {
        /* Far behind (the decoder fell behind): catch up. */
        drop_sound(video, behind);
    }
    /* Then it plays on, buffer after buffer, and the picture follows it. */
    size_t bytes = (size_t)length & ~(size_t)3;
    bytes = bytes < video->ring_fill ? bytes : video->ring_fill;
    size_t first =
        bytes < video->ring_size - video->ring_head ? bytes : video->ring_size - video->ring_head;
    memcpy(stream, video->ring + video->ring_head, first);
    memcpy(stream + first, video->ring, bytes - first);
    video->heard_at = ticks;
    video->heard_sample = video->ring_start;
    video->heard_samples = length / 4;
    drop_sound(video, (int64_t)(bytes / 4));
    /* Ran dry: start again aligned when more comes. */
    if (bytes < ((size_t)length & ~(size_t)3)) {
        video->sound_running = false;
    }
    SDL_mutexV(video->lock);
}

void mainui_video_close(MainUIVideo *video)
{
    if (!video) {
        return;
    }
    atomic_store(&video->stop, true);
    if (video->thread) {
        SDL_mutexP(video->lock);
        SDL_CondBroadcast(video->changed);
        SDL_mutexV(video->lock);
        SDL_WaitThread(video->thread, NULL);
    }
    for (int i = 0; i < SLOTS; i++) {
        if (video->frames[i]) {
            SDL_FreeSurface(video->frames[i]);
        }
    }
    for (int i = LIBRARIES - 1; i >= 0; i--) {
        if (video->handles[i]) {
            dlclose(video->handles[i]);
        }
    }
    free(video->ring);
    if (video->changed) {
        SDL_DestroyCond(video->changed);
    }
    if (video->lock) {
        SDL_DestroyMutex(video->lock);
    }
    free(video);
}
