/* Regression for space-containing local AAC filenames. */
#include "ffpath.h"
#include <stdio.h>
#include <stdlib.h>
#define ASSERT(e) do { if (!(e)) { fprintf(stderr, "FAIL: %s\n", #e); exit(1); } } while (0)
int main(void)
{
    ASSERT(ffpath_allowed("/mnt/SDCARD/Media/Music/AAC - 5MINS - LARGE.aac", 0));
    ASSERT(ffpath_allowed("/mnt/SDCARD/Media/Music/My album.m4a", 0));
    ASSERT(ffpath_allowed("/mnt/SDCARD/Media/Music/Song.aac", 0));
    ASSERT(!ffpath_allowed("relative.aac", 0));
    ASSERT(!ffpath_allowed("/mnt/SDCARD/song\n.aac", 0));
    ASSERT(!ffpath_allowed("http://example.org/song.aac", 0));
    ASSERT(ffpath_allowed("https://example.org/live.aac", 1));
    ASSERT(!ffpath_allowed("https://example.org/live radio.aac", 1));
    ASSERT(!ffpath_allowed("file:///etc/passwd", 1));
    puts("FFmpeg local paths with spaces and network restrictions: PASS");
    return 0;
}
