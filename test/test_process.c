/**
 * @file test_process.c
 * @brief Unit tests for src/common/utils/process.h
 *
 * Tests process_searchpid and process_isRunning using known Linux processes.
 *
 * Build and run: make -f Makefile.unit test_process && ./build_test/test_process
 */

#include "onion_test.h"
#include <unistd.h>
#include <string.h>

#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"
#include "../src/common/utils/process.h"

/* ---- process_searchpid ---- */

TEST(process_searchpid_finds_self) {
    /* PID 1 (init/systemd) is always running but is skipped (pid > 2 check).
     * Let's search for the test process itself instead. */
    char self_comm[128];
    char fname[64];
    snprintf(fname, sizeof(fname), "/proc/%d/comm", (int)getpid());
    FILE *fp = fopen(fname, "r");
    ASSERT_NOT_NULL(fp);
    if (fscanf(fp, "%127s", self_comm) != 1)
        self_comm[0] = '\0';
    fclose(fp);

    pid_t found = process_searchpid(self_comm);
    /* Should find our own process */
    ASSERT_TRUE(found > 0);
}

TEST(process_searchpid_not_found) {
    pid_t found = process_searchpid("nonexistent_process_xyz_123");
    ASSERT_EQ(found, 0);
}

TEST(process_searchpid_empty_name) {
    /* Empty string has strlen 0, so strncmp with len 0 always matches.
     * This is expected behavior — matches first pid > 2. */
    pid_t found = process_searchpid("");
    ASSERT_TRUE(found > 0);
}

/* ---- process_isRunning ---- */

TEST(process_isRunning_self) {
    char self_comm[128];
    char fname[64];
    snprintf(fname, sizeof(fname), "/proc/%d/comm", (int)getpid());
    FILE *fp = fopen(fname, "r");
    ASSERT_NOT_NULL(fp);
    if (fscanf(fp, "%127s", self_comm) != 1)
        self_comm[0] = '\0';
    fclose(fp);

    ASSERT_TRUE(process_isRunning(self_comm));
}

TEST(process_isRunning_not_found) {
    ASSERT_FALSE(process_isRunning("definitely_not_running_99"));
}

/* ---- process_searchpid forward match ---- */

TEST(process_searchpid_partial_match) {
    /* process_searchpid uses forward match (strncmp with commlen).
     * Searching for "test" should match "test_process" etc. */
    char self_comm[128];
    char fname[64];
    snprintf(fname, sizeof(fname), "/proc/%d/comm", (int)getpid());
    FILE *fp = fopen(fname, "r");
    ASSERT_NOT_NULL(fp);
    if (fscanf(fp, "%127s", self_comm) != 1)
        self_comm[0] = '\0';
    fclose(fp);

    /* Use only first 4 chars as partial match */
    if (strlen(self_comm) > 4) {
        char partial[5];
        strncpy(partial, self_comm, 4);
        partial[4] = '\0';
        pid_t found = process_searchpid(partial);
        ASSERT_TRUE(found > 0);
    }
}

/* ---- process_killall / process_killall_signal ---- */

TEST(process_killall_signal_missing_is_noop) {
    process_killall_signal("definitely_not_running_99", SIGTERM);
    ASSERT_FALSE(process_isRunning("definitely_not_running_99"));
}

TEST(process_killall_missing_is_noop) {
    process_killall("definitely_not_running_99");
    ASSERT_FALSE(process_isRunning("definitely_not_running_99"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== process.h Unit Tests ===\n\n");

    RUN_TEST(process_searchpid_finds_self);
    RUN_TEST(process_searchpid_not_found);
    RUN_TEST(process_searchpid_empty_name);
    RUN_TEST(process_isRunning_self);
    RUN_TEST(process_isRunning_not_found);
    RUN_TEST(process_searchpid_partial_match);
    RUN_TEST(process_killall_signal_missing_is_noop);
    RUN_TEST(process_killall_missing_is_noop);

    TEST_REPORT();
    return test_failures;
}
