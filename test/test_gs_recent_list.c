/**
 * @file test_gs_recent_list.c
 * @brief The GameSwitcher's reading and editing of MainUI's recent list
 *
 * Production code from src/gameSwitcher/gs_recent_item.h, on real files in
 * a temporary folder:
 *  - gs_readRecentFile(): which games are listed, duplicates removed from
 *    the file, line numbers (also when the file cannot be rewritten);
 *  - the first entry (readFirstEntry() reads one game) is the game the full
 *    read puts first, also when the newest line's ROM is gone;
 *  - gs_deleteRecentLine() / gs_recentLineMatches(): X Remove and resume
 *    act on a line only while it still holds the game.
 *
 * Build and run: make -f Makefile.unit test_gs_recent_list
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/gameSwitcher/gs_recent_item.h"

#define DIR_ "/tmp/onion_test_gs_recent_list"
#define LIST DIR_ "/recentlist.json"
#define LAUNCH DIR_ "/launch.sh"

static void touch(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f != NULL)
        fclose(f);
}

static void rom_path(char *out, size_t size, const char *name)
{
    snprintf(out, size, DIR_ "/%s.gba", name);
}

// A recent-list line for ROM `name` (created on disk if `present`).
static void line_for(char *out, size_t size, const char *name, bool present)
{
    char rom[256];
    rom_path(rom, sizeof(rom), name);
    if (present)
        touch(rom);
    snprintf(out, size,
             "{\"label\":\"%s\",\"rompath\":\"%s\",\"imgpath\":\"\",\"launch\":\"" LAUNCH "\",\"type\":5}\n",
             name, rom);
}

static void reset(void)
{
    system("rm -rf " DIR_);
    mkdir(DIR_, 0755);
    touch(LAUNCH);
}

static void write_list(const char *const *names, const bool *present, int n)
{
    FILE *f = fopen(LIST, "w");
    char line[1024];
    for (int i = 0; i < n; i++) {
        line_for(line, sizeof(line), names[i], present[i]);
        fputs(line, f);
    }
    fclose(f);
}

static int count_lines(void)
{
    FILE *f = fopen(LIST, "r");
    int n = 0, c;
    while (f != NULL && (c = fgetc(f)) != EOF)
        n += c == '\n';
    if (f != NULL)
        fclose(f);
    return n;
}

static bool line_is(int n, const char *name)
{
    char *line = file_read_lineN(LIST, n);
    char needle[300];
    snprintf(needle, sizeof(needle), "\"label\":\"%s\"", name);
    bool ok = line != NULL && strstr(line, needle) != NULL;
    free(line);
    return ok;
}

static bool is_rom(const RecentItem *item, const char *name)
{
    char rom[256];
    rom_path(rom, sizeof(rom), name);
    return strcmp(item->rompath, rom) == 0;
}

/* ---- gs_readRecentFile ---- */

TEST(reads_games_in_order_with_line_numbers)
{
    reset();
    const char *names[] = {"A", "B", "C"};
    const bool present[] = {true, true, true};
    write_list(names, present, 3);

    RecentItem items[10];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 10), 3);
    ASSERT_TRUE(is_rom(&items[0], "A") && items[0].lineNo == 1);
    ASSERT_TRUE(is_rom(&items[1], "B") && items[1].lineNo == 2);
    ASSERT_TRUE(is_rom(&items[2], "C") && items[2].lineNo == 3);
}

TEST(skips_missing_roms_but_keeps_their_lines)
{
    reset();
    const char *names[] = {"Gone", "B", "C"};
    const bool present[] = {false, true, true};
    write_list(names, present, 3);

    RecentItem items[10];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 10), 2);
    ASSERT_TRUE(is_rom(&items[0], "B") && items[0].lineNo == 2);
    ASSERT_TRUE(is_rom(&items[1], "C") && items[1].lineNo == 3);
    ASSERT_EQ(count_lines(), 3); // not deleted: MainUI still lists it
}

TEST(removes_duplicates_and_renumbers)
{
    reset();
    const char *names[] = {"A", "B", "A", "C", "B", "D"};
    const bool present[] = {true, true, true, true, true, true};
    write_list(names, present, 6);

    RecentItem items[10];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 10), 4);
    ASSERT_EQ(count_lines(), 4);
    // Every game's line number is its line in the rewritten file.
    for (int i = 0; i < 4; i++) {
        char *line = file_read_lineN(LIST, items[i].lineNo);
        ASSERT_TRUE(line != NULL);
        ASSERT_TRUE(strstr(line, items[i].rompath) != NULL);
        free(line);
    }
    ASSERT_TRUE(is_rom(&items[2], "C") && items[2].lineNo == 3);
    ASSERT_TRUE(is_rom(&items[3], "D") && items[3].lineNo == 4);
}

TEST(failed_rewrite_keeps_file_line_numbers)
{
    reset();
    const char *names[] = {"A", "A", "B", "C"};
    const bool present[] = {true, true, true, true};
    write_list(names, present, 4);
    if (geteuid() == 0) {
        // root ignores the read-only folder below; skip the case.
        printf("    (skipped as root)\n");
        return;
    }
    chmod(DIR_, 0555); // no temporary file: the rewrite fails

    RecentItem items[10];
    int n = gs_readRecentFile(LIST, items, 10);
    chmod(DIR_, 0755);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(count_lines(), 4); // duplicate still there
    ASSERT_TRUE(is_rom(&items[1], "B") && items[1].lineNo == 3);
    ASSERT_TRUE(is_rom(&items[2], "C") && items[2].lineNo == 4);
}

TEST(unreadable_list_is_minus_one)
{
    reset();
    RecentItem items[2];
    ASSERT_EQ(gs_readRecentFile(DIR_ "/missing.json", items, 2), -1);
}

TEST(first_entry_matches_full_read)
{
    // readFirstEntry() reads one game, readHistory() all: entry 0 must be
    // the same game, also when the newest line's ROM was deleted.
    reset();
    const char *names[] = {"Gone", "B", "C"};
    const bool present[] = {false, true, true};
    write_list(names, present, 3);

    RecentItem first, all[10];
    ASSERT_EQ(gs_readRecentFile(LIST, &first, 1), 1);
    ASSERT_EQ(gs_readRecentFile(LIST, all, 10), 2);
    ASSERT_TRUE(gs_sameRecentItem(&first, &all[0]));
    ASSERT_TRUE(is_rom(&first, "B"));
    ASSERT_EQ(first.lineNo, all[0].lineNo);
}

TEST(first_entry_needs_launcher)
{
    reset();
    const char *names[] = {"A", "B"};
    const bool present[] = {true, true};
    write_list(names, present, 2);
    remove(LAUNCH);

    RecentItem first;
    ASSERT_EQ(gs_readRecentFile(LIST, &first, 1), 0);
}

/* ---- gs_deleteRecentLine / gs_recentLineMatches ---- */

TEST(delete_removes_the_games_line)
{
    reset();
    const char *names[] = {"A", "B", "C"};
    const bool present[] = {true, true, true};
    write_list(names, present, 3);

    RecentItem items[10];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 10), 3);
    ASSERT_TRUE(gs_deleteRecentLine(LIST, &items[1]));
    ASSERT_EQ(count_lines(), 2);
    ASSERT_TRUE(line_is(1, "A"));
    ASSERT_TRUE(line_is(2, "C"));
}

TEST(delete_refuses_a_stale_line)
{
    // The list changed under the GameSwitcher: line 2 now holds another
    // game. Removing B must not delete it.
    reset();
    const char *names[] = {"A", "B", "C"};
    const bool present[] = {true, true, true};
    write_list(names, present, 3);

    RecentItem items[10];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 10), 3);
    const char *changed[] = {"A", "C"};
    const bool changed_present[] = {true, true};
    write_list(changed, changed_present, 2);

    ASSERT_FALSE(gs_recentLineMatches(LIST, &items[1]));
    ASSERT_FALSE(gs_deleteRecentLine(LIST, &items[1]));
    ASSERT_EQ(count_lines(), 2);
    ASSERT_TRUE(line_is(2, "C"));
}

TEST(delete_needs_same_launcher)
{
    reset();
    const char *names[] = {"A"};
    const bool present[] = {true};
    write_list(names, present, 1);

    RecentItem items[1];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 1), 1);
    snprintf(items[0].launch, sizeof(items[0].launch), "%s", DIR_ "/other.sh");
    ASSERT_FALSE(gs_deleteRecentLine(LIST, &items[0]));
    ASSERT_EQ(count_lines(), 1);
}

TEST(line_numbers_below_one_never_match)
{
    reset();
    const char *names[] = {"A"};
    const bool present[] = {true};
    write_list(names, present, 1);

    RecentItem items[1];
    ASSERT_EQ(gs_readRecentFile(LIST, items, 1), 1);
    items[0].lineNo = 0;
    ASSERT_FALSE(gs_recentLineMatches(LIST, &items[0]));
}

int main(void)
{
    printf("\n=== gs_recent_item.h: recent list ===\n\n");
    RUN_TEST(reads_games_in_order_with_line_numbers);
    RUN_TEST(skips_missing_roms_but_keeps_their_lines);
    RUN_TEST(removes_duplicates_and_renumbers);
    RUN_TEST(failed_rewrite_keeps_file_line_numbers);
    RUN_TEST(unreadable_list_is_minus_one);
    RUN_TEST(first_entry_matches_full_read);
    RUN_TEST(first_entry_needs_launcher);
    RUN_TEST(delete_removes_the_games_line);
    RUN_TEST(delete_refuses_a_stale_line);
    RUN_TEST(delete_needs_same_launcher);
    RUN_TEST(line_numbers_below_one_never_match);
    system("rm -rf " DIR_);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
