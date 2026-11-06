#include "onion_test.h"
#include "../src/gameSwitcher/gs_save_wait.h"

static uint32_t fake_ticks = 0;
static int sleep_calls = 0;

static uint32_t fake_get_ticks(void) { return fake_ticks; }
static int fake_sleep(long msec) { sleep_calls++; return 0; }

static bool fake_locked = false;
bool file_isLocked(const char *path) { (void)path; return fake_locked; }

void test_gs_wait_skips_when_not_saved(void)
{
    fake_ticks = 0;
    sleep_calls = 0;
    fake_locked = true;

    gs_waitStateFileRelease("test.state", false, 0, 5000, fake_get_ticks, fake_sleep);

    ASSERT_EQ(sleep_calls, 0);
}

void test_gs_wait_skips_when_path_null(void)
{
    fake_ticks = 0;
    sleep_calls = 0;
    fake_locked = true;

    gs_waitStateFileRelease(NULL, true, 0, 5000, fake_get_ticks, fake_sleep);

    ASSERT_EQ(sleep_calls, 0);
}

void test_gs_wait_exits_when_unlocked(void)
{
    fake_ticks = 0;
    sleep_calls = 0;
    fake_locked = false;

    gs_waitStateFileRelease("test.state", true, 0, 5000, fake_get_ticks, fake_sleep);

    ASSERT_EQ(sleep_calls, 0);
}

void test_gs_wait_respects_timeout(void)
{
    fake_ticks = 10000;
    sleep_calls = 0;
    fake_locked = true;

    gs_waitStateFileRelease("test.state", true, 0, 5000, fake_get_ticks, fake_sleep);

    ASSERT_EQ(sleep_calls, 0);
}

int main(void)
{
    RUN_TEST(gs_wait_skips_when_not_saved);
    RUN_TEST(gs_wait_skips_when_path_null);
    RUN_TEST(gs_wait_exits_when_unlocked);
    RUN_TEST(gs_wait_respects_timeout);

    TEST_REPORT();
    return 0;
}
