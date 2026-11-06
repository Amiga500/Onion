/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/save_states.h"
#include "platform/files.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int mainui_saves_test_fail = -1;

static bool regular(const char *path)
{
    struct stat info;
    return !stat(path, &info) && S_ISREG(info.st_mode);
}

/* 1 a regular file, 0 surely absent, -1 unknown: a read error (EIO, access
 * refused) or something else in its place. Only 0 may remove a target or
 * leave Auto without its backup. */
static int presence(const char *path)
{
    struct stat info;
    if (!stat(path, &info)) {
        return S_ISREG(info.st_mode) ? 1 : -1;
    }
    return errno == ENOENT || errno == ENOTDIR ? 0 : -1;
}

/* key = "value" or key = value on its own line, as Onion's get_info_value()
 * reads .game_config and .info files: the first line naming the key. */
static bool value_of(const char *text, const char *key, char *out, size_t size)
{
    size_t length = strlen(key);
    for (const char *line = text; line && *line;) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        const char *end = strchr(line, '\n');
        if (!strncmp(p, key, length) && !isalnum((unsigned char)p[length]) && p[length] != '_') {
            p += length;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == '=') {
                p++;
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                bool quoted = *p == '"';
                p += quoted;
                size_t n = 0;
                while (p[n] && p[n] != '\n' && p[n] != '\r' && !(quoted && p[n] == '"')) {
                    n++;
                }
                while (!quoted && n && (p[n - 1] == ' ' || p[n - 1] == '\t')) {
                    n--;
                }
                if (n && n < size) {
                    memcpy(out, p, n);
                    out[n] = 0;
                    return true;
                }
                return false;
            }
        }
        line = end ? end + 1 : NULL;
    }
    return false;
}

/* A core's file name: letters, digits, '_', '-' and '.' only, so it can
 * never leave the cores folder. */
static bool core_name_ok(const char *core)
{
    if (!*core || !strcmp(core, ".") || !strcmp(core, "..")) {
        return false;
    }
    for (const char *p = core; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
            return false;
        }
    }
    return true;
}

/* The launcher's core, as Game List Options reads it: a default_core= line
 * (its value plus _libretro), else the first .retroarch/cores/<core>.so. */
static bool launcher_core(const char *text, char core[256])
{
    const char *line = strstr(text, "default_core=");
    if (line) {
        line += strlen("default_core=");
        while (*line == ' ' || *line == '"' || *line == '\'') {
            line++;
        }
        size_t n = 0;
        while (line[n] && !strchr(" \t\r\n\"'", line[n])) {
            n++;
        }
        if (n && n + strlen("_libretro") < 256) {
            memcpy(core, line, n);
            strcpy(core + n, "_libretro");
            return core_name_ok(core);
        }
        return false;
    }
    const char *mark = strstr(text, ".retroarch/cores/");
    if (!mark) {
        return false;
    }
    mark += strlen(".retroarch/cores/");
    const char *so = strstr(mark, ".so");
    size_t n = so ? (size_t)(so - mark) : 0;
    if (!n || n >= 256 || memchr(mark, '\n', n)) {
        return false;
    }
    memcpy(core, mark, n);
    core[n] = 0;
    return core_name_ok(core);
}

/* An override's core, used only when its .so is there, as runtime.sh
 * launches it: otherwise the launcher's own core runs. */
static bool core_present(const char *sd, const char *core, char *path)
{
    int n = snprintf(path, 4096, "%s/RetroArch/.retroarch/cores/%s.so", sd, core);
    return *core && n > 0 && n < 4096 && regular(path);
}

/* RedOnion's core for all of a system's games, chosen in Game List Options:
 * the first line, spaces removed, of .tmp_update/config/system_cores/<key>,
 * key the launcher's Emu folder below the SD card with '/' as '-' (Emu-GBA
 * for Emu/GBA/launch.sh, also for Emu/GBA/../../x/proxy.sh). Stock Onion has
 * none. */
static bool system_core(const char *sd, const char *launch, char core[256], char *path)
{
    size_t root = strlen(sd);
    if (strncmp(launch, sd, root) || launch[root] != '/') {
        return false;
    }
    const char *folder = launch + root + 1;
    const char *end = strstr(folder, "/../");
    if (!end) {
        end = strrchr(folder, '/');
    }
    if (!end || end == folder) {
        return false;
    }
    int n = snprintf(path, 4096, "%s/.tmp_update/config/system_cores/%.*s", sd, (int)(end - folder),
                     folder);
    if (n <= 0 || n >= 4096) {
        return false;
    }
    for (char *p = path + n - (int)(end - folder); *p; p++) {
        *p = *p == '/' ? '-' : *p;
    }
    char *text = mainui_read_text(path, 4096);
    size_t used = 0;
    for (const char *p = text; p && *p && *p != '\n' && used < 255; p++) {
        if (*p != ' ' && *p != '\r') {
            core[used++] = *p;
        }
    }
    core[used] = 0;
    free(text);
    return core_name_ok(core);
}

static int slot_of(const char *file, const char *stem);

/* Whether folder holds any state of the game, shown or not: a numbered one,
 * Auto, or the Previous Session's backup. */
static bool has_states(const char *folder, const char *stem)
{
    DIR *dir = opendir(folder);
    if (!dir) {
        return false;
    }
    size_t length = strlen(stem);
    bool any = false;
    struct dirent *item;
    while (!any && (item = readdir(dir)) != NULL) {
        any = slot_of(item->d_name, stem) >= 0 ||
              (!strncmp(item->d_name, stem, length) &&
               (!strcmp(item->d_name + length, ".state.auto") ||
                !strcmp(item->d_name + length, MAINUI_SAVES_BACKUP)));
    }
    closedir(dir);
    return any;
}

bool mainui_saves_locate(MainUISaves *saves, const char *sd, const char *launch, const char *rom)
{
    saves->count = 0;
    saves->directory[0] = saves->stem[0] = 0;
    if (!sd || !*sd || !launch || !*launch || !rom || !*rom) {
        return false;
    }
    const char *name = strrchr(rom, '/');
    name = name ? name + 1 : rom;
    size_t length = strlen(name);
    if (!length || length >= sizeof saves->stem) {
        return false;
    }
    char stem[1024];
    memcpy(stem, name, length + 1);
    char *dot = strrchr(stem, '.');
    if (dot && dot != stem) {
        *dot = 0;
    }
    char *path = malloc(4096);
    char *text = NULL;
    char core[256] = "", corename[256] = "";
    bool found = false;
    if (!path) {
        return false;
    }
    /* As runtime.sh chooses: the ROM's own core, chosen in Game List
     * Options, then RedOnion's for the system, each only when its .so is
     * there, else the launcher's. Its file is named as Onion names it:
     * basename with the lowercased extension as suffix, so Game.gba uses
     * Game.cfg but Game.GBA keeps Game.GBA.cfg. The states keep stem. */
    char config[1024];
    memcpy(config, name, length + 1);
    char *suffix = strrchr(config, '.');
    if (suffix && suffix != config) {
        bool lower = true;
        for (const char *c = suffix + 1; *c; c++) {
            lower = lower && !isupper((unsigned char)*c);
        }
        if (lower) {
            *suffix = 0;
        }
    }
    int n = snprintf(path, 4096, "%.*s/.game_config/%s.cfg", (int)(name - rom - (name > rom)), rom,
                     config);
    if (n > 0 && n < 4096 && (text = mainui_read_text(path, 64 * 1024)) != NULL) {
        if (!value_of(text, "core", core, sizeof core) || !core_name_ok(core)) {
            core[0] = 0;
        }
        free(text);
    }
    if (*core && !core_present(sd, core, path)) {
        core[0] = 0;
    }
    if (!*core && (!system_core(sd, launch, core, path) || !core_present(sd, core, path))) {
        core[0] = 0;
    }
    if (!*core && (text = mainui_read_text(launch, 256 * 1024)) != NULL) {
        if (!launcher_core(text, core)) {
            core[0] = 0;
        }
        free(text);
    }
    n = snprintf(path, 4096, "%s/RetroArch/.retroarch/cores/%s.info", sd, core);
    if (*core && n > 0 && n < 4096 && (text = mainui_read_text(path, 256 * 1024)) != NULL) {
        found = value_of(text, "corename", corename, sizeof corename) && !strchr(corename, '/') &&
                strcmp(corename, ".") && strcmp(corename, "..");
        free(text);
    }
    free(path);
    if (!found) {
        return false;
    }
    n = snprintf(saves->directory, sizeof saves->directory, "%s/Saves/CurrentProfile/states/%s", sd,
                 corename);
    if (n < 0 || (size_t)n >= sizeof saves->directory) {
        saves->directory[0] = 0;
        return false;
    }
    strcpy(saves->stem, stem);
    /* A few cores' .info corename is not the folder RetroArch saves their
     * states in. Only when the corename's folder has no state of this game
     * at all, its known alias's folder is used; no other folder is read. */
    static const char *const aliases[][2] = {{"Beetle Supafaust", "Supafaust"}};
    for (size_t i = 0; i < sizeof aliases / sizeof *aliases; i++) {
        if (strcmp(corename, aliases[i][0]) || has_states(saves->directory, stem)) {
            continue;
        }
        char *other = malloc(sizeof saves->directory);
        n = other ? snprintf(other, sizeof saves->directory, "%s/Saves/CurrentProfile/states/%s",
                             sd, aliases[i][1])
                  : -1;
        if (n > 0 && (size_t)n < sizeof saves->directory && has_states(other, stem)) {
            strcpy(saves->directory, other);
        }
        free(other);
        break;
    }
    return true;
}

/* <stem>.state is slot 0, <stem>.stateN slot N (1 and up, no leading 0);
 * anything else, Auto and the pictures included, is no slot: -1. */
static int slot_of(const char *file, const char *stem)
{
    size_t length = strlen(stem);
    if (strncmp(file, stem, length) || strncmp(file + length, ".state", 6)) {
        return -1;
    }
    const char *digits = file + length + 6;
    if (!*digits) {
        return 0;
    }
    if (*digits == '0') {
        return -1;
    }
    long value = 0;
    for (const char *p = digits; *p; p++) {
        if (!isdigit((unsigned char)*p) || value > 100000) {
            return -1;
        }
        value = value * 10 + (*p - '0');
    }
    return (int)value;
}

static bool set_entry(const MainUISaves *saves, MainUISave *entry, int slot, const char *suffix)
{
    int n = snprintf(entry->state, sizeof entry->state, "%s/%s%s", saves->directory, saves->stem,
                     suffix);
    if (n < 0 || (size_t)n >= sizeof entry->state - 4 || !regular(entry->state)) {
        return false;
    }
    char shot[4100];
    snprintf(shot, sizeof shot, "%s.png", entry->state);
    entry->slot = slot;
    entry->shot = regular(shot);
    return true;
}

int mainui_saves_list(MainUISaves *saves, bool previous, bool autosave)
{
    saves->count = 0;
    DIR *dir = *saves->directory ? opendir(saves->directory) : NULL;
    if (!dir) {
        return 0;
    }
    /* The highest slots, kept in order as the folder is read. */
    int slots[MAINUI_SAVES_SHOWN], count = 0;
    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
        int slot = slot_of(item->d_name, saves->stem);
        if (slot < 0) {
            continue;
        }
        int where = count;
        while (where > 0 && slots[where - 1] < slot) {
            where--;
        }
        if (where >= MAINUI_SAVES_SHOWN) {
            continue;
        }
        count += count < MAINUI_SAVES_SHOWN;
        for (int i = count - 1; i > where; i--) {
            slots[i] = slots[i - 1];
        }
        slots[where] = slot;
    }
    closedir(dir);
    /* Previous Session is the leftmost entry. Numbered slots follow in
     * ascending order, with Auto Save rightmost when enabled. This is
     * deterministic even on devices with an unset system clock. */
    if (previous &&
        set_entry(saves, &saves->saves[saves->count], MAINUI_SAVES_PREVIOUS, MAINUI_SAVES_BACKUP)) {
        saves->count++;
    }
    /* Select the ten highest slot numbers, then display them ascending. */
    for (int i = count - 1; i >= 0; i--) {
        char suffix[32];
        snprintf(suffix, sizeof suffix, slots[i] ? ".state%d" : ".state", slots[i]);
        saves->count += set_entry(saves, &saves->saves[saves->count], slots[i], suffix);
    }
    if (autosave &&
        set_entry(saves, &saves->saves[saves->count], MAINUI_SAVES_AUTO, ".state.auto")) {
        saves->count++;
    }
    return saves->count;
}

/* --- the resume: Auto kept in its backup, then replaced; each file
 * written in full beside its target, then renamed onto it. */

/* Room for the new files' paths, on the heap: the stack stays small. */
typedef struct {
    int count;

    struct {
        char target[4100];
        char source[4100]; /* copied onto target; empty string removes target */
        bool optional;     /* a missing screenshot removes its old counterpart */
        bool copied;       /* staged: a new copy to rename onto target, else removal */
        bool picture;      /* a state's screenshot, not the state itself */
    } step[6];

    char next[4200], autosave[4100], backup[4100];
    int autosave_present; /* presence() of Auto when planned */
} Plan;

static void add(Plan *plan, const char *source, const char *target, bool optional)
{
    snprintf(plan->step[plan->count].source, sizeof plan->step[0].source, "%s", source);
    snprintf(plan->step[plan->count].target, sizeof plan->step[0].target, "%s", target);
    plan->step[plan->count].optional = optional;
    plan->step[plan->count].picture = false;
    plan->count++;
}

/* Target's state from source's, with its picture: copied when source has
 * one, else target's removed. */
static void add_state(Plan *plan, const char *source, const char *target)
{
    add(plan, source, target, !*source);
    char *from = plan->step[plan->count].source, *to = plan->step[plan->count].target;
    if (*source) {
        snprintf(from, sizeof plan->step[0].source, "%s.png", source);
    }
    else {
        *from = 0; /* explicit remove: never read a relative ".png" file */
    }
    snprintf(to, sizeof plan->step[0].target, "%s.png", target);
    plan->step[plan->count].optional = true;
    plan->step[plan->count].picture = true;
    plan->count++;
}

static bool staged(char out[4200], const char *target)
{
    int n = snprintf(out, 4200, "%s.mainui-new", target);
    return n > 0 && n < 4200;
}

/* Copy source to path in full and flush it: the copy is complete or false. */
static bool copy_file(const char *source, const char *path)
{
    FILE *in = fopen(source, "rb");
    FILE *out = in ? fopen(path, "wb") : NULL;
    char *buffer = out ? malloc(64 * 1024) : NULL;
    bool ok = buffer != NULL;
    while (ok) {
        size_t got = fread(buffer, 1, 64 * 1024, in);
        if (got && fwrite(buffer, 1, got, out) != got) {
            ok = false;
        }
        if (got < 64 * 1024) {
            ok = ok && !ferror(in);
            break;
        }
    }
    free(buffer);
    if (out) {
        ok = !fflush(out) && !fsync(fileno(out)) && ok;
        ok = !fclose(out) && ok;
    }
    if (in) {
        fclose(in);
    }
    return ok;
}

/* Every new file written in full beside its target, then each renamed onto
 * it, or the target removed: a target is always whole, old or new. False
 * when a file cannot be written, nothing changed then, or renamed, what
 * was not renamed left beside its target as <target>.mainui-new. One left
 * so before refuses it all, naming the file: it may be the only copy of
 * the Auto before, never to be overwritten; so does one the card cannot
 * confirm absent. */
static bool apply(Plan *plan, char error[256])
{
    char *next = plan->next;
    for (int i = 0; i < plan->count; i++) {
        struct stat info;
        if (!staged(next, plan->step[i].target) || !lstat(next, &info)) {
            const char *name = strrchr(next, '/');
            snprintf(error, 256, "Restore or remove %.200s first.", name ? name + 1 : next);
            return false;
        }
        /* Only "not found" lets it go on: a card that cannot say (EIO,
         * say) may hold that only copy. */
        if (errno != ENOENT) {
            snprintf(error, 256, "Cannot check the save files. Try again.");
            return false;
        }
    }
    for (int i = 0; i < plan->count; i++) {
        staged(next, plan->step[i].target);
        int exists = *plan->step[i].source ? presence(plan->step[i].source) : 0;
        /* A vanished required state must never turn into an instruction to
         * DELETE its destination, nor may one that cannot be read. Only
         * surely absent screenshots (or an explicit undo-remove step) are
         * allowed to remove a file. */
        plan->step[i].copied = exists > 0;
        if (exists < 0 || (!exists && !plan->step[i].optional) ||
            (exists > 0 && !copy_file(plan->step[i].source, next))) {
            for (int k = 0; k <= i; k++) {
                if (staged(next, plan->step[k].target)) {
                    unlink(next);
                }
            }
            snprintf(error, 256,
                     exists > 0   ? "Cannot write the save state."
                     : exists < 0 ? "The save state cannot be read."
                                  : "The save state is no longer available.");
            return false;
        }
    }
    for (int i = 0; i < plan->count; i++) {
        staged(next, plan->step[i].target);
        bool ok = mainui_saves_test_fail-- != 0; /* tests: as if the card refused */
        /* As staged, not as stat says now: a read error never turns a new
         * copy into the removal of its target. */
        if (ok && plan->step[i].copied) {
            ok = !rename(next, plan->step[i].target);
        }
        else if (ok) {
            ok = !unlink(plan->step[i].target) || errno == ENOENT;
        }
        if (!ok && plan->step[i].picture) {
            /* A screenshot the card refused: the states are what matter,
             * so the change goes on and is not reported as failed, which a
             * retry would repeat onto the backup. Its picture may be stale. */
            if (plan->step[i].copied) {
                unlink(next);
            }
            fprintf(stderr, "[saves] cannot update %.300s\n", plan->step[i].target);
            continue;
        }
        if (!ok) {
            snprintf(error, 256, "Cannot replace the save state.");
            return false;
        }
    }
    mainui_sync_parent(plan->step[0].target);
    return true;
}

/* The plan: what resume puts in Auto (and the backup or the slot), or what
 * undo puts back. */
static bool plan_for(const MainUISaves *saves, int index, int action, Plan *plan)
{
    char *autopath = plan->autosave, *backpath = plan->backup;
    int a = snprintf(autopath, sizeof plan->autosave, "%s/%s.state.auto", saves->directory,
                     saves->stem);
    int b = snprintf(backpath, sizeof plan->backup, "%s/%s%s", saves->directory, saves->stem,
                     MAINUI_SAVES_BACKUP);
    if (a < 0 || b < 0 || (size_t)a >= sizeof plan->autosave - 8 ||
        (size_t)b >= sizeof plan->backup - 8 || index < 0 || index >= saves->count) {
        return false;
    }
    const char *slot = saves->saves[index].state;
    plan->count = 0;
    /* Auto unknown, say after a read error: neither backed up nor replaced. */
    plan->autosave_present = presence(autopath);
    if (plan->autosave_present < 0) {
        return false;
    }
    switch (action) {
    case MAINUI_SAVES_UNDO_EXCHANGE: /* Auto and the backup, Auto first */
        add_state(plan, backpath, autopath);
        add_state(plan, autopath, backpath);
        break;
    case MAINUI_SAVES_UNDO_SWAP: /* Auto and the slot, Auto first */
        add_state(plan, slot, autopath);
        add_state(plan, autopath, slot);
        break;
    case MAINUI_SAVES_UNDO_BACKUP:
        add_state(plan, backpath, autopath);
        break;
    case MAINUI_SAVES_UNDO_REMOVE:
        add_state(plan, "", autopath);
        break;
    default: /* the backup mode's resume: Auto into the backup first */
        if (plan->autosave_present) {
            add_state(plan, autopath, backpath);
        }
        /* fall through */
    case MAINUI_SAVES_UNDO_KEPT: /* overwrite */
        add_state(plan, slot, autopath);
        break;
    }
    return true;
}

bool mainui_saves_resume(const MainUISaves *saves, int index, int mode, int *undo, char error[256])
{
    error[0] = 0;
    *undo = MAINUI_SAVES_UNDO_NONE;
    if (!saves || index < 0 || index >= saves->count) {
        snprintf(error, 256, "No save state chosen.");
        return false;
    }
    const MainUISave *chosen = &saves->saves[index];
    if (!regular(chosen->state)) {
        snprintf(error, 256, "The save state cannot be read.");
        return false;
    }
    /* Auto is resumed as it is: nothing to change. */
    if (chosen->slot == MAINUI_SAVES_AUTO) {
        return true;
    }
    Plan *plan = malloc(sizeof *plan);
    int action = -1;
    if (chosen->slot == MAINUI_SAVES_PREVIOUS) {
        action = MAINUI_SAVES_UNDO_EXCHANGE;
    }
    else if (mode == MAINUI_RESUME_SWAP || mode == MAINUI_RESUME_OVERWRITE) {
        action = mode == MAINUI_RESUME_SWAP ? MAINUI_SAVES_UNDO_SWAP : MAINUI_SAVES_UNDO_KEPT;
    }
    bool ok = plan && plan_for(saves, index, action, plan);
    if (ok) {
        /* Without an Auto before, undo removes the new one. */
        int then = action;
        if (!plan->autosave_present) {
            then = MAINUI_SAVES_UNDO_REMOVE;
            if (action == MAINUI_SAVES_UNDO_SWAP || action == MAINUI_SAVES_UNDO_EXCHANGE) {
                /* With no Auto to exchange, copy the selected state or
                 * Previous Session into Auto. Keep its source intact. */
                plan->count = 2;
            }
        }
        else if (action < 0) {
            then = MAINUI_SAVES_UNDO_BACKUP;
        }
        ok = apply(plan, error);
        *undo = ok ? then : MAINUI_SAVES_UNDO_NONE;
    }
    else if (!*error) {
        snprintf(error, 256, plan ? "The save state cannot be read." : "Not enough memory.");
    }
    free(plan);
    return ok;
}

bool mainui_saves_undo(const MainUISaves *saves, int index, int undo, char error[256])
{
    error[0] = 0;
    if (!saves || undo == MAINUI_SAVES_UNDO_NONE || undo == MAINUI_SAVES_UNDO_KEPT) {
        return true;
    }
    Plan *plan = malloc(sizeof *plan);
    bool ok = plan && plan_for(saves, index, undo, plan) && apply(plan, error);
    if (!ok && !*error) {
        snprintf(error, 256, "Not enough memory.");
    }
    free(plan);
    return ok;
}
