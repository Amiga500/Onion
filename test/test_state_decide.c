/**
 * @file test_state_decide.c
 * @brief Tests for src/common/system/state_decide.h (production header):
 *        which program keymon takes as the one in front.
 *
 * Build and run: make -f Makefile.unit test_state_decide
 */

#include "../src/common/system/state_decide.h"
#include "onion_test.h"

static SystemState decide(bool switcher_flag, bool has_cmd, bool ra_cmd, const pid_t pids[STATE_P_COUNT],
                          pid_t *pid)
{
    *pid = 0;
    return system_state_decide(switcher_flag, has_cmd, ra_cmd, pids, pid);
}

TEST(mainui_alone_is_mainui)
{
    pid_t pids[STATE_P_COUNT] = {0};
    pids[STATE_P_MAINUI] = 100;
    pid_t pid;
    ASSERT_EQ(decide(false, false, false, pids, &pid), MODE_MAIN_UI);
    ASSERT_EQ(pid, 100);
}

/* Game Info > Manual: Open MainUI runs Green as its child, no cmd_to_run.sh. */
TEST(pdf_reader_opened_by_mainui_is_an_app)
{
    pid_t pids[STATE_P_COUNT] = {0};
    pids[STATE_P_MAINUI] = 100;
    pids[STATE_P_GREEN] = 200;
    pid_t pid;
    ASSERT_EQ(decide(false, false, false, pids, &pid), MODE_APPS);
    ASSERT_EQ(pid, 0); /* the MainUI pid is not taken: MENU must not act on it */
}

/* The PDF Reader app (Apps tab): launched through cmd_to_run.sh. */
TEST(pdf_reader_app_is_an_app)
{
    pid_t pids[STATE_P_COUNT] = {0};
    pids[STATE_P_GREEN] = 200;
    pid_t pid;
    ASSERT_EQ(decide(false, true, false, pids, &pid), MODE_APPS);
}

TEST(order_unchanged)
{
    pid_t pids[STATE_P_COUNT] = {0};
    pid_t pid;
    pids[STATE_P_SWITCHER] = 10;
    pids[STATE_P_RETROARCH] = 20;
    pids[STATE_P_MAINUI] = 30;
    ASSERT_EQ(decide(true, true, true, pids, &pid), MODE_SWITCHER);
    ASSERT_EQ(pid, 10);
    ASSERT_EQ(decide(false, true, true, pids, &pid), MODE_GAME);
    ASSERT_EQ(pid, 20);
    pids[STATE_P_RETROARCH] = 0;
    pids[STATE_P_RA32] = 21;
    ASSERT_EQ(decide(false, true, true, pids, &pid), MODE_GAME);
    ASSERT_EQ(pid, 21);
    /* a game's Green would not change a game */
    pids[STATE_P_GREEN] = 40;
    ASSERT_EQ(decide(false, true, true, pids, &pid), MODE_GAME);
    pids[STATE_P_GREEN] = 0;
    pids[STATE_P_RA32] = 0;
    pids[STATE_P_MAINUI] = 0;
    pids[STATE_P_ADVMENU] = 50;
    ASSERT_EQ(decide(false, false, false, pids, &pid), MODE_ADVMENU);
    pids[STATE_P_ADVMENU] = 0;
    pids[STATE_P_DRASTIC] = 60;
    ASSERT_EQ(decide(false, true, false, pids, &pid), MODE_DRASTIC);
    ASSERT_EQ(pid, 60);
    pids[STATE_P_DRASTIC] = 0;
    ASSERT_EQ(decide(false, true, false, pids, &pid), MODE_APPS);
}

int main(void)
{
    RUN_TEST(mainui_alone_is_mainui);
    RUN_TEST(pdf_reader_opened_by_mainui_is_an_app);
    RUN_TEST(pdf_reader_app_is_an_app);
    RUN_TEST(order_unchanged);
    TEST_REPORT();
    return test_failures;
}
