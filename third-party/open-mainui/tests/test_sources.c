/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/browser.h"
#include "app/details.h"
#include "catalog/library.h"
#include "menus/menu.h"
#include "support.h"
#include "ui/preview.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built by tests/integration/case_saved_source.py: Game Boy (Emu/GB) and
 * Game Boy 2 (Emu/GB2) in Games, and Expert GB (RApp/GBX) in Expert, all with
 * the ROM folder Roms/GB and the image folders Imgs, Imgs2 and ImgsX. */
static MainUICatalog *catalog;
static MainUILibrary *favorites;
static MainUIPreview preview;
static MainUIDetails details;

static int favorite(const char *label)
{
    for (int i = 0; i < favorites->visible_count; i++) {
        if (!strcmp(mainui_library_label(favorites, i), label)) {
            return i;
        }
    }
    assert(!"no such Favorite");
    return -1;
}

static bool ends_with(const char *text, const char *end)
{
    size_t a = strlen(text), b = strlen(end);
    return a >= b && !strcmp(text + a - b, end);
}

/* Game Info's console and the cover the list shows for a Favorite. */
static void check_favorite(const char *label, const char *system, const char *cover)
{
    int row = favorite(label);
    assert(mainui_details_open(&details, catalog, favorites, favorites, row, &preview));
    assert(!strcmp(details.system, system));
    mainui_details_close(&details);
    mainui_preview_request(&preview, catalog, favorites, row, true);
    assert(ends_with(preview.key, cover));
}

int main(int argc, char **argv)
{
    assert(argc == 2 && SDL_Init(0) == 0);
    const char *sd = argv[1];
    catalog = calloc(1, sizeof *catalog);
    favorites = calloc(1, sizeof *favorites);
    assert(catalog && favorites && mainui_catalog_open(catalog, sd, false));
    assert(mainui_library_open(favorites, sd, false));
    /* By the saved launcher, not the first console with that ROM folder. */
    check_favorite("One", "Game Boy", "/Roms/GB/Imgs/game.png");
    check_favorite("Two", "Game Boy 2", "/Roms/GB/Imgs2/game.png");
    /* An Expert console while Games is open: from its own config.json. */
    check_favorite("Three", "Expert GB", "/Roms/GB/ImgsX/game.png");
    /* Asked again, as on every frame, its console is the kept one: its
     * config.json is not read again (here it is gone). */
    char config[4096], hidden[4096];
    TEST_PATH(config, "%s/RApp/GBX/config.json", sd);
    TEST_PATH(hidden, "%s.hidden", config);
    assert(rename(config, hidden) == 0);
    mainui_preview_request(&preview, catalog, favorites, favorite("Three"), true);
    assert(ends_with(preview.key, "/Roms/GB/ImgsX/game.png"));
    assert(rename(hidden, config) == 0);
    /* A launcher that is gone: the first console with that ROM folder. */
    check_favorite("Four", "Game Boy", "/Roms/GB/Imgs/game.png");
    /* An absolute saved imgpath is used as it is. */
    check_favorite("Five", "Game Boy 2", "/Roms/GB/ImgsX/game.png");
    /* A ROM list: the open console's name, also where consoles share it. */
    for (int i = 0; i < 2; i++) {
        const char *system = i ? "Game Boy 2" : "Game Boy";
        MainUIViewport view;
        mainui_grid_restore(&view, mainui_browser_count(catalog), 0, 4, 2);
        for (view.selected = 0; strcmp(mainui_browser_label(catalog, view.selected), system);) {
            view.selected++;
        }
        assert(mainui_browser_enter(catalog, &view, 6) && catalog->depth == 1);
        assert(mainui_details_open(&details, catalog, NULL, favorites, 0, &preview));
        assert(!strcmp(details.system, system));
        mainui_details_close(&details);
        assert(mainui_catalog_back(catalog) && catalog->depth == 0);
    }
    mainui_preview_close(&preview);
    mainui_library_close(favorites);
    mainui_catalog_close(catalog);
    free(favorites);
    free(catalog);
    SDL_Quit();
    puts("Saved games and ROM lists name their console by its launcher, and covers follow it");
    return 0;
}
