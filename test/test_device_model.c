/**
 * @file test_device_model.c
 * @brief Unit tests for src/common/system/device_model.h
 *
 * Tests getDeviceModel() and getDeviceSerial() by writing
 * known values to the expected temp file paths and verifying
 * the globals are populated correctly.
 *
 * Build and run: make -f Makefile.unit test_device_model
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Production code: system/device_model.h (reads /tmp/deviceModel and
 * /tmp/deviceSN through file_get from utils/file.h). */
#include "system/device_model.h"

/* ---- Helpers ---- */

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%s", content);
        fclose(fp);
    }
}

/* ==== getDeviceModel tests ==== */

TEST(device_model_283) {
    write_file("/tmp/deviceModel", "283");
    DEVICE_ID = 0;
    getDeviceModel();
    ASSERT_EQ(DEVICE_ID, MIYOO283);
}

TEST(device_model_354) {
    write_file("/tmp/deviceModel", "354");
    DEVICE_ID = 0;
    getDeviceModel();
    ASSERT_EQ(DEVICE_ID, MIYOO354);
}

TEST(device_model_285) {
    write_file("/tmp/deviceModel", "285");
    DEVICE_ID = 0;
    getDeviceModel();
    ASSERT_EQ(DEVICE_ID, MIYOO285);
}

TEST(device_model_arbitrary) {
    write_file("/tmp/deviceModel", "999");
    DEVICE_ID = 0;
    getDeviceModel();
    ASSERT_EQ(DEVICE_ID, 999);
}

TEST(device_model_zero) {
    write_file("/tmp/deviceModel", "0");
    DEVICE_ID = 42;
    getDeviceModel();
    ASSERT_EQ(DEVICE_ID, 0);
}

TEST(device_model_missing_file) {
    unlink("/tmp/deviceModel");
    DEVICE_ID = 42;
    getDeviceModel();
    /* When file doesn't exist, DEVICE_ID should remain unchanged */
    ASSERT_EQ(DEVICE_ID, 42);
}

/* ==== getDeviceSerial tests ==== */

TEST(device_serial_typical) {
    write_file("/tmp/deviceSN", "AB1234567890");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    ASSERT_STREQ(DEVICE_SN, "AB1234567890");
}

TEST(device_serial_short) {
    write_file("/tmp/deviceSN", "SN123");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    ASSERT_STREQ(DEVICE_SN, "SN123");
}

TEST(device_serial_empty) {
    write_file("/tmp/deviceSN", "");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    ASSERT_STREQ(DEVICE_SN, "");
}

TEST(device_serial_missing_file) {
    unlink("/tmp/deviceSN");
    strncpy(DEVICE_SN, "old", sizeof(DEVICE_SN));
    getDeviceSerial();
    /* When file doesn't exist, DEVICE_SN should remain unchanged */
    ASSERT_STREQ(DEVICE_SN, "old");
}

/* A line longer than DEVICE_SN holds is truncated, not written past the
 * array (ASan in unit-test-san would also catch the overflow). */
TEST(device_serial_too_long_is_bounded) {
    write_file("/tmp/deviceSN", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    ASSERT_EQ((int)strlen(DEVICE_SN), (int)sizeof(DEVICE_SN) - 1);
    ASSERT_STREQ(DEVICE_SN, "0123456789AB");
}

TEST(device_serial_with_newline) {
    write_file("/tmp/deviceSN", "SN123\nextra");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    /* %[^\n] stops at newline */
    ASSERT_STREQ(DEVICE_SN, "SN123");
}

TEST(device_model_constants) {
    ASSERT_EQ(MIYOO283, 283);
    ASSERT_EQ(MIYOO285, 285);
    ASSERT_EQ(MIYOO354, 354);
}

TEST(device_caps_macros) {
    DEVICE_ID = MIYOO283;
    ASSERT_FALSE(IS_MIYOO_PLUS_OR_FLIP());
    ASSERT_FALSE(HAS_AXP());
    ASSERT_FALSE(HAS_WIFI());

    DEVICE_ID = MIYOO354;
    ASSERT_TRUE(IS_MIYOO_PLUS_OR_FLIP());
    ASSERT_TRUE(HAS_AXP());
    ASSERT_TRUE(HAS_WIFI());

    DEVICE_ID = MIYOO285;
    ASSERT_TRUE(IS_MIYOO_PLUS_OR_FLIP());
    ASSERT_TRUE(HAS_AXP());
    ASSERT_TRUE(HAS_WIFI());
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== device_model.h Unit Tests ===\n\n");

    RUN_TEST(device_model_283);
    RUN_TEST(device_model_354);
    RUN_TEST(device_model_285);
    RUN_TEST(device_model_arbitrary);
    RUN_TEST(device_model_zero);
    RUN_TEST(device_model_missing_file);

    RUN_TEST(device_serial_typical);
    RUN_TEST(device_serial_short);
    RUN_TEST(device_serial_empty);
    RUN_TEST(device_serial_missing_file);
    RUN_TEST(device_serial_too_long_is_bounded);
    RUN_TEST(device_serial_with_newline);

    RUN_TEST(device_model_constants);
    RUN_TEST(device_caps_macros);

    /* Cleanup */
    unlink("/tmp/deviceModel");
    unlink("/tmp/deviceSN");

    TEST_REPORT();
    return test_failures;
}
