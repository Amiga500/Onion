/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/catalog.h"
#include "platform/files.h"
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
    MainUICatalog *apps = calloc(1, sizeof *apps);
    assert(apps && mainui_catalog_apps(apps, argv[1], false));
    assert(apps->pages[0].count == 2);
    MainUIEntry *first = mainui_catalog_entry(apps, 0);
    assert(!strcmp(first->label, "Alpha app") && !first->directory);
    assert(strstr(first->path, "/App/alpha/launch.sh"));
    assert(strstr(first->icon, "/Icons/Default/app/pacman.png"));
    mainui_catalog_close(apps);
    free(apps);
    puts("App discovery checks passed");
    return 0;
}
