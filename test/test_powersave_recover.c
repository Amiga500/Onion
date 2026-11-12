/**
 * @file test_powersave_recover.c
 * @brief system_powersave_recover(): undo a standby that keymon did not end
 *
 * A fake cpuclock in a temporary PATH records what it is asked to set. The
 * governor is not part of the state written here, so nothing is written to
 * /sys (the host's governor stays as it is).
 *
 * Build and run: make -f Makefile.unit test_powersave_recover
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_powersave"
#define POWERSAVE_STATE_PATH TEST_DIR "/powersave_state"
#define CLOCK_LOG TEST_DIR "/cpuclock.log"
#define CPU_DIR TEST_DIR "/"

#include "system/system.h"

static void reset_dir(void)
{
    if (system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR "/bin") != 0)
        fprintf(stderr, "reset failed\n");
    FILE *f = fopen(TEST_DIR "/bin/cpuclock", "w");
    if (f != NULL) {
        // with no argument it prints the clock, as the real one
        fputs("#!/bin/sh\necho \"$@\" >> " CLOCK_LOG "\n[ $# -eq 0 ] && echo 1200\nexit 0\n", f);
        fclose(f);
    }
    chmod(TEST_DIR "/bin/cpuclock", 0755);
    setenv("PATH", TEST_DIR "/bin:/usr/bin:/bin", 1);
}

static void write_state(const char *text)
{
    FILE *f = fopen(POWERSAVE_STATE_PATH, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

// Number of calls of the fake cpuclock; the first call's argument in `arg`.
static int clock_calls(char *arg, size_t size)
{
    FILE *f = fopen(CLOCK_LOG, "r");
    int n = 0;
    char line[64];
    if (arg != NULL && size > 0)
        arg[0] = '\0';
    if (f == NULL)
        return 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (n == 0 && arg != NULL) {
            line[strcspn(line, "\n")] = '\0';
            snprintf(arg, size, "%s", line);
        }
        n++;
    }
    fclose(f);
    return n;
}

TEST(powersave_off_without_on_does_nothing)
{
    reset_dir();
    system_powersave(false); // nothing was saved: no "cpuclock 0"
    ASSERT_EQ(clock_calls(NULL, 0), 0);
}

static void write_cpu(const char *name, const char *text)
{
    char path[128];
    snprintf(path, sizeof(path), "%s%s", CPU_DIR, name);
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static void read_cpu(const char *name, char *out, size_t size)
{
    char path[128];
    snprintf(path, sizeof(path), "%s%s", CPU_DIR, name);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (f != NULL) {
        if (fgets(out, (int)size, f) == NULL)
            out[0] = '\0';
        fclose(f);
    }
    out[strcspn(out, "\n")] = '\0';
}

/* Powersave on leaves what a restarted keymon needs in the state file, and
 * the recovery puts both back: the clock and the governor. */
TEST(powersave_on_saves_what_the_recovery_needs)
{
    char text[64];
    reset_dir();
    write_cpu("scaling_governor", "ondemand\n");
    write_cpu("scaling_min_freq", "400000\n");
    system_powersave(true);
    FILE *f = fopen(POWERSAVE_STATE_PATH, "r");
    ASSERT_TRUE(f != NULL);
    if (f != NULL) {
        text[0] = '\0';
        if (fgets(text, sizeof(text), f) == NULL)
            text[0] = '\0';
        fclose(f);
        ASSERT_STREQ(text, "1200 ondemand\n");
    }
    read_cpu("scaling_governor", text, sizeof(text));
    ASSERT_STREQ(text, "powersave");

    /* keymon ended here; the next one finds the state */
    remove(CLOCK_LOG);
    system_powersave_recover();
    char arg[32];
    ASSERT_EQ(clock_calls(arg, sizeof(arg)), 1);
    ASSERT_STREQ(arg, "1200");
    read_cpu("scaling_governor", text, sizeof(text));
    ASSERT_STREQ(text, "ondemand");
    ASSERT_TRUE(access(POWERSAVE_STATE_PATH, F_OK) != 0);
}

TEST(recover_without_state_does_nothing)
{
    reset_dir();
    system_powersave_recover();
    ASSERT_EQ(clock_calls(NULL, 0), 0);
}

TEST(recover_restores_the_clock_and_removes_the_state)
{
    char arg[64];
    reset_dir();
    write_state("1600\n");
    system_powersave_recover();
    ASSERT_EQ(clock_calls(arg, sizeof(arg)), 1);
    ASSERT_STREQ(arg, "1600");
    ASSERT_TRUE(access(POWERSAVE_STATE_PATH, F_OK) != 0);
}

TEST(recover_twice_runs_once)
{
    reset_dir();
    write_state("1200\n");
    system_powersave_recover();
    system_powersave_recover();
    ASSERT_EQ(clock_calls(NULL, 0), 1);
}

TEST(recover_ignores_garbage_and_zero)
{
    reset_dir();
    write_state("abc\n");
    system_powersave_recover();
    ASSERT_EQ(clock_calls(NULL, 0), 0);
    ASSERT_TRUE(access(POWERSAVE_STATE_PATH, F_OK) != 0);

    write_state("0\n");
    system_powersave_recover();
    ASSERT_EQ(clock_calls(NULL, 0), 0);

    write_state("");
    system_powersave_recover();
    ASSERT_EQ(clock_calls(NULL, 0), 0);
}

TEST(recover_handles_the_largest_value)
{
    char arg[64];
    reset_dir();
    write_state("4294967295\n");
    system_powersave_recover();
    ASSERT_EQ(clock_calls(arg, sizeof(arg)), 1);
    ASSERT_STREQ(arg, "4294967295");
}

int main(void)
{
    printf("\n=== system.h: powersave recovery ===\n\n");
    RUN_TEST(powersave_off_without_on_does_nothing); // first: uses a static
    RUN_TEST(recover_without_state_does_nothing);
    RUN_TEST(recover_restores_the_clock_and_removes_the_state);
    RUN_TEST(recover_twice_runs_once);
    RUN_TEST(recover_ignores_garbage_and_zero);
    RUN_TEST(recover_handles_the_largest_value);
    RUN_TEST(powersave_on_saves_what_the_recovery_needs); // last: sets the static
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
