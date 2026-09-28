/**
 * @file test_state_scan.c
 * @brief Tests for src/keymon/state_scan.h (production header)
 *
 * While /tmp/state_changed exists keymon rescans the system state on key
 * events. It must rescan when the flag was touched again or is about to be
 * removed, and otherwise at most every STATE_SCAN_INTERVAL_MS.
 *
 * Build and run: make -f Makefile.unit test_state_scan
 */

#include "../src/keymon/state_scan.h"
#include "onion_test.h"

TEST(touched_flag_always_scans)
{
    ASSERT_TRUE(stateScan_due(true, false, 1000, 999));
}

TEST(removal_always_scans)
{
    ASSERT_TRUE(stateScan_due(false, true, 1000, 999));
}

TEST(keys_within_interval_do_not_scan)
{
    for (uint32_t t = 1001; t < 1000 + STATE_SCAN_INTERVAL_MS; t += 16)
        ASSERT_FALSE(stateScan_due(false, false, t, 1000));
}

TEST(interval_elapsed_scans)
{
    ASSERT_TRUE(stateScan_due(false, false, 1000 + STATE_SCAN_INTERVAL_MS, 1000));
    ASSERT_TRUE(stateScan_due(false, false, 20000, 1000));
}

/* A burst of key repeats (30 Hz for 15 s) costs about 2 scans per second
 * instead of one per event. */
TEST(key_burst_is_capped)
{
    uint32_t last = 0;
    int scans = 0;
    for (uint32_t t = 1000; t < 16000; t += 33) {
        if (stateScan_due(t == 1000, false, t, last)) {
            last = t;
            scans++;
        }
    }
    ASSERT_TRUE(scans <= 31);
    ASSERT_TRUE(scans >= 29);
}

TEST(tick_wraparound)
{
    ASSERT_FALSE(stateScan_due(false, false, 0x00000010u, 0xFFFFFFF0u));
    ASSERT_TRUE(stateScan_due(false, false, 0x00000200u, 0xFFFFFFF0u));
}

int main(void)
{
    printf("\n=== state_scan.h Unit Tests ===\n\n");
    RUN_TEST(touched_flag_always_scans);
    RUN_TEST(removal_always_scans);
    RUN_TEST(keys_within_interval_do_not_scan);
    RUN_TEST(interval_elapsed_scans);
    RUN_TEST(key_burst_is_capped);
    RUN_TEST(tick_wraparound);
    TEST_REPORT();
    return test_failures;
}
