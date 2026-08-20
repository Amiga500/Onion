/**
 * @file test_gs_appstate.c
 * @brief Unit tests for src/gameSwitcher/gs_appState.h
 *
 * Tests the pure-logic game switcher application state functions:
 * currentGame() (returns current game or NULL for empty list),
 * sigHandler() (sets quit+exit_to_menu on SIGINT/SIGTERM),
 * and view mode constants.
 *
 * SDL dependencies are stubbed out to void pointers.
 *
 * Build and run: make -f Makefile.unit test_gs_appstate
 */

#include "onion_test.h"
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ---- Stub SDL types ---- */
typedef void SDL_Surface;
typedef struct { short x, y; unsigned short w, h; } SDL_Rect;

#define STR_MAX 256
#define MAX_HISTORY 100

/* ---- Stub log macros ---- */
#define print_debug(...)
#define printf_debug(...)

/* ---- Inline types from gs_model.h (without SDL) ---- */

typedef struct {
    char label[STR_MAX * 2];
    char rompath[STR_MAX * 2];
    char imgpath[STR_MAX * 2];
    char launch[STR_MAX * 2];
    int type;
    int lineNo;
} RecentItem;

typedef struct {
    RecentItem recentItem;
    SDL_Surface *romScreen;
    char rom_name[STR_MAX * 2];
    char name[STR_MAX * 2];
    char shortname[STR_MAX * 2];
    char core_name[STR_MAX * 2];
    char core_path[STR_MAX * 2];
    char totalTime[100];
    int index;
    bool processed;
    bool is_running;
} Game_s;

static Game_s game_list[MAX_HISTORY];
static int game_list_len = 0;

/* ---- Stub List type (from list.h) ---- */
typedef struct {
    char _padding[4096]; /* opaque placeholder */
} List;

/* ---- Inline view mode constants ---- */
#define VIEW_NORMAL 0
#define VIEW_MINIMAL 1
#define VIEW_FULLSCREEN -1

/* ---- Inline AppState from gs_appState.h ---- */

typedef struct {
    List pop_menu_list;
    bool quit;
    bool exit_to_menu;
    bool changed;
    bool current_game_changed;
    bool brightness_changed;
    bool pop_menu_open;
    bool show_time;
    bool show_total;
    bool show_legend;
    bool is_overlay;
    int view_mode;
    int view_restore;
    int pop_menu_game_index;
    int current_game;
} AppState;

static AppState appState = {
    .quit = false,
    .exit_to_menu = false,
    .changed = true,
    .current_game_changed = true,
    .brightness_changed = false,
    .pop_menu_open = false,
    .show_time = false,
    .show_total = true,
    .show_legend = true,
    .is_overlay = false,
    .view_mode = VIEW_NORMAL,
    .view_restore = VIEW_NORMAL,
    .pop_menu_game_index = 0,
    .current_game = 0,
};

/* ---- Inline sigHandler ---- */
static void sigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        appState.exit_to_menu = true;
        appState.quit = true;
    }
}

/* ---- Inline currentGame ---- */
static Game_s *currentGame(void)
{
    if (game_list_len == 0)
        return NULL;
    return &game_list[appState.current_game];
}

/* ---- Helper to reset state ---- */
static void reset_state(void)
{
    memset(game_list, 0, sizeof(game_list));
    game_list_len = 0;
    appState.quit = false;
    appState.exit_to_menu = false;
    appState.current_game = 0;
    appState.changed = true;
    appState.view_mode = VIEW_NORMAL;
}

/* ==== View mode constants ==== */

TEST(view_constants) {
    ASSERT_EQ(VIEW_NORMAL, 0);
    ASSERT_EQ(VIEW_MINIMAL, 1);
    ASSERT_EQ(VIEW_FULLSCREEN, -1);
}

/* ==== currentGame tests ==== */

TEST(currentGame_empty_list_returns_null) {
    reset_state();
    ASSERT_NULL(currentGame());
}

TEST(currentGame_single_game) {
    reset_state();
    game_list_len = 1;
    strncpy(game_list[0].name, "Super Mario", STR_MAX * 2 - 1);
    appState.current_game = 0;

    Game_s *g = currentGame();
    ASSERT_NOT_NULL(g);
    ASSERT_STREQ(g->name, "Super Mario");
}

TEST(currentGame_multiple_games) {
    reset_state();
    game_list_len = 3;
    strncpy(game_list[0].name, "Game A", STR_MAX * 2 - 1);
    strncpy(game_list[1].name, "Game B", STR_MAX * 2 - 1);
    strncpy(game_list[2].name, "Game C", STR_MAX * 2 - 1);
    appState.current_game = 2;

    Game_s *g = currentGame();
    ASSERT_NOT_NULL(g);
    ASSERT_STREQ(g->name, "Game C");
}

TEST(currentGame_switch_games) {
    reset_state();
    game_list_len = 2;
    strncpy(game_list[0].name, "First", STR_MAX * 2 - 1);
    strncpy(game_list[1].name, "Second", STR_MAX * 2 - 1);

    appState.current_game = 0;
    ASSERT_STREQ(currentGame()->name, "First");

    appState.current_game = 1;
    ASSERT_STREQ(currentGame()->name, "Second");
}

/* ==== sigHandler tests ==== */

TEST(sigHandler_sigint_sets_quit) {
    reset_state();
    ASSERT_FALSE(appState.quit);
    ASSERT_FALSE(appState.exit_to_menu);

    sigHandler(SIGINT);

    ASSERT_TRUE(appState.quit);
    ASSERT_TRUE(appState.exit_to_menu);
}

TEST(sigHandler_sigterm_sets_quit) {
    reset_state();
    sigHandler(SIGTERM);

    ASSERT_TRUE(appState.quit);
    ASSERT_TRUE(appState.exit_to_menu);
}

TEST(sigHandler_other_signal_no_change) {
    reset_state();
    sigHandler(SIGUSR1);

    ASSERT_FALSE(appState.quit);
    ASSERT_FALSE(appState.exit_to_menu);
}

/* ==== AppState default values ==== */

TEST(appstate_defaults) {
    /* Verify initial state matches expected defaults */
    AppState fresh = {
        .quit = false,
        .exit_to_menu = false,
        .changed = true,
        .current_game_changed = true,
        .show_total = true,
        .show_legend = true,
        .view_mode = VIEW_NORMAL,
    };
    ASSERT_FALSE(fresh.quit);
    ASSERT_FALSE(fresh.exit_to_menu);
    ASSERT_TRUE(fresh.changed);
    ASSERT_TRUE(fresh.current_game_changed);
    ASSERT_TRUE(fresh.show_total);
    ASSERT_TRUE(fresh.show_legend);
    ASSERT_EQ(fresh.view_mode, VIEW_NORMAL);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_appState.h Unit Tests ===\n\n");

    RUN_TEST(view_constants);

    RUN_TEST(currentGame_empty_list_returns_null);
    RUN_TEST(currentGame_single_game);
    RUN_TEST(currentGame_multiple_games);
    RUN_TEST(currentGame_switch_games);

    RUN_TEST(sigHandler_sigint_sets_quit);
    RUN_TEST(sigHandler_sigterm_sets_quit);
    RUN_TEST(sigHandler_other_signal_no_change);

    RUN_TEST(appstate_defaults);

    TEST_REPORT();
    return test_failures;
}
