#include <SDL/SDL.h>
#include <pthread.h>
#include <stdbool.h>

#include "imageCache.h"
#include "utils/log.h"

#define IMAGECACHE_MAXSIZE 50

static pthread_t romscreen_thread_pt;
static bool thread_active = false;

static const int image_cache_len = IMAGECACHE_MAXSIZE;
static SDL_Surface *image_cache[IMAGECACHE_MAXSIZE] = {NULL};
static int image_cache_offset = -1;

static SDL_Surface *(*load_image)(int) = NULL;
static int images_total = 0;

int modulo(int x, int n) { return (x % n + n) % n; }

static void *_imageCacheThread(void *param)
{
    int offset = *((int *)param) - image_cache_len / 2;
    int start = image_cache_offset - image_cache_len + 1;
    int end = image_cache_offset;

    if (offset > images_total - image_cache_len)
        offset = images_total - image_cache_len;
    if (offset < 0)
        offset = 0;

    for (int i = 0; i < image_cache_len; i++) {
        int curr = offset + i;

        if (curr >= images_total)
            break;

        if (curr >= start && curr <= end)
            continue;

        int idx = modulo(curr, image_cache_len);

        if (image_cache[idx] != NULL)
            SDL_FreeSurface(image_cache[idx]);

        if (load_image != NULL)
            image_cache[idx] = load_image(curr);
    }

    image_cache_offset = offset + image_cache_len - 1;

    thread_active = false;
    return 0;
}

void imageCache_load(int *offset, SDL_Surface *(*_load_image)(int), int total)
{
    if (thread_active)
        return;
    thread_active = true;
    load_image = _load_image;
    images_total = total;
    pthread_create(&romscreen_thread_pt, NULL, _imageCacheThread, offset);
}

void imageCache_removeItem(int image_index)
{
    int start = image_cache_offset - image_cache_len + 1;
    int end = image_cache_offset;

    if (image_index < start || image_index > end)
        return;

    int idx = modulo(image_index, image_cache_len);

    if (image_cache[idx] != NULL) {
        printf_debug("Removing image %d (%d)\n", image_index, idx);
        SDL_FreeSurface(image_cache[idx]);
        image_cache[idx] = NULL;
    }

    // Ring buffer optimization: no need to shift array elements
    // Just mark as NULL and let the cache reload when needed
    // The ring buffer will naturally handle the empty slot
}

SDL_Surface *imageCache_getItem(int *index)
{
    imageCache_load(index, load_image, images_total);
    int idx = modulo(*index, image_cache_len);
    return image_cache[idx];
}

bool imageCache_isActive(void) { return thread_active; }

void imageCache_freeAll(void)
{
    for (int i = 0; i < image_cache_len; i++) {
        if (image_cache[i] != NULL)
            SDL_FreeSurface(image_cache[i]);
    }
}