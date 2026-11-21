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

/* ---- The production code: write_mainui_state() and state_getAppName()
 * from system/state.h (they used to be copied here, and the copy had lost
 * the source's two lower bounds), with the state file in /tmp/<pid>. ---- */

static char state_file[64];
#define MAINUI_STATE_FILE state_file
#include "system/state.h"

/* write_mainui_state() with the two MainUI settings it reads, then the file
 * it wrote; false when it wrote none (MAIN_MENU removes it). */
static bool _write_mainui_state_to_buf(char *buf, size_t buf_size,
                                       MainUIState state, int currpos,
                                       int total,
                                       bool show_recents, bool show_expert)
{
    snprintf(state_file, sizeof(state_file), "/tmp/onion_test_state_%d.json", (int)getpid());
    remove(state_file);
    settings.show_recents = show_recents;
    settings.show_expert = show_expert;
    write_mainui_state(state, currpos, total);
    buf[0] = '\0';
    FILE *fp = fopen(state_file, "r");
    if (fp == NULL)
        return false;
    size_t n = fread(buf, 1, buf_size - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    remove(state_file);
    return true;
}

#define _state_getAppName state_getAppName

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
    /* Fewer entries than a page: the page starts at 0, never below (the
     * old copy of the function said -6 here, the source never did). */
    char buf[STR_MAX];
    _write_mainui_state_to_buf(buf, sizeof(buf),
                                EXPERT, 0, 3, true, true);
    /* EXPERT page_size = 9, total = 3 */
    ASSERT_NOT_NULL(strstr(buf, "\"currpos\":0,\"pagestart\":0,\"pageend\":8"));
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
