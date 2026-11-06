/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MENU_VIEW_H
#define MAINUI_MENU_VIEW_H
#include "catalog/catalog.h"
#include "menus/console_view.h"
#include "menus/menu.h"
#include "ui/carousel_worker.h"
#include "ui/theme.h"

typedef struct {
    int index;          /* the console's catalog index, or -1 */
    bool loaded, thumb; /* thumb: icon is its thumbnail (console-view.json) */
    bool attempted;     /* normal image lookup was attempted, even if missing */
    uint64_t key;       /* stable identity: paths and artwork dimensions */
    SDL_Surface *icon, *selected_icon, *label;
    SDL_Surface *small; /* a thumbnail at scale_unselected, NULL for full size */
    SDL_Surface *count; /* "(4562)": its games, when its cache exists */
} MainUICarouselSlot;

#define MAINUI_CAROUSEL_SLOTS 5
#define MAINUI_CAROUSEL_CACHE_SIZE 16

/* Owns decoded icons/text for one visible console page. The theme and catalog
 * are borrowed and must outlive the view. All access stays on the UI thread.
 */
typedef struct {
    MainUITheme *theme;
    SDL_Surface *home_icons[MAINUI_MENU_SECTIONS][2];
    SDL_Surface *console_icons[9][2];
    SDL_Surface *console_labels[9][2];
    /* The selected icon of a console not yet decoded: its path, while the
     * slot shows the normal icon in its place. NULL when decoded or shared. */
    char *pending_selected[9];
    int cached_start, crop_width, crop_height;
    /* Decode only the selected console's selected icon with the page; the
     * others follow one at a time while MainUI is idle. Off for snapshots and
     * scripted input, which draw every icon as soon as the page is shown. */
    bool defer_selected;
    /* The visible carousel slots borrow surfaces owned by carousel_cache.
     * Never free the visible copies: the cache owns every surface. */
    MainUICarouselSlot carousel[MAINUI_CAROUSEL_SLOTS];
    MainUICarouselSlot carousel_cache[MAINUI_CAROUSEL_CACHE_SIZE];
    Uint32 carousel_age[MAINUI_CAROUSEL_CACHE_SIZE], carousel_clock;
    MainUICarouselWorker carousel_worker;
    /* One job's paths, about 16 KiB: the UI thread's scratch, off its stack. */
    MainUICarouselJob carousel_scratch;
    const MainUICatalog *carousel_catalog;
    const MainUIEntry *carousel_entries; /* Catalog changes invalidate text, not artwork. */
    int carousel_total, carousel_height, carousel_scale;
    bool carousel_revealed; /* show initial covers together when first three are ready */
    /* The current indices are needed to protect visible images from LRU eviction. */
    int carousel_visible[MAINUI_CAROUSEL_SLOTS];
    /* Its slide ("animation": "slide"): from where each console was drawn,
     * by catalog index, toward its place round the selected one. */
    int slid_to; /* the selection the slide goes to, -1 before the first */
    /* The way the last move went: 1 Right (or forward), -1 Left, 0 none.
     * Set by the key handling; a console that is the neighbour on both
     * sides of a short list goes to the side the move leaves behind. */
    int carousel_direction;
    int slid_from; /* the selection before it */
    Uint32 slide_start;
    bool sliding;
    bool carousel_snap; /* Next selection change is a multi-console jump, not a slide. */
    int drawn_count, drawn_index[MAINUI_CAROUSEL_SLOTS + 2];
    int drawn_x[MAINUI_CAROUSEL_SLOTS + 2], from_x[MAINUI_CAROUSEL_SLOTS + 2];
    int from_count, from_index[MAINUI_CAROUSEL_SLOTS + 2];
    /* The neighbours' tile at scale_unselected, scaled from carousel_tile_source
     * (the theme's, borrowed) and kept while it and the scale stay. */
    SDL_Surface *carousel_tile;
    const SDL_Surface *carousel_tile_source;
    int carousel_tile_scale;
    /* The plain selection's background, kept while its size and colour stay. */
    SDL_Surface *selection;
    SDL_Color selection_colour;
    Uint8 selection_alpha;
} MainUIMenuView;

#define MAINUI_MENU_VIEW_CAROUSEL (-2)

void mainui_menu_view_open(MainUIMenuView *view, MainUITheme *theme);
void mainui_menu_view_close(MainUIMenuView *view);
void mainui_menu_draw_home(MainUIMenuView *view, SDL_Surface *screen, const MainUIMenu *menu,
                           const MainUIViewport *position);
void mainui_menu_draw_systems(MainUIMenuView *view, SDL_Surface *screen, MainUICatalog *catalog,
                              const MainUIViewport *position);
/* The consoles as a carousel instead: the selected one centred, two
 * neighbours each side, in catalog order, round the ends with console's
 * "wrap". A console with a thumbnail in console's thumbs folder shows it,
 * scaled down to fit 560x260 (560x300 with the name or the number of games
 * hidden), its neighbours' at scale_unselected, on a plain selection, its
 * name and number beneath; one without, its theme icon as a cell of the grid,
 * on the theme's tile. Sixteen entries may be retained across screen
 * transitions. An independent background worker decodes icons and prefetches
 * all consoles, nearest first; rendering never waits for an image. With
 * "animation": "slide", a new selection slides from where the consoles were
 * drawn, over duration_ms from now; pinned, when not negative, is the time
 * since it started instead (snapshots: their --elapsed, or its end). */
void mainui_menu_draw_carousel(MainUIMenuView *view, SDL_Surface *screen, MainUICatalog *catalog,
                               const MainUIViewport *position, const MainUIConsoleView *console,
                               Uint32 now, int pinned);
/* Reap a completed async image and continue off-screen prefetch without
 * requiring a full-frame composition. handled is true only on completion;
 * return true when the completed icon belongs to a currently visible slot. */
bool mainui_menu_carousel_service(MainUIMenuView *view, MainUICatalog *catalog,
                                  const MainUIViewport *position, const MainUIConsoleView *console,
                                  bool *handled);
/* A slide is under way: draw again at the title's pace. */
bool mainui_menu_carousel_sliding(const MainUIMenuView *view);
/* Load the icons and labels of the console page at position->start, once per
 * page. Expert icons keep only the part Expert draws, and identical normal and
 * selected icons share one surface. With defer_selected, every normal icon and
 * the selected console's selected icon are decoded now; the other selected
 * icons are left for mainui_menu_view_load_pending(). */
void mainui_menu_view_page(MainUIMenuView *view, MainUICatalog *catalog,
                           const MainUIViewport *position);
/* Decode one selected icon left for later by mainui_menu_view_page(): the
 * selected console's first, then the others in page order. Nothing when the
 * page at position->start is not the one loaded. *shown is set when the icon
 * is the selected console's, so the frame must be drawn again. Returns false
 * when none was left. UI thread only. */
bool mainui_menu_view_load_pending(MainUIMenuView *view, const MainUIViewport *position,
                                   bool *shown);
/* Bytes held by home, grid and cached carousel icons; capped at 24 MiB. */
size_t mainui_menu_view_bytes(const MainUIMenuView *view);
/* Decode the icon at path (mainui_theme_console_icon()), cropped to width x
 * height when they are set, or NULL when it would take its holder past the
 * 24 MiB icon budget with `retained` bytes already held. An icon dropped for
 * the budget is remembered with its size for the session, so it is not
 * decoded again while it still cannot fit. UI thread only. */
SDL_Surface *mainui_menu_view_icon(MainUITheme *theme, const char *path, size_t retained, int width,
                                   int height);
#endif
