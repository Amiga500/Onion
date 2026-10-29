/**
 * @file test_mainui_shortcut.c
 * @brief Tweaks > Button shortcuts > MainUI X / Y button, and keymon
 *        (utils/mainui_shortcut.h, tweaks/formatters.h, values.h,
 *        actions.h, keymon/menuButtonAction.h)
 *
 * Open MainUI 1.0.10 reads "glo" (Game List Options) and "fav" (toggle
 * favorite) for both buttons in keymap.json; 1.1.0 rc9 adds "disable" (or
 * false), which turns the button off in MainUI. Tweaks lists them after the
 * apps and the tools for X and Y; keymon flags the next launch for GLO on
 * either button.
 *
 * The three Tweaks functions are taken from the real sources by the
 * Makefile (sed between the function name and the closing brace) and run
 * here with two apps and the real tool names: the choice shown, the value
 * saved and the value read back must agree for every position.
 *
 * Build and run: make -f Makefile.unit test_mainui_shortcut
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils/mainui_shortcut.h"
#include "utils/str.h"

#define JSON_STRING_LEN 256

typedef struct {
    int value;
    int action_id;
} ListItem;

typedef struct {
    char dirName[STR_MAX];
    char label[STR_MAX];
    bool is_duplicate;
} InstalledApp;

static InstalledApp fake_apps[] = {{"Clock", "Clock", false}, {"Green", "PDF Reader", false}};
static int installed_apps_count = 2;
static InstalledApp *getInstalledApps(bool sort)
{
    (void)sort;
    return fake_apps;
}

#include "../src/tweaks/tools_defs.h"

static struct {
    char mainui_button_x[JSON_STRING_LEN];
    char mainui_button_y[JSON_STRING_LEN];
} settings;

#include SHORTCUT_FNS

static int max_value(void) { return installed_apps_count + NUM_TOOLS + MAINUI_SHORTCUT_EXTRA_COUNT; }

static void choose(int button, int value, char *label)
{
    ListItem item = {.value = value, .action_id = button};
    action_setAppShortcut(&item);
    formatter_appShortcut(&item, label);
}

static const char *saved(int button) { return button == 0 ? settings.mainui_button_x : settings.mainui_button_y; }

TEST(both_buttons_offer_glo_and_toggle_favorite)
{
    char label[STR_MAX];
    for (int button = 0; button < 2; button++) {
        choose(button, max_value() - 2, label);
        ASSERT_STREQ(label, "GLO");
        ASSERT_STREQ(saved(button), "glo");
        choose(button, max_value() - 1, label);
        ASSERT_STREQ(label, "Toggle favorite");
        ASSERT_STREQ(saved(button), "fav");
    }
}

/* Disabled is the last choice on both buttons and saves "disable". */
TEST(both_buttons_offer_disabled_last)
{
    char label[STR_MAX];
    for (int button = 0; button < 2; button++) {
        choose(button, max_value(), label);
        ASSERT_STREQ(label, "Disabled");
        ASSERT_STREQ(saved(button), "disable");
        ASSERT_EQ(value_appShortcut(button), max_value());
    }
}

/* Every position: what Tweaks shows, saves and reads back agree. */
TEST(every_choice_round_trips)
{
    char label[STR_MAX];
    for (int button = 0; button < 2; button++) {
        for (int value = 0; value <= max_value(); value++) {
            choose(button, value, label);
            ASSERT_EQ(value_appShortcut(button), value);
        }
    }
}

TEST(default_is_the_plain_button)
{
    char label[STR_MAX];
    choose(0, 0, label);
    ASSERT_STREQ(label, "B button");
    ASSERT_STREQ(settings.mainui_button_x, "");
    choose(1, 0, label);
    ASSERT_STREQ(label, "A button");
    ASSERT_STREQ(settings.mainui_button_y, "");
}

TEST(apps_and_tools_keep_their_places)
{
    char label[STR_MAX];
    choose(0, 2, label);
    ASSERT_STREQ(label, "PDF Reader");
    ASSERT_STREQ(settings.mainui_button_x, "app:Green");
    choose(1, 3, label);
    ASSERT_EQ(strncmp(label, "Tool: ", 6), 0);
    ASSERT_EQ(strncmp(settings.mainui_button_y, "tool:", 5), 0);
}

/* A Y set to GLO before this change keeps reading as GLO. */
TEST(old_y_glo_still_reads_as_glo)
{
    char label[STR_MAX];
    strcpy(settings.mainui_button_y, "glo");
    ListItem item = {.value = value_appShortcut(1), .action_id = 1};
    formatter_appShortcut(&item, label);
    ASSERT_STREQ(label, "GLO");
}

TEST(header_values)
{
    ASSERT_EQ(mainui_shortcut_extraIndex("glo"), 0);
    ASSERT_EQ(mainui_shortcut_extraIndex("fav"), 1);
    ASSERT_EQ(mainui_shortcut_extraIndex("disable"), 2);
    ASSERT_EQ(mainui_shortcut_extraIndex("Disable"), -1);
    ASSERT_FALSE(mainui_shortcut_launchesGlo("disable"));
    ASSERT_EQ(mainui_shortcut_extraIndex("app:Clock"), -1);
    ASSERT_EQ(mainui_shortcut_extraIndex(""), -1);
    ASSERT_EQ(mainui_shortcut_extraIndex(NULL), -1);
    ASSERT_TRUE(mainui_shortcut_launchesGlo("glo"));
    ASSERT_FALSE(mainui_shortcut_launchesGlo("fav"));
    ASSERT_FALSE(mainui_shortcut_launchesGlo(NULL));
}

static bool file_contains(const char *path, const char *needle)
{
    static char buf[1 << 20];
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

/* keymon: GLO on either button, nothing for "fav"; Tweaks offers the same
 * number of choices on both buttons. */
TEST(keymon_and_menu_sources)
{
    ASSERT_TRUE(file_contains("../src/keymon/menuButtonAction.h",
                              "if (mainui_shortcut_launchesGlo(action)) {\n"
                              "        temp_flag_set(\"launch_alt\", true);"));
    ASSERT_FALSE(file_contains("../src/keymon/menuButtonAction.h", "button == 1 && strcmp(action, \"glo\")"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h",
                              ".value_max = installed_apps_count + NUM_TOOLS + MAINUI_SHORTCUT_EXTRA_COUNT,\n"
                              "                                     .value = value_appShortcut(0),"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h",
                              ".value_max = installed_apps_count + NUM_TOOLS + MAINUI_SHORTCUT_EXTRA_COUNT,\n"
                              "                                     .value = value_appShortcut(1),"));
    /* keymap.json false reads as "disable", so saving keeps it off. */
    ASSERT_TRUE(file_contains("../src/common/system/settings.h",
                              "if (cJSON_IsFalse(cJSON_GetObjectItem(keymap, \"mainui_button_x\")))\n"
                              "        strcpy(settings.mainui_button_x, \"disable\");"));
    ASSERT_TRUE(file_contains("../src/common/system/settings.h",
                              "if (cJSON_IsFalse(cJSON_GetObjectItem(keymap, \"mainui_button_y\")))\n"
                              "        strcpy(settings.mainui_button_y, \"disable\");"));
}

int main(void)
{
    printf("\n=== MainUI X/Y shortcuts: GLO, Toggle favorite, Disabled ===\n\n");

    RUN_TEST(both_buttons_offer_glo_and_toggle_favorite);
    RUN_TEST(both_buttons_offer_disabled_last);
    RUN_TEST(every_choice_round_trips);
    RUN_TEST(default_is_the_plain_button);
    RUN_TEST(apps_and_tools_keep_their_places);
    RUN_TEST(old_y_glo_still_reads_as_glo);
    RUN_TEST(header_values);
    RUN_TEST(keymon_and_menu_sources);

    TEST_REPORT();
    return test_failures;
}
