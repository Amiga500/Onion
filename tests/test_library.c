/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/library.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    MainUILibrary *library = calloc(1, sizeof *library);
    assert(library);
    bool recent = !strcmp(argv[2], "recent") || !strcmp(argv[2], "cap");
    assert(mainui_library_open(library, argv[1], recent));
    if (!strcmp(argv[2], "recent")) {
        assert(library->count == 50);
        assert(!strcmp(mainui_library_label(library, 0), "First saved name"));
        assert(!strcmp(library->items[0].rom, "/mnt/SDCARD/Roms/FC/one.nes"));
        assert(!strcmp(library->items[0].launch, "/mnt/SDCARD/Emu/FC/launch.sh"));
        assert(!strcmp(mainui_library_label(library, 49), "Game 49"));
    }
    else if (!strcmp(argv[2], "cap")) {
        assert(library->count == 0);
    }
    else if (!strcmp(argv[2], "folders")) {
        assert(library->count == 3 && library->visible_count == 2);
        assert(!strcmp(mainui_library_label(library, 0), "Collection"));
        assert(!strcmp(mainui_library_label(library, 1), "Root game"));
        assert(mainui_library_enter(library, 0));
        assert(library->visible_count == 3);
        assert(library->leading_folders == 2 && library->visible_games == 1);
        assert(library->folders[0].direct_games == 1);
        assert(library->folders[1].direct_games == 1);
        assert(!strcmp(mainui_library_label(library, 0), ".."));
        assert(!strcmp(mainui_library_label(library, 1), "Nested"));
        assert(!strcmp(mainui_library_label(library, 2), "Inside game"));
        assert(mainui_library_enter(library, 1));
        assert(library->visible_count == 2);
        assert(!strcmp(mainui_library_label(library, 1), "Deep game"));
        assert(mainui_library_enter(library, 0));
        assert(mainui_library_back(library));
        assert(!mainui_library_back(library));
    }
    else if (!strcmp(argv[2], "flat")) {
        assert(library->count == 3 && library->folder_count == 0);
    }
    else if (!strcmp(argv[2], "missing")) {
        assert(library->visible_count == 0);
    }
    else {
        assert(false);
    }
    mainui_library_close(library);
    free(library);
    puts("Saved-library scenario passed");
    return 0;
}
