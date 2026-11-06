/**
 * @file test_gs_state_copy.c
 * @brief Load of a save state for a game that is not running
 *
 * gs_loadSlotIntoAuto() from src/gameSwitcher/gs_state_copy.h, on real
 * files: the slot becomes the auto save with its picture, copied atomically
 * (Overwrite, as RedOnion's Saved Games default: no backup is left), and a
 * failed copy leaves the auto save as it was and reports failure.
 *
 * Build and run: make -f Makefile.unit test_gs_state_copy
 */

#include "onion_test.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/gameSwitcher/gs_state_copy.h"

#define DIR_ "/tmp/onion_test_gs_state_copy"
#define SLOT DIR_ "/Game.state3"
#define AUTO DIR_ "/Game.state.auto"

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
}

static bool has(const char *path, const char *text)
{
    char buf[256] = "";
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return strcmp(buf, text) == 0;
}

static bool missing(const char *path)
{
    return access(path, F_OK) != 0;
}

// Files in DIR_ (not counting sub-folders' contents).
static int files_in_dir(void)
{
    int n = 0;
    DIR *d = opendir(DIR_);
    struct dirent *e;
    while (d != NULL && (e = readdir(d)) != NULL)
        n += e->d_name[0] != '.';
    if (d != NULL)
        closedir(d);
    return n;
}

static void reset(void)
{
    system("rm -rf " DIR_);
    mkdir(DIR_, 0755);
}

TEST(slot_becomes_auto_with_its_picture)
{
    reset();
    put(SLOT, "slot3");
    put(SLOT ".png", "slot3 picture");
    put(AUTO, "latest progress");
    put(AUTO ".png", "latest picture");

    ASSERT_TRUE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "slot3"));
    ASSERT_TRUE(has(AUTO ".png", "slot3 picture"));
    ASSERT_TRUE(has(SLOT, "slot3")); // the slot itself stays
    ASSERT_TRUE(has(SLOT ".png", "slot3 picture"));
}

TEST(overwrite_leaves_no_backup)
{
    // As Open MainUI's Saved Games with RedOnion's default (Overwrite): no
    // copy of the replaced auto save is left behind.
    reset();
    put(SLOT, "slot3");
    put(SLOT ".png", "slot3 picture");
    put(AUTO, "latest progress");
    put(AUTO ".png", "latest picture");

    ASSERT_TRUE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_EQ(files_in_dir(), 4); // slot, slot picture, auto, auto picture
    ASSERT_TRUE(missing(AUTO ".bak"));
}

TEST(no_auto_yet)
{
    reset();
    put(SLOT, "slot3");
    ASSERT_TRUE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "slot3"));
    ASSERT_TRUE(missing(AUTO ".png"));
}

TEST(slot_without_picture_drops_the_old_one)
{
    reset();
    put(SLOT, "slot3");
    put(AUTO, "latest progress");
    put(AUTO ".png", "latest picture");
    ASSERT_TRUE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "slot3"));
    ASSERT_TRUE(missing(AUTO ".png")); // not the replaced state's picture
}

TEST(missing_slot_changes_nothing)
{
    reset();
    put(AUTO, "latest progress");
    put(AUTO ".png", "latest picture");
    ASSERT_FALSE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "latest progress"));
    ASSERT_TRUE(has(AUTO ".png", "latest picture"));
}

TEST(slot_that_is_not_a_file_changes_nothing)
{
    reset();
    put(AUTO, "latest progress");
    mkdir(SLOT, 0755);
    ASSERT_FALSE(gs_loadSlotIntoAuto(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "latest progress"));

    // A slot that vanishes between the check and the copy: the copy must
    // leave the target alone.
    ASSERT_FALSE(gs_copyFileAtomic(DIR_ "/nothing", AUTO));
    ASSERT_TRUE(has(AUTO, "latest progress"));
}

// An auto save path so long that the copy's temporary name no longer fits
// (file_atomic_begin() gives up): the copy itself fails.
static bool long_auto_path(char *out, size_t size)
{
    const size_t target = PATH_MAX - 9; // + ".png" fits, + ".tmp.NN.N" does not
    size_t len = (size_t)snprintf(out, size, "%s", DIR_ "/deep");
    if (mkdir(out, 0755) != 0)
        return false;
    while (target - len > 255) {
        len += (size_t)snprintf(out + len, size - len, "/%0250d", 0);
        if (mkdir(out, 0755) != 0)
            return false;
    }
    size_t name_len = target - len - 1; // after the "/"
    out[len++] = '/';
    memset(out + len, 'a', name_len - strlen(".state.auto"));
    len += name_len - strlen(".state.auto");
    snprintf(out + len, size - len, ".state.auto");
    return strlen(out) == target;
}

TEST(failed_copy_keeps_auto_save_and_picture)
{
    reset();
    static char auto_path[PATH_MAX + 64];
    static char path[PATH_MAX + 64];
    ASSERT_TRUE(long_auto_path(auto_path, sizeof(auto_path)));
    put(SLOT, "slot3");
    put(SLOT ".png", "slot3 picture");
    put(auto_path, "latest progress");
    snprintf(path, sizeof(path), "%s.png", auto_path);
    put(path, "latest picture");
    ASSERT_TRUE(has(auto_path, "latest progress"));

    ASSERT_FALSE(gs_loadSlotIntoAuto(SLOT, auto_path));
    ASSERT_TRUE(has(auto_path, "latest progress"));
    ASSERT_TRUE(has(path, "latest picture"));
}

TEST(atomic_copy_leaves_no_temporary_file)
{
    reset();
    put(SLOT, "slot3");
    ASSERT_TRUE(gs_copyFileAtomic(SLOT, AUTO));
    ASSERT_TRUE(has(AUTO, "slot3"));
    ASSERT_EQ(files_in_dir(), 2);
}

TEST(big_state_copied_whole)
{
    reset();
    FILE *f = fopen(SLOT, "wb");
    for (int i = 0; i < 300000; i++)
        fputc(i & 0xFF, f);
    fclose(f);
    ASSERT_TRUE(gs_loadSlotIntoAuto(SLOT, AUTO));
    struct stat st;
    ASSERT_EQ(stat(AUTO, &st), 0);
    ASSERT_EQ((long)st.st_size, 300000L);
    f = fopen(AUTO, "rb");
    bool same = true;
    for (int i = 0; i < 300000 && same; i++)
        same = fgetc(f) == (i & 0xFF);
    fclose(f);
    ASSERT_TRUE(same);
}

int main(void)
{
    printf("\n=== gs_state_copy.h ===\n\n");
    RUN_TEST(slot_becomes_auto_with_its_picture);
    RUN_TEST(overwrite_leaves_no_backup);
    RUN_TEST(no_auto_yet);
    RUN_TEST(slot_without_picture_drops_the_old_one);
    RUN_TEST(missing_slot_changes_nothing);
    RUN_TEST(slot_that_is_not_a_file_changes_nothing);
    RUN_TEST(failed_copy_keeps_auto_save_and_picture);
    RUN_TEST(atomic_copy_leaves_no_temporary_file);
    RUN_TEST(big_state_copied_whole);
    system("rm -rf " DIR_);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
