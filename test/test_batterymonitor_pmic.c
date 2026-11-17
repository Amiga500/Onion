/**
 * @file test_batterymonitor_pmic.c
 * @brief Host tests for the PMIC code of src/batteryMonitorUI/batteryMonitorUI.c.
 *
 * The app cannot be included on the host (SDL, Miyoo headers), so the Makefile cuts the functions under
 * test and the register constants out of the real source into PMIC_FNS, and this file runs them on a fake
 * AXP223: 256 registers, with reads and writes that can be made to fail.
 *
 * Covers the review of PR #332:
 *   1. Charge Limit: the AXP221/AXP223 voltage table, and only 4.10 V / 4.20 V writable;
 *   2. "Set current capacity to 100%": only on a full battery that is not charging;
 *   4. enabling capacity learning never overwrites a capacity it could not read;
 *   5. capacity and counter writes are read back, and stop when REG B8H cannot be read.
 * (Point 3, batmon's start_time: test_batmon_start_time.c.)
 *
 * Build and run: make -f Makefile.unit test_batterymonitor_pmic
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- the fake PMIC ---- */

static int reg[256];           /* register values */
static bool fail_read[256];    /* a read of this register fails (-1) */
static bool fail_write[256];   /* a write to this register fails */
static int writes[256];        /* writes that reached each register */
static int total_writes;
static int stale_reads;        /* 16-bit reads that still answer with the value from before the last write */
static int stale_hi, stale_lo; /* that old value */
static int stale_reg = -1;
static int delays;             /* SDL_Delay calls */
static bool fake_has_axp = true;
static int open_fd = 3; /* -1: /dev/i2c-1 cannot be opened */
static int axp_edit_capacity = 3000;

static void pmic_reset(void)
{
    memset(reg, 0, sizeof(reg));
    memset(fail_read, 0, sizeof(fail_read));
    memset(fail_write, 0, sizeof(fail_write));
    memset(writes, 0, sizeof(writes));
    total_writes = 0;
    stale_reads = 0;
    stale_reg = -1;
    delays = 0;
    fake_has_axp = true;
    open_fd = 3;
    axp_edit_capacity = 3000;
    reg[0xB8] = 0xC0; /* gauge and Coulomb counter on, learning off */
    reg[0x33] = 0xCA; /* as on a Mini Plus: charge on, 4.20 V, 1800 mA */
}

static int axp_i2c_read(int fd, unsigned char r)
{
    (void)fd;
    return fail_read[r] ? -1 : reg[r];
}

static int axp_i2c_read16(int fd, unsigned char r)
{
    (void)fd;
    if (fail_read[r])
        return -1;
    if (stale_reg == r && stale_reads > 0) {
        stale_reads--;
        return (stale_hi << 8) | stale_lo;
    }
    return (reg[r] << 8) | reg[r + 1];
}

static int axp_i2c_write(int fd, unsigned char r, unsigned char val)
{
    (void)fd;
    if (fail_write[r])
        return -1;
    reg[r] = val;
    writes[r]++;
    total_writes++;
    return 0;
}

/* The PMIC answers with the old value for `n` 16-bit reads after the next write to the pair at `r`. */
static void pmic_answer_stale(unsigned char r, int n)
{
    stale_reg = r;
    stale_hi = reg[r];
    stale_lo = reg[r + 1];
    stale_reads = n;
}

static void set_mah16(unsigned char r, int mah)
{
    int units = (int)((mah / 1.456) + 0.5);
    reg[r] = 0x80 | ((units >> 8) & 0x7F);
    reg[r + 1] = units & 0xFF;
}

static int get_mah16(unsigned char r)
{
    return (int)((((reg[r] & 0x7F) << 8) | reg[r + 1]) * 1.456 + 0.5);
}

/* ---- stand-ins for what the cut functions call ---- */

#define HAS_AXP() fake_has_axp
#define O_RDWR 2
#define open(path, flags) (open_fd)
#define close(fd) ((void)(fd))
#define ioctl(fd, request, arg) ((void)0)
static void SDL_Delay(unsigned ms)
{
    (void)ms;
    delays++;
}

#include PMIC_FNS

/* ---- 1. Charge Limit ---- */

TEST(charge_voltage_table_is_the_axp22x_one)
{
    /* AXP221 datasheet v1.2, 10.2.27: 00 4.1 V, 01 4.22 V, 10 4.2 V, 11 4.24 V.
     * (4.15 V and 4.36 V are the AXP209's.) */
    ASSERT_EQ(chgv_code_to_mv(0), 4100);
    ASSERT_EQ(chgv_code_to_mv(1), 4220);
    ASSERT_EQ(chgv_code_to_mv(2), 4200);
    ASSERT_EQ(chgv_code_to_mv(3), 4240);
}

TEST(only_4_10_and_4_20_are_offered)
{
    ASSERT_TRUE(AXP_CHGV_CODE_OFFERED(0));
    ASSERT_FALSE(AXP_CHGV_CODE_OFFERED(1));
    ASSERT_TRUE(AXP_CHGV_CODE_OFFERED(2));
    ASSERT_FALSE(AXP_CHGV_CODE_OFFERED(3));
    /* nothing offered is above the cell's rated 4.20 V */
    for (int code = 0; code < 4; code++)
        if (AXP_CHGV_CODE_OFFERED(code))
            ASSERT_TRUE(chgv_code_to_mv(code) <= 4200);
}

TEST(charge_voltage_write_refuses_4_22_and_4_24)
{
    pmic_reset();
    ASSERT_EQ(axp_set_charge_voltage(1), -1);
    ASSERT_EQ(axp_set_charge_voltage(3), -1);
    ASSERT_EQ(axp_set_charge_voltage(-1), -1);
    ASSERT_EQ(axp_set_charge_voltage(4), -1);
    ASSERT_EQ(total_writes, 0);
    ASSERT_EQ(reg[0x33], 0xCA);
}

TEST(charge_voltage_write_changes_bits_6_5_only)
{
    pmic_reset();
    ASSERT_EQ(axp_set_charge_voltage(0), 0);
    ASSERT_EQ(reg[0x33], 0x8A); /* CAH -> 8AH, measured on a Mini Plus */
    ASSERT_EQ(axp_set_charge_voltage(2), 0);
    ASSERT_EQ(reg[0x33], 0xCA);

    /* a console left at code 01 (the standalone app v2.3 set it as "~95%"), with other bits set */
    reg[0x33] = 0xBF;
    ASSERT_EQ(axp_set_charge_voltage(2), 0);
    ASSERT_EQ(reg[0x33], 0xDF);
    ASSERT_EQ(writes[0x33], 3);
    ASSERT_EQ(total_writes, 3);
}

TEST(charge_voltage_write_fails_cleanly)
{
    pmic_reset();
    fail_read[0x33] = true;
    ASSERT_EQ(axp_set_charge_voltage(0), -1);
    ASSERT_EQ(total_writes, 0);

    pmic_reset();
    fail_write[0x33] = true;
    ASSERT_EQ(axp_set_charge_voltage(0), -1);
    ASSERT_EQ(reg[0x33], 0xCA);

    pmic_reset();
    fake_has_axp = false; /* Mini v1-v4 */
    ASSERT_EQ(axp_set_charge_voltage(0), -1);
    ASSERT_EQ(total_writes, 0);
}

/* ---- 2. Set current capacity to 100% ---- */

TEST(sync_only_on_a_full_battery_that_is_not_charging)
{
    ASSERT_EQ(axp_sync_check(true, false, 4200), AXP_SYNC_OK);
    ASSERT_EQ(axp_sync_check(true, false, AXP_SYNC_MIN_MV), AXP_SYNC_OK);
    /* pressed by mistake at 40 %: about 3.7 V */
    ASSERT_EQ(axp_sync_check(true, false, 3700), AXP_SYNC_NOT_FULL);
    ASSERT_EQ(axp_sync_check(true, false, AXP_SYNC_MIN_MV - 1), AXP_SYNC_NOT_FULL);
    /* while charging the voltage reads high long before the battery is full
     * (measured: 4.18 V at 87 %, 4.23 V at 95 %) */
    ASSERT_EQ(axp_sync_check(true, true, 4183), AXP_SYNC_CHARGING);
    ASSERT_EQ(axp_sync_check(true, true, 4232), AXP_SYNC_CHARGING);
    ASSERT_EQ(axp_sync_check(false, false, 4200), AXP_SYNC_NO_PMIC);
}

/* ---- 4. enabling capacity learning ---- */

TEST(learning_keeps_a_capacity_it_cannot_read)
{
    pmic_reset();
    set_mah16(0xE0, 3052); /* a learned capacity */
    axp_edit_capacity = 3000; /* the menu's default */
    fail_read[0xE0] = true;
    ASSERT_EQ(axp_set_calibration(true), -1);
    ASSERT_EQ(get_mah16(0xE0), 3052);
    ASSERT_EQ(writes[0xE0], 0);
    ASSERT_EQ(writes[0xE1], 0);
    ASSERT_EQ(writes[0xB8], 0); /* learning is not switched on either */
}

TEST(learning_keeps_a_capacity_that_is_set)
{
    pmic_reset();
    set_mah16(0xE0, 3052);
    ASSERT_EQ(axp_set_calibration(true), 0);
    ASSERT_EQ(get_mah16(0xE0), 3052);
    ASSERT_EQ(writes[0xE0], 0);
    ASSERT_EQ(reg[0xB8], 0xE0);
}

TEST(learning_seeds_a_capacity_only_on_a_factory_fresh_pmic)
{
    pmic_reset(); /* E0H/E1H read 0 */
    axp_edit_capacity = 2800;
    ASSERT_EQ(axp_set_calibration(true), 0);
    ASSERT_EQ(get_mah16(0xE0), 2800);
    ASSERT_TRUE(reg[0xE0] & 0x80);
    ASSERT_EQ(reg[0xB8], 0xE0);
}

TEST(learning_stops_when_the_seed_cannot_be_written)
{
    pmic_reset();
    fail_write[0xE1] = true;
    ASSERT_EQ(axp_set_calibration(true), -1);
    ASSERT_EQ(writes[0xB8], 0);
}

TEST(learning_off_keeps_gauge_counter_and_capacity)
{
    pmic_reset();
    reg[0xB8] = 0xE0;
    set_mah16(0xE0, 3052);
    ASSERT_EQ(axp_set_calibration(false), 0);
    ASSERT_EQ(reg[0xB8], 0xC0);
    ASSERT_EQ(get_mah16(0xE0), 3052);
}

TEST(learning_reports_a_pmic_that_did_not_take_the_bit)
{
    pmic_reset();
    fail_read[0xB8] = true;
    ASSERT_EQ(axp_set_calibration(true), -1);
    ASSERT_EQ(total_writes, 0);
}

/* ---- 5. capacity and counter writes ---- */

TEST(capacity_write_is_read_back)
{
    pmic_reset();
    ASSERT_EQ(axp_set_capacity(2800), 0);
    ASSERT_EQ(get_mah16(0xE0), 2800);
    ASSERT_TRUE(reg[0xE0] & 0x80);
    ASSERT_EQ(delays, 0);
}

TEST(capacity_write_waits_for_the_pmic_to_show_the_new_value)
{
    /* measured on a Mini Plus: E0H/E1H answer with the old value for 1-2 s */
    pmic_reset();
    set_mah16(0xE0, 3000);
    pmic_answer_stale(0xE0, 7);
    ASSERT_EQ(axp_set_capacity(2800), 0);
    ASSERT_EQ(delays, 7);
}

TEST(capacity_write_reports_a_value_the_pmic_does_not_hold)
{
    pmic_reset();
    set_mah16(0xE0, 3000);
    pmic_answer_stale(0xE0, 1000); /* never shows the new value */
    ASSERT_EQ(axp_set_capacity(2800), -2);
    ASSERT_EQ(delays, AXP_READBACK_TRIES);
}

TEST(capacity_write_stops_when_b8_cannot_be_read)
{
    pmic_reset();
    set_mah16(0xE0, 3052);
    fail_read[0xB8] = true;
    ASSERT_EQ(axp_set_capacity(2800), -1);
    ASSERT_EQ(axp_set_coulomb_counter(2800), -1);
    ASSERT_EQ(total_writes, 0);
    ASSERT_EQ(get_mah16(0xE0), 3052);
}

TEST(capacity_write_reports_i2c_failures)
{
    pmic_reset();
    fail_write[0xE1] = true;
    ASSERT_EQ(axp_set_capacity(2800), -1);

    pmic_reset();
    open_fd = -1;
    ASSERT_EQ(axp_set_capacity(2800), -1);
    ASSERT_EQ(total_writes, 0);

    pmic_reset();
    ASSERT_EQ(axp_set_capacity(0), -1);
    ASSERT_EQ(axp_set_capacity(10001), -1);
    ASSERT_EQ(total_writes, 0);
}

TEST(writes_switch_the_gauge_and_counter_on)
{
    pmic_reset();
    reg[0xB8] = 0x20; /* learning bit kept, gauge and counter off */
    ASSERT_EQ(axp_set_capacity(2800), 0);
    ASSERT_EQ(reg[0xB8], 0xE0);
}

TEST(counter_write_allows_for_the_counter_moving)
{
    pmic_reset();
    ASSERT_EQ(axp_set_coulomb_counter(3052), 0);
    ASSERT_EQ(get_mah16(0xE2), 3052);
    ASSERT_EQ(writes[0xE0], 0); /* the capacity is not touched */

    /* the counter counted down a few steps between the write and the read-back */
    pmic_reset();
    set_mah16(0xE2, 3052 - 4);
    pmic_answer_stale(0xE2, 1000);
    ASSERT_EQ(axp_set_coulomb_counter(3052), 0);

    /* far off: not the value that was written */
    pmic_reset();
    set_mah16(0xE2, 1200);
    pmic_answer_stale(0xE2, 1000);
    ASSERT_EQ(axp_set_coulomb_counter(3052), -2);
}

/* ---- Shown capacity ---- */

TEST(a_saved_round_capacity_is_shown_as_saved)
{
    /* every value the capacity menu can save (1500..4000 mAh in steps of 50) reads back as itself:
     * 3000 mAh is held as 2060 units = 2999.4 mAh and was shown as 2999 */
    for (int mah = 1500; mah <= 4000; mah += 50) {
        int units = (int)((mah / 1.456) + 0.5);
        ASSERT_EQ(axp_units_to_mah(units), mah);
    }
    ASSERT_EQ(axp_units_to_mah(2060), 3000);
}

TEST(other_values_are_shown_to_the_nearest_mah)
{
    ASSERT_EQ(axp_units_to_mah(0), 0);
    ASSERT_EQ(axp_units_to_mah(2059), 2998); /* 2997.9 */
    ASSERT_EQ(axp_units_to_mah(2061), 3001); /* 3000.8 */
    ASSERT_EQ(axp_units_to_mah(2096), 3052); /* a learned capacity */
    ASSERT_EQ(axp_units_to_mah(2579), 3755);
}

int main(void)
{
    printf("\n=== batteryMonitorUI PMIC tests ===\n\n");
    RUN_TEST(charge_voltage_table_is_the_axp22x_one);
    RUN_TEST(only_4_10_and_4_20_are_offered);
    RUN_TEST(charge_voltage_write_refuses_4_22_and_4_24);
    RUN_TEST(charge_voltage_write_changes_bits_6_5_only);
    RUN_TEST(charge_voltage_write_fails_cleanly);
    RUN_TEST(sync_only_on_a_full_battery_that_is_not_charging);
    RUN_TEST(learning_keeps_a_capacity_it_cannot_read);
    RUN_TEST(learning_keeps_a_capacity_that_is_set);
    RUN_TEST(learning_seeds_a_capacity_only_on_a_factory_fresh_pmic);
    RUN_TEST(learning_stops_when_the_seed_cannot_be_written);
    RUN_TEST(learning_off_keeps_gauge_counter_and_capacity);
    RUN_TEST(learning_reports_a_pmic_that_did_not_take_the_bit);
    RUN_TEST(capacity_write_is_read_back);
    RUN_TEST(capacity_write_waits_for_the_pmic_to_show_the_new_value);
    RUN_TEST(capacity_write_reports_a_value_the_pmic_does_not_hold);
    RUN_TEST(capacity_write_stops_when_b8_cannot_be_read);
    RUN_TEST(capacity_write_reports_i2c_failures);
    RUN_TEST(writes_switch_the_gauge_and_counter_on);
    RUN_TEST(counter_write_allows_for_the_counter_moving);
    RUN_TEST(a_saved_round_capacity_is_shown_as_saved);
    RUN_TEST(other_values_are_shown_to_the_nearest_mah);
    TEST_REPORT();
    return test_failures;
}
