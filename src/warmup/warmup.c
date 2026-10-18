/*
 * warmup - initialize SDL 1.2 video during boot to reduce the cost of a later
 * video initialization in MainUI or other applications.
 *
 * Log initialization time, status, selected video driver, and the loaded SDL
 * library for troubleshooting. The exit status indicates SDL_Init success.
 *
 * Exit without SDL_Quit() or process exit handlers, matching the original
 * warmup behavior. This does not keep the SDL process alive.
 */
#define _POSIX_C_SOURCE 199309L

#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void)
{
    struct timespec t0, t1;
    int have_start = (clock_gettime(CLOCK_MONOTONIC, &t0) == 0);
    int rc = SDL_Init(SDL_INIT_VIDEO);
    int have_end = (clock_gettime(CLOCK_MONOTONIC, &t1) == 0);

    char driver[32] = "none";
    if (rc == 0)
        SDL_VideoDriverName(driver, sizeof(driver));

    if (have_start && have_end) {
        /* Subtract first, then convert to milliseconds to avoid 32-bit overflow. */
        long long elapsed_ms =
            ((long long)t1.tv_sec - (long long)t0.tv_sec) * 1000LL +
            ((long long)t1.tv_nsec - (long long)t0.tv_nsec) / 1000000LL;
        fprintf(stderr, "warmup: SDL_Init=%d in %lld ms, driver=%s, error='%s'\n",
                rc, elapsed_ms, driver, rc == 0 ? "" : SDL_GetError());
    } else {
        fprintf(stderr, "warmup: SDL_Init=%d, timing unavailable, driver=%s, error='%s'\n",
                rc, driver, rc == 0 ? "" : SDL_GetError());
    }

    /* Report the mapped SDL library to help diagnose library-path overrides. */
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    while (maps && fgets(line, sizeof(line), maps)) {
        if (strstr(line, "libSDL") && strstr(line, "r-xp")) {
            char *path = strchr(line, '/');
            fprintf(stderr, "warmup: loaded %s", path ? path : line);
        }
    }
    if (maps)
        fclose(maps);

    /* _Exit skips atexit handlers and SDL shutdown. Flush diagnostics first. */
    fflush(stderr);
    _Exit(rc == 0 ? 0 : 1);
}
