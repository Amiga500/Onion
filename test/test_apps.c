/**
 * @file test_apps.c
 * @brief Unit tests for src/common/utils/apps.h
 *
 * Tests the pure-logic app utility functions: _comp_installed_apps
 * comparator used by qsort for sorting installed applications by
 * label (case-insensitive).
 *
 * File I/O functions (getInstalledApps, set_cmd_app, etc.) are
 * not tested here as they depend on the filesystem.
 *
 * Build and run: make -f Makefile.unit test_apps
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define STR_MAX 256

/* ---- Inline the pure-logic types and functions from apps.h ---- */

typedef struct {
    char dirName[STR_MAX];
    char label[STR_MAX];
    bool is_duplicate;
    int dup_id;
} InstalledApp;

static int _comp_installed_apps(const void *a, const void *b)
{
    return strcasecmp(((InstalledApp *)a)->label, ((InstalledApp *)b)->label);
}

/* ---- Helper ---- */

static InstalledApp _make_app(const char *dir_name, const char *label)
{
    InstalledApp app;
    memset(&app, 0, sizeof(app));
    strncpy(app.dirName, dir_name, STR_MAX - 1);
    strncpy(app.label, label, STR_MAX - 1);
    return app;
}

/* ---- Tests ---- */

/* ---- _comp_installed_apps comparator ---- */

TEST(comp_apps_equal) {
    InstalledApp a = _make_app("Clock", "Clock");
    InstalledApp b = _make_app("Clock2", "Clock");

    ASSERT_EQ(_comp_installed_apps(&a, &b), 0);
}

TEST(comp_apps_less_than) {
    InstalledApp a = _make_app("Apple", "Apple");
    InstalledApp b = _make_app("Banana", "Banana");

    ASSERT_TRUE(_comp_installed_apps(&a, &b) < 0);
}

TEST(comp_apps_greater_than) {
    InstalledApp a = _make_app("Cherry", "Cherry");
    InstalledApp b = _make_app("Apple", "Apple");

    ASSERT_TRUE(_comp_installed_apps(&a, &b) > 0);
}

TEST(comp_apps_case_insensitive) {
    InstalledApp a = _make_app("clock", "clock");
    InstalledApp b = _make_app("Clock", "Clock");

    ASSERT_EQ(_comp_installed_apps(&a, &b), 0);
}

TEST(comp_apps_case_insensitive_order) {
    InstalledApp a = _make_app("apple", "apple");
    InstalledApp b = _make_app("Banana", "Banana");

    ASSERT_TRUE(_comp_installed_apps(&a, &b) < 0);
}

TEST(comp_apps_empty_labels) {
    InstalledApp a = _make_app("A", "");
    InstalledApp b = _make_app("B", "");

    ASSERT_EQ(_comp_installed_apps(&a, &b), 0);
}

TEST(comp_apps_empty_vs_nonempty) {
    InstalledApp a = _make_app("A", "");
    InstalledApp b = _make_app("B", "App");

    ASSERT_TRUE(_comp_installed_apps(&a, &b) < 0);
}

/* ---- qsort integration ---- */

TEST(qsort_sorts_apps_correctly) {
    InstalledApp apps[4];
    apps[0] = _make_app("D", "Terminal");
    apps[1] = _make_app("A", "Calculator");
    apps[2] = _make_app("C", "Music Player");
    apps[3] = _make_app("B", "Clock");

    qsort(apps, 4, sizeof(InstalledApp), _comp_installed_apps);

    ASSERT_STREQ(apps[0].label, "Calculator");
    ASSERT_STREQ(apps[1].label, "Clock");
    ASSERT_STREQ(apps[2].label, "Music Player");
    ASSERT_STREQ(apps[3].label, "Terminal");

    /* dirName should follow the label */
    ASSERT_STREQ(apps[0].dirName, "A");
    ASSERT_STREQ(apps[1].dirName, "B");
}

TEST(qsort_mixed_case) {
    InstalledApp apps[3];
    apps[0] = _make_app("C", "zebra");
    apps[1] = _make_app("A", "Apple");
    apps[2] = _make_app("B", "banana");

    qsort(apps, 3, sizeof(InstalledApp), _comp_installed_apps);

    ASSERT_STREQ(apps[0].label, "Apple");
    ASSERT_STREQ(apps[1].label, "banana");
    ASSERT_STREQ(apps[2].label, "zebra");
}

TEST(qsort_already_sorted) {
    InstalledApp apps[3];
    apps[0] = _make_app("A", "Alpha");
    apps[1] = _make_app("B", "Beta");
    apps[2] = _make_app("C", "Gamma");

    qsort(apps, 3, sizeof(InstalledApp), _comp_installed_apps);

    ASSERT_STREQ(apps[0].label, "Alpha");
    ASSERT_STREQ(apps[1].label, "Beta");
    ASSERT_STREQ(apps[2].label, "Gamma");
}

TEST(qsort_single_element) {
    InstalledApp apps[1];
    apps[0] = _make_app("Solo", "OnlyOne");

    qsort(apps, 1, sizeof(InstalledApp), _comp_installed_apps);

    ASSERT_STREQ(apps[0].label, "OnlyOne");
}

/* ---- duplicate detection logic (pure struct operations) ---- */

TEST(duplicate_fields_default_false) {
    InstalledApp app = _make_app("Clock", "Clock");
    ASSERT_FALSE(app.is_duplicate);
    ASSERT_EQ(app.dup_id, 0);
}

TEST(duplicate_fields_set_correctly) {
    InstalledApp app = _make_app("Clock", "Clock");
    app.is_duplicate = true;
    app.dup_id = 2;

    ASSERT_TRUE(app.is_duplicate);
    ASSERT_EQ(app.dup_id, 2);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== apps.h Unit Tests ===\n\n");

    RUN_TEST(comp_apps_equal);
    RUN_TEST(comp_apps_less_than);
    RUN_TEST(comp_apps_greater_than);
    RUN_TEST(comp_apps_case_insensitive);
    RUN_TEST(comp_apps_case_insensitive_order);
    RUN_TEST(comp_apps_empty_labels);
    RUN_TEST(comp_apps_empty_vs_nonempty);

    RUN_TEST(qsort_sorts_apps_correctly);
    RUN_TEST(qsort_mixed_case);
    RUN_TEST(qsort_already_sorted);
    RUN_TEST(qsort_single_element);

    RUN_TEST(duplicate_fields_default_false);
    RUN_TEST(duplicate_fields_set_correctly);

    TEST_REPORT();
    return test_failures;
}
