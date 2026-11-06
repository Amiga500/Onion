/* SPDX-License-Identifier: GPL-3.0-only
 * Video playback against the stub FFmpeg (tests/ffmpeg_stub.c), which
 * case_media_gallery.py puts on LD_LIBRARY_PATH under Onion's sonames.
 *
 *   fixture-video DIR LIBS play      frames in order, on time, repeating
 *   fixture-video DIR LIBS errors    unusable files and sizes show an error
 *   fixture-video DIR LIBS missing   no libraries, or one without a function
 *   fixture-video DIR LIBS controls  pause and seek
 *   fixture-video DIR LIBS sound     sound in step with the picture, also
 *                                    as the file's first stream
 *   fixture-video DIR LIBS silent F  video F, whose sound cannot play
 *   fixture-video DIR LIBS cancel    closing while FFmpeg opens, probes or
 *                                    reads stops it at once
 *
 * DIR holds the stub video files; LIBS is the folder the libraries are
 * looked for in when not found by name, as Onion's parasyte folder, or "-"
 * for none. */
#include "platform/video.h"
#include "support.h"
#include <stdlib.h>
#include <string.h>

static SDL_Surface *screen(Uint32 r, Uint32 g, Uint32 b)
{
    SDL_Surface *surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, r, g, b, 0);
    assert(surface);
    return surface;
}

/* The stub frame index a frame shows, from its first pixel's red. */
static int shown_index(SDL_Surface *frame)
{
    Uint8 r, g, b;
    SDL_GetRGB(*(Uint32 *)frame->pixels, frame->format, &r, &g, &b);
    assert(b == 100 && r + g == 255);
    return r;
}

static const char *libraries;
static int rate; /* sound, or 0 */

static MainUIVideo *open_video(const char *dir, const char *name, SDL_Surface *target)
{
    char path[4096];
    TEST_PATH(path, "%s/%s", dir, name);
    MainUIVideo *video = mainui_video_open(path, libraries, target, 640, 480, rate);
    assert(video);
    return video;
}

static void play(const char *dir)
{
    /* 320x240 at 20 fps, 6 frames: shown 640x480, one every 50 ms. */
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    MainUIVideo *video = open_video(dir, "ok.mp4", target);
    assert(!mainui_video_frame(video, SDL_GetTicks()));
    mainui_video_wait_first(video, 5000);
    SDL_Surface *frame = mainui_video_frame(video, SDL_GetTicks());
    assert(frame && frame->w == 640 && frame->h == 480 && !*mainui_video_error(video));
    assert(shown_index(frame) == 0);
    Uint32 start = SDL_GetTicks();
    int expected = 0, seen = 0, repeats = 0;
    while (seen < 15) {
        int due = mainui_video_due(video, SDL_GetTicks());
        if (due < 0) {
            SDL_Delay(2);
            assert(SDL_GetTicks() - start < 5000);
            continue;
        }
        SDL_Delay((Uint32)due);
        frame = mainui_video_frame(video, SDL_GetTicks());
        int index = shown_index(frame);
        if (index == expected) {
            continue;
        }
        /* The next frame, or a skipped one on a slow host; the sixth is
         * followed by the first again. */
        assert(index > expected || index == 0);
        repeats += index < expected;
        expected = index;
        seen++;
    }
    /* About 50 ms a frame: 15 frames in 0.75 s, give or take a slow host. */
    Uint32 took = SDL_GetTicks() - start;
    assert(repeats >= 1 && took >= 600 && took < 3000);
    mainui_video_close(video);
    /* Other screen layouts get their own byte order. */
    const Uint32 masks[][3] = {
        {0xff, 0xff00, 0xff0000}, {0xff000000, 0xff0000, 0xff00}, {0xff00, 0xff0000, 0xff000000}};
    for (int i = 0; i < 3; i++) {
        SDL_Surface *other = screen(masks[i][0], masks[i][1], masks[i][2]);
        video = open_video(dir, "ok.mp4", other);
        mainui_video_wait_first(video, 5000);
        frame = mainui_video_frame(video, SDL_GetTicks());
        assert(frame && shown_index(frame) == 0);
        mainui_video_close(video);
        SDL_FreeSurface(other);
    }

    /* A wide video fits the width, a tall one the height. */
    const struct {
        const char *name;
        int w, h;
    } shapes[] = {{"wide.mp4", 640, 160}, {"tall.mp4", 240, 480}};

    for (int i = 0; i < 2; i++) {
        video = open_video(dir, shapes[i].name, target);
        mainui_video_wait_first(video, 5000);
        frame = mainui_video_frame(video, SDL_GetTicks());
        assert(frame && frame->w == shapes[i].w && frame->h == shapes[i].h);
        mainui_video_close(video);
    }
    /* Without timestamps, frames still follow each other. */
    video = open_video(dir, "nopts.mp4", target);
    mainui_video_wait_first(video, 5000);
    assert(mainui_video_frame(video, SDL_GetTicks()));
    SDL_Delay(200);
    assert(shown_index(mainui_video_frame(video, SDL_GetTicks())) > 0);
    mainui_video_close(video);
    /* A video that cannot go back to its start keeps its last frame. */
    video = open_video(dir, "noseek.mp4", target);
    for (int i = 0; i < 25; i++) {
        frame = mainui_video_frame(video, SDL_GetTicks());
        SDL_Delay(20);
    }
    assert(frame && shown_index(frame) == 2 && !*mainui_video_error(video));
    assert(mainui_video_due(video, SDL_GetTicks()) == -1);
    mainui_video_close(video);
    /* B stops at once, also while decoding slowly and with frames waiting
     * to be shown, and closing twice in a row is harmless. */
    for (int i = 0; i < 8; i++) {
        video = open_video(dir, "slow.mp4", target);
        SDL_Delay((Uint32)(i * 15));
        Uint32 before = SDL_GetTicks();
        mainui_video_close(video);
        assert(SDL_GetTicks() - before < 200);
    }
    mainui_video_close(NULL);
    SDL_FreeSurface(target);
    /* Cover art in the file is not decoded (the stub reports it if it is). */
    target = screen(0xff0000, 0xff00, 0xff);
    video = open_video(dir, "cover.mp4", target);
    mainui_video_wait_first(video, 5000);
    assert(mainui_video_frame(video, SDL_GetTicks()) && !*mainui_video_error(video));
    mainui_video_close(video);
    SDL_FreeSurface(target);
    puts("video frames play in order, on time and repeating; closing stops at once");
}

/* The frame shown after following the video for ms, as the UI does. */
static int follow(MainUIVideo *video, Uint32 ms)
{
    Uint32 start = SDL_GetTicks();
    SDL_Surface *frame = mainui_video_frame(video, start);
    while (SDL_GetTicks() - start < ms) {
        SDL_Delay(5);
        frame = mainui_video_frame(video, SDL_GetTicks());
    }
    assert(frame);
    return shown_index(frame);
}

/* 20 frames a second: frame 20 * t at t seconds. */
static void controls(const char *dir)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    MainUIVideo *video = open_video(dir, "long.mp4", target);
    mainui_video_wait_first(video, 5000);
    int at = follow(video, 150);
    assert(at >= 1 && at <= 6 && !mainui_video_paused(video));
    /* Paused, the frame stays and nothing is due; it goes on from there. */
    mainui_video_pause(video, true, SDL_GetTicks());
    at = follow(video, 60);
    assert(mainui_video_paused(video) && follow(video, 150) == at);
    assert(mainui_video_due(video, SDL_GetTicks()) == -1);
    mainui_video_pause(video, false, SDL_GetTicks());
    int on = follow(video, 150);
    assert(on > at && on <= at + 5);
    /* 5 s forward is 100 frames, and back. */
    at = follow(video, 0);
    mainui_video_seek(video, 5000);
    int there = follow(video, 150);
    assert(there >= at + 98 && there <= at + 106);
    mainui_video_seek(video, -5000);
    int back = follow(video, 150);
    assert(back >= there - 102 && back <= there - 94);
    /* Not before the start. */
    mainui_video_seek(video, -20000);
    assert(follow(video, 100) <= 4);
    /* Seeks asked for together add up. */
    at = follow(video, 0);
    mainui_video_seek(video, 2000);
    mainui_video_seek(video, 2000);
    there = follow(video, 150);
    assert(there >= at + 78 && there <= at + 86);
    /* After the first seek is decoded but before its frame is presented, a
     * second seek must be relative to that committed target, not the old
     * displayed picture. */
    at = follow(video, 50);
    mainui_video_seek(video, 2000);
    /* Leave the UI's displayed frame untouched while the decoder seeks. */
    SDL_Delay(100);
    mainui_video_seek(video, 2000);
    there = follow(video, 150);
    assert(there >= at + 78 && there <= at + 90);
    /* Return near the beginning before the following pause/seek checks. */
    mainui_video_seek(video, -20000);
    assert(follow(video, 100) <= 4);
    /* Paused, a seek shows its frame and stays paused there. */
    mainui_video_pause(video, true, SDL_GetTicks());
    at = follow(video, 50);
    mainui_video_seek(video, 2000);
    there = follow(video, 150);
    assert(there >= at + 38 && there <= at + 42 && follow(video, 200) == there);
    assert(mainui_video_paused(video));
    mainui_video_pause(video, false, SDL_GetTicks());
    /* While the device sleeps it stands still, paused or not, and goes on
     * from there on waking, still paused if it was. */
    at = follow(video, 50);
    mainui_video_hold(video, true, SDL_GetTicks());
    at = follow(video, 60);
    assert(follow(video, 300) == at && mainui_video_due(video, SDL_GetTicks()) == -1);
    assert(!mainui_video_paused(video));
    mainui_video_pause(video, true, SDL_GetTicks());
    mainui_video_hold(video, false, SDL_GetTicks());
    assert(follow(video, 200) == at && mainui_video_paused(video));
    mainui_video_hold(video, true, SDL_GetTicks());
    mainui_video_pause(video, false, SDL_GetTicks());
    assert(follow(video, 200) == at);
    mainui_video_hold(video, false, SDL_GetTicks());
    on = follow(video, 150);
    assert(on > at && on <= at + 5);
    /* Past the end it starts again. */
    mainui_video_seek(video, 20000);
    assert(follow(video, 250) < 20);
    mainui_video_close(video);
    /* With a keyframe every 50 frames, seeks land on one, and show it at
     * once: forward the first at or after the target, back the last at or
     * before it, so a jump never goes the other way; paused, it stays on
     * the keyframe. With no keyframe after the target it starts again. */
    video = open_video(dir, "gop.mp4", target);
    mainui_video_wait_first(video, 5000);
    at = follow(video, 150);
    mainui_video_seek(video, 5000);
    there = follow(video, 150);
    assert(there >= at + 100 && there <= at + 100 + 49 + 6 && there % 50 <= 6);
    mainui_video_seek(video, -2000);
    back = follow(video, 150);
    assert(back <= there - 40 + 6 && back >= there - 40 - 49 && back % 50 <= 6);
    mainui_video_pause(video, true, SDL_GetTicks());
    at = follow(video, 50);
    mainui_video_seek(video, 2000);
    there = follow(video, 150);
    assert(there % 50 == 0 && there >= at + 40 && there <= at + 89 && follow(video, 200) == there);
    mainui_video_pause(video, false, SDL_GetTicks());
    mainui_video_seek(video, 20000);
    assert(follow(video, 150) <= 6);
    mainui_video_close(video);
    SDL_FreeSurface(target);
    puts("videos pause, go on, stand still while asleep, and seek back and forward from the "
         "frame shown, landing on keyframes");
}

/* The sound the mixer would get now: the value of its samples (the stub's
 * chunk plus 1), 0 for silence. */
static int heard(MainUIVideo *video)
{
    Sint16 samples[1024 * 2];
    mainui_video_sound(video, (Uint8 *)samples, (int)sizeof samples);
    int value = 0;
    for (size_t i = 0; i < sizeof samples / sizeof *samples; i++) {
        value = samples[i] > value ? samples[i] : value;
    }
    return value;
}

/* The stub's sound chunk (1024 samples) at a picture's frame, 20 a second. */
static int chunk_at(int frame, int sound_rate)
{
    return (int)((int64_t)frame * 50 * sound_rate / 1000 / 1024);
}

/* Pull sound as SDL_mixer does: a buffer of samples at a time, on the
 * buffer's own cadence (two at once at the start), while the UI follows the
 * picture. From the first sound on, every buffer is full and goes on from
 * the one before, with no gap and nothing skipped; and the sound is in step
 * with the picture (see below for how closely that can be measured). */
static void mixer(MainUIVideo *video, int buffers, int samples, int sound_rate)
{
    static Sint16 buffer[4096 * 2];
    assert(samples <= 4096);
    Uint32 period = (Uint32)samples * 1000 / (Uint32)sound_rate;
    int last = 0, stepped = 0;
    Uint32 next = SDL_GetTicks();
    for (int i = 0; i < buffers; i++) {
        if (i > 1) {
            while ((Sint32)(SDL_GetTicks() - next) < 0) {
                mainui_video_frame(video, SDL_GetTicks());
                SDL_Delay(1);
            }
            next += period;
        }
        SDL_Surface *frame = mainui_video_frame(video, SDL_GetTicks());
        mainui_video_sound(video, (Uint8 *)buffer, samples * 4);
        for (int s = 0; s < samples * 2; s++) {
            int value = buffer[s];
            if (last && (value < last || value > last + 1)) {
                fprintf(stderr, "buffer %d sample %d: %d after %d\n", i, s, value, last);
                abort();
            }
            last = value ? value : last;
        }
        /* A buffer plays a buffer after it is taken, and the frame shown
         * is up to a frame (50 ms, 3 chunks) old: a buffer's first chunk is
         * the picture's up to a buffer and a frame on, give or take the
         * 50 ms the picture may drift before it follows (3 chunks). */
        if (frame && buffer[0] && i > 4) {
            int chunk = buffer[0] - 1;
            int expected = chunk_at(shown_index(frame), sound_rate);
            int ahead = samples / 1024 + 3;
            if (chunk < expected - 3 || chunk > expected + ahead + 3) {
                fprintf(stderr, "buffer %d: chunk %d, picture's %d\n", i, chunk, expected);
                abort();
            }
            stepped++;
        }
    }
    assert(last > 0 && stepped > 0);
}

static void in_step(MainUIVideo *video, int sound_rate)
{
    mixer(video, 16, 1024, sound_rate);
}

static void sound(const char *dir)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    const char *files[] = {"sound.mp4", "sound44.mp4"};
    const int sound_rates[] = {48000, 44100};
    for (int f = 0; f < 2; f++) {
        MainUIVideo *video = open_video(dir, files[f], target);
        /* Nothing before the first frame shows. */
        assert(!heard(video));
        mainui_video_wait_first(video, 5000);
        in_step(video, sound_rates[f]);
        /* Paused: silence, and afterwards the sound goes on in step. (At
         * 44.1 kHz it is only checked to keep in step.) */
        if (f) {
            mainui_video_close(video);
            continue;
        }
        mainui_video_pause(video, true, SDL_GetTicks());
        follow(video, 100);
        assert(!heard(video));
        mainui_video_pause(video, false, SDL_GetTicks());
        in_step(video, sound_rates[f]);
        /* After a seek, the sound of the new place. */
        mainui_video_seek(video, 2000);
        follow(video, 100);
        in_step(video, sound_rates[f]);
        /* Asleep: silence, then on in step. */
        mainui_video_hold(video, true, SDL_GetTicks());
        follow(video, 100);
        assert(!heard(video));
        mainui_video_hold(video, false, SDL_GetTicks());
        in_step(video, sound_rates[f]);
        mainui_video_close(video);
    }
    /* A seek landing on a keyframe: sound and picture in step there. */
    MainUIVideo *gop = open_video(dir, "soundgop.mp4", target);
    mainui_video_wait_first(gop, 5000);
    in_step(gop, 48000);
    mainui_video_seek(gop, 2000);
    follow(gop, 100);
    in_step(gop, 48000);
    mainui_video_close(gop);
    /* Pictures so large (0.3 MB each) that reading a second of sound ahead
     * fills the 4 MiB read-ahead: still decoded in order (the stub reports
     * otherwise), and in step. */
    MainUIVideo *big = open_video(dir, "big.mp4", target);
    mainui_video_wait_first(big, 5000);
    mixer(big, 24, 4096, 48000);
    mainui_video_seek(big, 2000);
    follow(big, 100);
    in_step(big, 48000);
    mainui_video_close(big);
    /* The sound as the file's first stream, before the pictures. */
    MainUIVideo *first = open_video(dir, "first.mp4", target);
    mainui_video_wait_first(first, 5000);
    in_step(first, 48000);
    mainui_video_seek(first, 2000);
    follow(first, 100);
    in_step(first, 48000);
    mainui_video_close(first);
    /* Sound stored well after its pictures, and sound read by the mixer's
     * whole buffers, plays without a gap and in step. */
    for (int f = 0; f < 2; f++) {
        MainUIVideo *video = open_video(dir, f ? "late.mp4" : "sound.mp4", target);
        mainui_video_wait_first(video, 5000);
        mainui_video_frame(video, SDL_GetTicks());
        mixer(video, 24, 4096, 48000);
        in_step(video, 48000);
        mainui_video_close(video);
    }
    /* Asked for no sound, there is none. */
    rate = 0;
    MainUIVideo *video = open_video(dir, "sound.mp4", target);
    mainui_video_wait_first(video, 5000);
    follow(video, 100);
    assert(!heard(video));
    mainui_video_close(video);
    SDL_FreeSurface(target);
    puts("sound plays in step with the picture, at the mixer's rate, also after pauses and "
         "seeks");
}

/* The sound cannot play: the picture still does, without an error. */
static void silent(const char *dir, const char *name)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    MainUIVideo *video = open_video(dir, name, target);
    mainui_video_wait_first(video, 5000);
    assert(follow(video, 300) > 0 && !*mainui_video_error(video) && !heard(video));
    mainui_video_close(video);
    SDL_FreeSurface(target);
}

/* Closing while FFmpeg opens the file, probes it or reads it, each held for
 * 3 s by the stub, stops it through the interrupt callback at once. */
static void cancel(const char *dir)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    const char *names[] = {"waitopen.mp4", "waitprobe.mp4", "waitread.mp4"};
    for (int i = 0; i < 3; i++) {
        MainUIVideo *video = open_video(dir, names[i], target);
        SDL_Delay(150);
        assert(!mainui_video_frame(video, SDL_GetTicks()));
        Uint32 start = SDL_GetTicks();
        mainui_video_close(video);
        Uint32 took = SDL_GetTicks() - start;
        if (took > 300) {
            fprintf(stderr, "%s: closing took %u ms\n", names[i], (unsigned)took);
            abort();
        }
    }
    /* Not held up, they play. */
    MainUIVideo *video = open_video(dir, "waitread.mp4", target);
    mainui_video_wait_first(video, 5000);
    assert(mainui_video_frame(video, SDL_GetTicks()) && !*mainui_video_error(video));
    mainui_video_close(video);
    SDL_FreeSurface(target);
    puts("closing a video stops FFmpeg's opening, probing and reading at once");
}

/* A stream whose pictures grow past the limit midway (from the tenth):
 * the first ones show, then the larger picture is refused before its
 * buffer is taken (the stub reports otherwise), and the video stops. */
static void grows(const char *dir)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    MainUIVideo *video = open_video(dir, "grow.mp4", target);
    mainui_video_wait_first(video, 5000);
    assert(mainui_video_frame(video, SDL_GetTicks()));
    for (Uint32 start = SDL_GetTicks();
         !*mainui_video_error(video) && SDL_GetTicks() - start < 5000;) {
        mainui_video_frame(video, SDL_GetTicks());
        SDL_Delay(5);
    }
    assert(!strcmp(mainui_video_error(video), "Video too large to play"));
    mainui_video_close(video);
    SDL_FreeSurface(target);
}

static void expect_error(const char *dir, const char *name, const char *error)
{
    SDL_Surface *target = screen(0xff0000, 0xff00, 0xff);
    MainUIVideo *video = open_video(dir, name, target);
    /* Read as the UI does while the decoder fails: "" or the whole text. */
    for (Uint32 start = SDL_GetTicks(); SDL_GetTicks() - start < 5000;) {
        const char *seen = mainui_video_error(video);
        if (*seen) {
            assert(!strcmp(seen, error) || !strcmp(seen, "Cannot play this video") ||
                   !strncmp(seen, "Video", 5));
            break;
        }
    }
    mainui_video_wait_first(video, 5000);
    if (strcmp(mainui_video_error(video), error)) {
        fprintf(stderr, "%s: \"%s\", not \"%s\"\n", name, mainui_video_error(video), error);
        abort();
    }
    assert(!mainui_video_frame(video, SDL_GetTicks()) && mainui_video_due(video, 0) == -1);
    mainui_video_close(video);
    SDL_FreeSurface(target);
}

int main(int argc, char **argv)
{
    assert((argc == 4 || argc == 5) && SDL_Init(SDL_INIT_TIMER) == 0);
    libraries = strcmp(argv[2], "-") ? argv[2] : NULL;
    if (!strcmp(argv[3], "play")) {
        play(argv[1]);
    }
    else if (!strcmp(argv[3], "errors")) {
        expect_error(argv[1], "text.mp4", "Cannot play this video");
        expect_error(argv[1], "missing.mp4", "Cannot play this video");
        expect_error(argv[1], "empty.mp4", "Cannot play this video");
        expect_error(argv[1], "nocodec.mp4", "Cannot play this video");
        expect_error(argv[1], "huge.mp4", "Video too large to play");
        /* Refused from the file's own size, before the decoder is opened
         * (this one cannot be), and from the size the stream itself gives,
         * before a picture buffer of that size is taken. */
        expect_error(argv[1], "hugenocodec.mp4", "Video too large to play");
        expect_error(argv[1], "liar.mp4", "Video too large to play");
        /* Before probing: a stream stating no size, or a second one too
         * large, is refused (the stub reports any probe that decodes them). */
        expect_error(argv[1], "nodims.mp4", "Cannot play this video");
        expect_error(argv[1], "extra.mp4", "Video too large to play");
        grows(argv[1]);
        SDL_Surface *paletted = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 8, 0, 0, 0, 0);
        char path[4096];
        TEST_PATH(path, "%s/ok.mp4", argv[1]);
        MainUIVideo *video = mainui_video_open(path, libraries, paletted, 640, 480, 0);
        assert(video && !strcmp(mainui_video_error(video), "Cannot play video on this screen"));
        mainui_video_close(video);
        SDL_FreeSurface(paletted);
        assert(!mainui_video_open(path, libraries, NULL, 640, 480, 0));
        puts("unusable videos, sizes and screens show why they cannot play");
    }
    else if (!strcmp(argv[3], "cancel")) {
        cancel(argv[1]);
    }
    else if (!strcmp(argv[3], "controls")) {
        controls(argv[1]);
    }
    else if (!strcmp(argv[3], "sound")) {
        rate = 48000;
        sound(argv[1]);
    }
    else if (!strcmp(argv[3], "silent")) {
        assert(argc == 5);
        rate = 48000;
        silent(argv[1], argv[4]);
        puts("a video whose sound cannot play shows without it");
    }
    else {
        expect_error(argv[1], "ok.mp4", "Video needs Onion's FFmpeg libraries");
        puts("without usable FFmpeg libraries videos show why they cannot play");
    }
    SDL_Quit();
    return 0;
}
