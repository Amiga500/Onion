/**
 * @file test_keymon_actions.c
 * @brief keymon functions that cannot be included on the host (keymon.c,
 * menuButtonAction.h), cut from the real sources by the Makefile and run
 * here with stand-ins for what they call.
 *
 * Build and run: make -f Makefile.unit test_keymon_actions
 */

#include "onion_test.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_keymon_actions"
#define CMD_TO_RUN_PATH TEST_DIR "/cmd_to_run.sh"
#define SUSPEND_STATE_PATH TEST_DIR "/keymon_suspended"
#define POWERSAVE_STATE_PATH TEST_DIR "/powersave_state"

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

static int powersave_recovers = 0;
static void system_powersave_recover(void)
{
    powersave_recovers++;
}

#define MAX_CONT 8
static int cont_pids[MAX_CONT];
static int cont_count = 0;
static int test_kill(pid_t pid, int sig)
{
    (void)sig;
    if (cont_count < MAX_CONT)
        cont_pids[cont_count++] = (int)pid;
    return 0;
}
#define kill test_kill

static struct {
    int brightness, volume;
    bool mute;
} settings = {7, 10, false};
static int display_ons = 0;
static void display_on(void) { display_ons++; }
static void display_setBrightness(int value) { (void)value; }
static void setVolume(int value) { (void)value; }
#define print_debug(str) ((void)0)

static int play_activity_resumes = 0;
static bool process_spawn_detached(char *const argv[])
{
    if (argv[0] != NULL && argv[1] != NULL && argv[2] == NULL &&
        strcmp(argv[0], "playActivity") == 0 && strcmp(argv[1], "resume") == 0)
        play_activity_resumes++;
    return true;
}

static void recover_reset(void)
{
    reset_dir();
    powersave_recovers = 0;
    cont_count = 0;
    display_ons = 0;
    play_activity_resumes = 0;
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
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

/* keymon starting again during a standby (after a crash): what
 * suspend_exec() does on a wake-up, the play time included. */
TEST(recover_wakes_up_and_counts_the_play_time_again)
{
    recover_reset();
    write_file(SUSPEND_STATE_PATH, "120\n345\n2\n");
    write_file(POWERSAVE_STATE_PATH, "1200 ondemand\n");
    suspend_recover();
    ASSERT_EQ(powersave_recovers, 1);
    ASSERT_EQ(cont_count, 2); /* not pid 2 */
    ASSERT_EQ(cont_pids[0], 120);
    ASSERT_EQ(cont_pids[1], 345);
    ASSERT_EQ(display_ons, 1);
    ASSERT_EQ(play_activity_resumes, 1);
    ASSERT_FALSE(exists(SUSPEND_STATE_PATH));
}

TEST(recover_without_a_standby_does_nothing)
{
    recover_reset();
    suspend_recover();
    ASSERT_EQ(powersave_recovers, 0);
    ASSERT_EQ(display_ons, 0);
    ASSERT_EQ(play_activity_resumes, 0);
}

/* Powersave alone (left by chargingState, before any game): the CPU goes
 * back, no play time is opened for the game about to be resumed at boot. */
TEST(recover_powersave_alone_opens_no_play_time)
{
    recover_reset();
    write_file(POWERSAVE_STATE_PATH, "1200 ondemand\n");
    suspend_recover();
    ASSERT_EQ(powersave_recovers, 1);
    ASSERT_EQ(cont_count, 0);
    ASSERT_EQ(play_activity_resumes, 0);
}

int main(void)
{
    printf("\n=== keymon: actions cut from the sources ===\n\n");
    RUN_TEST(resume_game_without_a_game_leaves_mainui_alone);
    RUN_TEST(resume_game_with_a_game_ends_mainui);
    RUN_TEST(recover_wakes_up_and_counts_the_play_time_again);
    RUN_TEST(recover_without_a_standby_does_nothing);
    RUN_TEST(recover_powersave_alone_opens_no_play_time);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
