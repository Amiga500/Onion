/**
 * @file test_apply_icons.c
 * @brief Unit tests for src/common/utils/apply_icons.h
 *
 * Tests the pure-logic icon helper functions: icons_getIconMode,
 * icons_getIconNameFormat, icons_getIconPathFormat, and
 * icons_getSelectedIconPathFormat.
 *
 * Tests the production header (no local copy).
 *
 * Build and run: make -f Makefile.unit test_apply_icons
 */

#include "onion_test.h"
#include <string.h>

/* Production code: the icon helpers from utils/apply_icons.h (links file.c,
 * str.c, log.c and cJSON for the JSON-based helpers it also defines). */
#include "utils/apply_icons.h"

/* ---- icons_getIconMode ---- */

TEST(icon_mode_emu_path) {
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/Emu/GBA/config.json"), ICON_MODE_EMU);
}

TEST(icon_mode_app_path) {
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/App/Clock/config.json"), ICON_MODE_APP);
}

TEST(icon_mode_rapp_path) {
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/RApp/Terminal/config.json"), ICON_MODE_RAPP);
}

TEST(icon_mode_unknown_defaults_to_emu) {
    ASSERT_EQ(icons_getIconMode("/some/other/path"), ICON_MODE_EMU);
}

TEST(icon_mode_app_exact_prefix) {
    /* Exact prefix match (no trailing slash) */
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/App"), ICON_MODE_APP);
}

TEST(icon_mode_rapp_exact_prefix) {
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/RApp"), ICON_MODE_RAPP);
}

TEST(icon_mode_emu_exact_prefix) {
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/Emu"), ICON_MODE_EMU);
}

TEST(icon_mode_case_sensitive) {
    /* Paths are case-sensitive */
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/app/Clock"), ICON_MODE_EMU);
    ASSERT_EQ(icons_getIconMode("/mnt/SDCARD/APP/Clock"), ICON_MODE_EMU);
}

/* ---- icons_getIconNameFormat ---- */

TEST(icon_name_format_emu) {
    ASSERT_STREQ(icons_getIconNameFormat(ICON_MODE_EMU), "%s");
}

TEST(icon_name_format_app) {
    ASSERT_STREQ(icons_getIconNameFormat(ICON_MODE_APP), "app/%s");
}

TEST(icon_name_format_rapp) {
    ASSERT_STREQ(icons_getIconNameFormat(ICON_MODE_RAPP), "rapp/%s");
}

/* ---- icons_getIconPathFormat ---- */

TEST(icon_path_format_emu) {
    ASSERT_STREQ(icons_getIconPathFormat(ICON_MODE_EMU), "%s/%s.png");
}

TEST(icon_path_format_app) {
    ASSERT_STREQ(icons_getIconPathFormat(ICON_MODE_APP), "%s/app/%s.png");
}

TEST(icon_path_format_rapp) {
    ASSERT_STREQ(icons_getIconPathFormat(ICON_MODE_RAPP), "%s/rapp/%s.png");
}

/* ---- icons_getSelectedIconPathFormat ---- */

TEST(sel_icon_path_format_emu) {
    ASSERT_STREQ(icons_getSelectedIconPathFormat(ICON_MODE_EMU), "%s/sel/%s.png");
}

TEST(sel_icon_path_format_app) {
    ASSERT_STREQ(icons_getSelectedIconPathFormat(ICON_MODE_APP), "%s/app/sel/%s.png");
}

TEST(sel_icon_path_format_rapp) {
    ASSERT_STREQ(icons_getSelectedIconPathFormat(ICON_MODE_RAPP), "%s/rapp/sel/%s.png");
}

/* ---- snprintf integration: formats produce expected paths ---- */

TEST(icon_path_format_produces_correct_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), icons_getIconPathFormat(ICON_MODE_APP),
             "/mnt/SDCARD/Icons/Default", "GBA");
    ASSERT_STREQ(buf, "/mnt/SDCARD/Icons/Default/app/GBA.png");
}

TEST(sel_icon_path_format_produces_correct_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), icons_getSelectedIconPathFormat(ICON_MODE_RAPP),
             "/mnt/SDCARD/Icons/MyPack", "Terminal");
    ASSERT_STREQ(buf, "/mnt/SDCARD/Icons/MyPack/rapp/sel/Terminal.png");
}

TEST(emu_path_format_produces_correct_path) {
    char buf[256];
    snprintf(buf, sizeof(buf), icons_getIconPathFormat(ICON_MODE_EMU),
             "/mnt/SDCARD/Icons/Default", "SNES");
    ASSERT_STREQ(buf, "/mnt/SDCARD/Icons/Default/SNES.png");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== apply_icons.h Unit Tests ===\n\n");

    RUN_TEST(icon_mode_emu_path);
    RUN_TEST(icon_mode_app_path);
    RUN_TEST(icon_mode_rapp_path);
    RUN_TEST(icon_mode_unknown_defaults_to_emu);
    RUN_TEST(icon_mode_app_exact_prefix);
    RUN_TEST(icon_mode_rapp_exact_prefix);
    RUN_TEST(icon_mode_emu_exact_prefix);
    RUN_TEST(icon_mode_case_sensitive);

    RUN_TEST(icon_name_format_emu);
    RUN_TEST(icon_name_format_app);
    RUN_TEST(icon_name_format_rapp);

    RUN_TEST(icon_path_format_emu);
    RUN_TEST(icon_path_format_app);
    RUN_TEST(icon_path_format_rapp);

    RUN_TEST(sel_icon_path_format_emu);
    RUN_TEST(sel_icon_path_format_app);
    RUN_TEST(sel_icon_path_format_rapp);

    RUN_TEST(icon_path_format_produces_correct_path);
    RUN_TEST(sel_icon_path_format_produces_correct_path);
    RUN_TEST(emu_path_format_produces_correct_path);

    TEST_REPORT();
    return test_failures;
}
