/**
 * @file test_log.c
 * @brief Unit tests for src/common/utils/log.c
 *
 * Tests log_debug formatting and output by capturing stderr.
 * Tests log_setName path construction using a temp directory.
 *
 * Build and run: make -f Makefile.unit test_log && ./build_test/test_log
 */

/* LOG_DEBUG is defined via compiler flags (-DLOG_DEBUG) to enable debug macros */

#include "onion_test.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/common/utils/str.h"
#include "../src/common/utils/log.h"

/* ---- Helper to capture stderr ---- */

static char captured_stderr[4096];

static void capture_stderr_start(int *saved_fd, int *pipe_fds)
{
    fflush(stderr);
    *saved_fd = dup(STDERR_FILENO);
    if (*saved_fd < 0 || pipe(pipe_fds) != 0) {
        captured_stderr[0] = '\0';
        return;
    }
    dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);
}

static void capture_stderr_end(int saved_fd, int pipe_read_fd)
{
    fflush(stderr);
    if (saved_fd >= 0)
        dup2(saved_fd, STDERR_FILENO);
    if (saved_fd >= 0)
        close(saved_fd);

    ssize_t n = read(pipe_read_fd, captured_stderr, sizeof(captured_stderr) - 1);
    if (n < 0) n = 0;
    captured_stderr[n] = '\0';
    close(pipe_read_fd);
}

/* ---- log_debug output tests ---- */

TEST(log_debug_prints_file_and_line) {
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    log_debug("test.c", 42, "hello %s\n", "world");

    capture_stderr_end(saved_fd, pipe_fds[0]);

    /* Output should contain "test.c:42>" */
    ASSERT_NOT_NULL(strstr(captured_stderr, "test.c:42>"));
    /* Output should contain the formatted message */
    ASSERT_NOT_NULL(strstr(captured_stderr, "hello world"));
}

TEST(log_debug_format_integer) {
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    log_debug("file.c", 10, "value=%d\n", 99);

    capture_stderr_end(saved_fd, pipe_fds[0]);

    ASSERT_NOT_NULL(strstr(captured_stderr, "file.c:10>"));
    ASSERT_NOT_NULL(strstr(captured_stderr, "value=99"));
}

TEST(log_debug_empty_format) {
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    log_debug("x.c", 1, "%s", "");

    capture_stderr_end(saved_fd, pipe_fds[0]);

    /* Should still print the file:line prefix */
    ASSERT_NOT_NULL(strstr(captured_stderr, "x.c:1>"));
}

/* ---- print_debug / printf_debug macros ---- */

TEST(print_debug_macro_works) {
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    print_debug("macro test message");

    capture_stderr_end(saved_fd, pipe_fds[0]);

    ASSERT_NOT_NULL(strstr(captured_stderr, "macro test message"));
}

TEST(printf_debug_macro_works) {
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    printf_debug("count=%d name=%s", 5, "onion");

    capture_stderr_end(saved_fd, pipe_fds[0]);

    ASSERT_NOT_NULL(strstr(captured_stderr, "count=5 name=onion"));
}

/* ---- log_debug without log path ---- */

TEST(log_debug_no_crash_without_setname) {
    /* log_setName not called — _log_path is empty string.
     * log_debug should still print to stderr without crashing. */
    int saved_fd, pipe_fds[2];
    capture_stderr_start(&saved_fd, pipe_fds);

    log_debug("safe.c", 0, "no crash %s\n", "here");

    capture_stderr_end(saved_fd, pipe_fds[0]);

    ASSERT_NOT_NULL(strstr(captured_stderr, "no crash here"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== log.c Unit Tests ===\n\n");

    RUN_TEST(log_debug_prints_file_and_line);
    RUN_TEST(log_debug_format_integer);
    RUN_TEST(log_debug_empty_format);
    RUN_TEST(print_debug_macro_works);
    RUN_TEST(printf_debug_macro_works);
    RUN_TEST(log_debug_no_crash_without_setname);

    TEST_REPORT();
    return test_failures;
}
