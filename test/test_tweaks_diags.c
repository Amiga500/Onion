/**
 * @file test_tweaks_diags.c
 * @brief Tweaks > Advanced > Diagnostics: script list and status
 *
 * diags_status.h and diags.h with real scripts run by /bin/sh:
 *  - menulabel/tooltip read into their fields without writing past them (a
 *    long label used to wipe the tooltip read before it, hiding the script);
 *  - a script thread never touches menu items: its status lands in the item
 *    of the same script in the menu as rebuilt meanwhile (it used to write
 *    into the freed item: ASan heap-use-after-free);
 *  - "already running" / "another script is already running", and the
 *    lock released after every run;
 *  - the "Idle" of an ended run doesn't hide a newer status.
 *
 * Build and run: make -f Makefile.unit test_tweaks_diags
 */

#include "onion_test.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_tweaks_diags"
#define DIAG_SCRIPT_PATH TEST_DIR
#define DIAG_IDLE_DELAY_S 1

/* As test_list.c: list.h without production lang.h. */
#define SYSTEM_LANG_H__
typedef int lang_hash;
static char **lang_list = NULL;
#ifndef LANG_MAX
#define LANG_MAX 400
#endif

#include "components/list.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

#include "../src/tweaks/diags.h"

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
    chmod(path, 0755); // FAT: every file can be run
}

static void reset_dir(void)
{
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    mkdir(TEST_DIR, 0755);
}

static bool parse_text(const char *text, diagScripts *entry)
{
    put(TEST_DIR "/s.sh", text);
    FILE *f = fopen(TEST_DIR "/s.sh", "r");
    bool listed = diags_parseScript(f, "s.sh", entry);
    fclose(f);
    return listed;
}

static void noop_action(void *pt) { (void)pt; }

static void wait_ms(int ms) { usleep((useconds_t)ms * 1000); }

// A menu as menu_diagnostics() makes it: one item per script.
static List make_menu(const char *const *names, int count)
{
    List menu = list_createWithSticky(count, "Diagnostics");
    for (int i = 0; i < count; i++) {
        ListItem item = {.payload_ptr = (void *)names[i], .action = noop_action};
        snprintf(item.label, sizeof(item.label), "%s", names[i]);
        strncpy(item.sticky_note, DIAG_STATUS_IDLE, STR_MAX - 1);
        list_addItem(&menu, item);
    }
    return menu;
}

static pthread_t run_script(const char *name)
{
    pthread_t t;
    pthread_create(&t, NULL, diags_runScript, strdup(name));
    return t;
}

static bool status_is(const char *name, const char *note)
{
    char got[STR_MAX] = "";
    return diags_getStatus(name, got, sizeof(got)) && strcmp(got, note) == 0;
}

TEST(long_label_keeps_the_tooltip)
{
    reset_dir();
    char text[1024];
    char label[101];
    memset(label, 'L', 100);
    label[100] = '\0';
    // The tooltip first: the label's overflow landed on its first byte.
    snprintf(text, sizeof(text), "tooltip=\"Help\"\nmenulabel=\"%s\"\n", label);
    diagScripts entry;
    ASSERT_TRUE(parse_text(text, &entry));
    ASSERT_EQ((int)strlen(entry.label), DIAG_MAX_LABEL_LENGTH - 1);
    ASSERT_STREQ(entry.tooltip, "Help");
}

TEST(long_tooltip_stays_in_its_field)
{
    reset_dir();
    char text[1024];
    char tip[301];
    memset(tip, 'T', 300);
    tip[300] = '\0';
    snprintf(text, sizeof(text), "menulabel=\"Check\"\ntooltip=\"%s\"\n", tip);
    struct {
        diagScripts entry;
        char after[8]; // what the old %256 wrote into
    } guarded;
    memset(guarded.after, 'G', sizeof(guarded.after));
    ASSERT_TRUE(parse_text(text, &guarded.entry));
    // Cut at the line buffer (STR_MAX, less 'tooltip="'), within the field.
    ASSERT_EQ((int)strlen(guarded.entry.tooltip), STR_MAX - 1 - 9);
    ASSERT_EQ(guarded.after[0], 'G');
}

TEST(listing_rules)
{
    reset_dir();
    diagScripts entry;
    ASSERT_TRUE(parse_text("menulabel=\"A\"\ntooltip=\"B\"\n", &entry));
    ASSERT_STREQ(entry.filename, "s.sh");
    ASSERT_FALSE(parse_text("menulabel=\"A\"\n", &entry)); // no tooltip
    ASSERT_FALSE(parse_text("# IGNORE\nmenulabel=\"A\"\ntooltip=\"B\"\n", &entry));
}

TEST(status_reaches_the_rebuilt_menu)
{
    reset_dir();
    put(TEST_DIR "/ok.sh", "sleep 0.3\nexit 0\n");
    const char *names[] = {"ok.sh"};
    List menu = make_menu(names, 1);

    pthread_t t = run_script("ok.sh");
    wait_ms(100);
    // Tweaks rebuilds its menus (menu_resetAll()) while the script runs.
    list_free(&menu);
    menu = make_menu(names, 1);
    ASSERT_TRUE(diags_applyStatus(&menu));
    ASSERT_STREQ(menu.items[0].sticky_note, "Script running...");

    pthread_join(t, NULL);
    ASSERT_TRUE(diags_applyStatus(&menu));
    ASSERT_STREQ(menu.items[0].sticky_note, "Script successfully completed.");
    ASSERT_FALSE(diags_applyStatus(&menu)); // nothing new

    wait_ms(DIAG_IDLE_DELAY_S * 1000 + 400);
    ASSERT_TRUE(diags_applyStatus(&menu));
    ASSERT_STREQ(menu.items[0].sticky_note, DIAG_STATUS_IDLE);
    list_free(&menu);
}

TEST(failed_script_and_lock_released)
{
    reset_dir();
    put(TEST_DIR "/bad.sh", "exit 3\n");
    put(TEST_DIR "/ok2.sh", "exit 0\n");
    pthread_join(run_script("bad.sh"), NULL);
    ASSERT_TRUE(status_is("bad.sh", "Script failed!"));
    pthread_t t = run_script("ok2.sh"); // runs: the lock was released
    pthread_join(t, NULL);
    ASSERT_TRUE(status_is("ok2.sh", "Script successfully completed."));
}

TEST(only_one_script_at_a_time)
{
    reset_dir();
    put(TEST_DIR "/long.sh", "sleep 0.5\n");
    put(TEST_DIR "/other.sh", "exit 0\n");
    pthread_t t = run_script("long.sh");
    wait_ms(100);
    pthread_join(run_script("long.sh"), NULL);
    ASSERT_TRUE(status_is("long.sh", "Script already running..."));
    pthread_join(run_script("other.sh"), NULL);
    ASSERT_TRUE(status_is("other.sh", "Another script is already running..."));
    pthread_join(t, NULL);
    ASSERT_TRUE(status_is("long.sh", "Script successfully completed."));
}

TEST(idle_of_an_ended_run_does_not_hide_a_newer_one)
{
    reset_dir();
    unsigned seq = diags_setStatus("x.sh", "Script successfully completed.");
    diags_setStatus("x.sh", "Script running..."); // run again meanwhile
    diags_setStatusIf("x.sh", DIAG_STATUS_IDLE, seq);
    ASSERT_TRUE(status_is("x.sh", "Script running..."));

    seq = diags_setStatus("x.sh", "Script failed!");
    diags_setStatusIf("x.sh", DIAG_STATUS_IDLE, seq);
    ASSERT_TRUE(status_is("x.sh", DIAG_STATUS_IDLE));

    char got[STR_MAX];
    ASSERT_FALSE(diags_getStatus("never-run.sh", got, sizeof(got)));
}

int main(void)
{
    printf("\n=== tweaks/diags.h ===\n\n");
    RUN_TEST(long_label_keeps_the_tooltip);
    RUN_TEST(long_tooltip_stays_in_its_field);
    RUN_TEST(listing_rules);
    RUN_TEST(status_reaches_the_rebuilt_menu);
    RUN_TEST(failed_script_and_lock_released);
    RUN_TEST(only_one_script_at_a_time);
    RUN_TEST(idle_of_an_ended_run_does_not_hide_a_newer_one);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
