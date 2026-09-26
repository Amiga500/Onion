/**
 * @file test_input_inject.c
 * @brief Tests for src/keymon/input_inject.h (production header)
 *
 * keymon injects keys into MainUI by writing input events itself instead
 * of running the sendkeys tool. The bytes must be what sendkeys wrote:
 * one struct input_event per key, type EV_KEY, the code and value given,
 * no EV_SYN.
 *
 * Build and run: make -f Makefile.unit test_input_inject
 */

#include "../src/keymon/input_inject.h"
#include "onion_test.h"

#include <fcntl.h>

TEST(writes_one_key_event)
{
    int p[2];
    ASSERT_EQ(0, pipe(p));
    ASSERT_TRUE(input_injectKey(p[1], 139, 1)); /* MENU pressed */
    ASSERT_TRUE(input_injectKey(p[1], 139, 0)); /* MENU released */
    close(p[1]);

    struct input_event ev[3];
    ssize_t n = read(p[0], ev, sizeof(ev));
    close(p[0]);
    ASSERT_EQ((ssize_t)(2 * sizeof(struct input_event)), n);
    ASSERT_EQ(EV_KEY, ev[0].type);
    ASSERT_EQ(139, ev[0].code);
    ASSERT_EQ(1, ev[0].value);
    ASSERT_EQ(EV_KEY, ev[1].type);
    ASSERT_EQ(139, ev[1].code);
    ASSERT_EQ(0, ev[1].value);
    ASSERT_EQ(0, (int)ev[0].time.tv_sec);
}

TEST(bad_fd_fails)
{
    ASSERT_FALSE(input_injectKey(-1, 139, 1));
    int fd = open("/dev/null", O_RDONLY);
    ASSERT_FALSE(input_injectKey(fd, 139, 1)); /* not writable */
    close(fd);
}

int main(void)
{
    printf("\n=== input_inject.h Unit Tests ===\n\n");
    RUN_TEST(writes_one_key_event);
    RUN_TEST(bad_fd_fails);
    TEST_REPORT();
    return test_failures;
}
