/**
 * @file test_theme_previews.c
 * @brief Tests for src/themeSwitcher/themePreview.h (production header)
 *
 * Compact themes are stored on the card as archives; the Themes app keeps only
 * a lightweight preview under Themes/.previews/<name>/ with a `source` file
 * naming the archive. The theme list shows a preview entry, so when the archive
 * is deleted (e.g. from the web file manager, which leaves the hidden .previews
 * cache untouched) the entry becomes an orphan: still listed, but the theme can
 * no longer be loaded. themePreview_hasArchive() is the predicate the listing
 * uses to skip such orphans; these tests pin its behaviour.
 *
 * Build and run: make -f Makefile.unit test_theme_previews
 */

#include "../src/themeSwitcher/themePreview.h"
#include "onion_test.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char root[] = "/tmp/onion_theme_previews_XXXXXX";

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror(path);
        return;
    }
    fputs(content, fp);
    fclose(fp);
}

static void make_preview(const char *name, const char *source_line)
{
    char dir[512], src[600];
    snprintf(dir, sizeof(dir), "%s/.previews/%s", root, name);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.previews/%s/config.json", root, name);
    write_file(dir, "{}\n");
    snprintf(src, sizeof(src), "%s/.previews/%s/source", root, name);
    if (source_line != NULL)
        write_file(src, source_line);
}

static void preview_dir(char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s/.previews/%s/", root, name);
}

/* A preview whose source archive still exists is usable. */
TEST(preview_with_existing_archive_is_usable)
{
    char archive[512], line[600], dir[512];
    snprintf(archive, sizeof(archive), "%s/Good.zip", root);
    write_file(archive, "PK\x03\x04");
    snprintf(line, sizeof(line), "%s\n", archive);
    make_preview("Good", line);

    preview_dir(dir, sizeof(dir), "Good");
    ASSERT_TRUE(themePreview_hasArchive(dir));
}

/* The bug: the archive was deleted, the preview is an orphan -> not usable. */
TEST(preview_with_missing_archive_is_orphan)
{
    char line[600], dir[512];
    snprintf(line, sizeof(line), "%s/Deleted.zip\n", root); /* never created */
    make_preview("Orphan", line);

    preview_dir(dir, sizeof(dir), "Orphan");
    ASSERT_FALSE(themePreview_hasArchive(dir));
}

/* A trailing-newline-free source line is handled the same way. */
TEST(source_without_newline_is_read)
{
    char archive[512], dir[512];
    snprintf(archive, sizeof(archive), "%s/NoNewline.zip", root);
    write_file(archive, "PK");
    make_preview("NoNewline", archive); /* no '\n' */

    preview_dir(dir, sizeof(dir), "NoNewline");
    ASSERT_TRUE(themePreview_hasArchive(dir));
}

/* A preview directory with no `source` file is not usable. */
TEST(preview_without_source_is_not_usable)
{
    char dir[512];
    make_preview("NoSource", NULL);

    preview_dir(dir, sizeof(dir), "NoSource");
    ASSERT_FALSE(themePreview_hasArchive(dir));
}

/* An empty `source` file is not usable (no archive path to check). */
TEST(empty_source_is_not_usable)
{
    char dir[512];
    make_preview("EmptySource", "");

    preview_dir(dir, sizeof(dir), "EmptySource");
    ASSERT_FALSE(themePreview_hasArchive(dir));
}

/* A path that is not a directory, and NULL, are rejected without crashing. */
TEST(missing_dir_and_null_are_rejected)
{
    char dir[512];
    preview_dir(dir, sizeof(dir), "DoesNotExist");
    ASSERT_FALSE(themePreview_hasArchive(dir));
    ASSERT_FALSE(themePreview_hasArchive(NULL));
}

int main(void)
{
    printf("\n=== themePreview.h Unit Tests ===\n\n");
    if (mkdtemp(root) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    char previews[512];
    snprintf(previews, sizeof(previews), "%s/.previews", root);
    mkdir(previews, 0755);

    RUN_TEST(preview_with_existing_archive_is_usable);
    RUN_TEST(preview_with_missing_archive_is_orphan);
    RUN_TEST(source_without_newline_is_read);
    RUN_TEST(preview_without_source_is_not_usable);
    RUN_TEST(empty_source_is_not_usable);
    RUN_TEST(missing_dir_and_null_are_rejected);

    TEST_REPORT();
    return test_failures;
}
