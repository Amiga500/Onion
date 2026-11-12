/**
 * @file test_keymon_actions.c
 * @brief keymon functions that cannot be included on the host (keymon.c,
 * menuButtonAction.h), cut from the real sources by the Makefile and run
 * here with stand-ins for what they call.
 *
 * Build and run: make -f Makefile.unit test_keymon_actions
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_keymon_actions"
#define CMD_TO_RUN_PATH TEST_DIR "/cmd_to_run.sh"

static void touch(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f != NULL)
        fclose(f);
}

static void reset_dir(void)
{
    if (system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR) != 0)
        fprintf(stderr, "reset failed\n");
}

/* ---- stand-ins ---- */

static bool exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static bool recent_list_has_a_game = false;
static void set_resumeGame(void)
{
    // state.h resumeGame(0): writes the command only when a game is found
    if (recent_list_has_a_game)
        touch(CMD_TO_RUN_PATH);
}

static int mainui_kills = 0;
static void kill_mainUI(void)
{
    mainui_kills++;
}

#include KEYMON_FNS

/* MENU "Resume game" in MainUI with nothing to resume: MainUI stays (it was
 * killed and started again with no command left, which runtime.sh counts as
 * a quick exit nobody asked for). */
TEST(resume_game_without_a_game_leaves_mainui_alone)
{
    reset_dir();
    recent_list_has_a_game = false;
    mainui_kills = 0;
    action_MainUI_resumeGame();
    ASSERT_EQ(mainui_kills, 0);
}

TEST(resume_game_with_a_game_ends_mainui)
{
    reset_dir();
    recent_list_has_a_game = true;
    mainui_kills = 0;
    action_MainUI_resumeGame();
    ASSERT_EQ(mainui_kills, 1);
    ASSERT_TRUE(exists(CMD_TO_RUN_PATH));
}

int main(void)
{
    printf("\n=== keymon: actions cut from the sources ===\n\n");
    RUN_TEST(resume_game_without_a_game_leaves_mainui_alone);
    RUN_TEST(resume_game_with_a_game_ends_mainui);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
