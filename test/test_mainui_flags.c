/**
 * @file test_mainui_flags.c
 * @brief Tweaks > Appearance > Footer layout and Long menu labels
 *        (src/common/system/settings.h, src/tweaks/actions.h)
 *
 * Open MainUI 1.0.10 reads two flag files in .tmp_update/config:
 *  - .mainuiFooterAltLayout: page number on the left, hints B then A;
 *  - .noContextMenuEllipsis: long SELECT menu labels cut, without "…".
 * Tweaks shows them as Footer layout (Classic / B A, page left) and Long
 * menu labels (Shorten with ... / Cut). A file made by hand, as Open MainUI
 * documents, shows as the matching choice.
 *
 * Build and run: make -f Makefile.unit test_mainui_flags
 */

#define TEST_DIR "/tmp/onion_test_mainui_flags"
#define CONFIG_PATH TEST_DIR "/config/"
#define MAIN_UI_SETTINGS TEST_DIR "/system.json"

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* settings.h sends the volume to /dev/mi_ao through volume.h. */
#define VOLUME_H__
#define MAX_VOLUME 20
static int setVolume(int value) { return value; }

#include "system/settings.h"

/* actions.h needs the whole Tweaks app; these two are copied from it and
 * checked against it below, so a change there fails here. */
typedef struct {
    int value;
} ListItemStub;

static void action_setFooterLayout(void *pt)
{
    settings.mainui_footer_alt = ((ListItemStub *)pt)->value == 1;
    config_flag_set(".mainuiFooterAltLayout", settings.mainui_footer_alt);
}

static void action_setLongMenuLabels(void *pt)
{
    settings.mainui_menu_no_ellipsis = ((ListItemStub *)pt)->value == 1;
    config_flag_set(".noContextMenuEllipsis", settings.mainui_menu_no_ellipsis);
}

static void reset_dir(void)
{
    (void)!system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR "/config");
    __settings_snapshot_valid = false;
}

static bool flag_file(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s%s", CONFIG_PATH, name);
    return access(path, F_OK) == 0;
}

static bool file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    static char buf[1 << 20];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

TEST(defaults_are_stock)
{
    reset_dir();
    settings_load();
    ASSERT_FALSE(settings.mainui_footer_alt);
    ASSERT_FALSE(settings.mainui_menu_no_ellipsis);
    settings_save();
    ASSERT_FALSE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_FALSE(flag_file(".noContextMenuEllipsis"));
}

TEST(footer_b_a_writes_the_flag_and_classic_removes_it)
{
    reset_dir();
    settings_load();
    settings_save();

    ListItemStub item = {.value = 1};
    action_setFooterLayout(&item);
    settings_save();
    ASSERT_TRUE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_FALSE(flag_file(".noContextMenuEllipsis"));

    settings_load();
    ASSERT_TRUE(settings.mainui_footer_alt);

    item.value = 0;
    action_setFooterLayout(&item);
    settings_save();
    ASSERT_FALSE(flag_file(".mainuiFooterAltLayout"));
}

TEST(cut_labels_writes_the_flag_and_shorten_removes_it)
{
    reset_dir();
    settings_load();
    settings_save();

    ListItemStub item = {.value = 1};
    action_setLongMenuLabels(&item);
    settings_save();
    ASSERT_TRUE(flag_file(".noContextMenuEllipsis"));
    ASSERT_FALSE(flag_file(".mainuiFooterAltLayout"));

    settings_load();
    ASSERT_TRUE(settings.mainui_menu_no_ellipsis);

    item.value = 0;
    action_setLongMenuLabels(&item);
    settings_save();
    ASSERT_FALSE(flag_file(".noContextMenuEllipsis"));
}

TEST(hand_made_files_show_as_on)
{
    reset_dir();
    (void)!system("touch " CONFIG_PATH ".mainuiFooterAltLayout " CONFIG_PATH ".noContextMenuEllipsis");
    settings_load();
    ASSERT_TRUE(settings.mainui_footer_alt);
    ASSERT_TRUE(settings.mainui_menu_no_ellipsis);
    /* A save of something else keeps them. */
    settings.vibration = 1;
    settings_save();
    ASSERT_TRUE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_TRUE(flag_file(".noContextMenuEllipsis"));
}

/* A choice is on the card at once, before Tweaks closes, so the footer of
 * Tweaks and the other programs can follow it. */
TEST(choice_is_written_at_once)
{
    reset_dir();
    settings_load();
    ListItemStub item = {.value = 1};
    action_setFooterLayout(&item);
    action_setLongMenuLabels(&item);
    ASSERT_TRUE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_TRUE(flag_file(".noContextMenuEllipsis"));

    item.value = 0;
    action_setFooterLayout(&item);
    action_setLongMenuLabels(&item);
    ASSERT_FALSE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_FALSE(flag_file(".noContextMenuEllipsis"));

    /* Closing Tweaks keeps the last choice. */
    item.value = 1;
    action_setFooterLayout(&item);
    settings_save();
    ASSERT_TRUE(flag_file(".mainuiFooterAltLayout"));
    ASSERT_FALSE(flag_file(".noContextMenuEllipsis"));
}

/* The real Tweaks code: the actions above match it, and the menu offers
 * the two choices in the order of the flag (0 = no file). */
TEST(tweaks_sources_match)
{
    ASSERT_TRUE(file_contains("../src/tweaks/actions.h",
                              "settings.mainui_footer_alt = ((ListItem *)pt)->value == 1;"));
    ASSERT_TRUE(file_contains("../src/tweaks/actions.h",
                              "settings.mainui_menu_no_ellipsis = ((ListItem *)pt)->value == 1;"));
    ASSERT_TRUE(file_contains("../src/tweaks/actions.h",
                              "config_flag_set(\".mainuiFooterAltLayout\", settings.mainui_footer_alt);"));
    ASSERT_TRUE(file_contains("../src/tweaks/actions.h",
                              "config_flag_set(\".noContextMenuEllipsis\", settings.mainui_menu_no_ellipsis);"));
    /* Tweaks redraws its own footer in the new layout right away. */
    ASSERT_TRUE(file_contains("../src/tweaks/actions.h",
                              "theme_footerSetAltLayout(settings.mainui_footer_alt);\n"
                              "    footer_changed = true;"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h",
                              ".value_labels = {\"Classic\", \"B A, page left\"},"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h",
                              ".value_labels = {\"Shorten with ...\", \"Cut\"},"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h", ".value = settings.mainui_footer_alt,"));
    ASSERT_TRUE(file_contains("../src/tweaks/menus.h", ".value = settings.mainui_menu_no_ellipsis,"));
}

/* list_addItem() drops items past the capacity given to list_create, so the
 * Appearance menu must have room for every item it adds. */
TEST(appearance_menu_has_room_for_every_item)
{
    FILE *fp = fopen("../src/tweaks/menus.h", "r");
    ASSERT_NOT_NULL(fp);
    static char src[1 << 20];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';

    char *start = strstr(src, "void menu_userInterface(");
    ASSERT_NOT_NULL(start);
    char *end = strstr(start, "menu_stack[++menu_level] = &_menu_user_interface;");
    ASSERT_NOT_NULL(end);
    *end = '\0';

    int capacity = 0;
    char *create = strstr(start, "_menu_user_interface = list_createWithTitle(");
    ASSERT_NOT_NULL(create);
    ASSERT_EQ(sscanf(create, "_menu_user_interface = list_createWithTitle(%d", &capacity), 1);

    int added = 0;
    for (char *p = start; (p = strstr(p, "list_addItem")) != NULL; p++)
        added++;
    ASSERT_EQ(added, capacity);
}

int main(void)
{
    printf("\n=== Tweaks: Footer layout and Long menu labels ===\n\n");

    RUN_TEST(defaults_are_stock);
    RUN_TEST(footer_b_a_writes_the_flag_and_classic_removes_it);
    RUN_TEST(cut_labels_writes_the_flag_and_shorten_removes_it);
    RUN_TEST(hand_made_files_show_as_on);
    RUN_TEST(choice_is_written_at_once);
    RUN_TEST(tweaks_sources_match);
    RUN_TEST(appearance_menu_has_room_for_every_item);

    (void)!system("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures;
}
