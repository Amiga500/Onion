/**
 * @file test_warn_reload.c
 * @brief Tests for warnReload_changed() (src/batmon/warn_reload.h)
 *
 * batmon re-reads the low-battery threshold when battery/warnAt changes,
 * so a value set in Tweaks applies within a second instead of at the next
 * 15 s check.
 *
 * Build and run: make -f Makefile.unit test_warn_reload
 */

#include "onion_test.h"

#include <stdio.h>
#include <unistd.h>
#include <utime.h>

#include "../src/batmon/warn_reload.h"

static char path[128];

static void write_file(const char *text, time_t mtime)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
    struct utimbuf t = {mtime, mtime};
    utime(path, &t);
}

TEST(first_read_and_unchanged_file) {
    snprintf(path, sizeof(path), "/tmp/test_warn_reload_%d", (int)getpid());
    time_t last = 0;
    write_file("10", 1000000);
    ASSERT_TRUE(warnReload_changed(path, &last));
    ASSERT_FALSE(warnReload_changed(path, &last));
    ASSERT_FALSE(warnReload_changed(path, &last));
    remove(path);
}

TEST(new_value_from_tweaks) {
    snprintf(path, sizeof(path), "/tmp/test_warn_reload_%d", (int)getpid());
    time_t last = 0;
    write_file("10", 1000000);
    ASSERT_TRUE(warnReload_changed(path, &last));
    write_file("20", 1000004);
    ASSERT_TRUE(warnReload_changed(path, &last));
    ASSERT_FALSE(warnReload_changed(path, &last));
    remove(path);
}

TEST(missing_file_changes_nothing) {
    time_t last = 1234;
    ASSERT_FALSE(warnReload_changed("/tmp/no_such_warn_at_file_xyz", &last));
    ASSERT_EQ((long)last, 1234L);
}

int main(void)
{
    printf("\n=== warn_reload.h Unit Tests ===\n\n");

    RUN_TEST(first_read_and_unchanged_file);
    RUN_TEST(new_value_from_tweaks);
    RUN_TEST(missing_file_changes_nothing);

    TEST_REPORT();
    return test_failures;
}
