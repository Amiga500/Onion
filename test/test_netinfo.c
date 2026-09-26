/**
 * @file test_netinfo.c
 * @brief Tests for netinfo_getIpAddress() from src/common/utils/netinfo.h
 *        (production header)
 *
 * Tweaks shows the IP address in the Network menu. With no address (Wi-Fi
 * off) the ioctl fails; the label must then read 0.0.0.0, not whatever an
 * uninitialised struct ifreq held, which could also change between calls
 * and redraw the menu every frame.
 *
 * Build and run: make -f Makefile.unit test_netinfo
 */

#include "onion_test.h"

#include "utils/netinfo.h"

TEST(missing_interface_reads_zero)
{
    char label[STR_MAX] = "";
    ASSERT_TRUE(netinfo_getIpAddress(label, "nosuchif0"));
    ASSERT_STREQ("IP address: 0.0.0.0 (nosuchif0)", label);
}

TEST(unchanged_address_reports_no_change)
{
    char label[STR_MAX] = "";
    ASSERT_TRUE(netinfo_getIpAddress(label, "nosuchif0"));
    for (int i = 0; i < 50; i++)
        ASSERT_FALSE(netinfo_getIpAddress(label, "nosuchif0"));
}

TEST(loopback_address)
{
    char label[STR_MAX] = "";
    netinfo_getIpAddress(label, "lo");
    /* 127.0.0.1 on any normal host; if lo has no IPv4 address the label
     * must still be the zero address, never garbage. */
    ASSERT_TRUE(strcmp(label, "IP address: 127.0.0.1 (lo)") == 0 ||
                strcmp(label, "IP address: 0.0.0.0 (lo)") == 0);
}

int main(void)
{
    printf("\n=== netinfo.h Unit Tests ===\n\n");
    RUN_TEST(missing_interface_reads_zero);
    RUN_TEST(unchanged_address_reports_no_change);
    RUN_TEST(loopback_address);
    TEST_REPORT();
    return test_failures;
}
