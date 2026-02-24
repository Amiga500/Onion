/**
 * @file test_volume.c
 * @brief Unit tests for volume curve calculations in system/volume.h
 *
 * Tests the pure-math volume curve function setVolume(), which converts
 * a user-facing volume level (0–20) into a raw hardware value using a
 * logarithmic curve: raw = round(48 * log10(1 + volume)).
 *
 * The volume curve is critical because:
 * - It maps perceived volume (linear 0–20 slider) to logarithmic dB values
 * - Boundary values (0 and 20) control mute and max volume
 * - The curve must be monotonically increasing
 *
 * Build and run: make -f Makefile.unit test_volume
 */

#include "onion_test.h"
#include <math.h>

/* ---- Constants from volume.h ---- */
#define MAX_VOLUME 20
#define MIN_RAW_VALUE -60
#define MAX_RAW_VALUE 30

/* ---- Inline the pure-logic calculation from setVolume ---- */

/**
 * Compute the raw volume value for a given user volume level.
 * This is the pure calculation extracted from setVolume() in volume.h.
 *
 * @param volume User volume level (0–20)
 * @return Raw volume value for hardware
 */
static int volume_to_raw(int volume)
{
    int volume_raw = 0;

    if (volume > 20)
        volume = 20;
    else if (volume < 0)
        volume = 0;

    if (volume != 0)
        volume_raw = round(48 * log10(1 + volume));

    return volume_raw;
}

/**
 * Clamp a volume level to the valid range.
 * Extracted from setVolume() in volume.h.
 */
static int volume_clamp(int volume)
{
    if (volume > 20)
        return 20;
    else if (volume < 0)
        return 0;
    return volume;
}

/* ---- Tests: volume clamping ---- */

TEST(volume_clamp_normal_range) {
    for (int v = 0; v <= 20; v++) {
        ASSERT_EQ(volume_clamp(v), v);
    }
}

TEST(volume_clamp_negative) {
    ASSERT_EQ(volume_clamp(-1), 0);
    ASSERT_EQ(volume_clamp(-100), 0);
}

TEST(volume_clamp_above_max) {
    ASSERT_EQ(volume_clamp(21), 20);
    ASSERT_EQ(volume_clamp(100), 20);
}

/* ---- Tests: volume curve at boundaries ---- */

TEST(volume_zero_is_silent) {
    ASSERT_EQ(volume_to_raw(0), 0);
}

TEST(volume_max_is_highest) {
    int raw = volume_to_raw(20);
    /* log10(21) ≈ 1.3222, 48 * 1.3222 ≈ 63.47, round → 63 */
    ASSERT_EQ(raw, 63);
}

TEST(volume_one_is_lowest_audible) {
    int raw = volume_to_raw(1);
    /* log10(2) ≈ 0.3010, 48 * 0.3010 ≈ 14.45, round → 14 */
    ASSERT_EQ(raw, 14);
}

/* ---- Tests: volume curve is monotonically increasing ---- */

TEST(volume_curve_monotonic) {
    int prev = volume_to_raw(0);
    for (int v = 1; v <= 20; v++) {
        int curr = volume_to_raw(v);
        ASSERT_GT(curr, prev);
        prev = curr;
    }
}

/* ---- Tests: specific volume curve points ---- */

TEST(volume_curve_midpoint) {
    int raw = volume_to_raw(10);
    /* log10(11) ≈ 1.0414, 48 * 1.0414 ≈ 49.99, round → 50 */
    ASSERT_EQ(raw, 50);
}

TEST(volume_curve_quarter) {
    int raw = volume_to_raw(5);
    /* log10(6) ≈ 0.7782, 48 * 0.7782 ≈ 37.35, round → 37 */
    ASSERT_EQ(raw, 37);
}

TEST(volume_curve_three_quarter) {
    int raw = volume_to_raw(15);
    /* log10(16) ≈ 1.2041, 48 * 1.2041 ≈ 57.80, round → 58 */
    ASSERT_EQ(raw, 58);
}

/* ---- Tests: all volume values produce expected raw values ---- */

TEST(volume_curve_all_values) {
    /* Verify the complete volume curve matches expected values.
     * These values define the audio experience for users. */
    int expected[] = {
        0,   /* vol  0 → silent */
        14,  /* vol  1 → round(48 * log10(2))  = 14 */
        23,  /* vol  2 → round(48 * log10(3))  = 23 */
        29,  /* vol  3 → round(48 * log10(4))  = 29 */
        34,  /* vol  4 → round(48 * log10(5))  = 34 */
        37,  /* vol  5 → round(48 * log10(6))  = 37 */
        41,  /* vol  6 → round(48 * log10(7))  = 41 */
        43,  /* vol  7 → round(48 * log10(8))  = 43 */
        46,  /* vol  8 → round(48 * log10(9))  = 46 */
        48,  /* vol  9 → round(48 * log10(10)) = 48 */
        50,  /* vol 10 → round(48 * log10(11)) = 50 */
        52,  /* vol 11 → round(48 * log10(12)) = 52 */
        53,  /* vol 12 → round(48 * log10(13)) = 53 */
        55,  /* vol 13 → round(48 * log10(14)) = 55 */
        56,  /* vol 14 → round(48 * log10(15)) = 56 */
        58,  /* vol 15 → round(48 * log10(16)) = 58 */
        59,  /* vol 16 → round(48 * log10(17)) = 59 */
        60,  /* vol 17 → round(48 * log10(18)) = 60 */
        61,  /* vol 18 → round(48 * log10(19)) = 61 */
        62,  /* vol 19 → round(48 * log10(20)) = 62 */
        63,  /* vol 20 → round(48 * log10(21)) = 63 */
    };

    for (int v = 0; v <= 20; v++) {
        int raw = volume_to_raw(v);
        if (raw != expected[v]) {
            fprintf(stderr, "    Volume %d: expected raw %d, got %d\n",
                    v, expected[v], raw);
        }
        ASSERT_EQ(raw, expected[v]);
    }
}

/* ---- Tests: negative and over-range inputs ---- */

TEST(volume_negative_treated_as_zero) {
    ASSERT_EQ(volume_to_raw(-1), 0);
    ASSERT_EQ(volume_to_raw(-100), 0);
}

TEST(volume_over_max_treated_as_max) {
    ASSERT_EQ(volume_to_raw(21), volume_to_raw(20));
    ASSERT_EQ(volume_to_raw(100), volume_to_raw(20));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== Volume Curve Unit Tests ===\n\n");

    /* Clamping */
    RUN_TEST(volume_clamp_normal_range);
    RUN_TEST(volume_clamp_negative);
    RUN_TEST(volume_clamp_above_max);

    /* Boundaries */
    RUN_TEST(volume_zero_is_silent);
    RUN_TEST(volume_max_is_highest);
    RUN_TEST(volume_one_is_lowest_audible);

    /* Curve shape */
    RUN_TEST(volume_curve_monotonic);
    RUN_TEST(volume_curve_midpoint);
    RUN_TEST(volume_curve_quarter);
    RUN_TEST(volume_curve_three_quarter);
    RUN_TEST(volume_curve_all_values);

    /* Edge cases */
    RUN_TEST(volume_negative_treated_as_zero);
    RUN_TEST(volume_over_max_treated_as_max);

    TEST_REPORT();
    return test_failures;
}
