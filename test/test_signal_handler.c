/**
 * @file test_signal_handler.c
 * @brief Unit tests for src/common/utils/signal_handler.h
 *
 * Tests the signal_handler_quit() inline function which sets a boolean flag
 * when receiving SIGINT or SIGTERM, and ignores other signals.
 *
 * Build and run: make -f Makefile.unit test_signal_handler
 */

#include "onion_test.h"
#include "../src/common/utils/signal_handler.h"
#include <signal.h>
#include <stdbool.h>

/* ---- Tests ---- */

TEST(sigint_sets_quit_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGINT);
    ASSERT_TRUE(quit);
}

TEST(sigterm_sets_quit_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGTERM);
    ASSERT_TRUE(quit);
}

TEST(sigusr1_does_not_set_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGUSR1);
    ASSERT_FALSE(quit);
}

TEST(sigusr2_does_not_set_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGUSR2);
    ASSERT_FALSE(quit);
}

TEST(sighup_does_not_set_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGHUP);
    ASSERT_FALSE(quit);
}

TEST(sigalrm_does_not_set_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGALRM);
    ASSERT_FALSE(quit);
}

TEST(zero_signal_does_not_set_flag) {
    volatile bool quit = false;
    signal_handler_quit(&quit, 0);
    ASSERT_FALSE(quit);
}

TEST(flag_stays_true_after_multiple_signals) {
    /* Once set, the flag should remain true regardless of further signals */
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGINT);
    ASSERT_TRUE(quit);

    /* Non-matching signal doesn't clear it */
    signal_handler_quit(&quit, SIGUSR1);
    ASSERT_TRUE(quit);

    /* Another matching signal keeps it true */
    signal_handler_quit(&quit, SIGTERM);
    ASSERT_TRUE(quit);
}

TEST(flag_initially_false_not_modified_by_unhandled) {
    /* Verify the flag is NOT modified by a stream of unhandled signals */
    volatile bool quit = false;
    int unhandled[] = {SIGUSR1, SIGUSR2, SIGHUP, SIGALRM, SIGPIPE, SIGCHLD};
    int num = (int)(sizeof(unhandled) / sizeof(unhandled[0]));

    for (int i = 0; i < num; i++) {
        signal_handler_quit(&quit, unhandled[i]);
    }
    ASSERT_FALSE(quit);
}

TEST(sigint_after_unhandled_signals) {
    /* First unhandled, then SIGINT should set the flag */
    volatile bool quit = false;
    signal_handler_quit(&quit, SIGUSR1);
    signal_handler_quit(&quit, SIGHUP);
    ASSERT_FALSE(quit);

    signal_handler_quit(&quit, SIGINT);
    ASSERT_TRUE(quit);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== signal_handler.h Unit Tests ===\n\n");

    RUN_TEST(sigint_sets_quit_flag);
    RUN_TEST(sigterm_sets_quit_flag);
    RUN_TEST(sigusr1_does_not_set_flag);
    RUN_TEST(sigusr2_does_not_set_flag);
    RUN_TEST(sighup_does_not_set_flag);
    RUN_TEST(sigalrm_does_not_set_flag);
    RUN_TEST(zero_signal_does_not_set_flag);
    RUN_TEST(flag_stays_true_after_multiple_signals);
    RUN_TEST(flag_initially_false_not_modified_by_unhandled);
    RUN_TEST(sigint_after_unhandled_signals);

    TEST_REPORT();
    return test_failures;
}
