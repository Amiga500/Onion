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

/* ---- Provide STR_MAX used by file.h ---- */
#define STR_MAX 256

/* ---- Inline the file_get macro from file.h ---- */
#define file_get(fp, path, format, dest) \
    {                                    \
        if ((fp = fopen(path, "r"))) {   \
            fscanf(fp, format, dest);    \
            fclose(fp);                  \
        }                                \
    }

/* ---- Inline device_model.h constants and functions ---- */

#define MIYOO283 283
#define MIYOO354 354
static int DEVICE_ID;
static char DEVICE_SN[13];

static void getDeviceModel(void)
{
    FILE *fp;
    file_get(fp, "/tmp/deviceModel", "%d", &DEVICE_ID);
}

static void getDeviceSerial(void)
{
    FILE *fp;
    file_get(fp, "/tmp/deviceSN", "%[^\n]", DEVICE_SN);
}

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

TEST(device_serial_with_newline) {
    write_file("/tmp/deviceSN", "SN123\nextra");
    memset(DEVICE_SN, 0, sizeof(DEVICE_SN));
    getDeviceSerial();
    /* %[^\n] stops at newline */
    ASSERT_STREQ(DEVICE_SN, "SN123");
}

TEST(device_model_constants) {
    ASSERT_EQ(MIYOO283, 283);
    ASSERT_EQ(MIYOO354, 354);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== device_model.h Unit Tests ===\n\n");

    RUN_TEST(device_model_283);
    RUN_TEST(device_model_354);
    RUN_TEST(device_model_arbitrary);
    RUN_TEST(device_model_zero);
    RUN_TEST(device_model_missing_file);

    RUN_TEST(device_serial_typical);
    RUN_TEST(device_serial_short);
    RUN_TEST(device_serial_empty);
    RUN_TEST(device_serial_missing_file);
    RUN_TEST(device_serial_with_newline);

    RUN_TEST(device_model_constants);

    /* Cleanup */
    unlink("/tmp/deviceModel");
    unlink("/tmp/deviceSN");

    TEST_REPORT();
    return test_failures;
}
