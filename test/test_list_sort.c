/**
 * @file test_list_sort.c
 * @brief Unit tests for _list_comp_labels() and list_sortByLabel()
 *        from components/list.h
 *
 * Tests the case-insensitive label comparison function used by
 * qsort(), and the sort-by-label wrapper that sorts ListItem arrays.
 *
 * Build and run: make -f Makefile.unit test_list_sort
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

#define STR_MAX 256
#define MAX_NUM_VALUES 100

/* ---- Minimal ListItem for tests ---- */

typedef struct {
    int _id;
    char label[STR_MAX];
    int value;
    bool disabled;
    bool _created;
} TestListItem;

typedef struct {
    int item_count;
    int max_items;
    TestListItem *items;
    bool _created;
} TestList;

/* ---- Inline functions under test from list.h ---- */

static int _list_comp_labels(const void *a, const void *b)
{
    return strcasecmp(((TestListItem *)a)->label, ((TestListItem *)b)->label);
}

static void list_sortByLabel(TestList *list)
{
    qsort(list->items, list->item_count, sizeof(TestListItem), _list_comp_labels);
}

/* ---- Helpers ---- */

static TestListItem make_item(const char *label, int id)
{
    TestListItem item;
    memset(&item, 0, sizeof(item));
    strncpy(item.label, label, STR_MAX - 1);
    item.label[STR_MAX - 1] = '\0';
    item._id = id;
    return item;
}

static TestList make_list(TestListItem *items, int count)
{
    TestList list;
    list.items = items;
    list.item_count = count;
    list.max_items = count;
    list._created = true;
    return list;
}

/* ==== Tests: _list_comp_labels ==== */

TEST(comp_labels_equal) {
    TestListItem a = make_item("Brightness", 0);
    TestListItem b = make_item("Brightness", 1);
    ASSERT_EQ(_list_comp_labels(&a, &b), 0);
}

TEST(comp_labels_case_insensitive) {
    TestListItem a = make_item("brightness", 0);
    TestListItem b = make_item("BRIGHTNESS", 1);
    ASSERT_EQ(_list_comp_labels(&a, &b), 0);
}

TEST(comp_labels_a_before_b) {
    TestListItem a = make_item("Audio", 0);
    TestListItem b = make_item("Video", 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) < 0);
}

TEST(comp_labels_b_before_a) {
    TestListItem a = make_item("Volume", 0);
    TestListItem b = make_item("Audio", 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) > 0);
}

TEST(comp_labels_mixed_case_ordering) {
    TestListItem a = make_item("audio", 0);
    TestListItem b = make_item("BRIGHTNESS", 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) < 0);
}

TEST(comp_labels_empty_strings) {
    TestListItem a = make_item("", 0);
    TestListItem b = make_item("", 1);
    ASSERT_EQ(_list_comp_labels(&a, &b), 0);
}

TEST(comp_labels_empty_vs_nonempty) {
    TestListItem a = make_item("", 0);
    TestListItem b = make_item("Zeta", 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) < 0);
}

/* ==== Tests: list_sortByLabel ==== */

TEST(sort_three_items) {
    TestListItem items[3] = {
        make_item("Volume", 0),
        make_item("Audio", 1),
        make_item("Brightness", 2),
    };
    TestList list = make_list(items, 3);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "Audio");
    ASSERT_STREQ(items[1].label, "Brightness");
    ASSERT_STREQ(items[2].label, "Volume");
}

TEST(sort_preserves_data) {
    TestListItem items[3] = {
        make_item("Zeta", 10),
        make_item("Alpha", 20),
        make_item("Mu", 30),
    };
    TestList list = make_list(items, 3);

    list_sortByLabel(&list);

    /* After sort, "Alpha" (id=20) should be first */
    ASSERT_STREQ(items[0].label, "Alpha");
    ASSERT_EQ(items[0]._id, 20);
    ASSERT_STREQ(items[1].label, "Mu");
    ASSERT_EQ(items[1]._id, 30);
    ASSERT_STREQ(items[2].label, "Zeta");
    ASSERT_EQ(items[2]._id, 10);
}

TEST(sort_case_insensitive_order) {
    TestListItem items[4] = {
        make_item("zebra", 0),
        make_item("ALPHA", 1),
        make_item("beta", 2),
        make_item("Gamma", 3),
    };
    TestList list = make_list(items, 4);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "ALPHA");
    ASSERT_STREQ(items[1].label, "beta");
    ASSERT_STREQ(items[2].label, "Gamma");
    ASSERT_STREQ(items[3].label, "zebra");
}

TEST(sort_single_item) {
    TestListItem items[1] = {make_item("Only", 0)};
    TestList list = make_list(items, 1);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "Only");
}

TEST(sort_already_sorted) {
    TestListItem items[3] = {
        make_item("A", 0),
        make_item("B", 1),
        make_item("C", 2),
    };
    TestList list = make_list(items, 3);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "A");
    ASSERT_STREQ(items[1].label, "B");
    ASSERT_STREQ(items[2].label, "C");
}

TEST(sort_reverse_sorted) {
    TestListItem items[3] = {
        make_item("C", 0),
        make_item("B", 1),
        make_item("A", 2),
    };
    TestList list = make_list(items, 3);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "A");
    ASSERT_STREQ(items[1].label, "B");
    ASSERT_STREQ(items[2].label, "C");
}

TEST(sort_settings_menu_items) {
    /* Realistic settings menu labels */
    TestListItem items[6] = {
        make_item("WiFi", 0),
        make_item("Brightness", 1),
        make_item("Volume", 2),
        make_item("Audio Fix", 3),
        make_item("Theme", 4),
        make_item("Language", 5),
    };
    TestList list = make_list(items, 6);

    list_sortByLabel(&list);

    ASSERT_STREQ(items[0].label, "Audio Fix");
    ASSERT_STREQ(items[1].label, "Brightness");
    ASSERT_STREQ(items[2].label, "Language");
    ASSERT_STREQ(items[3].label, "Theme");
    ASSERT_STREQ(items[4].label, "Volume");
    ASSERT_STREQ(items[5].label, "WiFi");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== components/list.h Sort Unit Tests ===\n\n");

    /* Comparison function */
    RUN_TEST(comp_labels_equal);
    RUN_TEST(comp_labels_case_insensitive);
    RUN_TEST(comp_labels_a_before_b);
    RUN_TEST(comp_labels_b_before_a);
    RUN_TEST(comp_labels_mixed_case_ordering);
    RUN_TEST(comp_labels_empty_strings);
    RUN_TEST(comp_labels_empty_vs_nonempty);

    /* Sort integration */
    RUN_TEST(sort_three_items);
    RUN_TEST(sort_preserves_data);
    RUN_TEST(sort_case_insensitive_order);
    RUN_TEST(sort_single_item);
    RUN_TEST(sort_already_sorted);
    RUN_TEST(sort_reverse_sorted);
    RUN_TEST(sort_settings_menu_items);

    TEST_REPORT();
    return test_failures;
}
