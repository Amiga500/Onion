/**
 * @file test_keymon_robust.c
 * @brief keymon: starting again after a crash, and input that fails
 *
 * crash_restart.h and input_fd.h, in real processes and on pipes:
 *  - a crash (SIGSEGV, SIGABRT) starts the program again with execve(), at
 *    most KEYMON_RESTART_MAX times, then it crashes as before; the counter
 *    is not duplicated in the environment, the rest of it is kept;
 *  - a child of keymon that crashes before running a program is not
 *    restarted (it would be a second keymon);
 *  - a descriptor opened with O_CLOEXEC (keymon's input device, and its
 *    grab) does not reach the restarted program;
 *  - a read that gets no whole event leaves no stale event behind (the
 *    previous key was handled again);
 *  - a grab that can't be taken no longer keeps keymon waiting forever;
 *  - the double-press wait after a MENU held over 300 ms is 0, not a
 *    negative poll() timeout (which waits forever).
 *
 * Build and run: make -f Makefile.unit test_keymon_robust
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

#include "utils/log.h"
#include "utils/msleep.h"

#include "../src/keymon/crash_restart.h"
#include "../src/keymon/double_press.h"
#include "../src/keymon/input_fd.h"

#define TEST_DIR "/tmp/onion_test_keymon_robust"
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

TEST(read_without_a_whole_event_leaves_none)
{
    int p[2];
    ASSERT_EQ(pipe(p), 0);
    input_fd = p[0];

    // A whole event.
    ASSERT_TRUE(input_injectKey(p[1], 116, 0)); // POWER released
    ASSERT_TRUE(keyinput_read());
    ASSERT_EQ(ev.code, 116);

    // Then half of one, and the end of the input: nothing is left in ev
    // (the POWER release above was handled a second time).
    struct input_event half;
    memset(&half, 0, sizeof(half));
    ASSERT_EQ(write(p[1], &half, sizeof(half) / 2), (ssize_t)(sizeof(half) / 2));
    close(p[1]);
    ASSERT_FALSE(keyinput_read());
    ASSERT_EQ(ev.code, 0);
    ASSERT_EQ(ev.type, 0);
    ASSERT_FALSE(keyinput_isValid());
    close(p[0]);
}

TEST(grab_that_fails_does_not_hang)
{
    // A pipe can't be grabbed: the old loop never ended.
    int p[2];
    ASSERT_EQ(pipe(p), 0);
    input_fd = p[0];
    keyinput_disabled = false;
    time_t start = time(NULL);
    keyinput_disable();
    ASSERT_FALSE(keyinput_disabled);
    ASSERT_TRUE(time(NULL) - start <= 4);

    // And an ungrab that fails still ends.
    keyinput_disabled = true;
    keyinput_enable();
    ASSERT_FALSE(keyinput_disabled);
    close(p[0]);
    close(p[1]);
}

TEST(double_press_wait_never_negative)
{
    // A slow tap (held 500 ms): no wait, not poll(-200), i.e. forever.
    ASSERT_EQ(menuButton_doublePressWait(10500, 10000), 0);
    ASSERT_EQ(menuButton_doublePressWait(10300, 10000), 0);
    ASSERT_EQ(menuButton_doublePressWait(10100, 10000), 200);
    ASSERT_EQ(menuButton_doublePressWait(10000, 10000), 300);
    ASSERT_EQ(menuButton_doublePressWait(9000, 10000), 300); // clock went back
}

int main(int argc, char *argv[])
{
    (void)fds; // input_fd.h's poll set: not used here
    if (argc >= 3 && strcmp(argv[1], "--child") == 0)
        return child_main(argv);

    printf("\n=== keymon: crash restart, input ===\n\n");
    RUN_TEST(crash_restarts_then_gives_up);
    RUN_TEST(abort_restarts_too);
    RUN_TEST(crashed_child_is_not_restarted);
    RUN_TEST(read_without_a_whole_event_leaves_none);
    RUN_TEST(grab_that_fails_does_not_hang);
    RUN_TEST(double_press_wait_never_negative);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
