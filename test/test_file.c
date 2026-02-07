/**
 * @file test_file.c
 * @brief Unit tests for src/common/utils/file.c helpers.
 */

#include "onion_test.h"
#include "../src/common/utils/file.h"

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *get_temp_path(void)
{
    static char path_template[PATH_MAX] = "";
    static bool initialized = false;

    if (!initialized) {
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir == NULL || tmpdir[0] == '\0')
            tmpdir = "/tmp";

        int written =
            snprintf(path_template, sizeof(path_template), "%s/onion_file_testXXXXXX", tmpdir);
        if (written < 0 || (size_t)written >= sizeof(path_template))
            return NULL;

        int fd = mkstemp(path_template);
        if (fd == -1)
            return NULL;
        close(fd);
        unlink(path_template);
        initialized = true;
    }

    return path_template;
}

static bool write_text(const char *content)
{
    const char *path = get_temp_path();
    if (path == NULL)
        return false;

    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return false;
    bool ok = fputs(content, fp) >= 0;
    fclose(fp);
    return ok;
}

TEST(file_readLastLine_basic)
{
    ASSERT_TRUE(write_text("first\nsecond\nthird\n"));

    char out[FILE_LASTLINE_MAX + 1];
    const char *path = get_temp_path();
    ASSERT_NOT_NULL(path);

    file_readLastLine(path, out);

    ASSERT_STREQ("third", out);
}

TEST(file_readLastLine_missing_file)
{
    const char *path = get_temp_path();
    ASSERT_NOT_NULL(path);

    remove(path);

    char out[FILE_LASTLINE_MAX + 1];
    /* Ensure the buffer is overwritten even when the file is missing. */
    memset(out, 'x', sizeof(out));

    file_readLastLine(path, out);

    ASSERT_STREQ("", out);
}

TEST(file_readLastLine_large_file)
{
    const char *path = get_temp_path();
    ASSERT_NOT_NULL(path);

    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);

    for (int i = 0; i < 80; i++)
        fprintf(fp, "line%02d\n", i);

    fclose(fp);

    char out[FILE_LASTLINE_MAX + 1];
    file_readLastLine(path, out);

    ASSERT_STREQ("line79", out);
}

int main(void)
{
    printf("\n=== file.c Unit Tests ===\n\n");

    RUN_TEST(file_readLastLine_basic);
    RUN_TEST(file_readLastLine_missing_file);
    RUN_TEST(file_readLastLine_large_file);

    TEST_REPORT();

    const char *path = get_temp_path();
    if (path != NULL)
        remove(path);
    return test_failures;
}
