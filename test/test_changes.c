/**
 * @file test_changes.c
 * @brief Unit tests for src/packageManager/changes.h and listActions.h
 *
 * Tests the pure-logic package management functions: changesInstalls,
 * changesRemovals, changesTotal, totalInstalls, setItemsInstallValue,
 * layerToggleAll, and layerReset.
 *
 * Global state arrays from globals.h are inlined and reset between
 * tests to avoid cross-contamination.
 *
 * Build and run: make -f Makefile.unit test_changes
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ---- Inline types and globals from packageManager/globals.h ---- */

#define STR_MAX 256
#define LAYER_ITEM_COUNT 200

typedef struct package_s {
    char name[STR_MAX];
    bool installed;
    bool changed;
    bool complete;
    bool has_roms;
} Package;

static const int tab_count = 4;

static Package packages[4][LAYER_ITEM_COUNT];
static int package_count[] = {0, 0, 0, 0};
static int package_installed_count[] = {0, 0, 0, 0};
static int changes_installs[] = {0, 0, 0, 0};
static int changes_removals[] = {0, 0, 0, 0};

/* ---- Inline pure-logic functions from changes.h ---- */

static int changesInstalls(void)
{
    int total = 0;
    for (int i = 0; i < tab_count; i++)
        total += changes_installs[i];
    return total;
}

static int changesRemovals(void)
{
    int total = 0;
    for (int i = 0; i < tab_count; i++)
        total += changes_removals[i];
    return total;
}

static int changesTotal(void) { return changesInstalls() + changesRemovals(); }

static int totalInstalls(void)
{
    int total = 0;
    for (int i = 0; i < tab_count; i++)
        total += package_installed_count[i];
    return total;
}

static void setItemsInstallValue(int mode, int layer)
{
    for (int i = 0; i < package_count[layer]; i++) {
        Package *package = &packages[layer][i];
        bool is_active = mode == 2 ? package->has_roms : mode;
        bool new_value = is_active != package->installed;

        if (package->changed != new_value) {
            package->changed = new_value;

            if (package->installed) {
                changes_removals[layer] += new_value ? 1 : -1;
                if (!package->complete)
                    changes_installs[layer] += new_value ? -1 : 1;
            }
            else
                changes_installs[layer] += new_value ? 1 : -1;
        }
    }
}

/* ---- Inline pure-logic functions from listActions.h ---- */

static void layerToggleAll(int layer, bool only_when_all_off_and_has_roms)
{
    bool all_on = true;
    bool all_off = true;
    bool has_roms = false;

    for (int i = 0; i < package_count[layer]; i++) {
        Package *package = &packages[layer][i];
        if (package->installed == package->changed) {
            all_on = false;
            break;
        }
    }
    for (int i = 0; i < package_count[layer]; i++) {
        Package *package = &packages[layer][i];
        if (package->installed != package->changed) {
            all_off = false;
            break;
        }
    }
    for (int i = 0; i < package_count[layer]; i++) {
        Package *package = &packages[layer][i];
        if (package->has_roms) {
            has_roms = true;
            break;
        }
    }

    int mode = 1;
    if (all_off && has_roms)
        mode = 2;
    else if (all_on)
        mode = 0;

    if (only_when_all_off_and_has_roms && (!all_off || !has_roms))
        return;

    setItemsInstallValue(mode, layer);
}

static void layerReset(int layer)
{
    for (int i = 0; i < package_count[layer]; i++) {
        Package *package = &packages[layer][i];

        if (package->changed) {
            if (package->installed) {
                changes_removals[layer]--;
                if (!package->complete)
                    changes_installs[layer]++;
            }
            else
                changes_installs[layer]--;

            package->changed = false;
        }
    }
}

/* ---- Helper: reset all global state ---- */

static void _reset_globals(void)
{
    memset(packages, 0, sizeof(packages));
    for (int i = 0; i < 4; i++) {
        package_count[i] = 0;
        package_installed_count[i] = 0;
        changes_installs[i] = 0;
        changes_removals[i] = 0;
    }
}

static Package *_add_package(int layer, const char *name, bool installed,
                             bool complete, bool has_roms)
{
    int idx = package_count[layer];
    Package *pkg = &packages[layer][idx];
    memset(pkg, 0, sizeof(Package));
    strncpy(pkg->name, name, STR_MAX - 1);
    pkg->installed = installed;
    pkg->complete = complete;
    pkg->has_roms = has_roms;
    pkg->changed = false;
    package_count[layer]++;
    if (installed)
        package_installed_count[layer]++;
    return pkg;
}

/* ---- Tests ---- */

/* ---- changesInstalls / changesRemovals / changesTotal ---- */

TEST(changes_installs_empty) {
    _reset_globals();
    ASSERT_EQ(changesInstalls(), 0);
}

TEST(changes_removals_empty) {
    _reset_globals();
    ASSERT_EQ(changesRemovals(), 0);
}

TEST(changes_total_empty) {
    _reset_globals();
    ASSERT_EQ(changesTotal(), 0);
}

TEST(changes_installs_sums_layers) {
    _reset_globals();
    changes_installs[0] = 3;
    changes_installs[1] = 5;
    changes_installs[2] = 2;
    changes_installs[3] = 1;
    ASSERT_EQ(changesInstalls(), 11);
}

TEST(changes_removals_sums_layers) {
    _reset_globals();
    changes_removals[0] = 1;
    changes_removals[1] = 4;
    changes_removals[2] = 0;
    changes_removals[3] = 2;
    ASSERT_EQ(changesRemovals(), 7);
}

TEST(changes_total_sums_both) {
    _reset_globals();
    changes_installs[0] = 3;
    changes_removals[0] = 2;
    changes_installs[1] = 1;
    changes_removals[1] = 4;
    ASSERT_EQ(changesTotal(), 10);
}

/* ---- totalInstalls ---- */

TEST(total_installs_empty) {
    _reset_globals();
    ASSERT_EQ(totalInstalls(), 0);
}

TEST(total_installs_sums_layers) {
    _reset_globals();
    package_installed_count[0] = 5;
    package_installed_count[1] = 3;
    package_installed_count[2] = 8;
    package_installed_count[3] = 0;
    ASSERT_EQ(totalInstalls(), 16);
}

/* ---- setItemsInstallValue ---- */

TEST(set_items_mode_1_marks_uninstalled_as_changed) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);
    _add_package(0, "SNES", false, false, false);

    setItemsInstallValue(1, 0);

    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_TRUE(packages[0][1].changed);
    ASSERT_EQ(changes_installs[0], 2);
}

TEST(set_items_mode_0_marks_installed_as_changed) {
    _reset_globals();
    _add_package(0, "GBA", true, true, false);
    _add_package(0, "SNES", true, true, false);

    setItemsInstallValue(0, 0);

    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_TRUE(packages[0][1].changed);
    ASSERT_EQ(changes_removals[0], 2);
}

TEST(set_items_mode_0_no_change_for_uninstalled) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);

    setItemsInstallValue(0, 0);

    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

TEST(set_items_mode_1_no_change_for_installed) {
    _reset_globals();
    _add_package(0, "GBA", true, true, false);

    setItemsInstallValue(1, 0);

    ASSERT_FALSE(packages[0][0].changed);
}

TEST(set_items_mode_2_uses_has_roms) {
    _reset_globals();
    _add_package(0, "GBA", false, false, true);   /* has roms => activate */
    _add_package(0, "SNES", false, false, false);  /* no roms => skip */

    setItemsInstallValue(2, 0);

    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_FALSE(packages[0][1].changed);
    ASSERT_EQ(changes_installs[0], 1);
}

TEST(set_items_incomplete_installed_tracks_installs) {
    _reset_globals();
    /* Installed but not complete => changes_installs tracks incomplete */
    _add_package(0, "GBA", true, false, false);

    /* When we mark installed-but-incomplete as "changed" (removal),
       it decrements changes_installs */
    setItemsInstallValue(0, 0);

    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_removals[0], 1);
    ASSERT_EQ(changes_installs[0], -1);
}

TEST(set_items_idempotent) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);

    setItemsInstallValue(1, 0);
    int installs_after_first = changes_installs[0];

    setItemsInstallValue(1, 0);
    ASSERT_EQ(changes_installs[0], installs_after_first);
}

/* ---- layerReset ---- */

TEST(layer_reset_clears_changes) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);
    _add_package(0, "SNES", false, false, false);

    setItemsInstallValue(1, 0);
    ASSERT_EQ(changes_installs[0], 2);

    layerReset(0);
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_FALSE(packages[0][1].changed);
    ASSERT_EQ(changes_installs[0], 0);
}

TEST(layer_reset_clears_removals) {
    _reset_globals();
    _add_package(0, "GBA", true, true, false);
    _add_package(0, "SNES", true, true, false);

    setItemsInstallValue(0, 0);
    ASSERT_EQ(changes_removals[0], 2);

    layerReset(0);
    ASSERT_EQ(changes_removals[0], 0);
}

TEST(layer_reset_no_effect_when_no_changes) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);

    layerReset(0);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

TEST(layer_reset_per_layer_isolation) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);
    _add_package(1, "Clock", false, false, false);

    setItemsInstallValue(1, 0);
    setItemsInstallValue(1, 1);

    layerReset(0);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_installs[1], 1);
}

/* ---- layerToggleAll ---- */

TEST(toggle_all_enables_uninstalled) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);
    _add_package(0, "SNES", false, false, false);

    layerToggleAll(0, false);

    /* all_off=true but has_roms=false => mode=1 (all on) */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_TRUE(packages[0][1].changed);
    ASSERT_EQ(changes_installs[0], 2);
}

TEST(toggle_all_disables_all_on) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);
    _add_package(0, "SNES", false, false, false);

    /* First toggle: all on */
    setItemsInstallValue(1, 0);
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_TRUE(packages[0][1].changed);

    /* Second toggle: all off (mode=0) because all_on=true */
    layerToggleAll(0, false);
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_FALSE(packages[0][1].changed);
    ASSERT_EQ(changes_installs[0], 0);
}

TEST(toggle_all_auto_mode_with_roms) {
    _reset_globals();
    _add_package(0, "GBA", false, false, true);    /* has roms */
    _add_package(0, "SNES", false, false, false);   /* no roms */

    layerToggleAll(0, false);

    /* all_off=true and has_roms=true => mode=2 (auto) */
    ASSERT_TRUE(packages[0][0].changed);   /* GBA has roms => changed */
    ASSERT_FALSE(packages[0][1].changed);  /* SNES no roms => unchanged */
}

TEST(toggle_only_when_all_off_and_has_roms_skips) {
    _reset_globals();
    _add_package(0, "GBA", false, false, false);

    /* Only toggle when all_off=true AND has_roms=true.
       No package has_roms => should do nothing. */
    layerToggleAll(0, true);
    ASSERT_FALSE(packages[0][0].changed);
}

TEST(toggle_only_when_all_off_and_has_roms_applies) {
    _reset_globals();
    _add_package(0, "GBA", false, false, true);

    layerToggleAll(0, true);
    ASSERT_TRUE(packages[0][0].changed);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== changes.h + listActions.h Unit Tests ===\n\n");

    RUN_TEST(changes_installs_empty);
    RUN_TEST(changes_removals_empty);
    RUN_TEST(changes_total_empty);
    RUN_TEST(changes_installs_sums_layers);
    RUN_TEST(changes_removals_sums_layers);
    RUN_TEST(changes_total_sums_both);

    RUN_TEST(total_installs_empty);
    RUN_TEST(total_installs_sums_layers);

    RUN_TEST(set_items_mode_1_marks_uninstalled_as_changed);
    RUN_TEST(set_items_mode_0_marks_installed_as_changed);
    RUN_TEST(set_items_mode_0_no_change_for_uninstalled);
    RUN_TEST(set_items_mode_1_no_change_for_installed);
    RUN_TEST(set_items_mode_2_uses_has_roms);
    RUN_TEST(set_items_incomplete_installed_tracks_installs);
    RUN_TEST(set_items_idempotent);

    RUN_TEST(layer_reset_clears_changes);
    RUN_TEST(layer_reset_clears_removals);
    RUN_TEST(layer_reset_no_effect_when_no_changes);
    RUN_TEST(layer_reset_per_layer_isolation);

    RUN_TEST(toggle_all_enables_uninstalled);
    RUN_TEST(toggle_all_disables_all_on);
    RUN_TEST(toggle_all_auto_mode_with_roms);
    RUN_TEST(toggle_only_when_all_off_and_has_roms_skips);
    RUN_TEST(toggle_only_when_all_off_and_has_roms_applies);

    TEST_REPORT();
    return test_failures;
}
