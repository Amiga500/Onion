/* SPDX-License-Identifier: GPL-3.0-only */
/* Executable fixture for exact media/maps discovery without SDL initialization. */
#include "ui/media_maps.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(content, 1, strlen(content), f) == strlen(content));
    assert(!fclose(f));
}

int main(void)
{
    char root[] = "/tmp/mainui-maps-XXXXXX";
    assert(mkdtemp(root));
    char path[4096];
    snprintf(path, sizeof path, "%s/media", root);
    assert(!mkdir(path, 0700));
    snprintf(path, sizeof path, "%s/media/maps", root);
    assert(!mkdir(path, 0700));
    const char *names[] = {
        "Test Game-10.png",   "Test Game-2.PNG",     "Test Game-1.png",  "Test Game-0007.png",
        "Test Game-3.gif",    "Test Game-4.png.tmp", "Test Game-20.png", "Test Game.png",
        "Test Game-2.png",    "Test Game-22.PNG",    "Other Game-1.png", "Test Game-1.txt",
        "Test Game_map2.png", "Test Game-0.png",     "Test Game-5.PNG",  "Test Game-6.jpg",
        "Test Game-8.JPEG",   "Test Game-11.bmp",    "test game-30.png"};
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        snprintf(path, sizeof path, "%s/media/maps/%s", root, names[i]);
        file(path, "image"); /* discovery does not decode image contents */
    }
    /* A directory with a valid-looking name is not a map. */
    snprintf(path, sizeof path, "%s/media/maps/Test Game-9.png", root);
    assert(!mkdir(path, 0700));
    /* A case-variant duplicate should not create another map. */
    snprintf(path, sizeof path, "%s/media/maps/Test Game-2.pNg", root);
    file(path, "duplicate");

    MainUIMap *maps = calloc(MAINUI_MEDIA_MAPS_MAX, sizeof *maps);
    assert(maps);
    char rom[4096];
    snprintf(rom, sizeof rom, "%s/Sub/Test Game.sfc", root);
    int count = mainui_maps_find(root, rom, maps, MAINUI_MEDIA_MAPS_MAX);
    assert(count == 9);
    assert(strstr(maps[0].path, "Test Game.png") && maps[0].number == 0);
    assert(strstr(maps[1].path, "-1.png") && maps[1].number == 1);
    assert(maps[2].number == 2);
    assert(strstr(maps[3].path, "-5.PNG") && maps[3].number == 5);
    assert(strstr(maps[4].path, "-0007.png") && maps[4].number == 7);
    assert(strstr(maps[5].path, "-10.png") && maps[5].number == 10);
    assert(strstr(maps[6].path, "-20.png") && maps[6].number == 20);
    assert(strstr(maps[7].path, "-22.PNG") && maps[7].number == 22);
    /* The ROM's name in any case, as on the SD card. */
    assert(strstr(maps[8].path, "test game-30.png") && maps[8].number == 30);
    /* The old _map suffix, a non-image, BMP, JPEG and -0 are not maps. */
    assert(mainui_maps_find(root, rom, maps, 3) == 3);
    assert(mainui_maps_find(root, "Other Game.sfc", maps, MAINUI_MEDIA_MAPS_MAX) == 1);
    assert(mainui_maps_find(root, "No Maps.sfc", maps, MAINUI_MEDIA_MAPS_MAX) == 0);
    assert(mainui_maps_find(root, rom, NULL, MAINUI_MEDIA_MAPS_MAX) == 0);

    /* Only exact lowercase directory maps; never media/Maps. */
    snprintf(path, sizeof path, "%s/media/Maps", root);
    assert(!mkdir(path, 0700));
    snprintf(path, sizeof path, "%s/media/Maps/Only-1.png", root);
    file(path, "image");
    assert(mainui_maps_find(root, "Only.sfc", maps, MAINUI_MEDIA_MAPS_MAX) == 0);

    /* No recursion, no deletion of any user file except this fixture. */
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        snprintf(path, sizeof path, "%s/media/maps/%s", root, names[i]);
        assert(!unlink(path));
    }
    snprintf(path, sizeof path, "%s/media/maps/Test Game-2.pNg", root);
    assert(!unlink(path));
    snprintf(path, sizeof path, "%s/media/maps/Test Game-9.png", root);
    assert(!rmdir(path));
    snprintf(path, sizeof path, "%s/media/maps", root);
    assert(!rmdir(path));
    snprintf(path, sizeof path, "%s/media/Maps/Only-1.png", root);
    assert(!unlink(path));
    snprintf(path, sizeof path, "%s/media/Maps", root);
    assert(!rmdir(path));
    snprintf(path, sizeof path, "%s/media", root);
    assert(!rmdir(path));
    assert(!rmdir(root));
    free(maps);
    puts("map discovery OK");
    return 0;
}
