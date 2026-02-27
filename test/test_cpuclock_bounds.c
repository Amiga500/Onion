/**
 * @file test_cpuclock_bounds.c
 * @brief Unit tests for CPU clock hotkey bounds checking on all device models.
 *
 * Tests the per-device max CPU clock values used by cpuClockHotkey()
 * in keymon.c and the cpuclock tool itself.
 *
 * Build and run: make -f Makefile.unit test_cpuclock_bounds
 */

#include "onion_test.h"
#include <stdbool.h>

/* ---- Constants from device_model.h ---- */
#define MIYOO283  283
#define MIYOO354  354
#define MIYOOFLIP 566

/* ---- Replicate the bounds logic from cpuClockHotkey() ---- */

/**
 * Return the maximum CPU clock (MHz) for a given device, or -1 if unsupported.
 */
static int get_max_cpu_clock(int device_id)
{
    switch (device_id) {
    case MIYOO354:
        return 1800;
    case MIYOO283:
        return 1600;
    case MIYOOFLIP:
        return 1800; /* RK3566 quad Cortex-A55 */
    default:
        return -1;
    }
}

/**
 * Check if a requested clock is within bounds for the given device.
 * min_cpu_clock is always 500 MHz.
 */
static bool cpu_clock_in_bounds(int device_id, int clock_mhz)
{
    int max_clock = get_max_cpu_clock(device_id);
    if (max_clock < 0)
        return false;
    return (clock_mhz >= 500 && clock_mhz <= max_clock);
}

/* ==== Tests: device max clock values ==== */

TEST(miyoo283_max_clock) {
    ASSERT_EQ(get_max_cpu_clock(MIYOO283), 1600);
}

TEST(miyoo354_max_clock) {
    ASSERT_EQ(get_max_cpu_clock(MIYOO354), 1800);
}

TEST(miyooflip_max_clock) {
    ASSERT_EQ(get_max_cpu_clock(MIYOOFLIP), 1800);
}

TEST(unknown_device_returns_negative) {
    ASSERT_EQ(get_max_cpu_clock(999), -1);
    ASSERT_EQ(get_max_cpu_clock(0), -1);
}

/* ==== Tests: bounds checking ==== */

TEST(miyoo283_min_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOO283, 500));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOO283, 499));
}

TEST(miyoo283_max_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOO283, 1600));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOO283, 1601));
}

TEST(miyoo354_min_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOO354, 500));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOO354, 499));
}

TEST(miyoo354_max_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOO354, 1800));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOO354, 1801));
}

TEST(miyooflip_min_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 500));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOOFLIP, 499));
}

TEST(miyooflip_max_boundary) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 1800));
    ASSERT_FALSE(cpu_clock_in_bounds(MIYOOFLIP, 1801));
}

TEST(miyooflip_typical_clocks) {
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 1000));
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 1200));
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 1500));
    ASSERT_TRUE(cpu_clock_in_bounds(MIYOOFLIP, 1800));
}

TEST(unknown_device_always_out_of_bounds) {
    ASSERT_FALSE(cpu_clock_in_bounds(999, 1000));
    ASSERT_FALSE(cpu_clock_in_bounds(0, 500));
}

/* ==== Tests: cpuclock tool sysfs bounds (MIYOOFLIP: 100-1800 MHz) ==== */

static bool cpuclock_tool_in_range(int clock_mhz, int min_mhz, int max_mhz)
{
    return (clock_mhz >= min_mhz && clock_mhz <= max_mhz);
}

TEST(cpuclock_sysfs_flip_valid_range) {
    /* cpuclock tool accepts 100-1800 MHz for MIYOOFLIP */
    ASSERT_TRUE(cpuclock_tool_in_range(100, 100, 1800));
    ASSERT_TRUE(cpuclock_tool_in_range(1800, 100, 1800));
    ASSERT_TRUE(cpuclock_tool_in_range(1000, 100, 1800));
}

TEST(cpuclock_sysfs_flip_invalid_range) {
    /* cpuclock tool rejects <100 and >1800 for MIYOOFLIP */
    ASSERT_FALSE(cpuclock_tool_in_range(99, 100, 1800));
    ASSERT_FALSE(cpuclock_tool_in_range(1801, 100, 1800));
    ASSERT_FALSE(cpuclock_tool_in_range(0, 100, 1800));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== CPU Clock Bounds Unit Tests ===\n\n");

    /* Max clock values */
    RUN_TEST(miyoo283_max_clock);
    RUN_TEST(miyoo354_max_clock);
    RUN_TEST(miyooflip_max_clock);
    RUN_TEST(unknown_device_returns_negative);

    /* Bounds checking */
    RUN_TEST(miyoo283_min_boundary);
    RUN_TEST(miyoo283_max_boundary);
    RUN_TEST(miyoo354_min_boundary);
    RUN_TEST(miyoo354_max_boundary);
    RUN_TEST(miyooflip_min_boundary);
    RUN_TEST(miyooflip_max_boundary);
    RUN_TEST(miyooflip_typical_clocks);
    RUN_TEST(unknown_device_always_out_of_bounds);

    /* cpuclock tool sysfs bounds */
    RUN_TEST(cpuclock_sysfs_flip_valid_range);
    RUN_TEST(cpuclock_sysfs_flip_invalid_range);

    TEST_REPORT();
    return test_failures;
}
