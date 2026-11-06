/* SPDX-License-Identifier: GPL-3.0-only */
/* SDL-free fixture for Saved Games: the core and states folder as Onion
 * finds them, which states are listed, and what a resume and its undo do to
 * Auto and its backup, including a failed copy and refused renames. */
#include "platform/save_states.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[] = "/tmp/mainui-saves-XXXXXX";

static void put(const char *relative, const char *text)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    for (char *p = path + strlen(root) + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(path, 0700);
            *p = '/';
        }
    }
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file));
}

/* A file's text, "" when it is missing. */
static const char *get(const char *relative)
{
    static char text[256];
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    FILE *file = fopen(path, "rb");
    text[0] = 0;
    if (file) {
        text[fread(text, 1, sizeof text - 1, file)] = 0;
        fclose(file);
    }
    return text;
}

static bool exists(const char *relative)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    return !access(path, F_OK);
}

#define STATES "Saves/CurrentProfile/states/Test Core/"

/* The test game's states afresh: Auto "auto" with its picture, slots 1, 3
 * and 5, 5 without a picture. */
static void states(void)
{
    char command[4200];
    snprintf(command, sizeof command, "rm -rf '%s/Saves'", root);
    assert(!system(command));
    put(STATES "Game.state.auto", "auto");
    put(STATES "Game.state.auto.png", "auto-png");
    put(STATES "Game.state1", "one");
    put(STATES "Game.state1.png", "one-png");
    put(STATES "Game.state3", "three");
    put(STATES "Game.state3.png", "three-png");
    put(STATES "Game.state5", "five");
}

static int found(MainUISaves *saves, bool previous)
{
    char sd[4096], launch[4096], rom[4096];
    snprintf(sd, sizeof sd, "%s", root);
    snprintf(launch, sizeof launch, "%s/Emu/SFC/launch.sh", root);
    snprintf(rom, sizeof rom, "%s/Roms/SFC/Game.sfc", root);
    assert(mainui_saves_locate(saves, sd, launch, rom));
    return mainui_saves_list(saves, previous, true);
}

/* No file staged beside its target is left in the states folder. */
static bool staged_left(void)
{
    char command[4300];
    snprintf(command, sizeof command, "ls '%s/" STATES "' | grep -q 'mainui-new'", root);
    return !system(command);
}

/* The states folder, every name and its contents, sorted: to compare. */
static char *snapshot(void)
{
    char command[4300];
    snprintf(command, sizeof command,
             "cd '%s/" STATES "' && for f in *; do printf '%%s=' \"$f\"; cat \"$f\" 2>&1; echo; "
             "done",
             root);
    FILE *pipe = popen(command, "r");
    assert(pipe);
    char *text = calloc(1, 64 * 1024);
    assert(text);
    size_t got = fread(text, 1, 64 * 1024 - 1, pipe);
    text[got] = 0;
    pclose(pipe);
    return text;
}

/* The entry of slot (MAINUI_SAVES_AUTO and _PREVIOUS too), listed afresh. */
static int entry(MainUISaves *saves, int slot)
{
    found(saves, true);
    for (int i = 0; i < saves->count; i++) {
        if (saves->saves[i].slot == slot) {
            return i;
        }
    }
    assert(!"slot listed");
    return -1;
}

/* Resume slot as mode says; what undoes it. */
static int resume_as(MainUISaves *saves, int slot, int mode)
{
    char error[256];
    int undo = -1;
    assert(mainui_saves_resume(saves, entry(saves, slot), mode, &undo, error) && !*error);
    assert(!staged_left());
    return undo;
}

static int resume(MainUISaves *saves, int slot)
{
    return resume_as(saves, slot, MAINUI_RESUME_BACKUP);
}

static void numbered_untouched(void)
{
    assert(!strcmp(get(STATES "Game.state1"), "one") &&
           !strcmp(get(STATES "Game.state1.png"), "one-png"));
    assert(!strcmp(get(STATES "Game.state3"), "three") &&
           !strcmp(get(STATES "Game.state3.png"), "three-png"));
    assert(!strcmp(get(STATES "Game.state5"), "five") && !exists(STATES "Game.state5.png"));
}

int main(void)
{
    assert(mkdtemp(root));
    MainUISaves *saves = calloc(1, sizeof *saves);
    assert(saves);
    char sd[4096], launch[4096], rom[4096];
    snprintf(sd, sizeof sd, "%s", root);
    snprintf(launch, sizeof launch, "%s/Emu/SFC/launch.sh", root);
    snprintf(rom, sizeof rom, "%s/Roms/SFC/Game.sfc", root);

    /* The core: the launcher's .retroarch/cores/<core>.so, named by its
     * .info's corename; nothing without either. */
    put("Roms/SFC/Game.sfc", "rom");
    assert(!mainui_saves_locate(saves, sd, launch, rom));
    put("Emu/SFC/launch.sh", "#!/bin/sh\ncd /mnt/SDCARD/RetroArch/\nHOME=/mnt/SDCARD/RetroArch/ "
                             "$progdir/../../RetroArch/retroarch -v -L "
                             "$progdir/../../RetroArch/.retroarch/cores/test_libretro.so \"$1\"\n");
    assert(!mainui_saves_locate(saves, sd, launch, rom));
    put("RetroArch/.retroarch/cores/test_libretro.info",
        "display_name = \"Test\"\ncorename = \"Test Core\"\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && !strcmp(saves->stem, "Game"));
    assert(strstr(saves->directory, "/Saves/CurrentProfile/states/Test Core"));
    /* A default_core= line, and the ROM's own core, chosen in Game List
     * Options, come first. */
    put("RetroArch/.retroarch/cores/other_libretro.info", "corename = \"Other Core\"\n");
    put("Emu/SFC/launch.sh", "default_core=other\n./retroarch -L .retroarch/cores/$default_core"
                             "_libretro.so \"$1\"\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Other Core"));
    /* The ROM's own core counts only when its .so is there, as runtime.sh
     * launches it; so does RedOnion's for the whole system. */
    put("Roms/SFC/.game_config/Game.cfg", "core = \"test_libretro\"\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Other Core"));
    put("RetroArch/.retroarch/cores/test_libretro.so", "core");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Test Core"));
    put("RetroArch/.retroarch/cores/third_libretro.info", "corename = \"Third Core\"\n");
    put(".tmp_update/config/system_cores/Emu-SFC", " third_libretro \r\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Test Core"));
    put("Roms/SFC/.game_config/Game.cfg", "");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Other Core"));
    put("RetroArch/.retroarch/cores/third_libretro.so", "core");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Third Core"));
    put(".tmp_update/config/system_cores/Emu-SFC", "");
    put("Roms/SFC/.game_config/Game.cfg", "core = \"test_libretro\"\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Test Core"));
    /* A core name that would leave the cores folder is none. */
    put("Roms/SFC/.game_config/Game.cfg", "core = \"../../x\"\n");
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Other Core"));
    put("Roms/SFC/.game_config/Game.cfg", "");
    /* An uppercase extension keeps its name, as Onion spells the file:
     * Game.SFC's core is in Game.SFC.cfg, its states still Game.state*. */
    char upper[4096];
    snprintf(upper, sizeof upper, "%s/Roms/SFC/Game.SFC", root);
    put("Roms/SFC/Game.SFC", "rom");
    put("Roms/SFC/.game_config/Game.cfg", "core = \"test_libretro\"\n");
    assert(mainui_saves_locate(saves, sd, launch, upper) && strstr(saves->directory, "Other Core"));
    put("Roms/SFC/.game_config/Game.SFC.cfg", "core = \"test_libretro\"\n");
    assert(mainui_saves_locate(saves, sd, launch, upper) && strstr(saves->directory, "Test Core") &&
           !strcmp(saves->stem, "Game"));
    assert(mainui_saves_locate(saves, sd, launch, rom) && strstr(saves->directory, "Test Core"));
    put("Roms/SFC/.game_config/Game.SFC.cfg", "");
    put("Roms/SFC/.game_config/Game.cfg", "");
    put("Emu/SFC/launch.sh", "./retroarch -L .retroarch/cores/test_libretro.so \"$1\"\n");

    /* Beetle Supafaust's states are in Supafaust: that folder is used only
     * when the corename's has no state of the game, even when it exists. */
    char wipe[4300];
    snprintf(wipe, sizeof wipe, "rm -rf '%s/Saves'", root);
    assert(!system(wipe));
    put("RetroArch/.retroarch/cores/test_libretro.info", "corename = \"Beetle Supafaust\"\n");
    put("Saves/CurrentProfile/states/Supafaust/Game.state.auto", "auto");
    put("Saves/CurrentProfile/states/Supafaust/Game.state17", "17");
    put("Saves/CurrentProfile/states/Beetle Supafaust/Other.state.auto", "other game");
    assert(mainui_saves_locate(saves, sd, launch, rom) &&
           strstr(saves->directory, "/states/Supafaust"));
    assert(mainui_saves_list(saves, false, true) == 2 && saves->saves[0].slot == 17 &&
           saves->saves[1].slot == MAINUI_SAVES_AUTO);
    assert(!system(wipe));
    put("Saves/CurrentProfile/states/Supafaust/Game.state3", "3");
    assert(mainui_saves_locate(saves, sd, launch, rom) &&
           strstr(saves->directory, "/states/Supafaust") &&
           mainui_saves_list(saves, false, false) == 1 && saves->saves[0].slot == 3);
    /* The corename's folder wins when it has a state, shown or not. */
    put("Saves/CurrentProfile/states/Beetle Supafaust/Game" MAINUI_SAVES_BACKUP, "previous");
    assert(mainui_saves_locate(saves, sd, launch, rom) &&
           strstr(saves->directory, "/states/Beetle Supafaust"));
    put("Saves/CurrentProfile/states/Beetle Supafaust/Game.state5", "5");
    assert(mainui_saves_locate(saves, sd, launch, rom) &&
           strstr(saves->directory, "/states/Beetle Supafaust") &&
           mainui_saves_list(saves, false, false) == 1 && saves->saves[0].slot == 5);
    /* Any other core keeps its own folder: no other folder is looked at. */
    assert(!system(wipe));
    put("RetroArch/.retroarch/cores/test_libretro.info", "corename = \"Expected Core\"\n");
    put("Saves/CurrentProfile/states/Supafaust/Game.state.auto", "auto");
    put("Saves/CurrentProfile/states/Actual Core/Game.state17", "17");
    assert(mainui_saves_locate(saves, sd, launch, rom) &&
           strstr(saves->directory, "/states/Expected Core") &&
           !mainui_saves_list(saves, true, true));
    assert(!system(wipe));
    put("RetroArch/.retroarch/cores/test_libretro.info",
        "display_name = \"Test\"\ncorename = \"Test Core\"\n");

    /* Nothing listed without states. Slots ascend and Auto Save comes last;
     * pictures, other games and .state0 are not numbered slots. */
    assert(!found(saves, true));
    states();
    put(STATES "Game Two.state9", "other game");
    put(STATES "Game.state0", "not a slot");
    put(STATES "Game.state", "zero");
    put(STATES "Game.state12.png", "a picture without its state");
    assert(found(saves, true) == 5);
    assert(saves->saves[0].slot == 0 && !saves->saves[0].shot);
    assert(saves->saves[1].slot == 1 && saves->saves[1].shot);
    assert(saves->saves[2].slot == 3 && saves->saves[2].shot);
    assert(saves->saves[3].slot == 5 && !saves->saves[3].shot);
    assert(saves->saves[4].slot == MAINUI_SAVES_AUTO && saves->saves[4].shot);
    /* Without autosave, Auto Save is left out. */
    assert(mainui_saves_list(saves, true, false) == 4 && saves->saves[0].slot == 0 &&
           saves->saves[3].slot == 5);
    /* At most the ten highest. */
    for (int slot = 6; slot <= 16; slot++) {
        char name[64];
        snprintf(name, sizeof name, STATES "Game.state%d", slot);
        put(name, "more");
    }
    assert(found(saves, true) == 11 && saves->saves[0].slot == 7 && saves->saves[9].slot == 16 &&
           saves->saves[10].slot == MAINUI_SAVES_AUTO);

    /* Auto Save resumes as it is: nothing changes, nothing to undo. A game
     * with only Auto has Saved Games too. */
    states();
    char *before = snapshot(), *after;
    assert(resume(saves, MAINUI_SAVES_AUTO) == MAINUI_SAVES_UNDO_NONE);
    after = snapshot();
    assert(!strcmp(before, after));
    free(before);
    free(after);
    for (int slot = 1; slot <= 5; slot += 2) {
        char name[4200];
        snprintf(name, sizeof name, "%s/" STATES "Game.state%d", root, slot);
        assert(!unlink(name));
    }
    assert(found(saves, true) == 1 && saves->saves[0].slot == MAINUI_SAVES_AUTO);

    /* A slot: Auto and its picture into the one backup, then the state and
     * its picture into Auto; the numbered states never change. */
    states();
    assert(found(saves, true) == 4); /* no backup yet: no Previous Session */
    assert(resume(saves, 3) == MAINUI_SAVES_UNDO_BACKUP);
    assert(!strcmp(get(STATES "Game.state.auto"), "three") &&
           !strcmp(get(STATES "Game.state.auto.png"), "three-png"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "auto") &&
           !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP ".png"), "auto-png"));
    numbered_untouched();
    /* The next keeps only the latest Auto; a state without a picture
     * leaves Auto without one. */
    assert(resume(saves, 5) == MAINUI_SAVES_UNDO_BACKUP);
    assert(!strcmp(get(STATES "Game.state.auto"), "five") && !exists(STATES "Game.state.auto.png"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "three") &&
           !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP ".png"), "three-png"));
    numbered_untouched();

    /* Previous Session is leftmost and Auto Save is rightmost; the numbered
     * states remain ascending between them. They exchange pictures as well. */
    assert(found(saves, true) == 5 && saves->saves[0].slot == MAINUI_SAVES_PREVIOUS &&
           saves->saves[1].slot == 1 && saves->saves[2].slot == 3 && saves->saves[3].slot == 5 &&
           saves->saves[4].slot == MAINUI_SAVES_AUTO);
    /* Suppressing Auto does not move Previous Session to the right. */
    assert(mainui_saves_list(saves, true, false) == 4 &&
           saves->saves[0].slot == MAINUI_SAVES_PREVIOUS && saves->saves[3].slot == 5);
    assert(resume(saves, MAINUI_SAVES_PREVIOUS) == MAINUI_SAVES_UNDO_EXCHANGE);
    assert(!strcmp(get(STATES "Game.state.auto"), "three") &&
           !strcmp(get(STATES "Game.state.auto.png"), "three-png"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "five") &&
           !exists(STATES "Game" MAINUI_SAVES_BACKUP ".png"));
    assert(resume(saves, MAINUI_SAVES_PREVIOUS) == MAINUI_SAVES_UNDO_EXCHANGE);
    assert(!strcmp(get(STATES "Game.state.auto"), "five") && !exists(STATES "Game.state.auto.png"));
    numbered_untouched();

    /* A game that did not launch: undo puts Auto back as it was, from the
     * backup, by exchanging again, or, with no Auto before, removing it. */
    char error[256], path[4200];
    int index = 0;
    for (int slot = 3; slot != MAINUI_SAVES_AUTO;
         slot = slot == 3 ? MAINUI_SAVES_PREVIOUS : MAINUI_SAVES_AUTO) {
        states();
        put(STATES "Game" MAINUI_SAVES_BACKUP, "old backup");
        before = snapshot();
        int undo = resume(saves, slot);
        assert(mainui_saves_undo(saves, index, undo, error) && !staged_left());
        assert(!strcmp(get(STATES "Game.state.auto"), "auto") &&
               !strcmp(get(STATES "Game.state.auto.png"), "auto-png"));
        if (slot == MAINUI_SAVES_PREVIOUS) {
            after = snapshot();
            assert(!strcmp(before, after));
            free(after);
        }
        free(before);
        numbered_untouched();
    }
    states();
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto", root);
    assert(!unlink(path));
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto.png", root);
    assert(!unlink(path));
    before = snapshot();
    int undo = resume(saves, 1);
    assert(undo == MAINUI_SAVES_UNDO_REMOVE && !strcmp(get(STATES "Game.state.auto"), "one"));
    assert(!exists(STATES "Game" MAINUI_SAVES_BACKUP));
    assert(mainui_saves_undo(saves, index, undo, error));
    after = snapshot();
    assert(!strcmp(before, after));
    free(before);
    free(after);

    /* Overwrite: the state and its picture replace Auto's, no backup; a
     * failed launch keeps it. Without an Auto before, undo removes it. */
    states();
    assert(found(saves, false) == 4);
    assert(resume_as(saves, 3, MAINUI_RESUME_OVERWRITE) == MAINUI_SAVES_UNDO_KEPT);
    assert(!strcmp(get(STATES "Game.state.auto"), "three") &&
           !strcmp(get(STATES "Game.state.auto.png"), "three-png"));
    assert(!exists(STATES "Game" MAINUI_SAVES_BACKUP));
    assert(mainui_saves_undo(saves, index, MAINUI_SAVES_UNDO_KEPT, error));
    assert(!strcmp(get(STATES "Game.state.auto"), "three"));
    assert(resume_as(saves, 5, MAINUI_RESUME_OVERWRITE) == MAINUI_SAVES_UNDO_KEPT);
    assert(!strcmp(get(STATES "Game.state.auto"), "five") && !exists(STATES "Game.state.auto.png"));
    numbered_untouched();

    /* Swap: Auto and the state exchange, pictures too, and undo exchanges
     * them back; without an Auto, only a copy, the slot kept. */
    states();
    before = snapshot();
    index = entry(saves, 3);
    undo = resume_as(saves, 3, MAINUI_RESUME_SWAP);
    assert(undo == MAINUI_SAVES_UNDO_SWAP);
    assert(!strcmp(get(STATES "Game.state.auto"), "three") &&
           !strcmp(get(STATES "Game.state.auto.png"), "three-png"));
    assert(!strcmp(get(STATES "Game.state3"), "auto") &&
           !strcmp(get(STATES "Game.state3.png"), "auto-png"));
    assert(mainui_saves_undo(saves, index, undo, error));
    after = snapshot();
    assert(!strcmp(before, after));
    free(before);
    free(after);
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto", root);
    assert(!unlink(path));
    index = entry(saves, 5);
    undo = resume_as(saves, 5, MAINUI_RESUME_SWAP);
    assert(undo == MAINUI_SAVES_UNDO_REMOVE && !strcmp(get(STATES "Game.state.auto"), "five"));
    numbered_untouched();

    /* Previous Session with no Auto is a COPY, never an exchange that
     * consumes the only backup. Undoing a failed launch removes only Auto. */
    states();
    put(STATES "Game" MAINUI_SAVES_BACKUP, "previous");
    put(STATES "Game" MAINUI_SAVES_BACKUP ".png", "previous-png");
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto", root);
    assert(!unlink(path));
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto.png", root);
    assert(!unlink(path));
    index = entry(saves, MAINUI_SAVES_PREVIOUS);
    undo = resume(saves, MAINUI_SAVES_PREVIOUS);
    assert(undo == MAINUI_SAVES_UNDO_REMOVE);
    assert(!strcmp(get(STATES "Game.state.auto"), "previous"));
    assert(!strcmp(get(STATES "Game.state.auto.png"), "previous-png"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "previous"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP ".png"), "previous-png"));
    /* An explicit remove must not take its screenshot from a coincidental
     * .png in the process working directory. */
    char cwd[4096];
    assert(getcwd(cwd, sizeof cwd));
    put(".png", "unrelated-file");
    assert(!chdir(root));
    assert(mainui_saves_undo(saves, index, undo, error));
    assert(!chdir(cwd));
    assert(!exists(STATES "Game.state.auto") && !exists(STATES "Game.state.auto.png"));
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "previous"));
    assert(!staged_left());

    /* Undo must not delete Auto if the backup disappeared before staging.
     * The normal screenshot-absent case still removes an old image. */
    states();
    index = entry(saves, 3);
    undo = resume(saves, 3);
    snprintf(path, sizeof path, "%s/" STATES "Game" MAINUI_SAVES_BACKUP, root);
    assert(!unlink(path));
    assert(!mainui_saves_undo(saves, index, undo, error) && *error);
    assert(!strcmp(get(STATES "Game.state.auto"), "three"));
    assert(!staged_left());

    /* A file left beside a target refuses every resume, naming it, and is
     * never overwritten: it may hold the only copy of the Auto before. */
    states();
    before = snapshot();
    put(STATES "Game.state.auto.mainui-new", "only copy");
    index = entry(saves, 3);
    assert(!mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error));
    assert(strstr(error, "Game.state.auto.mainui-new") && undo == MAINUI_SAVES_UNDO_NONE);
    assert(!strcmp(get(STATES "Game.state.auto.mainui-new"), "only copy"));
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto.mainui-new", root);
    assert(!unlink(path));
    after = snapshot();
    assert(!strcmp(before, after));
    free(after);
    snprintf(path, sizeof path, "%s/" STATES "Game.state5", root);
    index = entry(saves, 5);
    assert(!unlink(path));
    assert(!mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error) && *error);
    put(STATES "Game.state5", "five");
    after = snapshot();
    assert(!strcmp(before, after));
    free(before);
    free(after);

    /* A rename the card refuses, at each step: false, the numbered states
     * unchanged, Auto whole, and the Auto before still in Auto or the
     * backup. A refused undo says so, the Auto before kept in the backup. */
    for (int step = 0; step < 4; step++) {
        states();
        index = entry(saves, 3);
        mainui_saves_test_fail = step;
        /* Steps 1 and 3 are screenshots: refused, the states still change
         * and the resume goes on rather than failing. */
        bool picture = step % 2;
        assert(mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error) == picture);
        assert(picture || *error);
        mainui_saves_test_fail = -1;
        numbered_untouched();
        const char *now = get(STATES "Game.state.auto");
        assert(!picture ||
               (!strcmp(now, "three") && !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "auto")));
        assert(!strcmp(now, "auto") || !strcmp(now, "three"));
        assert(!strcmp(now, "auto") || !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "auto"));
    }
    /* Auto with a screenshot, the slot without: removing Auto's old picture
     * refused. The resume succeeds, so nothing retries it, and the backup
     * stays the Auto before. */
    states();
    snprintf(path, sizeof path, "%s/" STATES "Game.state5.png", root);
    unlink(path);
    index = entry(saves, 5);
    mainui_saves_test_fail = 3;
    assert(mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error));
    mainui_saves_test_fail = -1;
    assert(!strcmp(get(STATES "Game.state.auto"), "five") &&
           !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "auto"));
    /* Auto that cannot be read, here a folder in its place, is neither
     * backed up nor replaced. */
    states();
    snprintf(path, sizeof path, "%s/" STATES "Game.state.auto", root);
    assert(!unlink(path) && !mkdir(path, 0755));
    index = entry(saves, 3);
    assert(!mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error) && *error);
    numbered_untouched();
    assert(!rmdir(path));
    /* A Previous Session exchange refused after Auto was replaced: the Auto
     * before is only beside the backup, and a retry is refused until it is
     * restored, keeping it. */
    states();
    put(STATES "Game" MAINUI_SAVES_BACKUP, "old backup");
    index = entry(saves, MAINUI_SAVES_PREVIOUS);
    mainui_saves_test_fail = 2;
    assert(!mainui_saves_resume(saves, index, MAINUI_RESUME_BACKUP, &undo, error) && *error);
    mainui_saves_test_fail = -1;
    assert(!strcmp(get(STATES "Game.state.auto"), "old backup") &&
           !strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP ".mainui-new"), "auto"));
    for (int retry = 0; retry < 2; retry++) {
        assert(!mainui_saves_resume(saves, entry(saves, retry ? 3 : MAINUI_SAVES_PREVIOUS),
                                    MAINUI_RESUME_BACKUP, &undo, error));
        assert(strstr(error, ".mainui-new"));
        assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP ".mainui-new"), "auto"));
    }
    states();
    index = entry(saves, 3);
    undo = resume(saves, 3);
    mainui_saves_test_fail = 0;
    assert(!mainui_saves_undo(saves, index, undo, error) && *error);
    mainui_saves_test_fail = -1;
    assert(!strcmp(get(STATES "Game" MAINUI_SAVES_BACKUP), "auto"));
    numbered_untouched();

    char command[4200];
    snprintf(command, sizeof command, "rm -rf '%s'", root);
    assert(!system(command));
    free(saves);
    puts("save states: core, listing, resume, undo and failures OK");
    return 0;
}
