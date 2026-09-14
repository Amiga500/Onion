#include "onion_test.h"

#include "utils/flags.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_dir[256];

static void setup(void)
{
    snprintf(g_dir, sizeof(g_dir), "/tmp/onion_unit_flags_%d/", (int)getpid());
    mkdir(g_dir, 0755);
}

static void teardown(void)
{
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_dir);
    system(cmd);
}

TEST(flag_set_true_creates)
{
    ASSERT_FALSE(flag_get(g_dir, "wifi"));
    flag_set(g_dir, "wifi", true);
    ASSERT_TRUE(flag_get(g_dir, "wifi"));
}

TEST(flag_set_false_removes)
{
    flag_set(g_dir, "wifi", true);
    flag_set(g_dir, "wifi", false);
    ASSERT_FALSE(flag_get(g_dir, "wifi"));
}

TEST(config_style_hidden_inverse)
{
    /* config_flag_set(key, value) writes key and key_ inverted. */
    char hidden[STR_MAX];
    concat(hidden, "mute", "_");
    flag_set(g_dir, "mute", true);
    flag_set(g_dir, hidden, false);
    ASSERT_TRUE(flag_get(g_dir, "mute"));
    ASSERT_FALSE(flag_get(g_dir, "mute_"));

    flag_set(g_dir, "mute", false);
    flag_set(g_dir, hidden, true);
    ASSERT_FALSE(flag_get(g_dir, "mute"));
    ASSERT_TRUE(flag_get(g_dir, "mute_"));
}

TEST(temp_flag_macros)
{
    temp_flag_set("onion_unit_tmpflag", true);
    ASSERT_TRUE(temp_flag_get("onion_unit_tmpflag"));
    temp_flag_set("onion_unit_tmpflag", false);
    ASSERT_FALSE(temp_flag_get("onion_unit_tmpflag"));
}

int main(void)
{
    printf("\n=== flags.h unit tests ===\n\n");
    setup();
    RUN_TEST(flag_set_true_creates);
    RUN_TEST(flag_set_false_removes);
    RUN_TEST(config_style_hidden_inverse);
    RUN_TEST(temp_flag_macros);
    teardown();
    return onion_test_report("test_flags");
}
