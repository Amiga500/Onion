#include "onion_test.h"

#include "utils/config.h"
#include "utils/file.h"

#include <stdio.h>
#include <unistd.h>

static void setup(void)
{
    mkdirs(CONFIG_PATH);
}

static void teardown(void)
{
    file_remove_tree(CONFIG_PATH);
}

TEST(set_and_get_number)
{
    int v = 0;
    config_setNumber("volume", 12);
    ASSERT_TRUE(config_get("volume", CONFIG_INT, &v));
    ASSERT_EQ(v, 12);
}

TEST(get_missing_is_false)
{
    int v = 99;
    ASSERT_FALSE(config_get("no_such_key", CONFIG_INT, &v));
    ASSERT_EQ(v, 99);
}

TEST(set_and_get_string)
{
    char buf[64] = "";
    config_setString("theme", "onion");
    ASSERT_TRUE(config_get("theme", CONFIG_STR, buf));
    ASSERT_STREQ(buf, "onion");
}

TEST(flag_hidden_inverse)
{
    config_flag_set("mute", true);
    ASSERT_TRUE(config_flag_get("mute"));
    ASSERT_FALSE(flag_get(CONFIG_PATH, "mute_"));
    config_flag_set("mute", false);
    ASSERT_FALSE(config_flag_get("mute"));
    ASSERT_TRUE(flag_get(CONFIG_PATH, "mute_"));
}

int main(void)
{
    printf("\n=== config.h unit tests ===\n\n");
    setup();
    RUN_TEST(set_and_get_number);
    RUN_TEST(get_missing_is_false);
    RUN_TEST(set_and_get_string);
    RUN_TEST(flag_hidden_inverse);
    teardown();
    return onion_test_report("test_config");
}
