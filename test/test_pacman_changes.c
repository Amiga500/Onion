/**
 * @file test_pacman_changes.c
 * @brief Unit tests for packageManager/changes.h counting logic
 *
 * Tests the pure counting and state-tracking functions used by the
 * package manager to track installs, removals, and total changes
 * across layers (tabs). Uses local copies of the globals to avoid
 * SDL dependencies.
 *
 * Build and run: make -f Makefile.unit test_pacman_changes
 */

#include "onion_test.h"
#include <stdbool.h>
#include <string.h>

#define STR_MAX 256
#define LAYER_ITEM_COUNT 200

/* ---- Minimal Package struct from globals.h ---- */

typedef struct package_s {
    char name[STR_MAX];
    bool installed;
    bool changed;
    bool complete;
    bool has_roms;
} Package;

/* ---- Simulated globals ---- */

static const int tab_count = 4;
static Package packages[4][LAYER_ITEM_COUNT];
static int package_count[] = {0, 0, 0, 0};
static int package_installed_count[] = {0, 0, 0, 0};
static int changes_installs[] = {0, 0, 0, 0};
static int changes_removals[] = {0, 0, 0, 0};

/* ---- Inline functions under test from changes.h ---- */

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

/* ---- Helpers ---- */

static void reset_globals(void)
{
    memset(packages, 0, sizeof(packages));
    for (int i = 0; i < 4; i++) {
        package_count[i] = 0;
        package_installed_count[i] = 0;
        changes_installs[i] = 0;
        changes_removals[i] = 0;
    }
}

static Package make_package(const char *name, bool installed, bool complete, bool has_roms)
{
    Package p;
    memset(&p, 0, sizeof(p));
    strncpy(p.name, name, STR_MAX - 1);
    p.installed = installed;
    p.changed = false;
    p.complete = complete;
    p.has_roms = has_roms;
    return p;
}

/* ==== Tests: changesInstalls / changesRemovals / changesTotal ==== */

TEST(changes_all_zero) {
    reset_globals();
    ASSERT_EQ(changesInstalls(), 0);
    ASSERT_EQ(changesRemovals(), 0);
    ASSERT_EQ(changesTotal(), 0);
}

TEST(changes_installs_sum) {
    reset_globals();
    changes_installs[0] = 3;
    changes_installs[1] = 2;
    changes_installs[2] = 1;
    changes_installs[3] = 0;
    ASSERT_EQ(changesInstalls(), 6);
}

TEST(changes_removals_sum) {
    reset_globals();
    changes_removals[0] = 1;
    changes_removals[1] = 4;
    changes_removals[2] = 0;
    changes_removals[3] = 2;
    ASSERT_EQ(changesRemovals(), 7);
}

TEST(changes_total_combined) {
    reset_globals();
    changes_installs[0] = 5;
    changes_removals[0] = 3;
    ASSERT_EQ(changesTotal(), 8);
}

/* ==== Tests: totalInstalls ==== */

TEST(total_installs_zero) {
    reset_globals();
    ASSERT_EQ(totalInstalls(), 0);
}

TEST(total_installs_sum) {
    reset_globals();
    package_installed_count[0] = 10;
    package_installed_count[1] = 5;
    package_installed_count[2] = 3;
    package_installed_count[3] = 0;
    ASSERT_EQ(totalInstalls(), 18);
}

/* ==== Tests: setItemsInstallValue mode=1 (all on) ==== */

TEST(set_items_mode1_uninstalled_marks_install) {
    reset_globals();
    packages[0][0] = make_package("GBA", false, false, false);
    package_count[0] = 1;

    setItemsInstallValue(1, 0);

    /* uninstalled + mode=1 → is_active=true, new_value = true != false = true → changed */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 1);
    ASSERT_EQ(changes_removals[0], 0);
}

TEST(set_items_mode1_installed_no_change) {
    reset_globals();
    packages[0][0] = make_package("SNES", true, true, false);
    package_count[0] = 1;

    setItemsInstallValue(1, 0);

    /* installed + mode=1 → is_active=true, new_value = true != true = false → no change */
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

/* ==== Tests: setItemsInstallValue mode=0 (all off) ==== */

TEST(set_items_mode0_installed_marks_removal) {
    reset_globals();
    packages[0][0] = make_package("NES", true, true, false);
    package_count[0] = 1;

    setItemsInstallValue(0, 0);

    /* installed + mode=0 → is_active=false, new_value = false != true = true → removal */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_removals[0], 1);
    ASSERT_EQ(changes_installs[0], 0);
}

TEST(set_items_mode0_uninstalled_no_change) {
    reset_globals();
    packages[0][0] = make_package("PCE", false, false, false);
    package_count[0] = 1;

    setItemsInstallValue(0, 0);

    /* uninstalled + mode=0 → is_active=false, new_value = false != false = false → no change */
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

/* ==== Tests: setItemsInstallValue mode=2 (auto / has_roms) ==== */

TEST(set_items_mode2_has_roms_not_installed) {
    reset_globals();
    packages[0][0] = make_package("GBA", false, false, true);
    package_count[0] = 1;

    setItemsInstallValue(2, 0);

    /* has_roms=true, not installed → should install */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 1);
}

TEST(set_items_mode2_no_roms_installed) {
    reset_globals();
    packages[0][0] = make_package("SNES", true, true, false);
    package_count[0] = 1;

    setItemsInstallValue(2, 0);

    /* has_roms=false, installed → should remove */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_removals[0], 1);
}

TEST(set_items_mode2_has_roms_installed) {
    reset_globals();
    packages[0][0] = make_package("NES", true, true, true);
    package_count[0] = 1;

    setItemsInstallValue(2, 0);

    /* has_roms=true, installed → no change needed */
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

TEST(set_items_mode2_no_roms_not_installed) {
    reset_globals();
    packages[0][0] = make_package("PCE", false, false, false);
    package_count[0] = 1;

    setItemsInstallValue(2, 0);

    /* has_roms=false, not installed → no change */
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
    ASSERT_EQ(changes_removals[0], 0);
}

/* ==== Tests: multiple packages in layer ==== */

TEST(set_items_multiple_packages) {
    reset_globals();
    packages[0][0] = make_package("GBA", false, false, false);
    packages[0][1] = make_package("SNES", false, false, false);
    packages[0][2] = make_package("NES", true, true, false);
    package_count[0] = 3;

    setItemsInstallValue(1, 0);

    /* Two uninstalled → 2 installs, one already installed → no change */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_TRUE(packages[0][1].changed);
    ASSERT_FALSE(packages[0][2].changed);
    ASSERT_EQ(changes_installs[0], 2);
    ASSERT_EQ(changes_removals[0], 0);
}

/* ==== Tests: toggle on then off ==== */

TEST(set_items_toggle_on_then_off) {
    reset_globals();
    packages[0][0] = make_package("GBA", false, false, false);
    package_count[0] = 1;

    /* Turn on: mark for install */
    setItemsInstallValue(1, 0);
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 1);

    /* Turn off: undo the install */
    setItemsInstallValue(0, 0);
    ASSERT_FALSE(packages[0][0].changed);
    ASSERT_EQ(changes_installs[0], 0);
}

/* ==== Tests: incomplete package (installed but not complete) ==== */

TEST(set_items_incomplete_package_removal) {
    reset_globals();
    packages[0][0] = make_package("GBA", true, false, false);
    package_count[0] = 1;

    setItemsInstallValue(0, 0);

    /* installed=true, complete=false, mode=0 → removal + re-install counter adjusts */
    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_EQ(changes_removals[0], 1);
    /* Incomplete install: when removing, installs counter gets -1 */
    ASSERT_EQ(changes_installs[0], -1);
}

/* ==== Tests: cross-layer isolation ==== */

TEST(set_items_layer_isolation) {
    reset_globals();
    packages[0][0] = make_package("GBA", false, false, false);
    packages[1][0] = make_package("App1", false, false, false);
    package_count[0] = 1;
    package_count[1] = 1;

    setItemsInstallValue(1, 0); /* only layer 0 */

    ASSERT_TRUE(packages[0][0].changed);
    ASSERT_FALSE(packages[1][0].changed);
    ASSERT_EQ(changes_installs[0], 1);
    ASSERT_EQ(changes_installs[1], 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== packageManager/changes.h Unit Tests ===\n\n");

    /* Counting functions */
    RUN_TEST(changes_all_zero);
    RUN_TEST(changes_installs_sum);
    RUN_TEST(changes_removals_sum);
    RUN_TEST(changes_total_combined);
    RUN_TEST(total_installs_zero);
    RUN_TEST(total_installs_sum);

    /* setItemsInstallValue mode=1 */
    RUN_TEST(set_items_mode1_uninstalled_marks_install);
    RUN_TEST(set_items_mode1_installed_no_change);

    /* setItemsInstallValue mode=0 */
    RUN_TEST(set_items_mode0_installed_marks_removal);
    RUN_TEST(set_items_mode0_uninstalled_no_change);

    /* setItemsInstallValue mode=2 (auto) */
    RUN_TEST(set_items_mode2_has_roms_not_installed);
    RUN_TEST(set_items_mode2_no_roms_installed);
    RUN_TEST(set_items_mode2_has_roms_installed);
    RUN_TEST(set_items_mode2_no_roms_not_installed);

    /* Multiple packages */
    RUN_TEST(set_items_multiple_packages);

    /* Toggle on/off */
    RUN_TEST(set_items_toggle_on_then_off);

    /* Incomplete package */
    RUN_TEST(set_items_incomplete_package_removal);

    /* Layer isolation */
    RUN_TEST(set_items_layer_isolation);

    TEST_REPORT();
    return test_failures;
}
