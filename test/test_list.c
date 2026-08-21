/**
 * @file test_list.c
 * @brief Unit tests for src/common/components/list.h (production header)
 *
 * Includes the real list.h. SDL_FreeSurface is a no-op stub; system/lang.h
 * is replaced with a local stub so settings/display/cJSON are not pulled in.
 * Cache TTF fields are not populated by src/ and are not tested as if they were.
 *
 * Build and run: make -f Makefile.unit test_list
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int g_sdl_free_surface_calls = 0;

/* Skip production system/lang.h (settings/display/cJSON). list.h still
 * includes it; the include guard makes that a no-op. */
#define SYSTEM_LANG_H__
typedef int lang_hash;
static char **lang_list = NULL;
#ifndef LANG_MAX
#define LANG_MAX 400
#endif

#include "components/list.h"

void SDL_FreeSurface(SDL_Surface *surface)
{
    (void)surface;
    g_sdl_free_surface_calls++;
}

/* ---- Helper: create a simple item ---- */

static ListItem _make_item(const char *label, ListItemType type, int value)
{
    ListItem item;
    memset(&item, 0, sizeof(item));
    strncpy(item.label, label, STR_MAX - 1);
    item.item_type = type;
    item.value = value;
    return item;
}

/* ---- Action callback tracker ---- */
static int _action_call_count = 0;
static void _stub_action(void *self) { (void)self; _action_call_count++; }

/* ---- Tests ---- */

/* ---- _list_modulo ---- */

TEST(modulo_positive) {
    ASSERT_EQ(_list_modulo(7, 5), 2);
}

TEST(modulo_zero) {
    ASSERT_EQ(_list_modulo(0, 5), 0);
}

TEST(modulo_negative) {
    ASSERT_EQ(_list_modulo(-1, 5), 4);
}

TEST(modulo_negative_large) {
    ASSERT_EQ(_list_modulo(-7, 5), 3);
}

TEST(modulo_exact_multiple) {
    ASSERT_EQ(_list_modulo(10, 5), 0);
}

/* ---- _list_did_wraparound ---- */

TEST(wraparound_no_change) {
    ASSERT_FALSE(_list_did_wraparound(3, 3, 1));
}

TEST(wraparound_forward_normal) {
    /* Moving forward from 2 to 3: offset=+1, direction=+1 => no wrap */
    ASSERT_FALSE(_list_did_wraparound(2, 3, 1));
}

TEST(wraparound_forward_wrap) {
    /* Moving forward from 4 to 0: offset=-4, direction=+1 => wrap */
    ASSERT_TRUE(_list_did_wraparound(4, 0, 1));
}

TEST(wraparound_backward_normal) {
    /* Moving backward from 3 to 2: offset=-1, direction=-1 => no wrap */
    ASSERT_FALSE(_list_did_wraparound(3, 2, -1));
}

TEST(wraparound_backward_wrap) {
    /* Moving backward from 0 to 4: offset=+4, direction=-1 => wrap */
    ASSERT_TRUE(_list_did_wraparound(0, 4, -1));
}

/* ---- list_create ---- */

TEST(create_small_list) {
    List list = list_create(10, LIST_SMALL);
    ASSERT_EQ(list.scroll_height, 6);
    ASSERT_EQ(list.max_items, 10);
    ASSERT_EQ(list.item_count, 0);
    ASSERT_EQ(list.active_pos, 0);
    ASSERT_TRUE(list._created);
    ASSERT_NOT_NULL(list.items);
    list_free(&list);
}

TEST(create_large_list) {
    List list = list_create(10, LIST_LARGE);
    ASSERT_EQ(list.scroll_height, 4);
    list_free(&list);
}

TEST(create_with_title) {
    List list = list_createWithTitle(5, LIST_SMALL, "Settings");
    ASSERT_STREQ(list.title, "Settings");
    ASSERT_EQ(list.scroll_height, 6);
    list_free(&list);
}

TEST(create_with_sticky) {
    List list = list_createWithSticky(5, "Sticky Menu");
    ASSERT_STREQ(list.title, "Sticky Menu");
    ASSERT_EQ(list.scroll_height, 5);
    ASSERT_TRUE(list.has_sticky);
    list_free(&list);
}

/* ---- list_addItem ---- */

TEST(add_item_basic) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Volume", TOGGLE, 1);
    ListItem *added = list_addItem(&list, item);

    ASSERT_NOT_NULL(added);
    ASSERT_EQ(list.item_count, 1);
    ASSERT_STREQ(added->label, "Volume");
    ASSERT_EQ(added->_id, 0);
    ASSERT_EQ(added->_reset_value, 1);
    list_free(&list);
}

TEST(add_item_multiple) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", TOGGLE, 1));
    list_addItem(&list, _make_item("C", MULTIVALUE, 3));

    ASSERT_EQ(list.item_count, 3);
    ASSERT_STREQ(list.items[0].label, "A");
    ASSERT_STREQ(list.items[1].label, "B");
    ASSERT_STREQ(list.items[2].label, "C");
    list_free(&list);
}

TEST(add_item_exceeds_max) {
    List list = list_create(2, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    ListItem *overflow = list_addItem(&list, _make_item("C", ACTION, 0));

    ASSERT_NULL(overflow);
    ASSERT_EQ(list.item_count, 2);
    list_free(&list);
}

TEST(add_item_disabled_skips_active_pos) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Disabled", ACTION, 0);
    item.disabled = true;
    list_addItem(&list, item);

    /* active_pos should advance past the disabled item */
    ASSERT_EQ(list.active_pos, 1);
    list_free(&list);
}

TEST(add_item_with_info_note) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Item", ACTION, 0);
    ListItem *added = list_addItemWithInfoNote(&list, item, "Some info");

    ASSERT_NOT_NULL(added);
    ASSERT_STREQ(added->info_note, "Some info");
    list_free(&list);
}

/* ---- list_currentItem ---- */

TEST(current_item_valid) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("First", ACTION, 0));
    list_addItem(&list, _make_item("Second", ACTION, 0));

    ListItem *current = list_currentItem(&list);
    ASSERT_NOT_NULL(current);
    ASSERT_STREQ(current->label, "First");
    list_free(&list);
}

TEST(current_item_empty_list) {
    List list = list_create(5, LIST_SMALL);
    ListItem *current = list_currentItem(&list);
    ASSERT_NULL(current);
    list_free(&list);
}

/* ---- list_countVisible ---- */

TEST(count_visible_all_enabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    ASSERT_EQ(list_countVisible(&list), 3);
    list_free(&list);
}

TEST(count_visible_some_disabled) {
    List list = list_create(5, LIST_SMALL);
    ListItem a = _make_item("A", ACTION, 0);
    ListItem b = _make_item("B", ACTION, 0);
    b.disabled = true;
    ListItem c = _make_item("C", ACTION, 0);

    list_addItem(&list, a);
    list_addItem(&list, b);
    list_addItem(&list, c);

    ASSERT_EQ(list_countVisible(&list), 2);
    list_free(&list);
}

TEST(count_visible_all_disabled) {
    List list = list_create(5, LIST_SMALL);
    ListItem a = _make_item("A", ACTION, 0);
    a.disabled = true;
    ListItem b = _make_item("B", ACTION, 0);
    b.disabled = true;

    list_addItem(&list, a);
    list_addItem(&list, b);

    ASSERT_EQ(list_countVisible(&list), 0);
    list_free(&list);
}

/* ---- list_keyDown / list_keyUp ---- */

TEST(key_down_moves_forward) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list.active_pos = 0;

    list_keyDown(&list, false);
    ASSERT_EQ(list.active_pos, 1);

    list_keyDown(&list, false);
    ASSERT_EQ(list.active_pos, 2);
    list_free(&list);
}

TEST(key_down_wraps_around) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list.active_pos = 2;

    list_keyDown(&list, false);
    ASSERT_EQ(list.active_pos, 0);
    list_free(&list);
}

TEST(key_down_repeat_at_end_no_wrap) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list.active_pos = 1;

    bool result = list_keyDown(&list, true);
    ASSERT_FALSE(result);
    ASSERT_EQ(list.active_pos, 1);
    list_free(&list);
}

TEST(key_up_moves_backward) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list.active_pos = 2;

    list_keyUp(&list, false);
    ASSERT_EQ(list.active_pos, 1);

    list_keyUp(&list, false);
    ASSERT_EQ(list.active_pos, 0);
    list_free(&list);
}

TEST(key_up_wraps_around) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list.active_pos = 0;

    list_keyUp(&list, false);
    ASSERT_EQ(list.active_pos, 2);
    list_free(&list);
}

TEST(key_up_repeat_at_top_no_wrap) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list.active_pos = 0;

    bool result = list_keyUp(&list, true);
    ASSERT_FALSE(result);
    ASSERT_EQ(list.active_pos, 0);
    list_free(&list);
}

/* ---- list_keyLeft / list_keyRight with TOGGLE ---- */

TEST(key_right_toggle_on) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Toggle", TOGGLE, 0);
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 1);
    list_free(&list);
}

TEST(key_left_toggle_off) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Toggle", TOGGLE, 1);
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(key_right_toggle_already_on) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Toggle", TOGGLE, 1);
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 1);
    list_free(&list);
}

TEST(key_left_toggle_already_off) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Toggle", TOGGLE, 0);
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, false);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

/* ---- list_keyLeft / list_keyRight with MULTIVALUE ---- */

TEST(key_right_multivalue_increment) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 2);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 3);
    list_free(&list);
}

TEST(key_left_multivalue_decrement) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 3);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 2);
    list_free(&list);
}

TEST(key_right_multivalue_wraps_to_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 5);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(key_left_multivalue_wraps_to_max) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 0);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 5);
    list_free(&list);
}

TEST(key_right_multivalue_repeat_at_max_no_wrap) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 5);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, true);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 5);
    list_free(&list);
}

TEST(key_left_multivalue_repeat_at_min_no_wrap) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 0);
    item.value_min = 0;
    item.value_max = 5;
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, true);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

/* ---- list_keyLeft/Right with disabled arrows ---- */

TEST(key_right_disabled_arrows) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("No Arrows", TOGGLE, 0);
    item.disable_arrows = true;
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

/* ---- list_keyLeft/Right with ACTION type ---- */

TEST(key_right_action_type_no_change) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Action", ACTION, 0);
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_FALSE(changed);
    list_free(&list);
}

/* ---- list_activateItem ---- */

TEST(activate_toggle) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Toggle", TOGGLE, 0);
    list_addItem(&list, item);

    bool changed = list_activateItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 1);

    changed = list_activateItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(activate_multivalue_cycles) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Multi", MULTIVALUE, 0);
    item.value_min = 0;
    item.value_max = 2;
    list_addItem(&list, item);

    list_activateItem(&list);
    ASSERT_EQ(list.items[0].value, 1);

    list_activateItem(&list);
    ASSERT_EQ(list.items[0].value, 2);

    list_activateItem(&list);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(activate_disabled_a_btn) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Disabled", TOGGLE, 0);
    item.disable_a_btn = true;
    list_addItem(&list, item);

    bool changed = list_activateItem(&list);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(activate_calls_action_callback) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("WithAction", TOGGLE, 0);
    item.action = _stub_action;
    list_addItem(&list, item);

    _action_call_count = 0;
    list_activateItem(&list);
    ASSERT_EQ(_action_call_count, 1);
    list_free(&list);
}

/* ---- list_hasInfoNote ---- */

TEST(has_info_note_true) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Info", ACTION, 0);
    list_addItemWithInfoNote(&list, item, "Details here");

    ASSERT_TRUE(list_hasInfoNote(&list));
    list_free(&list);
}

TEST(has_info_note_false) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("No Info", ACTION, 0));

    ASSERT_FALSE(list_hasInfoNote(&list));
    list_free(&list);
}

/* ---- list_resetCurrentItem ---- */

TEST(reset_item_restores_value) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Reset", MULTIVALUE, 3);
    item.value_min = 0;
    item.value_max = 10;
    list_addItem(&list, item);

    /* _reset_value should be the initial value (3) */
    list.items[0].value = 7;

    bool changed = list_resetCurrentItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 3);
    list_free(&list);
}

TEST(reset_item_no_change) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Reset", MULTIVALUE, 3);
    list_addItem(&list, item);

    /* Value already equals reset value */
    bool changed = list_resetCurrentItem(&list);
    ASSERT_FALSE(changed);
    list_free(&list);
}

/* ---- list_getItemValueLabel ---- */

TEST(value_label_numeric) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = 42;

    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "42");
}

TEST(value_label_from_labels_array) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = 1;
    strncpy(item.value_labels[0], "Off", STR_MAX - 1);
    strncpy(item.value_labels[1], "On", STR_MAX - 1);

    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "On");
}

static void _test_formatter(void *self, char *out_label)
{
    ListItem *item = (ListItem *)self;
    snprintf(out_label, STR_MAX, "Level %d", item->value);
}

TEST(value_label_custom_formatter) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = 5;
    item.value_formatter = _test_formatter;

    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "Level 5");
}

/* ---- list_scrollTo ---- */

TEST(scroll_to_valid_position) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Item %d", i);
        list_addItem(&list, _make_item(label, ACTION, 0));
    }

    list_scrollTo(&list, 7);
    ASSERT_EQ(list.active_pos, 7);
    list_free(&list);
}

TEST(scroll_to_wraps_negative) {
    List list = list_create(5, LIST_SMALL);
    for (int i = 0; i < 5; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Item %d", i);
        list_addItem(&list, _make_item(label, ACTION, 0));
    }

    list_scrollTo(&list, -1);
    ASSERT_EQ(list.active_pos, 4);
    list_free(&list);
}

/* ---- list_sortByLabel ---- */

TEST(sort_by_label) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Cherry", ACTION, 0));
    list_addItem(&list, _make_item("Apple", ACTION, 0));
    list_addItem(&list, _make_item("Banana", ACTION, 0));

    list_sortByLabel(&list);

    ASSERT_STREQ(list.items[0].label, "Apple");
    ASSERT_STREQ(list.items[1].label, "Banana");
    ASSERT_STREQ(list.items[2].label, "Cherry");
    list_free(&list);
}

TEST(sort_by_label_case_insensitive) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("banana", ACTION, 0));
    list_addItem(&list, _make_item("Apple", ACTION, 0));
    list_addItem(&list, _make_item("cherry", ACTION, 0));

    list_sortByLabel(&list);

    ASSERT_STREQ(list.items[0].label, "Apple");
    ASSERT_STREQ(list.items[1].label, "banana");
    ASSERT_STREQ(list.items[2].label, "cherry");
    list_free(&list);
}

/* ---- list_scroll ---- */

TEST(scroll_position_within_height) {
    List list = list_create(10, LIST_SMALL);
    /* scroll_height = 6 for LIST_SMALL */
    for (int i = 0; i < 10; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Item %d", i);
        list_addItem(&list, _make_item(label, ACTION, 0));
    }

    list.active_pos = 0;
    list_scroll(&list);
    ASSERT_EQ(list.scroll_pos, 0);

    list.active_pos = 8;
    list_scroll(&list);
    /* scroll_pos should move to show item 8 within view */
    ASSERT_GE(list.scroll_pos, 3);
    list_free(&list);
}

TEST(scroll_not_needed_few_items) {
    List list = list_create(5, LIST_SMALL);
    /* Only 3 items, scroll_height = 6 => no scrolling needed */
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 2;
    list_scroll(&list);
    ASSERT_EQ(list.scroll_pos, 0);
    list_free(&list);
}

/* ---- ensureVisible skips disabled items ---- */

TEST(ensure_visible_skips_disabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));

    ListItem disabled = _make_item("B", ACTION, 0);
    disabled.disabled = true;
    list_addItem(&list, disabled);

    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 1; /* positioned on disabled item */
    list_ensureVisible(&list, 1);
    ASSERT_EQ(list.active_pos, 2);
    list_free(&list);
}

/* ---- list_updateStickyNote / list_getStickyNote ---- */

TEST(sticky_note_set_and_get) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    list_updateStickyNote(&item, "Hello sticky");
    ASSERT_STREQ(list_getStickyNote(&item), "Hello sticky");
}

TEST(sticky_note_overwrite) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    list_updateStickyNote(&item, "First");
    list_updateStickyNote(&item, "Second");
    ASSERT_STREQ(list_getStickyNote(&item), "Second");
}

TEST(sticky_note_empty_string) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    list_updateStickyNote(&item, "");
    ASSERT_STREQ(list_getStickyNote(&item), "");
}

TEST(sticky_note_get_unset) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    /* sticky_note is zeroed, should return empty string */
    ASSERT_STREQ(list_getStickyNote(&item), "");
}

TEST(sticky_note_long_message) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    /* Build a string of length STR_MAX + 10 to test truncation */
    char long_msg[STR_MAX + 10];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';
    list_updateStickyNote(&item, long_msg);
    /* Should be truncated to STR_MAX - 1 characters */
    ASSERT_EQ((int)strlen(list_getStickyNote(&item)), STR_MAX - 1);
}

TEST(sticky_note_with_list_item) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Item", ACTION, 0);
    ListItem *added = list_addItem(&list, item);
    list_updateStickyNote(added, "Note on item");
    ASSERT_STREQ(list_getStickyNote(added), "Note on item");
    list_free(&list);
}

/* ---- list_getVisibleItemAt ---- */

TEST(visible_item_at_all_enabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    ListItem *item = list_getVisibleItemAt(&list, 1);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "B");
    list_free(&list);
}

TEST(visible_item_at_skips_disabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));

    ListItem disabled = _make_item("B", ACTION, 0);
    disabled.disabled = true;
    list_addItem(&list, disabled);

    list_addItem(&list, _make_item("C", ACTION, 0));

    ListItem *item = list_getVisibleItemAt(&list, 1);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "C");
    list_free(&list);
}

TEST(visible_item_at_first_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));

    ListItem *item = list_getVisibleItemAt(&list, 0);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "A");
    list_free(&list);
}

TEST(visible_item_at_all_disabled_returns_null) {
    List list = list_create(5, LIST_SMALL);

    ListItem d1 = _make_item("A", ACTION, 0);
    d1.disabled = true;
    list_addItem(&list, d1);

    ListItem d2 = _make_item("B", ACTION, 0);
    d2.disabled = true;
    list_addItem(&list, d2);

    ListItem *item = list_getVisibleItemAt(&list, 0);
    ASSERT_NULL(item);
    list_free(&list);
}

TEST(visible_item_at_last_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    ListItem *item = list_getVisibleItemAt(&list, 2);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "C");
    list_free(&list);
}

TEST(visible_item_at_skips_multiple_disabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));

    ListItem d1 = _make_item("B", ACTION, 0);
    d1.disabled = true;
    list_addItem(&list, d1);

    ListItem d2 = _make_item("C", ACTION, 0);
    d2.disabled = true;
    list_addItem(&list, d2);

    list_addItem(&list, _make_item("D", ACTION, 0));

    ListItem *item = list_getVisibleItemAt(&list, 1);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "D");
    list_free(&list);
}

/* ---- list_hideAllExcept ---- */

TEST(hide_all_except_disables_others) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    list_hideAllExcept(&list, &list.items[1], true);

    ASSERT_TRUE(list.items[0].disabled);
    ASSERT_FALSE(list.items[1].disabled);
    ASSERT_TRUE(list.items[2].disabled);
    list_free(&list);
}

TEST(hide_all_except_re_enables_others) {
    List list = list_create(5, LIST_SMALL);
    ListItem d1 = _make_item("A", ACTION, 0);
    d1.disabled = true;
    list_addItem(&list, d1);

    list_addItem(&list, _make_item("B", ACTION, 0));

    ListItem d2 = _make_item("C", ACTION, 0);
    d2.disabled = true;
    list_addItem(&list, d2);

    /* Re-enable all except B */
    list_hideAllExcept(&list, &list.items[1], false);

    ASSERT_FALSE(list.items[0].disabled);
    ASSERT_FALSE(list.items[1].disabled);
    ASSERT_FALSE(list.items[2].disabled);
    list_free(&list);
}

TEST(hide_all_except_single_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Only", ACTION, 0));

    list_hideAllExcept(&list, &list.items[0], true);
    /* The only item should remain enabled */
    ASSERT_FALSE(list.items[0].disabled);
    list_free(&list);
}

TEST(hide_all_except_first_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    list_hideAllExcept(&list, &list.items[0], true);

    ASSERT_FALSE(list.items[0].disabled);
    ASSERT_TRUE(list.items[1].disabled);
    ASSERT_TRUE(list.items[2].disabled);
    list_free(&list);
}

TEST(hide_all_except_last_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));

    list_hideAllExcept(&list, &list.items[2], true);

    ASSERT_TRUE(list.items[0].disabled);
    ASSERT_TRUE(list.items[1].disabled);
    ASSERT_FALSE(list.items[2].disabled);
    list_free(&list);
}

TEST(hide_all_except_visible_count) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list_addItem(&list, _make_item("D", ACTION, 0));

    list_hideAllExcept(&list, &list.items[2], true);
    ASSERT_EQ(list_countVisible(&list), 1);

    list_hideAllExcept(&list, &list.items[2], false);
    ASSERT_EQ(list_countVisible(&list), 4);
    list_free(&list);
}

/* ---- list_free guard path ---- */

TEST(free_not_created) {
    List list;
    memset(&list, 0, sizeof(list));
    list._created = false;
    /* Should not crash or double-free */
    list_free(&list);
    ASSERT_FALSE(list._created);
}

TEST(free_sets_created_false) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ASSERT_TRUE(list._created);
    list_free(&list);
    ASSERT_FALSE(list._created);
}

TEST(free_double_free_safe) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_free(&list);
    /* Second free should be safe (guard by _created flag) */
    list_free(&list);
    ASSERT_FALSE(list._created);
}

/* Production list_free must SDL_FreeSurface every non-NULL cache/preview/icon
 * pointer. TTF caching is not populated in src/; this only checks the free path. */
TEST(free_releases_nonnull_cache_pointers) {
    List list = list_create(2, LIST_SMALL);
    ListItem *item = list_addItem(&list, _make_item("A", ACTION, 0));
    ASSERT_NOT_NULL(item);
    SDL_Surface dummy;
    item->icon_ptr = &dummy;
    item->preview_ptr = &dummy;
    item->_label_cache = &dummy;
    item->_value_cache = &dummy;
    item->_scaled_preview = &dummy;
    g_sdl_free_surface_calls = 0;
    list_free(&list);
    ASSERT_EQ(g_sdl_free_surface_calls, 5);
    ASSERT_FALSE(list._created);
}

TEST(add_item_at_max_returns_null) {
    List list = list_create(1, LIST_SMALL);
    ASSERT_NOT_NULL(list_addItem(&list, _make_item("A", ACTION, 0)));
    ASSERT_NULL(list_addItem(&list, _make_item("B", ACTION, 0)));
    list_free(&list);
}

/* ---- list_addItemWithInfoNote edge cases ---- */

TEST(add_item_with_info_note_null_check) {
    List list = list_create(1, LIST_SMALL);
    list_addItem(&list, _make_item("Fill", ACTION, 0));
    /* List is full, should return NULL */
    ListItem *result = list_addItemWithInfoNote(&list, _make_item("Extra", ACTION, 0), "Note");
    ASSERT_NULL(result);
    list_free(&list);
}

TEST(add_item_with_info_note_preserves_label) {
    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithInfoNote(&list, _make_item("MyItem", ACTION, 0), "A note");
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "MyItem");
    ASSERT_STREQ(item->info_note, "A note");
    list_free(&list);
}

/* ---- list_createWithTitle edge cases ---- */

TEST(create_with_title_long_truncates) {
    char long_title[STR_MAX + 20];
    memset(long_title, 'T', sizeof(long_title) - 1);
    long_title[sizeof(long_title) - 1] = '\0';
    List list = list_createWithTitle(5, LIST_SMALL, long_title);
    /* Title should be truncated to STR_MAX - 1 characters */
    ASSERT_EQ((int)strlen(list.title), STR_MAX - 1);
    list_free(&list);
}

TEST(create_with_title_empty) {
    List list = list_createWithTitle(5, LIST_SMALL, "");
    ASSERT_STREQ(list.title, "");
    ASSERT_TRUE(list._created);
    list_free(&list);
}

TEST(create_with_title_preserves_list_type) {
    List list = list_createWithTitle(10, LIST_LARGE, "Large List");
    ASSERT_STREQ(list.title, "Large List");
    ASSERT_EQ(list.list_type, LIST_LARGE);
    ASSERT_EQ(list.scroll_height, 4);
    ASSERT_EQ(list.max_items, 10);
    list_free(&list);
}

/* ---- list_createWithSticky edge cases ---- */

TEST(create_with_sticky_has_sticky_flag) {
    List list = list_createWithSticky(8, "Sticky Title");
    ASSERT_TRUE(list.has_sticky);
    ASSERT_EQ(list.scroll_height, 5);
    ASSERT_STREQ(list.title, "Sticky Title");
    ASSERT_EQ(list.list_type, LIST_SMALL);
    list_free(&list);
}

TEST(create_with_sticky_empty_title) {
    List list = list_createWithSticky(3, "");
    ASSERT_TRUE(list.has_sticky);
    ASSERT_STREQ(list.title, "");
    list_free(&list);
}

TEST(create_with_sticky_long_title) {
    char long_title[STR_MAX + 20];
    memset(long_title, 'S', sizeof(long_title) - 1);
    long_title[sizeof(long_title) - 1] = '\0';
    List list = list_createWithSticky(5, long_title);
    ASSERT_TRUE(list.has_sticky);
    ASSERT_EQ((int)strlen(list.title), STR_MAX - 1);
    list_free(&list);
}

/* ---- _list_scroll direct edge cases ---- */

TEST(scroll_down_past_height) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    /* scroll_height is 6 for LIST_SMALL, scroll to pos 8 */
    _list_scroll(&list, 8);
    /* scroll_pos should be 8 - 6 + 1 = 3 */
    ASSERT_EQ(list.scroll_pos, 3);
    list_free(&list);
}

TEST(scroll_up_before_pos) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    /* First scroll down to set scroll_pos */
    _list_scroll(&list, 8);
    ASSERT_EQ(list.scroll_pos, 3);
    /* Now scroll up to pos 1 */
    _list_scroll(&list, 1);
    ASSERT_EQ(list.scroll_pos, 1);
    list_free(&list);
}

TEST(scroll_clamps_to_max) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    /* Scroll to last item */
    _list_scroll(&list, 9);
    /* scroll_pos should be clamped to item_count - scroll_height = 4 */
    ASSERT_EQ(list.scroll_pos, 4);
    list_free(&list);
}

TEST(scroll_few_items_stays_zero) {
    List list = list_create(5, LIST_SMALL);
    for (int i = 0; i < 3; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    /* With 3 items and scroll_height 6, no scrolling needed */
    _list_scroll(&list, 2);
    ASSERT_EQ(list.scroll_pos, 0);
    list_free(&list);
}

TEST(scroll_wraps_negative_pos) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    /* Negative pos wraps via _list_modulo: -1 mod 10 = 9 */
    _list_scroll(&list, -1);
    ASSERT_EQ(list.scroll_pos, 4);
    list_free(&list);
}

/* ---- list_scroll via active_pos ---- */

TEST(list_scroll_uses_active_pos) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 10; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    list.active_pos = 8;
    list_scroll(&list);
    ASSERT_EQ(list.scroll_pos, 3);
    list_free(&list);
}

/* ---- list_keyLeft / list_keyRight alternative_arrow_action ---- */

static int _arrow_action_count = 0;
static void _stub_arrow_action(void *self) { (void)self; _arrow_action_count++; }

TEST(key_right_alternative_arrow_action) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Alt", MULTIVALUE, 0);
    item.value_max = 3;
    item.alternative_arrow_action = true;
    item.arrow_action = _stub_arrow_action;
    item.action = _stub_action;
    list_addItem(&list, item);

    _arrow_action_count = 0;
    _action_call_count = 0;
    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 1);
    /* arrow_action called, not action */
    ASSERT_EQ(_arrow_action_count, 1);
    ASSERT_EQ(_action_call_count, 0);
    list_free(&list);
}

TEST(key_left_alternative_arrow_action) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Alt", MULTIVALUE, 3);
    item.value_max = 3;
    item.alternative_arrow_action = true;
    item.arrow_action = _stub_arrow_action;
    item.action = _stub_action;
    list_addItem(&list, item);

    _arrow_action_count = 0;
    _action_call_count = 0;
    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 2);
    ASSERT_EQ(_arrow_action_count, 1);
    ASSERT_EQ(_action_call_count, 0);
    list_free(&list);
}

/* ---- list_activateItem ACTION type ---- */

TEST(activate_action_calls_callback) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Act", ACTION, 0);
    item.action = _stub_action;
    list_addItem(&list, item);

    _action_call_count = 0;
    bool changed = list_activateItem(&list);
    /* ACTION type doesn't change value but calls action */
    ASSERT_FALSE(changed);
    ASSERT_EQ(_action_call_count, 1);
    list_free(&list);
}

TEST(activate_action_no_callback) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("NoAct", ACTION, 0);
    list_addItem(&list, item);

    bool changed = list_activateItem(&list);
    ASSERT_FALSE(changed);
    list_free(&list);
}

/* ---- list_resetCurrentItem with action callback ---- */

TEST(reset_item_calls_action) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Reset", MULTIVALUE, 5);
    item.value_max = 10;
    item.action = _stub_action;
    list_addItem(&list, item);

    /* Modify value away from reset value */
    list.items[0].value = 8;
    _action_call_count = 0;
    bool changed = list_resetCurrentItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 5);
    ASSERT_EQ(_action_call_count, 1);
    list_free(&list);
}

TEST(reset_item_empty_list) {
    List list = list_create(5, LIST_SMALL);
    bool changed = list_resetCurrentItem(&list);
    ASSERT_FALSE(changed);
    list_free(&list);
}

/* ---- list_addItem NULL items guard ---- */

TEST(add_item_null_items) {
    List list;
    memset(&list, 0, sizeof(list));
    list.items = NULL;
    list._created = false;
    ListItem *result = list_addItem(&list, _make_item("X", ACTION, 0));
    ASSERT_NULL(result);
}

/* ---- list_currentItem boundary cases ---- */

TEST(current_item_at_last_index) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    list_addItem(&list, _make_item("C", ACTION, 0));
    list.active_pos = 2;
    ListItem *item = list_currentItem(&list);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "C");
    list_free(&list);
}

TEST(current_item_beyond_count) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list.active_pos = 5;
    ListItem *item = list_currentItem(&list);
    ASSERT_NULL(item);
    list_free(&list);
}

/* ---- list_addItemWithLang ---- */

TEST(add_item_with_lang_null_lang_list) {
    /* When lang_list is NULL, label should be preserved from original item */
    lang_list = NULL;
    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithLang(&list, _make_item("Original", ACTION, 0), 0);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "Original");
    list_free(&list);
}

TEST(add_item_with_lang_valid_key) {
    /* Set up a mock lang_list with a valid entry */
    char *mock_lang[LANG_MAX];
    memset(mock_lang, 0, sizeof(mock_lang));
    char translated[] = "Tradotto";
    mock_lang[5] = translated;
    lang_list = mock_lang;

    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithLang(&list, _make_item("Original", ACTION, 0), 5);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "Tradotto");

    lang_list = NULL;
    list_free(&list);
}

TEST(add_item_with_lang_null_key_entry) {
    /* lang_list exists but the specific key entry is NULL */
    char *mock_lang[LANG_MAX];
    memset(mock_lang, 0, sizeof(mock_lang));
    lang_list = mock_lang;

    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithLang(&list, _make_item("Fallback", ACTION, 0), 10);
    ASSERT_NOT_NULL(item);
    /* Label should remain as the original since key entry is NULL */
    ASSERT_STREQ(item->label, "Fallback");

    lang_list = NULL;
    list_free(&list);
}

TEST(add_item_with_lang_full_list) {
    /* When list is full, should return NULL */
    lang_list = NULL;
    List list = list_create(1, LIST_SMALL);
    list_addItem(&list, _make_item("Fill", ACTION, 0));
    ListItem *item = list_addItemWithLang(&list, _make_item("Extra", ACTION, 0), 0);
    ASSERT_NULL(item);
    list_free(&list);
}

TEST(add_item_with_lang_long_translation) {
    /* Translation longer than STR_MAX should be truncated */
    char long_str[STR_MAX + 20];
    memset(long_str, 'L', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    char *mock_lang[LANG_MAX];
    memset(mock_lang, 0, sizeof(mock_lang));
    mock_lang[3] = long_str;
    lang_list = mock_lang;

    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithLang(&list, _make_item("Short", ACTION, 0), 3);
    ASSERT_NOT_NULL(item);
    ASSERT_EQ((int)strlen(item->label), STR_MAX - 1);

    lang_list = NULL;
    list_free(&list);
}

/* ---- list_sortByLabel edge cases ---- */

TEST(sort_by_label_empty_list) {
    List list = list_create(5, LIST_SMALL);
    /* Sorting empty list should not crash */
    list_sortByLabel(&list);
    ASSERT_EQ(list.item_count, 0);
    list_free(&list);
}

TEST(sort_by_label_single_item) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Only", ACTION, 0));
    list_sortByLabel(&list);
    ASSERT_STREQ(list.items[0].label, "Only");
    list_free(&list);
}

TEST(sort_by_label_duplicate_labels) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Same", ACTION, 1));
    list_addItem(&list, _make_item("Same", ACTION, 2));
    list_addItem(&list, _make_item("Same", ACTION, 3));
    list_sortByLabel(&list);
    /* All labels should still be "Same" */
    ASSERT_STREQ(list.items[0].label, "Same");
    ASSERT_STREQ(list.items[1].label, "Same");
    ASSERT_STREQ(list.items[2].label, "Same");
    list_free(&list);
}

TEST(sort_by_label_already_sorted) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Alpha", ACTION, 0));
    list_addItem(&list, _make_item("Beta", ACTION, 0));
    list_addItem(&list, _make_item("Gamma", ACTION, 0));
    list_sortByLabel(&list);
    ASSERT_STREQ(list.items[0].label, "Alpha");
    ASSERT_STREQ(list.items[1].label, "Beta");
    ASSERT_STREQ(list.items[2].label, "Gamma");
    list_free(&list);
}

TEST(sort_by_label_reverse_order) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("Zulu", ACTION, 0));
    list_addItem(&list, _make_item("Mike", ACTION, 0));
    list_addItem(&list, _make_item("Alpha", ACTION, 0));
    list_sortByLabel(&list);
    ASSERT_STREQ(list.items[0].label, "Alpha");
    ASSERT_STREQ(list.items[1].label, "Mike");
    ASSERT_STREQ(list.items[2].label, "Zulu");
    list_free(&list);
}

/* ---- list_ensureVisible / _list_ensureVisible ---- */

TEST(ensure_visible_forward_direction) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ListItem dis = _make_item("B", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);
    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 1; /* disabled item */
    list_ensureVisible(&list, 1);
    ASSERT_EQ(list.active_pos, 2); /* should skip to next enabled */
    list_free(&list);
}

TEST(ensure_visible_backward_direction) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ListItem dis = _make_item("B", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);
    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 1; /* disabled item */
    list_ensureVisible(&list, -1);
    ASSERT_EQ(list.active_pos, 0); /* should skip backward to first enabled */
    list_free(&list);
}

TEST(ensure_visible_wraps_forward) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    ListItem dis = _make_item("C", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);

    list.active_pos = 2; /* disabled last item */
    list_ensureVisible(&list, 1);
    ASSERT_EQ(list.active_pos, 0); /* wraps to first enabled */
    list_free(&list);
}

TEST(ensure_visible_enabled_item_unchanged) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));

    list.active_pos = 0;
    list_ensureVisible(&list, 1);
    ASSERT_EQ(list.active_pos, 0); /* already enabled, unchanged */
    list_free(&list);
}

TEST(ensure_visible_multiple_disabled_wraps) {
    List list = list_create(5, LIST_SMALL);
    ListItem dis1 = _make_item("A", ACTION, 0);
    dis1.disabled = true;
    list_addItem(&list, dis1);
    ListItem dis2 = _make_item("B", ACTION, 0);
    dis2.disabled = true;
    list_addItem(&list, dis2);
    list_addItem(&list, _make_item("C", ACTION, 0));
    ListItem dis3 = _make_item("D", ACTION, 0);
    dis3.disabled = true;
    list_addItem(&list, dis3);

    list.active_pos = 3; /* disabled last item */
    list_ensureVisible(&list, 1);
    ASSERT_EQ(list.active_pos, 2); /* wraps past disabled 0,1 to enabled 2 */
    list_free(&list);
}

/* ---- list_scrollTo edge cases ---- */

TEST(scroll_to_large_positive) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 5; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    list_scrollTo(&list, 15); /* 15 mod 5 = 0 */
    ASSERT_EQ(list.active_pos, 0);
    list_free(&list);
}

TEST(scroll_to_first_item) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 8; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    list_scrollTo(&list, 0);
    ASSERT_EQ(list.active_pos, 0);
    ASSERT_EQ(list.scroll_pos, 0);
    list_free(&list);
}

TEST(scroll_to_last_item) {
    List list = list_create(10, LIST_SMALL);
    for (int i = 0; i < 8; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));
    list_scrollTo(&list, 7);
    ASSERT_EQ(list.active_pos, 7);
    /* scroll_pos should show last items */
    ASSERT_EQ(list.scroll_pos, 2); /* 8 - 6 = 2 */
    list_free(&list);
}

/* ---- list_keyUp / list_keyDown with disabled items ---- */

TEST(key_down_skips_disabled) {
    List list = list_create(10, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ListItem dis = _make_item("B", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);
    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 0;
    list_keyDown(&list, false);
    /* Should skip disabled item B and land on C */
    ASSERT_EQ(list.active_pos, 2);
    list_free(&list);
}

TEST(key_up_skips_disabled) {
    List list = list_create(10, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ListItem dis = _make_item("B", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);
    list_addItem(&list, _make_item("C", ACTION, 0));

    list.active_pos = 2;
    list_keyUp(&list, false);
    /* Should skip disabled item B and land on A */
    ASSERT_EQ(list.active_pos, 0);
    list_free(&list);
}

TEST(key_down_with_scroll_update) {
    List list = list_create(10, LIST_SMALL);
    /* Add 8 items (more than scroll_height 6) */
    for (int i = 0; i < 8; i++)
        list_addItem(&list, _make_item("X", ACTION, 0));

    list.active_pos = 0;
    list.scroll_pos = 0;
    /* Navigate down to position 6 (beyond scroll height) */
    for (int i = 0; i < 6; i++)
        list_keyDown(&list, false);

    ASSERT_EQ(list.active_pos, 6);
    /* scroll_pos should have adjusted to show the item */
    ASSERT_GE(list.scroll_pos, 1);
    list_free(&list);
}

/* ---- list_keyLeft / list_keyRight edge cases ---- */

TEST(key_left_on_empty_list) {
    List list = list_create(5, LIST_SMALL);
    bool changed = list_keyLeft(&list, false);
    ASSERT_FALSE(changed);
    list_free(&list);
}

TEST(key_right_on_empty_list) {
    List list = list_create(5, LIST_SMALL);
    bool changed = list_keyRight(&list, false);
    ASSERT_FALSE(changed);
    list_free(&list);
}

TEST(key_left_toggle_repeat_at_zero) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Tog", TOGGLE, 0);
    list_addItem(&list, item);
    /* Already at 0, key_left should not change */
    bool changed = list_keyLeft(&list, true);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 0);
    list_free(&list);
}

TEST(key_right_toggle_repeat_at_one) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("Tog", TOGGLE, 1);
    list_addItem(&list, item);
    /* Already at 1, key_right should not change */
    bool changed = list_keyRight(&list, true);
    ASSERT_FALSE(changed);
    ASSERT_EQ(list.items[0].value, 1);
    list_free(&list);
}

TEST(key_left_multivalue_with_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 5);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 4);
    list_free(&list);
}

TEST(key_right_multivalue_with_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 5);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 6);
    list_free(&list);
}

TEST(key_left_multivalue_wraps_with_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 3);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    /* At min, should wrap to max */
    bool changed = list_keyLeft(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 7);
    list_free(&list);
}

TEST(key_right_multivalue_wraps_with_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 7);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    /* At max, should wrap to min */
    bool changed = list_keyRight(&list, false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 3);
    list_free(&list);
}

/* ---- list_activateItem edge cases ---- */

TEST(activate_multivalue_wraps_with_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 7);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    bool changed = list_activateItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 3); /* wraps to min */
    list_free(&list);
}

TEST(activate_multivalue_increments_nonzero_min) {
    List list = list_create(5, LIST_SMALL);
    ListItem item = _make_item("MV", MULTIVALUE, 4);
    item.value_min = 3;
    item.value_max = 7;
    list_addItem(&list, item);

    bool changed = list_activateItem(&list);
    ASSERT_TRUE(changed);
    ASSERT_EQ(list.items[0].value, 5);
    list_free(&list);
}

TEST(activate_on_empty_list) {
    List list = list_create(5, LIST_SMALL);
    bool changed = list_activateItem(&list);
    ASSERT_FALSE(changed);
    list_free(&list);
}

/* ---- list_hasInfoNote edge cases ---- */

TEST(has_info_note_empty_list) {
    List list = list_create(5, LIST_SMALL);
    ASSERT_FALSE(list_hasInfoNote(&list));
    list_free(&list);
}

TEST(has_info_note_with_whitespace) {
    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItemWithInfoNote(&list, _make_item("X", ACTION, 0), " ");
    (void)item;
    ASSERT_TRUE(list_hasInfoNote(&list));
    list_free(&list);
}

/* ---- list_getItemValueLabel edge cases ---- */

TEST(value_label_multivalue_nonzero) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = 42;
    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "42");
}

TEST(value_label_negative_value) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = -5;
    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "-5");
}

TEST(value_label_zero_value) {
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.value = 0;
    char buf[STR_MAX];
    list_getItemValueLabel(&item, buf);
    ASSERT_STREQ(buf, "0");
}

/* ---- list_addItem edge cases ---- */

TEST(add_item_preserves_reset_value) {
    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_addItem(&list, _make_item("X", MULTIVALUE, 42));
    ASSERT_EQ(item->_reset_value, 42);
    list_free(&list);
}

TEST(add_item_clears_info_note) {
    List list = list_create(5, LIST_SMALL);
    ListItem proto = _make_item("X", ACTION, 0);
    strncpy(proto.info_note, "Should be cleared", STR_MAX - 1);
    ListItem *item = list_addItem(&list, proto);
    ASSERT_STREQ(item->info_note, "");
    list_free(&list);
}

TEST(add_item_assigns_sequential_ids) {
    List list = list_create(5, LIST_SMALL);
    ListItem *a = list_addItem(&list, _make_item("A", ACTION, 0));
    ListItem *b = list_addItem(&list, _make_item("B", ACTION, 0));
    ListItem *c = list_addItem(&list, _make_item("C", ACTION, 0));
    ASSERT_EQ(a->_id, 0);
    ASSERT_EQ(b->_id, 1);
    ASSERT_EQ(c->_id, 2);
    list_free(&list);
}

TEST(add_item_multiple_disabled_skips) {
    List list = list_create(5, LIST_SMALL);
    ListItem dis1 = _make_item("A", ACTION, 0);
    dis1.disabled = true;
    list_addItem(&list, dis1);
    ListItem dis2 = _make_item("B", ACTION, 0);
    dis2.disabled = true;
    list_addItem(&list, dis2);
    list_addItem(&list, _make_item("C", ACTION, 0));
    /* active_pos should skip past both disabled items */
    ASSERT_EQ(list.active_pos, 2);
    list_free(&list);
}

/* ---- _list_comp_labels ---- */

TEST(comp_labels_equal) {
    ListItem a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    strncpy(a.label, "Same", STR_MAX - 1);
    strncpy(b.label, "Same", STR_MAX - 1);
    ASSERT_EQ(_list_comp_labels(&a, &b), 0);
}

TEST(comp_labels_less_than) {
    ListItem a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    strncpy(a.label, "Alpha", STR_MAX - 1);
    strncpy(b.label, "Beta", STR_MAX - 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) < 0);
}

TEST(comp_labels_greater_than) {
    ListItem a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    strncpy(a.label, "Zulu", STR_MAX - 1);
    strncpy(b.label, "Alpha", STR_MAX - 1);
    ASSERT_TRUE(_list_comp_labels(&a, &b) > 0);
}

TEST(comp_labels_case_insensitive) {
    ListItem a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    strncpy(a.label, "alpha", STR_MAX - 1);
    strncpy(b.label, "ALPHA", STR_MAX - 1);
    ASSERT_EQ(_list_comp_labels(&a, &b), 0);
}

/* ---- list_getVisibleItemAt edge cases ---- */

TEST(visible_item_at_zero_index) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    list_addItem(&list, _make_item("B", ACTION, 0));
    ListItem *item = list_getVisibleItemAt(&list, 0);
    ASSERT_NOT_NULL(item);
    ASSERT_STREQ(item->label, "A");
    list_free(&list);
}

TEST(visible_item_at_empty_list) {
    List list = list_create(5, LIST_SMALL);
    ListItem *item = list_getVisibleItemAt(&list, 0);
    ASSERT_NULL(item);
    list_free(&list);
}

/* ---- list_countVisible edge cases ---- */

TEST(count_visible_empty_list) {
    List list = list_create(5, LIST_SMALL);
    ASSERT_EQ(list_countVisible(&list), 0);
    list_free(&list);
}

TEST(count_visible_single_enabled) {
    List list = list_create(5, LIST_SMALL);
    list_addItem(&list, _make_item("A", ACTION, 0));
    ASSERT_EQ(list_countVisible(&list), 1);
    list_free(&list);
}

TEST(count_visible_single_disabled) {
    List list = list_create(5, LIST_SMALL);
    ListItem dis = _make_item("A", ACTION, 0);
    dis.disabled = true;
    list_addItem(&list, dis);
    ASSERT_EQ(list_countVisible(&list), 0);
    list_free(&list);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== list.h Unit Tests ===\n\n");

    RUN_TEST(modulo_positive);
    RUN_TEST(modulo_zero);
    RUN_TEST(modulo_negative);
    RUN_TEST(modulo_negative_large);
    RUN_TEST(modulo_exact_multiple);

    RUN_TEST(wraparound_no_change);
    RUN_TEST(wraparound_forward_normal);
    RUN_TEST(wraparound_forward_wrap);
    RUN_TEST(wraparound_backward_normal);
    RUN_TEST(wraparound_backward_wrap);

    RUN_TEST(create_small_list);
    RUN_TEST(create_large_list);
    RUN_TEST(create_with_title);
    RUN_TEST(create_with_sticky);

    RUN_TEST(add_item_basic);
    RUN_TEST(add_item_multiple);
    RUN_TEST(add_item_exceeds_max);
    RUN_TEST(add_item_disabled_skips_active_pos);
    RUN_TEST(add_item_with_info_note);

    RUN_TEST(current_item_valid);
    RUN_TEST(current_item_empty_list);

    RUN_TEST(count_visible_all_enabled);
    RUN_TEST(count_visible_some_disabled);
    RUN_TEST(count_visible_all_disabled);

    RUN_TEST(key_down_moves_forward);
    RUN_TEST(key_down_wraps_around);
    RUN_TEST(key_down_repeat_at_end_no_wrap);
    RUN_TEST(key_up_moves_backward);
    RUN_TEST(key_up_wraps_around);
    RUN_TEST(key_up_repeat_at_top_no_wrap);

    RUN_TEST(key_right_toggle_on);
    RUN_TEST(key_left_toggle_off);
    RUN_TEST(key_right_toggle_already_on);
    RUN_TEST(key_left_toggle_already_off);

    RUN_TEST(key_right_multivalue_increment);
    RUN_TEST(key_left_multivalue_decrement);
    RUN_TEST(key_right_multivalue_wraps_to_min);
    RUN_TEST(key_left_multivalue_wraps_to_max);
    RUN_TEST(key_right_multivalue_repeat_at_max_no_wrap);
    RUN_TEST(key_left_multivalue_repeat_at_min_no_wrap);

    RUN_TEST(key_right_disabled_arrows);
    RUN_TEST(key_right_action_type_no_change);

    RUN_TEST(activate_toggle);
    RUN_TEST(activate_multivalue_cycles);
    RUN_TEST(activate_disabled_a_btn);
    RUN_TEST(activate_calls_action_callback);

    RUN_TEST(has_info_note_true);
    RUN_TEST(has_info_note_false);

    RUN_TEST(reset_item_restores_value);
    RUN_TEST(reset_item_no_change);

    RUN_TEST(value_label_numeric);
    RUN_TEST(value_label_from_labels_array);
    RUN_TEST(value_label_custom_formatter);

    RUN_TEST(scroll_to_valid_position);
    RUN_TEST(scroll_to_wraps_negative);

    RUN_TEST(sort_by_label);
    RUN_TEST(sort_by_label_case_insensitive);

    RUN_TEST(scroll_position_within_height);
    RUN_TEST(scroll_not_needed_few_items);

    RUN_TEST(ensure_visible_skips_disabled);

    RUN_TEST(sticky_note_set_and_get);
    RUN_TEST(sticky_note_overwrite);
    RUN_TEST(sticky_note_empty_string);
    RUN_TEST(sticky_note_get_unset);
    RUN_TEST(sticky_note_long_message);
    RUN_TEST(sticky_note_with_list_item);

    RUN_TEST(visible_item_at_all_enabled);
    RUN_TEST(visible_item_at_skips_disabled);
    RUN_TEST(visible_item_at_first_item);
    RUN_TEST(visible_item_at_all_disabled_returns_null);
    RUN_TEST(visible_item_at_last_item);
    RUN_TEST(visible_item_at_skips_multiple_disabled);

    RUN_TEST(hide_all_except_disables_others);
    RUN_TEST(hide_all_except_re_enables_others);
    RUN_TEST(hide_all_except_single_item);
    RUN_TEST(hide_all_except_first_item);
    RUN_TEST(hide_all_except_last_item);
    RUN_TEST(hide_all_except_visible_count);

    RUN_TEST(free_not_created);
    RUN_TEST(free_sets_created_false);
    RUN_TEST(free_double_free_safe);
    RUN_TEST(free_releases_nonnull_cache_pointers);
    RUN_TEST(add_item_at_max_returns_null);

    RUN_TEST(add_item_with_info_note_null_check);
    RUN_TEST(add_item_with_info_note_preserves_label);

    RUN_TEST(create_with_title_long_truncates);
    RUN_TEST(create_with_title_empty);
    RUN_TEST(create_with_title_preserves_list_type);

    RUN_TEST(create_with_sticky_has_sticky_flag);
    RUN_TEST(create_with_sticky_empty_title);
    RUN_TEST(create_with_sticky_long_title);

    RUN_TEST(scroll_down_past_height);
    RUN_TEST(scroll_up_before_pos);
    RUN_TEST(scroll_clamps_to_max);
    RUN_TEST(scroll_few_items_stays_zero);
    RUN_TEST(scroll_wraps_negative_pos);

    RUN_TEST(list_scroll_uses_active_pos);

    RUN_TEST(key_right_alternative_arrow_action);
    RUN_TEST(key_left_alternative_arrow_action);

    RUN_TEST(activate_action_calls_callback);
    RUN_TEST(activate_action_no_callback);

    RUN_TEST(reset_item_calls_action);
    RUN_TEST(reset_item_empty_list);

    RUN_TEST(add_item_null_items);

    RUN_TEST(current_item_at_last_index);
    RUN_TEST(current_item_beyond_count);

    RUN_TEST(add_item_with_lang_null_lang_list);
    RUN_TEST(add_item_with_lang_valid_key);
    RUN_TEST(add_item_with_lang_null_key_entry);
    RUN_TEST(add_item_with_lang_full_list);
    RUN_TEST(add_item_with_lang_long_translation);

    RUN_TEST(sort_by_label_empty_list);
    RUN_TEST(sort_by_label_single_item);
    RUN_TEST(sort_by_label_duplicate_labels);
    RUN_TEST(sort_by_label_already_sorted);
    RUN_TEST(sort_by_label_reverse_order);

    RUN_TEST(ensure_visible_forward_direction);
    RUN_TEST(ensure_visible_backward_direction);
    RUN_TEST(ensure_visible_wraps_forward);
    RUN_TEST(ensure_visible_enabled_item_unchanged);
    RUN_TEST(ensure_visible_multiple_disabled_wraps);

    RUN_TEST(scroll_to_large_positive);
    RUN_TEST(scroll_to_first_item);
    RUN_TEST(scroll_to_last_item);

    RUN_TEST(key_down_skips_disabled);
    RUN_TEST(key_up_skips_disabled);
    RUN_TEST(key_down_with_scroll_update);

    RUN_TEST(key_left_on_empty_list);
    RUN_TEST(key_right_on_empty_list);
    RUN_TEST(key_left_toggle_repeat_at_zero);
    RUN_TEST(key_right_toggle_repeat_at_one);
    RUN_TEST(key_left_multivalue_with_nonzero_min);
    RUN_TEST(key_right_multivalue_with_nonzero_min);
    RUN_TEST(key_left_multivalue_wraps_with_nonzero_min);
    RUN_TEST(key_right_multivalue_wraps_with_nonzero_min);

    RUN_TEST(activate_multivalue_wraps_with_nonzero_min);
    RUN_TEST(activate_multivalue_increments_nonzero_min);
    RUN_TEST(activate_on_empty_list);

    RUN_TEST(has_info_note_empty_list);
    RUN_TEST(has_info_note_with_whitespace);

    RUN_TEST(value_label_multivalue_nonzero);
    RUN_TEST(value_label_negative_value);
    RUN_TEST(value_label_zero_value);

    RUN_TEST(add_item_preserves_reset_value);
    RUN_TEST(add_item_clears_info_note);
    RUN_TEST(add_item_assigns_sequential_ids);
    RUN_TEST(add_item_multiple_disabled_skips);

    RUN_TEST(comp_labels_equal);
    RUN_TEST(comp_labels_less_than);
    RUN_TEST(comp_labels_greater_than);
    RUN_TEST(comp_labels_case_insensitive);

    RUN_TEST(visible_item_at_zero_index);
    RUN_TEST(visible_item_at_empty_list);

    RUN_TEST(count_visible_empty_list);
    RUN_TEST(count_visible_single_enabled);
    RUN_TEST(count_visible_single_disabled);

    TEST_REPORT();
    return test_failures;
}
