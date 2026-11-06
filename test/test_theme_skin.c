/**
 * @file test_theme_skin.c
 * @brief Theme Switcher: MainUI's fixed images, and compact themes
 *
 * themeSkin.h and the real script/themes_extract_theme.sh (with a fake 7z
 * in PATH), on real folders:
 *  - a theme's image replaces Onion's, which is kept once as _back.png;
 *    the overrides win; without one, Onion's comes back;
 *  - a restore that can't be copied keeps the backup (it was deleted
 *    anyway, losing Onion's image);
 *  - a compact theme whose extraction fails leaves no cut theme folder,
 *    keeps its preview and its archive, and is reported (the archive was
 *    deleted when the cut folder existed);
 *  - a successful extraction puts the theme in place, removes the preview,
 *    and the archive once all its themes are out;
 *  - folder names with ", $ or ` reach the script as they are.
 *
 * Build and run: make -f Makefile.unit test_theme_skin
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/themeSwitcher/themeSkin.h"

#define TEST_DIR "/tmp/onion_test_theme_skin"
#define SKIN TEST_DIR "/skin"            // miyoo/app/skin
#define OVERRIDES TEST_DIR "/overrides/" // Saves/CurrentProfile/theme/
#define THEME TEST_DIR "/Themes/Mine/"
#define THEMES TEST_DIR "/Themes"
#define EXTRACT_SCRIPT "../src/themeSwitcher/script/themes_extract_theme.sh"

static void sh(const char *cmd)
{
    if (system(cmd) != 0)
        fprintf(stderr, "command failed: %s\n", cmd);
}

static void put(const char *path, const char *text)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash != NULL) {
        *slash = '\0';
        mkdirs(dir);
    }
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static bool has(const char *path, const char *text)
{
    char *got = file_read(path);
    bool same = got != NULL && strcmp(got, text) == 0;
    free(got);
    return same;
}

static void reset_dir(void)
{
    sh("rm -rf " TEST_DIR);
    mkdirs(SKIN);
    put(SKIN "/progress-dot.png", "onion");
}

TEST(theme_image_replaces_onions_and_keeps_it)
{
    reset_dir();
    put(THEME "skin/progress-dot.png", "mine");
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, THEME, "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "mine"));
    ASSERT_TRUE(has(SKIN "/progress-dot_back.png", "onion"));

    // Another theme: the backup is still Onion's.
    put(TEST_DIR "/Themes/Other/skin/progress-dot.png", "other");
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, TEST_DIR "/Themes/Other/", "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "other"));
    ASSERT_TRUE(has(SKIN "/progress-dot_back.png", "onion"));

    // An override wins over the theme.
    put(OVERRIDES "skin/progress-dot.png", "override");
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, THEME, "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "override"));
}

TEST(theme_without_image_restores_onions)
{
    reset_dir();
    put(THEME "skin/progress-dot.png", "mine");
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, THEME, "progress-dot", SKIN));
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, TEST_DIR "/Themes/Plain/", "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "onion"));
    ASSERT_FALSE(exists(SKIN "/progress-dot_back.png"));

    // Nothing to restore: nothing changes.
    ASSERT_TRUE(ts_installSkinImage(OVERRIDES, TEST_DIR "/Themes/Plain/", "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "onion"));
}

TEST(failed_restore_keeps_the_backup)
{
    reset_dir();
    put(SKIN "/progress-dot_back.png", "onion");
    remove(SKIN "/progress-dot.png");
    mkdirs(SKIN "/progress-dot.png"); // can't be replaced by a file
    ASSERT_FALSE(ts_installSkinImage(OVERRIDES, TEST_DIR "/Themes/Plain/", "progress-dot", SKIN));
    ASSERT_TRUE(has(SKIN "/progress-dot_back.png", "onion"));
}

TEST(failed_copy_leaves_the_old_file)
{
    reset_dir();
    ASSERT_FALSE(ts_copyFileAtomic(TEST_DIR "/missing.png", SKIN "/progress-dot.png"));
    ASSERT_TRUE(has(SKIN "/progress-dot.png", "onion"));
}

// A fake 7z: "x" makes the theme from $FAKE_7Z_TREE (or a cut one, failing,
// with FAKE_7Z_FAIL), "l" lists the archive's config.json files.
static void fake_7z(void)
{
    put(TEST_DIR "/bin/7z",
        "#!/bin/sh\n"
        "cmd=$1; shift\n"
        "if [ \"$cmd\" = l ]; then\n"
        "  echo 'Path = archive'\n"
        "  for t in $FAKE_7Z_THEMES; do echo \"Path = $t/config.json\"; done\n"
        "  exit 0\n"
        "fi\n"
        "for a in \"$@\"; do case \"$a\" in -o*) out=${a#-o};; esac; done\n"
        "mkdir -p \"$out/Mine\"\n"
        "echo cut > \"$out/Mine/skin.png\"\n"
        "[ -n \"$FAKE_7Z_FAIL\" ] && exit 2\n"
        "echo '{}' > \"$out/Mine/config.json\"\n"
        "exit 0\n");
    chmod(TEST_DIR "/bin/7z", 0755);
    static char path[PATH_MAX * 2];
    const char *old = getenv("PATH");
    snprintf(path, sizeof(path), TEST_DIR "/bin:%s", old != NULL ? old : "/usr/bin:/bin");
    setenv("PATH", path, 1);
    setenv("THEMES_DIR", THEMES, 1);
    setenv("FAKE_7Z_THEMES", "Mine", 1);
}

static void compact_theme(void)
{
    reset_dir();
    put(THEMES "/Mine.7z", "archive");
    put(THEMES "/.previews/Mine/config.json", "{}");
    put(THEMES "/.previews/Mine/source", THEMES "/Mine.7z");
    fake_7z();
}

TEST(failed_extraction_changes_nothing)
{
    compact_theme();
    setenv("FAKE_7Z_FAIL", "1", 1);
    char path[PATH_MAX];
    ASSERT_FALSE(ts_extractCompactTheme(EXTRACT_SCRIPT, THEMES "/.previews/Mine/", THEMES, path, sizeof(path)));
    unsetenv("FAKE_7Z_FAIL");
    ASSERT_FALSE(exists(THEMES "/Mine"));                 // no cut theme
    ASSERT_TRUE(exists(THEMES "/.previews/Mine/source")); // still listed
    ASSERT_TRUE(has(THEMES "/Mine.7z", "archive"));       // not deleted
    ASSERT_FALSE(exists(THEMES "/.extracting"));
}

TEST(failed_extraction_over_an_older_copy)
{
    // An older copy is there: it stays as it was, and the install is
    // still reported as failed (not done with the old copy).
    compact_theme();
    put(THEMES "/Mine/config.json", "{\"old\":1}");
    setenv("FAKE_7Z_FAIL", "1", 1);
    char path[PATH_MAX];
    ASSERT_FALSE(ts_extractCompactTheme(EXTRACT_SCRIPT, THEMES "/.previews/Mine/", THEMES, path, sizeof(path)));
    unsetenv("FAKE_7Z_FAIL");
    ASSERT_TRUE(has(THEMES "/Mine/config.json", "{\"old\":1}"));
    ASSERT_FALSE(exists(THEMES "/Mine/skin.png"));
    ASSERT_TRUE(has(THEMES "/Mine.7z", "archive"));
}

TEST(script_success_without_a_theme_is_a_failure)
{
    compact_theme();
    put(TEST_DIR "/ok.sh", "exit 0\n");
    char path[PATH_MAX];
    ASSERT_FALSE(ts_extractCompactTheme(TEST_DIR "/ok.sh", THEMES "/.previews/Mine/", THEMES, path, sizeof(path)));
}

TEST(extraction_puts_the_theme_in_place)
{
    compact_theme();
    put(THEMES "/Mine/stale.png", "from an older copy");
    char path[PATH_MAX];
    ASSERT_TRUE(ts_extractCompactTheme(EXTRACT_SCRIPT, THEMES "/.previews/Mine/", THEMES, path, sizeof(path)));
    ASSERT_STREQ(path, THEMES "/Mine/");
    ASSERT_TRUE(exists(THEMES "/Mine/config.json"));
    ASSERT_FALSE(exists(THEMES "/Mine/stale.png")); // replaced whole
    ASSERT_FALSE(exists(THEMES "/.previews/Mine"));
    ASSERT_FALSE(exists(THEMES "/Mine.7z")); // all its themes are out
    ASSERT_FALSE(exists(THEMES "/.Mine.old"));
    ASSERT_FALSE(exists(THEMES "/.extracting"));
}

TEST(archive_with_another_theme_is_kept)
{
    compact_theme();
    setenv("FAKE_7Z_THEMES", "Mine Second", 1);
    char path[PATH_MAX];
    ASSERT_TRUE(ts_extractCompactTheme(EXTRACT_SCRIPT, THEMES "/.previews/Mine/", THEMES, path, sizeof(path)));
    ASSERT_TRUE(has(THEMES "/Mine.7z", "archive")); // Second is still in it
}

TEST(script_gets_names_as_they_are)
{
    reset_dir();
    put(TEST_DIR "/echo.sh", "printf '%s' \"$1\" > " TEST_DIR "/got\n");
    const char *name = TEST_DIR "/Themes/My \"$HOME\" `x` theme/";
    ASSERT_TRUE(ts_runScript(TEST_DIR "/echo.sh", name));
    ASSERT_TRUE(has(TEST_DIR "/got", name));

    char out[16];
    ASSERT_TRUE(ts_lastName("/a/b/Theme/", out, sizeof(out)));
    ASSERT_STREQ(out, "Theme");
    ASSERT_FALSE(ts_lastName("/", out, sizeof(out)));
}

int main(void)
{
    printf("\n=== themeSwitcher/themeSkin.h ===\n\n");
    RUN_TEST(theme_image_replaces_onions_and_keeps_it);
    RUN_TEST(theme_without_image_restores_onions);
    RUN_TEST(failed_restore_keeps_the_backup);
    RUN_TEST(failed_copy_leaves_the_old_file);
    RUN_TEST(failed_extraction_changes_nothing);
    RUN_TEST(failed_extraction_over_an_older_copy);
    RUN_TEST(script_success_without_a_theme_is_a_failure);
    RUN_TEST(extraction_puts_the_theme_in_place);
    RUN_TEST(archive_with_another_theme_is_kept);
    RUN_TEST(script_gets_names_as_they_are);
    sh("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
