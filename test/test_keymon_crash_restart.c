/**
 * @file test_keymon_crash_restart.c
 * @brief keymon: starting again after a crash
 *
 * crash_restart.h, in real processes (from test_keymon_robust.c on
 * v5-alpha, beabcc59, without its input and double-press cases):
 *  - a crash (SIGSEGV, SIGABRT) starts the program again with execve(), at
 *    most KEYMON_RESTART_MAX times, then it crashes as before; the counter
 *    is not duplicated in the environment, the rest of it is kept;
 *  - a child of keymon that crashes before running a program is not
 *    restarted (it would be a second keymon);
 *  - a descriptor opened with O_CLOEXEC (keymon's input device, and its
 *    grab) does not reach the restarted program;
 *
 * Build and run: make -f Makefile.unit test_keymon_crash_restart
 */

#include "onion_test.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../src/keymon/crash_restart.h"

#define TEST_DIR "/tmp/onion_test_keymon_crash_restart"
#define LOG TEST_DIR "/runs"

// Child mode (argv: exe, "--child", mode): note the run (restart count, whether the marked fd survived,
// how many counters the environment has), then crash.
static int child_main(char **argv)
{
    int restarts = crashRestart_install("/proc/self/exe", argv, environ);
    int counters = 0;
    for (char **e = environ; *e != NULL; e++)
        counters += strncmp(*e, KEYMON_RESTART_ENV "=", strlen(KEYMON_RESTART_ENV "=")) == 0;
    bool fd_open = fcntl(100, F_GETFD) != -1;
    FILE *f = fopen(LOG, "a");
    if (f != NULL) {
        fprintf(f, "%d %d %d %s\n", restarts, fd_open, counters,
                getenv("KEYMON_TEST_KEEP") != NULL ? "kept" : "lost");
        fclose(f);
    }
    if (restarts == 0) {
        // The first run opens fd 100 with O_CLOEXEC, as keymon its input.
        int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd >= 0 && fd != 100) {
            dup3(fd, 100, O_CLOEXEC);
            close(fd);
        }
    }
    if (strcmp(argv[2], "abort") == 0 && restarts == 2)
        abort();
    if (strcmp(argv[2], "forkcrash") == 0) {
        // A child that crashes before it runs a program: not restarted.
        pid_t child = fork();
        if (child == 0)
            raise(SIGSEGV);
        int status = 0;
        waitpid(child, &status, 0);
        f = fopen(LOG, "a");
        if (f != NULL) {
            fprintf(f, "child %d\n", WIFSIGNALED(status) ? WTERMSIG(status) : -1);
            fclose(f);
        }
        return 0;
    }
    raise(SIGSEGV);
    return 0;
}

static int run_child(const char *mode, int *runs, char lines[][64])
{
    if (system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR) != 0)
        return -1;
    pid_t pid = fork();
    if (pid == 0) {
        setenv("KEYMON_TEST_KEEP", "1", 1);
        unsetenv(KEYMON_RESTART_ENV);
        char *const argv[] = {"/proc/self/exe", "--child", (char *)mode, NULL};
        execv("/proc/self/exe", argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);

    *runs = 0;
    FILE *f = fopen(LOG, "r");
    if (f != NULL) {
        while (*runs < 16 && fgets(lines[*runs], 64, f) != NULL)
            (*runs)++;
        fclose(f);
    }
    return status;
}

TEST(crash_restarts_then_gives_up)
{
    int runs = 0;
    char lines[16][64];
    int status = run_child("segv", &runs, lines);
    ASSERT_EQ(runs, KEYMON_RESTART_MAX + 1); // the first run and 5 restarts
    ASSERT_TRUE(WIFSIGNALED(status));
    ASSERT_EQ(WTERMSIG(status), SIGSEGV); // then crashes as it used to
    char want[64];
    for (int i = 0; i < runs; i++) {
        // count, fd 100 not passed on, one counter at most, env kept
        snprintf(want, sizeof(want), "%d 0 %d kept\n", i, i > 0 ? 1 : 0);
        ASSERT_STREQ(lines[i], want);
    }
}

TEST(abort_restarts_too)
{
    int runs = 0;
    char lines[16][64];
    int status = run_child("abort", &runs, lines);
    ASSERT_EQ(runs, KEYMON_RESTART_MAX + 1);
    ASSERT_TRUE(WIFSIGNALED(status));
}

TEST(crashed_child_is_not_restarted)
{
    int runs = 0;
    char lines[16][64];
    int status = run_child("forkcrash", &runs, lines);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(runs, 2);
    ASSERT_STREQ(lines[0], "0 0 0 kept\n");
    ASSERT_STREQ(lines[1], "child 11\n");
}

int main(int argc, char *argv[])
{
    if (argc >= 3 && strcmp(argv[1], "--child") == 0)
        return child_main(argv);

    printf("\n=== keymon: crash restart ===\n\n");
    RUN_TEST(crash_restarts_then_gives_up);
    RUN_TEST(abort_restarts_too);
    RUN_TEST(crashed_child_is_not_restarted);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
