/**
 * @file test_theme_marker.c
 * @brief Tests for src/themeSwitcher/themeMarker.h (production header)
 *
 * runtime.sh forces the theme's icons back on at boot when
 * config/theme-applied-<serial> does not name the current theme. The Themes
 * app must keep that marker in sync, in the exact format runtime.sh writes
 * (`echo -n "$system_theme"`), or custom icons are reset after every theme
 * change.
 *
 * Build and run: make -f Makefile.unit test_theme_marker
 */

#include "../src/themeSwitcher/themeMarker.h"
#include "onion_test.h"

#include <stdlib.h>
#include <unistd.h>

static char dir[] = "/tmp/onion_theme_marker_XXXXXX";
static char sn_file[300];

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    fputs(content, fp);
    fclose(fp);
}

static void read_file(const char *path, char *out, size_t size)
{
    out[0] = '\0';
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return;
    size_t n = fread(out, 1, size - 1, fp);
    out[n] = '\0';
    fclose(fp);
}

TEST(marker_path_uses_serial)
{
    char path[512];
    write_file(sn_file, "0123abcd");
    ASSERT_TRUE(theme_markerPath(path, sizeof(path), sn_file, dir));
    char expected[512];
    snprintf(expected, sizeof(expected), "%s/theme-applied-0123abcd", dir);
    ASSERT_STREQ(expected, path);
}

TEST(marker_path_strips_newline)
{
    char path[512], expected[512];
    write_file(sn_file, "0123abcd\n");
    ASSERT_TRUE(theme_markerPath(path, sizeof(path), sn_file, dir));
    snprintf(expected, sizeof(expected), "%s/theme-applied-0123abcd", dir);
    ASSERT_STREQ(expected, path);
}

TEST(no_serial_no_marker)
{
    char path[512];
    remove(sn_file);
    ASSERT_FALSE(theme_markerPath(path, sizeof(path), sn_file, dir));
    ASSERT_FALSE(theme_markApplied("/mnt/SDCARD/Themes/A/", sn_file, dir));

    write_file(sn_file, "");
    ASSERT_FALSE(theme_markerPath(path, sizeof(path), sn_file, dir));

    write_file(sn_file, "../evil");
    ASSERT_FALSE(theme_markerPath(path, sizeof(path), sn_file, dir));
}

TEST(marker_path_too_long_is_refused)
{
    char path[16];
    write_file(sn_file, "0123abcd");
    ASSERT_FALSE(theme_markerPath(path, sizeof(path), sn_file, dir));
}

/* The value runtime.sh compares: the theme path with no trailing newline. */
TEST(mark_applied_writes_runtime_format)
{
    char path[512], content[512];
    write_file(sn_file, "0123abcd");
    ASSERT_TRUE(theme_markApplied("/mnt/SDCARD/Themes/Silky/", sn_file, dir));
    ASSERT_TRUE(theme_markerPath(path, sizeof(path), sn_file, dir));
    read_file(path, content, sizeof(content));
    ASSERT_STREQ("/mnt/SDCARD/Themes/Silky/", content);

    /* A later theme change replaces it completely. */
    ASSERT_TRUE(theme_markApplied("/mnt/SDCARD/Themes/B/", sn_file, dir));
    read_file(path, content, sizeof(content));
    ASSERT_STREQ("/mnt/SDCARD/Themes/B/", content);

    char tmp_path[600];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    ASSERT_FALSE(access(tmp_path, F_OK) == 0);
    remove(path);
}

TEST(mark_applied_null_theme)
{
    write_file(sn_file, "0123abcd");
    ASSERT_FALSE(theme_markApplied(NULL, sn_file, dir));
}

int main(void)
{
    printf("\n=== themeMarker.h Unit Tests ===\n\n");
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(sn_file, sizeof(sn_file), "%s/deviceSN", dir);

    RUN_TEST(marker_path_uses_serial);
    RUN_TEST(marker_path_strips_newline);
    RUN_TEST(no_serial_no_marker);
    RUN_TEST(marker_path_too_long_is_refused);
    RUN_TEST(mark_applied_writes_runtime_format);
    RUN_TEST(mark_applied_null_theme);

    remove(sn_file);
    rmdir(dir);
    TEST_REPORT();
    return test_failures;
}
