/**
 * @file test_batmon_flip.c
 * @brief Unit tests for MIYOOFLIP (RK3566) battery reading via power_supply sysfs.
 *
 * Tests getBatPercFlip() which reads battery capacity from
 * /sys/class/power_supply/battery/capacity on the Miyoo Flip.
 * Uses temporary files to simulate the sysfs interface.
 *
 * Build and run: make -f Makefile.unit test_batmon_flip
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ---- Replicate getBatPercFlip() from batmon.c for host testing ---- */

static int getBatPercFlip(const char *path)
{
    int capacity = -1;
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%d", &capacity) != 1)
            capacity = -1;
        fclose(fp);
    }
    return capacity;
}

/* ---- Helpers ---- */

static const char *test_capacity_path = "/tmp/test_flip_capacity";

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%s", content);
        fclose(fp);
    }
}

/* ==== getBatPercFlip tests ==== */

TEST(flip_battery_full) {
    write_file(test_capacity_path, "100");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 100);
}

TEST(flip_battery_zero) {
    write_file(test_capacity_path, "0");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 0);
}

TEST(flip_battery_mid) {
    write_file(test_capacity_path, "57");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 57);
}

TEST(flip_battery_low) {
    write_file(test_capacity_path, "5");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 5);
}

TEST(flip_battery_missing_file) {
    unlink(test_capacity_path);
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, -1);
}

TEST(flip_battery_empty_file) {
    write_file(test_capacity_path, "");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, -1);
}

TEST(flip_battery_non_numeric) {
    write_file(test_capacity_path, "unknown");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, -1);
}

TEST(flip_battery_with_newline) {
    write_file(test_capacity_path, "85\n");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 85);
}

TEST(flip_battery_with_whitespace) {
    write_file(test_capacity_path, "  42  \n");
    int perc = getBatPercFlip(test_capacity_path);
    ASSERT_EQ(perc, 42);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== batmon MIYOOFLIP Battery Reading Unit Tests ===\n\n");

    RUN_TEST(flip_battery_full);
    RUN_TEST(flip_battery_zero);
    RUN_TEST(flip_battery_mid);
    RUN_TEST(flip_battery_low);
    RUN_TEST(flip_battery_missing_file);
    RUN_TEST(flip_battery_empty_file);
    RUN_TEST(flip_battery_non_numeric);
    RUN_TEST(flip_battery_with_newline);
    RUN_TEST(flip_battery_with_whitespace);

    /* Cleanup */
    unlink(test_capacity_path);

    TEST_REPORT();
    return test_failures;
}
