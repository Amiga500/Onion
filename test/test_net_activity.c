/**
 * @file test_net_activity.c
 * @brief Tests for src/keymon/net_activity.h (production header)
 *
 * keymon's sleep timer counts a file transfer as activity: Wi-Fi traffic
 * above 2 KB/s while a file-transfer service is on in Tweaks > Network.
 *
 * Build and run: make -f Makefile.unit test_net_activity
 */

#include <stdlib.h>
#include <sys/stat.h>

#include "../src/keymon/net_activity.h"
#include "onion_test.h"

#define BASE "/tmp/onion_test_net_activity"
#define SYSFS BASE "/net"
#define CONFIG BASE "/config"

static void write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(text, fp);
        fclose(fp);
    }
}

static void reset_dirs(void)
{
    (void)system("rm -rf " BASE);
    (void)system("mkdir -p " CONFIG " " SYSFS "/wlan0/statistics");
}

static void set_wlan0(const char *rx, const char *tx)
{
    write_text(SYSFS "/wlan0/statistics/rx_bytes", rx);
    write_text(SYSFS "/wlan0/statistics/tx_bytes", tx);
}

TEST(counters_of_both_interfaces_are_summed)
{
    reset_dirs();
    unsigned long long total = 1;
    ASSERT_FALSE(netActivity_bytes(BASE "/none", &total));
    ASSERT_TRUE(total == 0);
    set_wlan0("1000\n", "500\n");
    ASSERT_TRUE(netActivity_bytes(SYSFS, &total));
    ASSERT_TRUE(total == 1500);
    (void)system("mkdir -p " SYSFS "/wlan1/statistics");
    write_text(SYSFS "/wlan1/statistics/rx_bytes", "4294967296\n");
    ASSERT_TRUE(netActivity_bytes(SYSFS, &total));
    ASSERT_TRUE(total == 4294968796ULL);
    /* A garbled counter is skipped. */
    write_text(SYSFS "/wlan0/statistics/tx_bytes", "x\n");
    ASSERT_TRUE(netActivity_bytes(SYSFS, &total));
    ASSERT_TRUE(total == 4294968296ULL);
}

TEST(only_file_transfer_services_count)
{
    reset_dirs();
    ASSERT_FALSE(netActivity_servicesOn(CONFIG));
    write_text(CONFIG "/.ntpState", "");
    ASSERT_FALSE(netActivity_servicesOn(CONFIG));
    const char *flags[] = {"/.ftpState", "/.sshState", "/.httpState", "/.smbdState", "/.telnetState"};
    for (int i = 0; i < 5; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s%s", CONFIG, flags[i]);
        write_text(path, "");
        ASSERT_TRUE(netActivity_servicesOn(CONFIG));
        remove(path);
        ASSERT_FALSE(netActivity_servicesOn(CONFIG));
    }
}

TEST(busy_from_two_kb_per_second)
{
    NetActivity na = {0};
    /* The first reading only records. */
    ASSERT_FALSE(netActivity_busy(&na, true, 1000000, 0));
    /* 15 s check: 30720 bytes is exactly 2 KB/s. */
    ASSERT_TRUE(netActivity_busy(&na, true, 1000000 + 30720, 15000));
    ASSERT_FALSE(netActivity_busy(&na, true, 1000000 + 30720 + 30719, 30000));
    /* Background chatter: a few hundred bytes per second. */
    ASSERT_FALSE(netActivity_busy(&na, true, 1000000 + 61439 + 6000, 45000));
    /* A transfer: 1 MB in 15 s. */
    ASSERT_TRUE(netActivity_busy(&na, true, 1000000 + 67439 + 1048576, 60000));
}

TEST(counter_going_back_or_missing_resyncs)
{
    NetActivity na = {0};
    ASSERT_FALSE(netActivity_busy(&na, true, 5000000, 0));
    /* Wi-Fi restarted (counters reset) or a 32-bit counter wrapped. */
    ASSERT_FALSE(netActivity_busy(&na, true, 100, 15000));
    ASSERT_TRUE(netActivity_busy(&na, true, 100 + 1048576, 30000));
    /* Wi-Fi gone: nothing to read, the next reading starts over. */
    ASSERT_FALSE(netActivity_busy(&na, false, 0, 45000));
    ASSERT_FALSE(na.valid);
    ASSERT_FALSE(netActivity_busy(&na, true, 9000000, 60000));
    /* No time elapsed: not busy, whatever the bytes. */
    ASSERT_FALSE(netActivity_busy(&na, true, 99000000, 60000));
}

TEST(keep_awake_needs_a_service_and_traffic)
{
    reset_dirs();
    NetActivity na = {0};
    set_wlan0("0\n", "0\n");
    /* No service on: a transfer does not keep the device awake. */
    ASSERT_FALSE(netActivity_keepAwake(&na, SYSFS, CONFIG, 0));
    set_wlan0("2000000\n", "0\n");
    ASSERT_FALSE(netActivity_keepAwake(&na, SYSFS, CONFIG, 15000));
    ASSERT_FALSE(na.valid);
    /* FTP on: first check records, then a transfer counts. */
    write_text(CONFIG "/.ftpState", "");
    ASSERT_FALSE(netActivity_keepAwake(&na, SYSFS, CONFIG, 30000));
    set_wlan0("2000000\n", "3000000\n");
    ASSERT_TRUE(netActivity_keepAwake(&na, SYSFS, CONFIG, 45000));
    /* Transfer over: idle again. */
    ASSERT_FALSE(netActivity_keepAwake(&na, SYSFS, CONFIG, 60000));
    /* Wi-Fi off with the service still on. */
    (void)system("rm -rf " SYSFS);
    ASSERT_FALSE(netActivity_keepAwake(&na, SYSFS, CONFIG, 75000));
    (void)system("rm -rf " BASE);
}

TEST(keymon_uses_it_in_the_sleep_timer)
{
    FILE *fp = fopen("../src/keymon/keymon.c", "r");
    ASSERT_TRUE(fp != NULL);
    static char src[200000];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';
    const char *check = strstr(src, "netActivity_keepAwake(&net_activity");
    const char *timer = strstr(src, "ticks - hibernate_start > hibernate_time");
    const char *stay = strstr(src, "if (hibernate_time && !temp_flag_get(\"stay_awake\"))");
    ASSERT_TRUE(check != NULL && timer != NULL && stay != NULL);
    /* Inside the sleep-timer check, before the timeout is tested. */
    ASSERT_TRUE(stay < check && check < timer);
}

int main(void)
{
    printf("\n=== net_activity.h (file transfers keep keymon awake) ===\n\n");
    RUN_TEST(counters_of_both_interfaces_are_summed);
    RUN_TEST(only_file_transfer_services_count);
    RUN_TEST(busy_from_two_kb_per_second);
    RUN_TEST(counter_going_back_or_missing_resyncs);
    RUN_TEST(keep_awake_needs_a_service_and_traffic);
    RUN_TEST(keymon_uses_it_in_the_sleep_timer);
    TEST_REPORT();
    return test_failures;
}
