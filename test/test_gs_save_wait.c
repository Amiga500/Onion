/**
 * @file test_gs_save_wait.c
 * @brief Unit tests for gs_waitStateFileRelease (gs_save_wait.h)
 *
 * Production header, real file_isLocked() from file.c. Ticks and sleep are
 * faked so the 30 s deadline runs instantly.
 *
 * Regression: before this helper, gs_popMenu.h polled file_isLocked()
 * (O_RDONLY | O_CREAT) even after a failed save and before checking the
 * deadline, which left an empty state file behind.
 *
 * Build and run: make -f Makefile.unit test_gs_save_wait
 */

#include "onion_test.h"
#include "../src/gameSwitcher/gs_save_wait.h"
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_STATE_FILE "/tmp/onion_test_gs_save_wait.state"
#define TEST_BAD_PATH "/tmp/onion_test_gs_save_wait_no_dir/x.state"

static uint32_t fake_now = 0;
static int sleep_calls = 0;

static uint32_t fake_ticks(void) { return fake_now; }

static int fake_sleep(long msec)
{
    sleep_calls++;
    fake_now += (uint32_t)msec;
    return 0;
}

static void reset_fakes(uint32_t now)
{
    fake_now = now;
    sleep_calls = 0;
}

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

TEST(failed_save_does_not_create_state_file) {
    unlink(TEST_STATE_FILE);
    reset_fakes(1000);
    gs_waitStateFileRelease(TEST_STATE_FILE, false, 0, 30000, fake_ticks, fake_sleep);
    ASSERT_FALSE(file_exists(TEST_STATE_FILE));
    ASSERT_EQ(sleep_calls, 0);
}

TEST(expired_deadline_does_not_create_state_file) {
    unlink(TEST_STATE_FILE);
    reset_fakes(30000);
    gs_waitStateFileRelease(TEST_STATE_FILE, true, 0, 30000, fake_ticks, fake_sleep);
    ASSERT_FALSE(file_exists(TEST_STATE_FILE));
    ASSERT_EQ(sleep_calls, 0);
}

TEST(confirmed_save_existing_file_returns_immediately) {
    FILE *fp = fopen(TEST_STATE_FILE, "w");
    ASSERT_NOT_NULL(fp);
    fputs("state", fp);
    fclose(fp);

    reset_fakes(500);
    gs_waitStateFileRelease(TEST_STATE_FILE, true, 0, 30000, fake_ticks, fake_sleep);
    ASSERT_EQ(sleep_calls, 0);
    ASSERT_TRUE(file_exists(TEST_STATE_FILE));
    unlink(TEST_STATE_FILE);
}

TEST(locked_path_stops_at_deadline) {
    /* Uncreatable path: file_isLocked() reports true forever. */
    reset_fakes(0);
    gs_waitStateFileRelease(TEST_BAD_PATH, true, 0, 30000, fake_ticks, fake_sleep);
    ASSERT_EQ(sleep_calls, 300);
    ASSERT_EQ(fake_now, 30000u);
}

TEST(deadline_survives_tick_wraparound) {
    uint32_t start = UINT32_MAX - 50;
    reset_fakes(start);
    gs_waitStateFileRelease(TEST_BAD_PATH, true, start, 1000, fake_ticks, fake_sleep);
    ASSERT_EQ(sleep_calls, 10);
}

TEST(null_path_is_noop) {
    reset_fakes(0);
    gs_waitStateFileRelease(NULL, true, 0, 30000, fake_ticks, fake_sleep);
    ASSERT_EQ(sleep_calls, 0);
}

int main(void)
{
    printf("\n=== gs_save_wait Unit Tests ===\n\n");

    RUN_TEST(failed_save_does_not_create_state_file);
    RUN_TEST(expired_deadline_does_not_create_state_file);
    RUN_TEST(confirmed_save_existing_file_returns_immediately);
    RUN_TEST(locked_path_stops_at_deadline);
    RUN_TEST(deadline_survives_tick_wraparound);
    RUN_TEST(null_path_is_noop);

    TEST_REPORT();
    return test_failures;
}
