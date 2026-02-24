/**
 * @file test_list.c
 * @brief Unit tests for src/common/components/list.h
 *
 * Tests the pure-logic list/menu functions: _list_modulo,
 * list_countVisible, list_create, list_addItem, list_currentItem,
 * list_keyUp, list_keyDown, list_keyLeft, list_keyRight,
 * list_activateItem, list_resetCurrentItem, list_getItemValueLabel,
 * list_scrollTo, list_sortByLabel, list_hasInfoNote, and
 * _list_did_wraparound.
 *
 * SDL-dependent functions (list_free with SDL_FreeSurface) are
 * stubbed out to avoid pulling in SDL dependencies.
 *
 * Build and run: make -f Makefile.unit test_list
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---- Stub out external dependencies ---- */

#define STR_MAX 256
#define MAX_NUM_VALUES 100

/* ---- Inline the types and pure-logic functions from list.h ---- */

typedef enum list_type { LIST_SMALL,
                         LIST_LARGE } ListType;
typedef enum item_type { ACTION,
                         TOGGLE,
                         MULTIVALUE } ListItemType;

typedef struct ListItem {
    int _id;
    ListItemType item_type;
    bool disabled;
    bool show_opaque;
    bool disable_arrows;
    bool disable_a_btn;
    bool alternative_arrow_action;
    char label[STR_MAX];
    char description[STR_MAX];
    char payload[STR_MAX];
    void *payload_ptr;
    int value;
    int value_min;
    int value_max;
    char value_labels[MAX_NUM_VALUES][STR_MAX];
    void (*value_formatter)(void *self, char *out_label);
    void (*action)(void *self);
    void (*arrow_action)(void *self);
    int action_id;
    int _reset_value;
    void *icon_ptr;
    void *preview_ptr;
    char preview_path[4096];
    char sticky_note[STR_MAX];
    char info_note[STR_MAX];
    void *_label_cache;
    uint32_t _label_hash;
    void *_value_cache;
    int _cached_value;
    void *_scaled_preview;
    int _scaled_preview_w;
} ListItem;

typedef struct List {
    char title[STR_MAX];
    int _id;
    int item_count;
    int max_items;
    int active_pos;
    int scroll_pos;
    int scroll_height;
    ListType list_type;
    ListItem *items;
    bool has_sticky;
    bool _created;
} List;

static int list_id_incr = 0;

int _list_modulo(int x, int n) { return (x % n + n) % n; }

int list_countVisible(List *list)
{
    int n = 0, i;
    for (i = 0; i < list->item_count; i++) {
        if (!list->items[i].disabled)
            n++;
    }
    return n;
}

void _list_ensureVisible(List *list, int direction, int items_left)
{
    if (list->items[list->active_pos].disabled) {
        list->active_pos = _list_modulo(list->active_pos + direction, list->item_count);
        if (items_left > 0) {
            _list_ensureVisible(list, direction, items_left - 1);
        }
    }
}

void list_ensureVisible(List *list, int direction)
{
    _list_ensureVisible(list, direction, list->item_count);
}

bool _list_did_wraparound(int before, int after, int direction)
{
    int offset = after - before;
    return offset != 0 && (direction > 0) != (offset > 0);
}

List list_create(int max_items, ListType list_type)
{
    return (List){.scroll_height = list_type == LIST_SMALL ? 6 : 4,
                  .list_type = list_type,
                  .max_items = max_items,
                  .items = (ListItem *)calloc(max_items, sizeof(ListItem)),
                  ._created = true,
                  ._id = list_id_incr++};
}

List list_createWithTitle(int max_items, ListType list_type, const char *title)
{
    List list = list_create(max_items, list_type);
    strncpy(list.title, title, STR_MAX - 1);
    return list;
}

List list_createWithSticky(int max_items, const char *title)
{
    List list = list_createWithTitle(max_items, LIST_SMALL, title);
    list.scroll_height = 5;
    list.has_sticky = true;
    return list;
}

ListItem *list_addItem(List *list, ListItem item)
{
    if (list->items == NULL || list->item_count >= list->max_items)
        return NULL;
    item._reset_value = item.value;
    item._id = list->item_count;
    memset(item.info_note, 0, STR_MAX);
    list->items[item._id] = item;
    list->item_count++;
    if (item.disabled && list->active_pos == item._id) {
        list->active_pos = item._id + 1;
    }
    return &(list->items[item._id]);
}

ListItem *list_addItemWithInfoNote(List *list, ListItem item, const char *info_note)
{
    ListItem *_item = list_addItem(list, item);
    if (_item == NULL)
        return NULL;
    strncpy(_item->info_note, info_note, sizeof(_item->info_note) - 1);
    _item->info_note[sizeof(_item->info_note) - 1] = '\0';
    return _item;
}

ListItem *list_currentItem(List *list)
{
    if (list->active_pos >= list->item_count)
        return NULL;
    return &list->items[list->active_pos];
}

void _list_scroll(List *list, int pos)
{
    pos = _list_modulo(pos, list->item_count);

    if (pos < list->scroll_pos)
        list->scroll_pos = pos;
    else if (pos >= list->scroll_pos + list->scroll_height)
        list->scroll_pos = pos - list->scroll_height + 1;

    if (list->item_count <= list->scroll_height)
        list->scroll_pos = 0;
    else if (list->scroll_pos + list->scroll_height > list->item_count)
        list->scroll_pos = list->item_count - list->scroll_height;
}

void list_scroll(List *list)
{
    _list_scroll(list, list->active_pos);
}

bool list_scrollTo(List *list, int active_pos)
{
    list->active_pos = _list_modulo(active_pos, list->item_count);
    list_ensureVisible(list, 1);
    list_scroll(list);
    return true;
}

bool list_keyUp(List *list, bool key_repeat)
{
    int old_pos = list->active_pos;

    if (list->active_pos == 0) {
        if (key_repeat)
            return false;
        list->active_pos = list->item_count - 1;
    }
    else
        list->active_pos -= 1;

    list_ensureVisible(list, -1);

    if (_list_did_wraparound(old_pos, list->active_pos, -1)) {
        if (list->scroll_pos > 0) {
            _list_scroll(list, list->scroll_pos - 1);
            list->active_pos = old_pos;
        }
        else {
            _list_scroll(list, list->item_count - 1);
        }
    }
    else {
        list_scroll(list);
    }

    return true;
}

bool list_keyDown(List *list, bool key_repeat)
{
    int old_pos = list->active_pos;

    if (list->active_pos == list->item_count - 1) {
        if (key_repeat)
            return false;
        list->active_pos = 0;
    }
    else
        list->active_pos += 1;

    list_ensureVisible(list, 1);

    if (_list_did_wraparound(old_pos, list->active_pos, 1)) {
        if (list->scroll_pos < list->item_count - list->scroll_height) {
            _list_scroll(list, list->scroll_pos + list->scroll_height);
            list->active_pos = old_pos;
        }
        else {
            _list_scroll(list, 0);
        }
    }
    else {
        list_scroll(list);
    }

    return true;
}

bool list_keyLeft(List *list, bool key_repeat)
{
    bool apply_action = false;
    ListItem *item = list_currentItem(list);

    if (item == NULL || item->disable_arrows)
        return false;

    int old_value = item->value;

    switch (item->item_type) {
    case TOGGLE:
        if (item->value != 0) {
            item->value = 0;
            apply_action = true;
        }
        break;
    case MULTIVALUE:
        if (item->value == item->value_min) {
            if (!key_repeat)
                item->value = item->value_max;
        }
        else
            item->value--;
        apply_action = true;
        break;
    default:
        break;
    }

    if (apply_action) {
        if (item->alternative_arrow_action)
            item->arrow_action((void *)item);
        else if (item->action != NULL)
            item->action((void *)item);
    }

    return old_value != item->value;
}

bool list_keyRight(List *list, bool key_repeat)
{
    bool apply_action = false;
    ListItem *item = list_currentItem(list);

    if (item == NULL || item->disable_arrows)
        return false;

    int old_value = item->value;

    switch (item->item_type) {
    case TOGGLE:
        if (item->value != 1) {
            item->value = 1;
            apply_action = true;
        }
        break;
    case MULTIVALUE:
        if (item->value == item->value_max) {
            if (!key_repeat)
                item->value = item->value_min;
        }
        else
            item->value++;
        apply_action = true;
        break;
    default:
        break;
    }

    if (apply_action) {
        if (item->alternative_arrow_action)
            item->arrow_action((void *)item);
        else if (item->action != NULL)
            item->action((void *)item);
    }

    return old_value != item->value;
}

bool list_activateItem(List *list)
{
    ListItem *item = list_currentItem(list);

    if (item == NULL || item->disable_a_btn)
        return false;

    int old_value = item->value;

    switch (item->item_type) {
    case TOGGLE:
        item->value = !item->value;
        break;
    case MULTIVALUE:
        if (item->value == item->value_max)
            item->value = item->value_min;
        else
            item->value++;
        break;
    default:
        break;
    }

    if (item->action != NULL)
        item->action((void *)item);

    return old_value != item->value;
}

bool list_hasInfoNote(List *list)
{
    ListItem *item = list_currentItem(list);

    if (item == NULL || strlen(item->info_note) == 0)
        return false;

    return true;
}

bool list_resetCurrentItem(List *list)
{
    ListItem *item = list_currentItem(list);

    if (item == NULL || item->value == item->_reset_value)
        return false;

    item->value = item->_reset_value;

    if (item->action != NULL)
        item->action((void *)item);

    return true;
}

void list_getItemValueLabel(ListItem *item, char *out_label)
{
    if (item->value_formatter != NULL)
        item->value_formatter(item, out_label);
    else if (item->value_labels[0][0] != '\0')
        snprintf(out_label, STR_MAX, "%s", item->value_labels[item->value]);
    else
        snprintf(out_label, STR_MAX, "%d", item->value);
}

int _list_comp_labels(const void *a, const void *b)
{
    return strcasecmp(((ListItem *)a)->label, ((ListItem *)b)->label);
}

void list_sortByLabel(List *list)
{
    qsort(list->items, list->item_count, sizeof(ListItem), _list_comp_labels);
}

/* Simplified list_free without SDL_FreeSurface calls */
void list_free(List *list)
{
    if (!list->_created)
        return;
    if (list->items != NULL)
        free(list->items);
    list->_created = false;
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

    TEST_REPORT();
    return test_failures;
}
