#ifndef TWEAKS_DIAGS_STATUS_H__
#define TWEAKS_DIAGS_STATUS_H__

// Tweaks > Advanced > Diagnostics without SDL, so host tests run it: the
// script list and the status line of each script.
//
// A script runs in a thread. That thread used to write its status straight
// into the script's menu item, and again 5 s later to say "Idle"; Tweaks
// rebuilds its menus after many changes (a network toggle, the blue light
// schedule starting...), which frees the items, and the thread then wrote
// into freed memory. Threads now only write here, under a lock, and the
// main loop copies each status into the item of the same script, whichever
// menu holds it.

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/str.h"

#ifndef DIAG_SCRIPT_PATH
#define DIAG_SCRIPT_PATH "/mnt/SDCARD/.tmp_update/script/diagnostics"
#endif
#define DIAG_MAX_LABEL_LENGTH 64
#define DIAG_MAX_FILENAME_LENGTH 64
#define DIAG_MAX_PATH_LENGTH (strlen(DIAG_SCRIPT_PATH) + DIAG_MAX_FILENAME_LENGTH + 2)
#define DIAG_STATUS_IDLE "Idle: Selected script not running"
#define DIAG_STATUS_MAX 64

typedef struct {
    char filename[DIAG_MAX_FILENAME_LENGTH]; // store the filename/path so we can call it later as the payload
    char label[DIAG_MAX_LABEL_LENGTH];
    char tooltip[STR_MAX];
} diagScripts;

// Read the menulabel="..." and tooltip="..." lines of a script, up to a
// "# IGNORE" line. True when it has both (it is listed then).
static bool diags_parseScript(FILE *file, const char *filename, diagScripts *entry)
{
    char line[STR_MAX];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "# IGNORE") == 0)
            break;
        // Widths leave room for the '\0': %64 into the 64-byte label wrote
        // it onto the tooltip (a script whose tooltip came first vanished
        // from the menu); the tooltip's %256 had the same flaw.
        if (sscanf(line, "menulabel=\"%63[^\"]\"", entry->label) == 1)
            continue;
        if (sscanf(line, "tooltip=\"%255[^\"]\"", entry->tooltip) == 1)
            continue;
    }
    return entry->label[0] && entry->tooltip[0];
}

typedef struct {
    char filename[DIAG_MAX_FILENAME_LENGTH];
    char note[STR_MAX];
    unsigned seq; // of its latest change
} DiagStatus;

static pthread_mutex_t diags_status_lock = PTHREAD_MUTEX_INITIALIZER;
static DiagStatus diags_status[DIAG_STATUS_MAX];
static int diags_status_count = 0;
static unsigned diags_status_seq = 0;
static char diags_running[DIAG_MAX_FILENAME_LENGTH] = "";

// Call with the lock held.
static DiagStatus *_diags_statusOf(const char *filename, bool create)
{
    for (int i = 0; i < diags_status_count; i++)
        if (strcmp(diags_status[i].filename, filename) == 0)
            return &diags_status[i];
    if (!create || diags_status_count >= DIAG_STATUS_MAX)
        return NULL;
    DiagStatus *s = &diags_status[diags_status_count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->filename, filename, sizeof(s->filename) - 1);
    return s;
}

// Set the status of a script; returns the change's number (for
// diags_setStatusIf()).
static unsigned diags_setStatus(const char *filename, const char *note)
{
    pthread_mutex_lock(&diags_status_lock);
    unsigned seq = ++diags_status_seq;
    DiagStatus *s = _diags_statusOf(filename, true);
    if (s != NULL) {
        strncpy(s->note, note, sizeof(s->note) - 1);
        s->note[sizeof(s->note) - 1] = '\0';
        s->seq = seq;
    }
    pthread_mutex_unlock(&diags_status_lock);
    return seq;
}

// Set it only if nothing changed it since change seq: the "Idle" of a run
// that ended must not hide the status of a newer one.
static void diags_setStatusIf(const char *filename, const char *note, unsigned seq)
{
    pthread_mutex_lock(&diags_status_lock);
    DiagStatus *s = _diags_statusOf(filename, false);
    if (s != NULL && s->seq == seq) {
        strncpy(s->note, note, sizeof(s->note) - 1);
        s->note[sizeof(s->note) - 1] = '\0';
        s->seq = ++diags_status_seq;
    }
    pthread_mutex_unlock(&diags_status_lock);
}

// Copy the status of a script to out (size bytes). False when it has none
// yet (its item keeps the note it was made with).
static bool diags_getStatus(const char *filename, char *out, size_t size)
{
    bool found = false;
    pthread_mutex_lock(&diags_status_lock);
    DiagStatus *s = _diags_statusOf(filename, false);
    if (s != NULL && size > 0) {
        strncpy(out, s->note, size - 1);
        out[size - 1] = '\0';
        found = true;
    }
    pthread_mutex_unlock(&diags_status_lock);
    return found;
}

// The script that is running ("" for none).
static void diags_setRunning(const char *filename)
{
    pthread_mutex_lock(&diags_status_lock);
    strncpy(diags_running, filename, sizeof(diags_running) - 1);
    diags_running[sizeof(diags_running) - 1] = '\0';
    pthread_mutex_unlock(&diags_status_lock);
}

static bool diags_isRunning(const char *filename)
{
    pthread_mutex_lock(&diags_status_lock);
    bool running = strcmp(diags_running, filename) == 0;
    pthread_mutex_unlock(&diags_status_lock);
    return running;
}

#endif // TWEAKS_DIAGS_STATUS_H__
