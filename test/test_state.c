#include "onion_test.h"

#include "utils/str.h"

#include <string.h>

/*
 * state_getAppName in system/state.h is an inline in a header that pulls
 * display/settings/fbdev. Replicate the 19-byte skip + ';' contract here
 * so the suite stays host-only. Keep in sync with state.h.
 */
static size_t state_getAppName(char *out, const char *str)
{
    char *end;
    size_t out_size;

    str += 19;
    end = (char *)strchr(str, ';');

    out_size = (end - str) < STR_MAX - 1 ? (end - str) : STR_MAX - 1;
    memcpy(out, str, out_size);
    out[out_size] = 0;
    return out_size;
}

TEST(appname_from_cmd_prefix)
{
    /* "cd /mnt/SDCARD/App/" is 19 chars; name runs until ';' */
    const char *cmd = "cd /mnt/SDCARD/App/BatteryMonitor; chmod +x ...";
    char out[STR_MAX];
    size_t n = state_getAppName(out, cmd);
    ASSERT_STREQ(out, "BatteryMonitor");
    ASSERT_EQ(n, strlen("BatteryMonitor"));
}

TEST(prefix_length_is_19)
{
    ASSERT_EQ(strlen("cd /mnt/SDCARD/App/"), 19);
}

int main(void)
{
    printf("\n=== state_getAppName unit tests ===\n\n");
    RUN_TEST(prefix_length_is_19);
    RUN_TEST(appname_from_cmd_prefix);
    return onion_test_report("test_state");
}
