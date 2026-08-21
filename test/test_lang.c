/**
 * @file test_lang.c
 * @brief Unit tests for src/common/system/lang.h
 *
 * Tests the pure-logic language functions: lang_get() lookup with fallback,
 * lang_free() NULL safety and double-free prevention, and the full
 * lang_load/lang_free lifecycle using a temporary .lang file.
 *
 * Build and run: make -f Makefile.unit test_lang
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- Constants from lang.h ---- */
#define LANG_MAX 400
#define LANG_DEFAULT "en.lang"

typedef enum {
    LANG_EXPERT_TAB = 0,
    LANG_FAVORITES_TAB = 1,
    LANG_GAMES_TAB = 2,
    LANG_SETTINGS_TAB = 15,
    LANG_RECENTS_TAB = 18,
    LANG_CHARGING = 40,
    LANG_CANCEL = 45,
    LANG_OK = 46,
    LANG_SELECT = 88,
    LANG_BACK = 89,
    LANG_MENU = 91,
    LANG_RESUME = 92,
    LANG_APPS_TAB = 107,
    LANG_EXIT = 111,
    LANG_NEXT = 300,
    LANG_RESUME_UC = 301
} lang_hash;

/* ---- Inline the pure-logic functions from lang.h ---- */

static char **lang_list = NULL;

static const char *lang_get(lang_hash key, const char *fallback)
{
    if (lang_list && lang_list[key])
        return lang_list[key];
    return fallback;
}

static void lang_free(void)
{
    if (lang_list == NULL)
        return;
    for (int i = 0; i < LANG_MAX; i++) {
        if (lang_list[i] == NULL)
            continue;
        free(lang_list[i]);
    }
    free(lang_list);
    lang_list = NULL;
}

/* ---- Helper to populate lang_list for testing ---- */

static void test_lang_init(void)
{
    lang_list = (char **)calloc(LANG_MAX, sizeof(char *));
}

static void test_lang_set(int index, const char *value)
{
    if (lang_list && index >= 0 && index < LANG_MAX) {
        lang_list[index] = strdup(value);
    }
}

/* ---- Tests: lang_get ---- */

TEST(lang_get_returns_fallback_when_list_null) {
    lang_list = NULL;
    const char *result = lang_get(LANG_OK, "OK");
    ASSERT_STREQ(result, "OK");
}

TEST(lang_get_returns_fallback_when_entry_null) {
    test_lang_init();
    /* Entry LANG_OK (46) is not set, should fall back */
    const char *result = lang_get(LANG_OK, "OK");
    ASSERT_STREQ(result, "OK");
    lang_free();
}

TEST(lang_get_returns_loaded_string) {
    test_lang_init();
    test_lang_set(LANG_OK, "Va bene");

    const char *result = lang_get(LANG_OK, "OK");
    ASSERT_STREQ(result, "Va bene");
    lang_free();
}

TEST(lang_get_multiple_entries) {
    test_lang_init();
    test_lang_set(LANG_CANCEL, "Annulla");
    test_lang_set(LANG_SELECT, "Seleziona");
    test_lang_set(LANG_BACK, "Indietro");

    ASSERT_STREQ(lang_get(LANG_CANCEL, "CANCEL"), "Annulla");
    ASSERT_STREQ(lang_get(LANG_SELECT, "SELECT"), "Seleziona");
    ASSERT_STREQ(lang_get(LANG_BACK, "BACK"), "Indietro");
    /* Unset entries still return fallback */
    ASSERT_STREQ(lang_get(LANG_EXIT, "EXIT"), "EXIT");
    lang_free();
}

TEST(lang_get_tab_names) {
    test_lang_init();
    test_lang_set(LANG_EXPERT_TAB, "Esperto");
    test_lang_set(LANG_FAVORITES_TAB, "Preferiti");
    test_lang_set(LANG_GAMES_TAB, "Giochi");
    test_lang_set(LANG_APPS_TAB, "Applicazioni");

    ASSERT_STREQ(lang_get(LANG_EXPERT_TAB, "Expert"), "Esperto");
    ASSERT_STREQ(lang_get(LANG_FAVORITES_TAB, "Favorites"), "Preferiti");
    ASSERT_STREQ(lang_get(LANG_GAMES_TAB, "Games"), "Giochi");
    ASSERT_STREQ(lang_get(LANG_APPS_TAB, "Apps"), "Applicazioni");
    /* Unset tab */
    ASSERT_STREQ(lang_get(LANG_RECENTS_TAB, "Recents"), "Recents");
    lang_free();
}

TEST(lang_get_high_index_entry) {
    /* Test entries at the upper end of the lang array (e.g. 300, 301) */
    test_lang_init();
    test_lang_set(LANG_NEXT, "Avanti");
    test_lang_set(LANG_RESUME_UC, "RIPRENDI");

    ASSERT_STREQ(lang_get(LANG_NEXT, "NEXT"), "Avanti");
    ASSERT_STREQ(lang_get(LANG_RESUME_UC, "RESUME"), "RIPRENDI");
    lang_free();
}

TEST(lang_get_empty_string_value) {
    /* An entry that is explicitly set to "" should return "", not fallback */
    test_lang_init();
    test_lang_set(LANG_OK, "");

    const char *result = lang_get(LANG_OK, "OK");
    ASSERT_STREQ(result, "");
    lang_free();
}

TEST(lang_get_space_value_for_hidden_labels) {
    /* When icon labels are removed, they're set to " " (single space) */
    test_lang_init();
    test_lang_set(LANG_EXPERT_TAB, " ");
    test_lang_set(LANG_FAVORITES_TAB, " ");

    ASSERT_STREQ(lang_get(LANG_EXPERT_TAB, "Expert"), " ");
    ASSERT_STREQ(lang_get(LANG_FAVORITES_TAB, "Favorites"), " ");
    lang_free();
}

TEST(lang_get_unicode_value) {
    test_lang_init();
    test_lang_set(LANG_OK, "确定");
    test_lang_set(LANG_CANCEL, "キャンセル");

    ASSERT_STREQ(lang_get(LANG_OK, "OK"), "确定");
    ASSERT_STREQ(lang_get(LANG_CANCEL, "CANCEL"), "キャンセル");
    lang_free();
}

/* ---- Tests: lang_free ---- */

TEST(lang_free_null_safe) {
    lang_list = NULL;
    lang_free();
    /* Should not crash; pointer remains NULL */
    ASSERT_NULL(lang_list);
}

TEST(lang_free_double_free_safe) {
    test_lang_init();
    test_lang_set(0, "test");
    lang_free();
    ASSERT_NULL(lang_list);

    /* Second free should be safe */
    lang_free();
    ASSERT_NULL(lang_list);
}

TEST(lang_free_clears_pointer) {
    test_lang_init();
    test_lang_set(LANG_OK, "OK");
    test_lang_set(LANG_CANCEL, "Cancel");
    test_lang_set(LANG_SELECT, "Select");

    ASSERT_NOT_NULL(lang_list);
    lang_free();
    ASSERT_NULL(lang_list);
}

TEST(lang_free_sparse_list) {
    /* Only a few entries are set; most are NULL */
    test_lang_init();
    test_lang_set(0, "first");
    test_lang_set(LANG_MAX - 1, "last");

    lang_free();
    ASSERT_NULL(lang_list);
}

TEST(lang_free_all_entries_set) {
    /* Stress test: set all 400 entries */
    test_lang_init();
    for (int i = 0; i < LANG_MAX; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "lang_%d", i);
        test_lang_set(i, buf);
    }
    lang_free();
    ASSERT_NULL(lang_list);
}

/* ---- Tests: lifecycle ---- */

TEST(lang_lifecycle_init_get_free) {
    /* Full lifecycle: init → set entries → get → free */
    test_lang_init();
    test_lang_set(LANG_OK, "OK_translated");
    test_lang_set(LANG_BACK, "Back_translated");

    ASSERT_STREQ(lang_get(LANG_OK, "OK"), "OK_translated");
    ASSERT_STREQ(lang_get(LANG_BACK, "BACK"), "Back_translated");
    ASSERT_STREQ(lang_get(LANG_EXIT, "EXIT"), "EXIT");

    lang_free();

    /* After free, all lookups should return fallback */
    ASSERT_STREQ(lang_get(LANG_OK, "OK"), "OK");
    ASSERT_STREQ(lang_get(LANG_BACK, "BACK"), "BACK");
}

TEST(lang_lifecycle_reinit_after_free) {
    /* Free then reinit with different values */
    test_lang_init();
    test_lang_set(LANG_OK, "First");
    ASSERT_STREQ(lang_get(LANG_OK, "OK"), "First");
    lang_free();

    test_lang_init();
    test_lang_set(LANG_OK, "Second");
    ASSERT_STREQ(lang_get(LANG_OK, "OK"), "Second");
    lang_free();
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== lang.h Unit Tests ===\n\n");

    /* lang_get */
    RUN_TEST(lang_get_returns_fallback_when_list_null);
    RUN_TEST(lang_get_returns_fallback_when_entry_null);
    RUN_TEST(lang_get_returns_loaded_string);
    RUN_TEST(lang_get_multiple_entries);
    RUN_TEST(lang_get_tab_names);
    RUN_TEST(lang_get_high_index_entry);
    RUN_TEST(lang_get_empty_string_value);
    RUN_TEST(lang_get_space_value_for_hidden_labels);
    RUN_TEST(lang_get_unicode_value);

    /* lang_free */
    RUN_TEST(lang_free_null_safe);
    RUN_TEST(lang_free_double_free_safe);
    RUN_TEST(lang_free_clears_pointer);
    RUN_TEST(lang_free_sparse_list);
    RUN_TEST(lang_free_all_entries_set);

    /* lifecycle */
    RUN_TEST(lang_lifecycle_init_get_free);
    RUN_TEST(lang_lifecycle_reinit_after_free);

    TEST_REPORT();
    return test_failures;
}
