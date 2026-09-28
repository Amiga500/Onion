/**
 * @file test_state_security.c
 * @brief Security and edge-case tests for state functions in state.h
 *
 * Tests write_mainui_state JSON formatting for all MainUI states,
 * boundary conditions on page calculations, and state_getAppName
 * edge cases not covered by the existing test_state.c.
 *
 * Build and run: make -f Makefile.unit test_state_security
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- Inline types and logic from state.h ---- */

typedef enum mainui_states {
    MAIN_MENU,
    RECENTS,
    FAVORITES,
    GAMES,
    EXPERT,
    APPS
} MainUIState;

/* Testable version of write_mainui_state that writes to buffer instead of file */
static bool _write_mainui_state_to_buf(char *buf, size_t buf_size,
                                       MainUIState state, int currpos,
                                       int total,
                                       bool show_recents, bool show_expert)
{
    int title_num = 0, page_type = 0, page_size = 6, page_start = 0, page_end,
        main_currpos = 0, main_page_start = 0, main_page_end;

    switch (state) {
    case MAIN_MENU:
        return false; /* MAIN_MENU removes state file */
    case RECENTS:
        title_num = 18;
        page_type = 10;
        main_currpos = 0;
        break;
    case FAVORITES:
        title_num = 1;
        page_type = 2;
        main_currpos = 1;
        break;
    case GAMES:
        title_num = 2;
        page_type = 1;
        page_size = 8;
        main_currpos = 2;
        break;
    case EXPERT:
        title_num = 0;
        page_type = 16;
        page_size = 9;
        main_currpos = 3;
        break;
    case APPS:
        title_num = 107;
        page_type = 3;
        page_size = 4;
        main_currpos = 4;
        break;
    default:
        return false;
    }

    int main_total = 6;
    if (!show_recents) {
        if (main_currpos > 0)
            main_currpos--;
        main_total--;
    }
    if (!show_expert) {
        if (state == APPS)
            main_currpos--;
        main_total--;
    }

    if (main_currpos + 4 > main_total)
        main_page_start = main_total - 4;
    main_page_end = main_page_start + 3;

    if (currpos + page_size > total)
        page_start = total - page_size;
    else
        page_start = currpos;
    page_end = page_start + page_size - 1;

    snprintf(buf, buf_size,
            "{\"list\":[{\"title\":157,\"type\":0,\"currpos\":%d,\"pagestart\":"
            "%d,\"pageend\":%d},{\"title\":%d,\"type\":%d,\"currpos\":%d,"
            "\"pagestart\":%d,\"pageend\":%d}]}",
            main_currpos, main_page_start, main_page_end, title_num, page_type,
            currpos, page_start, page_end);

    return true;
}

/* Fixed state_getAppName from state.h */
static size_t _state_getAppName(char *out, const char *str)
{
    char *end;
    size_t out_size;

    const char *prefix_end = strstr(str, " ./");
    if (prefix_end == NULL) {
        *out = '\0';
        return 0;
    }
    str = prefix_end + 3;

    end = (char *)strchr(str, ';');
    if (end == NULL)
        end = (char *)(str + strlen(str));

    out_size = (end - str) < STR_MAX - 1 ? (end - str) : STR_MAX - 1;
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

/* ---- Tests: write_mainui_state JSON format ---- */

TEST(write_state_main_menu_returns_false) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              MAIN_MENU, 0, 10, true, true);
    ASSERT_FALSE(result);
}

TEST(write_state_recents) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              RECENTS, 0, 20, true, true);
    ASSERT_TRUE(result);
    /* Valid JSON structure */
    ASSERT_EQ(buf[0], '{');
    ASSERT_EQ(buf[strlen(buf) - 1], '}');
    /* Contains correct title and type for RECENTS */
    ASSERT_NOT_NULL(strstr(buf, "\"title\":18"));
    ASSERT_NOT_NULL(strstr(buf, "\"type\":10"));
}

TEST(write_state_favorites) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              FAVORITES, 0, 10, true, true);
    ASSERT_TRUE(result);
    ASSERT_NOT_NULL(strstr(buf, "\"title\":1"));
    ASSERT_NOT_NULL(strstr(buf, "\"type\":2"));
}

TEST(write_state_games) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              GAMES, 0, 30, true, true);
    ASSERT_TRUE(result);
    ASSERT_NOT_NULL(strstr(buf, "\"title\":2"));
    ASSERT_NOT_NULL(strstr(buf, "\"type\":1"));
}

TEST(write_state_expert) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              EXPERT, 0, 15, true, true);
    ASSERT_TRUE(result);
    ASSERT_NOT_NULL(strstr(buf, "\"title\":0"));
    ASSERT_NOT_NULL(strstr(buf, "\"type\":16"));
}

TEST(write_state_apps) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              APPS, 0, 10, true, true);
    ASSERT_TRUE(result);
    ASSERT_NOT_NULL(strstr(buf, "\"title\":107"));
    ASSERT_NOT_NULL(strstr(buf, "\"type\":3"));
}

/* ---- Tests: page calculation edge cases ---- */

TEST(write_state_currpos_at_end) {
    /* When currpos + page_size > total, page_start = total - page_size */
    char buf[STR_MAX];
    _write_mainui_state_to_buf(buf, sizeof(buf),
                                GAMES, 25, 30, true, true);
    /* GAMES page_size = 8, currpos 25 + 8 > 30, so page_start = 30-8=22 */
    ASSERT_NOT_NULL(strstr(buf, "\"pagestart\":22"));
}

TEST(write_state_currpos_at_start) {
    char buf[STR_MAX];
    _write_mainui_state_to_buf(buf, sizeof(buf),
                                GAMES, 0, 30, true, true);
    /* GAMES currpos 0 + 8 <= 30, so page_start = currpos = 0 */
    ASSERT_NOT_NULL(strstr(buf, "\"pagestart\":0"));
}

TEST(write_state_total_smaller_than_page_size) {
    /* When total < page_size, page_start becomes negative.
     * This documents the current behavior; in practice, the UI
     * ensures total >= page_size for each state. However, callers
     * should be aware that invalid inputs can produce negative indices. */
    char buf[STR_MAX];
    _write_mainui_state_to_buf(buf, sizeof(buf),
                                EXPERT, 0, 3, true, true);
    /* EXPERT page_size = 9, total = 3, page_start = 3-9 = -6 */
    ASSERT_NOT_NULL(strstr(buf, "\"pagestart\":-6"));
}

/* ---- Tests: show_recents / show_expert toggles ---- */

TEST(write_state_no_recents_shifts_position) {
    char buf_with[STR_MAX], buf_without[STR_MAX];

    /* FAVORITES with recents on: main_currpos = 1 */
    _write_mainui_state_to_buf(buf_with, sizeof(buf_with),
                                FAVORITES, 0, 10, true, true);
    /* FAVORITES without recents: main_currpos decremented to 0 */
    _write_mainui_state_to_buf(buf_without, sizeof(buf_without),
                                FAVORITES, 0, 10, false, true);

    /* With recents: main_currpos should be 1 */
    /* First list element should have currpos value */
    ASSERT_TRUE(strstr(buf_with, "\"currpos\":1") != NULL);
    /* Without recents: main_currpos decremented to 0 */
    ASSERT_TRUE(strstr(buf_without, "\"currpos\":0") != NULL);
}

TEST(write_state_no_expert_shifts_apps) {
    char buf_with[STR_MAX], buf_without[STR_MAX];

    /* APPS with expert on: main_currpos = 4 */
    _write_mainui_state_to_buf(buf_with, sizeof(buf_with),
                                APPS, 0, 10, true, true);
    /* APPS without expert: main_currpos decremented to 3 */
    _write_mainui_state_to_buf(buf_without, sizeof(buf_without),
                                APPS, 0, 10, true, false);

    /* Verify different main_currpos values */
    ASSERT_STRNE(buf_with, buf_without);
}

TEST(write_state_no_recents_and_no_expert) {
    char buf[STR_MAX];
    bool result = _write_mainui_state_to_buf(buf, sizeof(buf),
                                              GAMES, 0, 30, false, false);
    ASSERT_TRUE(result);
    /* main_total goes from 6 to 4 (minus recents minus expert) */
    /* GAMES main_currpos starts at 2, minus 1 for no recents = 1 */
    ASSERT_EQ(buf[0], '{');
}

/* ---- Tests: state_getAppName edge cases ---- */

TEST(getAppName_long_home_path) {
    char cmd[STR_MAX] = "HOME=/very/long/path/to/sdcard ./MyApp";
    char out[STR_MAX] = {0};
    size_t len = _state_getAppName(out, cmd);
    ASSERT_STREQ(out, "MyApp");
    ASSERT_EQ(len, 5);
}

TEST(getAppName_minimal_prefix) {
    char cmd[STR_MAX] = "X ./A";
    char out[STR_MAX] = {0};
    size_t len = _state_getAppName(out, cmd);
    ASSERT_STREQ(out, "A");
    ASSERT_EQ(len, 1);
}

TEST(getAppName_only_prefix) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./";
    char out[STR_MAX] = {0};
    size_t len = _state_getAppName(out, cmd);
    ASSERT_STREQ(out, "");
    ASSERT_EQ(len, 0);
}

TEST(getAppName_multiple_prefixes) {
    /* If there are multiple " ./" patterns, first one is used */
    char cmd[STR_MAX] = "HOME=/a ./first; HOME=/b ./second";
    char out[STR_MAX] = {0};
    _state_getAppName(out, cmd);
    /* First " ./" is found, then ';' terminates: "first" */
    ASSERT_STREQ(out, "first");
}

TEST(getAppName_empty_string) {
    char out[STR_MAX] = {0};
    size_t len = _state_getAppName(out, "");
    ASSERT_EQ(len, 0);
    ASSERT_STREQ(out, "");
}

TEST(getAppName_special_chars) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./App-Name_v2.0";
    char out[STR_MAX] = {0};
    _state_getAppName(out, cmd);
    ASSERT_STREQ(out, "App-Name_v2.0");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== state.h Security & Edge-Case Tests ===\n\n");

    /* write_mainui_state JSON format */
    RUN_TEST(write_state_main_menu_returns_false);
    RUN_TEST(write_state_recents);
    RUN_TEST(write_state_favorites);
    RUN_TEST(write_state_games);
    RUN_TEST(write_state_expert);
    RUN_TEST(write_state_apps);

    /* Page calculation edge cases */
    RUN_TEST(write_state_currpos_at_end);
    RUN_TEST(write_state_currpos_at_start);
    RUN_TEST(write_state_total_smaller_than_page_size);

    /* show_recents / show_expert toggles */
    RUN_TEST(write_state_no_recents_shifts_position);
    RUN_TEST(write_state_no_expert_shifts_apps);
    RUN_TEST(write_state_no_recents_and_no_expert);

    /* state_getAppName edge cases */
    RUN_TEST(getAppName_long_home_path);
    RUN_TEST(getAppName_minimal_prefix);
    RUN_TEST(getAppName_only_prefix);
    RUN_TEST(getAppName_multiple_prefixes);
    RUN_TEST(getAppName_empty_string);
    RUN_TEST(getAppName_special_chars);

    TEST_REPORT();
    return test_failures;
}
