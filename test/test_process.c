/**
 * @file test_process.c
 * @brief Unit tests for src/common/utils/process.h
 *
 * Tests process_searchpid and process_isRunning using known Linux processes.
 *
 * Build and run: make -f Makefile.unit test_process && ./build_test/test_process
 */

#include "onion_test.h"
#include <signal.h>
#include <sys/stat.h>
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

/* With SIGCHLD ignored, waitpid() fails with ECHILD and status was read
 * uninitialised. The spawn itself still happens. */
TEST(process_spawn_detached_with_sigchld_ignored) {
    const char *marker = "/tmp/onion_test_spawn_marker";
    unlink(marker);
    void (*old)(int) = signal(SIGCHLD, SIG_IGN);
    char *const argv[] = {"touch", (char *)marker, NULL};
    ASSERT_TRUE(process_spawn_detached(argv));
    signal(SIGCHLD, old);
    for (int i = 0; i < 100 && access(marker, F_OK) != 0; i++)
        usleep(10000);
    ASSERT_EQ(access(marker, F_OK), 0);
    unlink(marker);
}

TEST(process_spawn_detached_runs_program) {
    const char *marker = "/tmp/onion_test_spawn_marker2";
    unlink(marker);
    char *const argv[] = {"touch", (char *)marker, NULL};
    ASSERT_TRUE(process_spawn_detached(argv));
    for (int i = 0; i < 100 && access(marker, F_OK) != 0; i++)
        usleep(10000);
    ASSERT_EQ(access(marker, F_OK), 0);
    unlink(marker);
}

/* ---- process_readFirstLine (axp_test without popen) ---- */

static char rfl_dir[] = "/tmp/onion_rfl_XXXXXX";

static void rfl_tool(const char *body)
{
    char path[64];
    snprintf(path, sizeof(path), "%s/tool", rfl_dir);
    FILE *fp = fopen(path, "w");
    fprintf(fp, "#!/bin/sh\n%s\n", body);
    fclose(fp);
    chmod(path, 0755);
}

TEST(read_first_line_like_fgets) {
    char buf[100];
    rfl_tool("echo '{\"battery\":87, \"voltage\":4011, \"charging\":3}'; echo second");
    ASSERT_TRUE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    ASSERT_STREQ("{\"battery\":87, \"voltage\":4011, \"charging\":3}\n", buf);
}

/* The tool runs in dir (axp_test is started as ./axp_test from
 * /customer/app). */
TEST(read_first_line_runs_in_dir) {
    char buf[256];
    rfl_tool("pwd");
    ASSERT_TRUE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    char expected[256];
    snprintf(expected, sizeof(expected), "%s\n", rfl_dir);
    ASSERT_STREQ(expected, buf);
}

TEST(read_first_line_truncates_like_fgets) {
    char buf[6];
    rfl_tool("echo 0123456789");
    ASSERT_TRUE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    ASSERT_STREQ("01234", buf);
}

TEST(read_first_line_no_newline_and_empty) {
    char buf[32];
    rfl_tool("printf abc");
    ASSERT_TRUE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    ASSERT_STREQ("abc", buf);
    rfl_tool("true");
    ASSERT_FALSE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    ASSERT_STREQ("", buf);
}

TEST(read_first_line_missing_program) {
    char buf[32] = "junk";
    ASSERT_FALSE(process_readFirstLine(rfl_dir, "./missing", buf, sizeof(buf)));
    ASSERT_STREQ("", buf);
    ASSERT_FALSE(process_readFirstLine("/nonexistent_dir", "./tool", buf, sizeof(buf)));
}

/* A long output: the child must not block (the rest is drained). */
TEST(read_first_line_long_output) {
    char buf[16];
    rfl_tool("echo first; i=0; while [ $i -lt 4000 ]; do echo 0123456789012345678901234567890; i=$((i+1)); done");
    ASSERT_TRUE(process_readFirstLine(rfl_dir, "./tool", buf, sizeof(buf)));
    ASSERT_STREQ("first\n", buf);
}

int main(void)
{
    printf("\n=== process.h Unit Tests ===\n\n");

    if (mkdtemp(rfl_dir) == NULL)
        return 1;
    RUN_TEST(read_first_line_like_fgets);
    RUN_TEST(read_first_line_runs_in_dir);
    RUN_TEST(read_first_line_truncates_like_fgets);
    RUN_TEST(read_first_line_no_newline_and_empty);
    RUN_TEST(read_first_line_missing_program);
    RUN_TEST(read_first_line_long_output);
    RUN_TEST(process_searchpid_finds_self);
    RUN_TEST(process_searchpid_not_found);
    RUN_TEST(process_searchpid_empty_name);
    RUN_TEST(process_isRunning_self);
    RUN_TEST(process_isRunning_not_found);
    RUN_TEST(process_searchpid_partial_match);
    RUN_TEST(process_killall_signal_missing_is_noop);
    RUN_TEST(process_killall_missing_is_noop);
    RUN_TEST(process_spawn_detached_with_sigchld_ignored);
    RUN_TEST(process_spawn_detached_runs_program);

    {
        char tool[64];
        snprintf(tool, sizeof(tool), "%s/tool", rfl_dir);
        remove(tool);
        rmdir(rfl_dir);
    }
    TEST_REPORT();
    return test_failures;
}
