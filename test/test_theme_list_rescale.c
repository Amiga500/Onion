/**
 * @file test_theme_list_rescale.c
 * @brief Tests for src/themeSwitcher/themeListRescale.h (production header)
 *
 * Installing a theme in the Themes app runs rescale_theme_list_icons.sh with
 * the arguments Tweaks > Appearance > Game lists uses (theme path, 360 / rows,
 * rows), so a new theme's list icons fit the row count without toggling the
 * option again. The script here records its arguments.
 *
 * Build and run: make -f Makefile.unit test_theme_list_rescale
 */

#include "../src/themeSwitcher/themeListRescale.h"
#include "onion_test.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char dir[] = "/tmp/onion_theme_list_rescale_XXXXXX";
static char script[300], args_file[300], theme[300];

static void write_file(const char *name, const char *content)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *fp = fopen(path, "w");
    fputs(content, fp);
    fclose(fp);
}

static void remove_file(const char *name)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    unlink(path);
}

static void read_args(char *out, size_t size)
{
    out[0] = '\0';
    FILE *fp = fopen(args_file, "r");
    if (fp == NULL)
        return;
    size_t n = fread(out, 1, size - 1, fp);
    out[n] = '\0';
    fclose(fp);
}

static void fake_script(int exit_code)
{
    char body[600];
    snprintf(body, sizeof(body), "echo \"$*\" > \"%s\"\nexit %d\n", args_file, exit_code);
    write_file("rescale.sh", body);
}

static void reset(void)
{
    unlink(args_file);
    remove_file(".romListRows");
    remove_file("gameLists/rescaleThemeListIcons");
    fake_script(0);
}

static void expect_args(const char *expected)
{
    char got[512], want[512];
    read_args(got, sizeof(got));
    snprintf(want, sizeof(want), "%s %s\n", theme, expected);
    ASSERT_STREQ(want, got);
}

TEST(default_six_rows)
{
    reset();
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("60 6");
}

TEST(row_count_from_tweaks)
{
    reset();
    write_file(".romListRows", "8\n");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("45 8");
    write_file(".romListRows", "13");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("27 13");
}

TEST(row_count_clamped_as_tweaks)
{
    reset();
    write_file(".romListRows", "25");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("18 20");
    write_file(".romListRows", "3");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("60 6");
    write_file(".romListRows", "rows");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("60 6");
}

TEST(option_off_runs_nothing)
{
    reset();
    write_file(".romListRows", "8");
    write_file("gameLists/rescaleThemeListIcons", "0");
    ASSERT_FALSE(theme_listRescale(theme, dir, script));
    ASSERT_FALSE(access(args_file, F_OK) == 0);
    write_file("gameLists/rescaleThemeListIcons", "1");
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    expect_args("45 8");
}

TEST(missing_script_or_theme)
{
    reset();
    char missing[400];
    snprintf(missing, sizeof(missing), "%s/none.sh", dir);
    ASSERT_FALSE(theme_listRescale(theme, dir, missing));
    ASSERT_FALSE(theme_listRescale("", dir, script));
    ASSERT_FALSE(theme_listRescale(NULL, dir, script));
    ASSERT_FALSE(access(args_file, F_OK) == 0);
}

TEST(script_failure_reported)
{
    reset();
    fake_script(1);
    ASSERT_FALSE(theme_listRescale(theme, dir, script));
    expect_args("60 6");
}

TEST(path_with_spaces_is_one_argument)
{
    reset();
    char spaced[300];
    snprintf(spaced, sizeof(spaced), "%s", theme);
    snprintf(theme, sizeof(theme), "/mnt/SDCARD/Themes/Silky RedOnion/");
    char body[600];
    snprintf(body, sizeof(body), "printf '%%s|' \"$@\" > \"%s\"\n", args_file);
    write_file("rescale.sh", body);
    ASSERT_TRUE(theme_listRescale(theme, dir, script));
    char got[512];
    read_args(got, sizeof(got));
    ASSERT_STREQ("/mnt/SDCARD/Themes/Silky RedOnion/|60|6|", got);
    snprintf(theme, sizeof(theme), "%s", spaced);
}

int main(void)
{
    printf("\n=== themeListRescale.h Unit Tests ===\n\n");
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    char sub[400];
    snprintf(sub, sizeof(sub), "%s/gameLists", dir);
    mkdir(sub, 0755);
    snprintf(script, sizeof(script), "%s/rescale.sh", dir);
    snprintf(args_file, sizeof(args_file), "%s/args", dir);
    snprintf(theme, sizeof(theme), "/mnt/SDCARD/Themes/Blue/");

    RUN_TEST(default_six_rows);
    RUN_TEST(row_count_from_tweaks);
    RUN_TEST(row_count_clamped_as_tweaks);
    RUN_TEST(option_off_runs_nothing);
    RUN_TEST(missing_script_or_theme);
    RUN_TEST(script_failure_reported);
    RUN_TEST(path_with_spaces_is_one_argument);

    char cmd[400];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    if (system(cmd) != 0)
        return 1;
    TEST_REPORT();
    return test_failures;
}
