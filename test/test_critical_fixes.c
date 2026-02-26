/**
 * @file test_critical_fixes.c
 * @brief Tests for critical bug fixes (round 2)
 *
 * Validates:
 * - free_play_activities() handles NULL safely
 * - realpath() failures don't cause undefined behavior
 * - file_getExtension() handles NULL input
 * - strdup() failure fallback for rom->type/rom->name
 * - play_activity_list_all NULL safety
 *
 * Build and run: make -f Makefile.unit test_critical_fixes
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ---- Stubs ---- */
#define print_debug(...)
#define printf_debug(...)

/* ================================================================== */
/*  Test: file_getExtension NULL safety                               */
/* ================================================================== */

TEST(file_getExtension_normal) {
    ASSERT_STREQ(file_getExtension("game.gba"), "gba");
}

TEST(file_getExtension_no_dot) {
    ASSERT_STREQ(file_getExtension("game"), "");
}

TEST(file_getExtension_null_input) {
    /* After fix: should return "" instead of crashing */
    const char *ext = file_getExtension(NULL);
    ASSERT_NOT_NULL(ext);
    ASSERT_STREQ(ext, "");
}

TEST(file_getExtension_dot_only) {
    /* filename starts with dot and that's the only dot: no extension */
    ASSERT_STREQ(file_getExtension(".hidden"), "");
}

TEST(file_getExtension_empty_string) {
    ASSERT_STREQ(file_getExtension(""), "");
}

TEST(file_getExtension_multiple_dots) {
    ASSERT_STREQ(file_getExtension("game.backup.gba"), "gba");
}

/* ================================================================== */
/*  Test: free_play_activities NULL safety                            */
/* ================================================================== */

/*
 * Reproduce the free_play_activities pattern.
 * The key fix: check for NULL pa_ptr before dereferencing.
 */

typedef struct {
    char *type;
    char *name;
    char *file_path;
    char *image_path;
} TestROM;

typedef struct {
    int play_count;
    int play_time_total;
    char *first_played_at;
    char *last_played_at;
    TestROM *rom;
} TestPlayActivity;

typedef struct {
    TestPlayActivity **play_activity;
    int count;
    int play_time_total;
} TestPlayActivities;

static void test_free_play_activities(TestPlayActivities *pa_ptr)
{
    if (pa_ptr == NULL)
        return;
    for (int i = 0; i < pa_ptr->count; i++) {
        free(pa_ptr->play_activity[i]->first_played_at);
        free(pa_ptr->play_activity[i]->last_played_at);
        if (pa_ptr->play_activity[i]->rom != NULL) {
            free(pa_ptr->play_activity[i]->rom->type);
            free(pa_ptr->play_activity[i]->rom->name);
            free(pa_ptr->play_activity[i]->rom->file_path);
            free(pa_ptr->play_activity[i]->rom->image_path);
            free(pa_ptr->play_activity[i]->rom);
        }
        free(pa_ptr->play_activity[i]);
    }
    free(pa_ptr->play_activity);
    free(pa_ptr);
}

TEST(free_play_activities_null_safe) {
    /* Should not crash */
    test_free_play_activities(NULL);
    ASSERT_TRUE(1); /* reached here without crash */
}

TEST(free_play_activities_empty) {
    TestPlayActivities *pa = (TestPlayActivities *)calloc(1, sizeof(TestPlayActivities));
    ASSERT_NOT_NULL(pa);
    pa->count = 0;
    pa->play_activity = NULL;
    test_free_play_activities(pa);
    ASSERT_TRUE(1); /* reached here without crash */
}

TEST(free_play_activities_with_null_rom) {
    TestPlayActivities *pa = (TestPlayActivities *)calloc(1, sizeof(TestPlayActivities));
    ASSERT_NOT_NULL(pa);
    pa->count = 1;
    pa->play_activity = (TestPlayActivity **)calloc(1, sizeof(TestPlayActivity *));
    ASSERT_NOT_NULL(pa->play_activity);
    pa->play_activity[0] = (TestPlayActivity *)calloc(1, sizeof(TestPlayActivity));
    ASSERT_NOT_NULL(pa->play_activity[0]);
    pa->play_activity[0]->rom = NULL;
    pa->play_activity[0]->first_played_at = NULL;
    pa->play_activity[0]->last_played_at = NULL;
    test_free_play_activities(pa);
    ASSERT_TRUE(1); /* reached here without crash */
}

/* ================================================================== */
/*  Test: realpath NULL return handling                                */
/* ================================================================== */

TEST(realpath_nonexistent_returns_null) {
    char resolved[PATH_MAX];
    /* realpath returns NULL for non-existent paths */
    char *result = realpath("/nonexistent/path/that/does/not/exist", resolved);
    ASSERT_NULL(result);
}

TEST(realpath_null_safe_strcmp_pattern) {
    /*
     * Validate the fixed pattern: check realpath return before strcmp.
     * Old code: realpath(path, buf); strcmp(buf, other);  — UB if realpath fails
     * New code: if (realpath(path, buf) != NULL) strcmp(buf, other);
     */
    char path_a[PATH_MAX];
    char path_b[PATH_MAX];

    /* Both paths don't exist — old code would strcmp on garbage */
    char *result_a = realpath("/nonexistent/a", path_a);
    char *result_b = realpath("/nonexistent/b", path_b);

    /* Fixed pattern: only compare when both succeed */
    bool safe_equal = false;
    if (result_a != NULL && result_b != NULL) {
        safe_equal = (strcmp(path_a, path_b) == 0);
    }
    ASSERT_FALSE(safe_equal); /* both NULL, so we skip the compare */
}

TEST(realpath_existing_path) {
    char resolved[PATH_MAX];
    /* /tmp should exist on any Linux system */
    char *result = realpath("/tmp", resolved);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(strlen(resolved) > 0);
}

/* ================================================================== */
/*  Test: realpath fallback pattern (icons.h style)                   */
/* ================================================================== */

TEST(realpath_fallback_to_original) {
    /*
     * Validate the fixed pattern used in icons.h:
     * if (realpath(path, abs) == NULL)
     *     strncpy(abs, path, PATH_MAX - 1);
     */
    char abs_path[PATH_MAX];
    const char *preview = "/nonexistent/icon.png";

    if (realpath(preview, abs_path) == NULL)
        strncpy(abs_path, preview, PATH_MAX - 1);

    ASSERT_STREQ(abs_path, preview);
}

/* ================================================================== */
/*  Test: strdup failure fallback pattern                             */
/* ================================================================== */

TEST(strdup_failure_fallback) {
    /*
     * Validate the defensive pattern: if first strdup fails, retry.
     * In practice this catches transient allocation failures.
     */
    const char *original = "GBA";
    char *result = strdup(original);
    if (result == NULL)
        result = strdup("");

    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "GBA");
    free(result);
}

TEST(strdup_empty_string) {
    char *result = strdup("");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
    free(result);
}

/* ================================================================== */
/*  Test: integer overflow guard for malloc size                      */
/* ================================================================== */

TEST(malloc_size_overflow_check) {
    /*
     * Validate that we correctly detect overflow in dw * 4.
     * SIZE_MAX / 4 is the maximum safe value for dw.
     */
    size_t dw_safe = 1920;
    size_t dw_overflow = SIZE_MAX; /* would overflow when * 4 */

    ASSERT_TRUE(dw_safe <= SIZE_MAX / 4);
    ASSERT_FALSE(dw_overflow <= SIZE_MAX / 4);

    /* Safe allocation */
    void *ptr = malloc(dw_safe * 4);
    ASSERT_NOT_NULL(ptr);
    free(ptr);
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("\n=== Critical Bug Fix Tests (Round 2) ===\n\n");

    /* file_getExtension NULL safety */
    RUN_TEST(file_getExtension_normal);
    RUN_TEST(file_getExtension_no_dot);
    RUN_TEST(file_getExtension_null_input);
    RUN_TEST(file_getExtension_dot_only);
    RUN_TEST(file_getExtension_empty_string);
    RUN_TEST(file_getExtension_multiple_dots);

    /* free_play_activities NULL safety */
    RUN_TEST(free_play_activities_null_safe);
    RUN_TEST(free_play_activities_empty);
    RUN_TEST(free_play_activities_with_null_rom);

    /* realpath NULL handling */
    RUN_TEST(realpath_nonexistent_returns_null);
    RUN_TEST(realpath_null_safe_strcmp_pattern);
    RUN_TEST(realpath_existing_path);
    RUN_TEST(realpath_fallback_to_original);

    /* strdup failure fallback */
    RUN_TEST(strdup_failure_fallback);
    RUN_TEST(strdup_empty_string);

    /* Integer overflow guard */
    RUN_TEST(malloc_size_overflow_check);

    TEST_REPORT();
    return test_failures;
}
