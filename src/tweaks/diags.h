#ifndef TWEAKS_DIAGS_H__
#define TWEAKS_DIAGS_H__

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include "components/list.h"

#include "./diags_status.h"

static int diags_numScripts;
static diagScripts *scripts = NULL;

void diags_getEntries(void)
{
    DIR *dir;
    struct dirent *ent;
    diags_numScripts = 0;

    if ((dir = opendir(DIAG_SCRIPT_PATH)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type != DT_REG)
                continue;

            char path[PATH_MAX];
            if (snprintf(path, sizeof(path), "%s/%s", DIAG_SCRIPT_PATH, ent->d_name) >= (int)sizeof(path))
                continue;

            FILE *file = fopen(path, "r");
            if (file == NULL)
                continue;
            diagScripts entry;
            bool listed = diags_parseScript(file, ent->d_name, &entry);
            fclose(file);

            if (listed) {
                diagScripts *grown = realloc(scripts, (diags_numScripts + 1) * sizeof(diagScripts));
                if (grown == NULL) {
                    printf("Memory allocation failed...\n");
                    break;
                }
                scripts = grown;
                scripts[diags_numScripts++] = entry;
            }
        }
        closedir(dir);
    }
    else {
        printf("Could not open directory\n");
    }
}

void diags_freeEntries(void)
{
    if (scripts != NULL) {
        free(scripts);
        scripts = NULL;
    }
}

char *diags_parseNewLines(const char *input)
{ // helper function to parse /n in the scripts
    char *output = malloc(strlen(input) + 1);
    if (!output)
        return NULL;

    int j = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '\\' && input[i + 1] == 'n') {
            output[j++] = '\n';
            i++;
        }
        else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    return output;
}

#ifndef DIAG_IDLE_DELAY_S
#define DIAG_IDLE_DELAY_S 5
#endif

typedef struct {
    char filename[DIAG_MAX_FILENAME_LENGTH];
    unsigned seq;
} diagsIdleLater;

static void *diags_resetStickyNote(void *arg)
{
    diagsIdleLater *later = (diagsIdleLater *)arg;
    sleep(DIAG_IDLE_DELAY_S);
    diags_setStatusIf(later->filename, DIAG_STATUS_IDLE, later->seq);
    free(later);
    return NULL;
}

// Back to "Idle" in DIAG_IDLE_DELAY_S s, unless the status changes again meanwhile.
static void diags_idleLater(const char *filename, unsigned seq)
{
    diagsIdleLater *later = calloc(1, sizeof(*later));
    if (later == NULL)
        return;
    strncpy(later->filename, filename, sizeof(later->filename) - 1);
    later->seq = seq;

    pthread_t reset_thread;
    if (pthread_create(&reset_thread, NULL, diags_resetStickyNote, later) != 0) {
        perror("Failed to create reset_thread");
        free(later);
        return;
    }
    pthread_detach(reset_thread);
}

volatile int isScriptRunning = 0;

// Thread: arg is the script's file name, malloc'ed (freed here).
void *diags_runScript(void *arg)
{
    char *filename = (char *)arg;

    if (__sync_lock_test_and_set(&isScriptRunning, 1)) {
        if (diags_isRunning(filename))
            diags_setStatus(filename, "Script already running...");
        else
            diags_idleLater(filename, diags_setStatus(filename, "Another script is already running..."));
        free(filename);
        return NULL;
    }

    diags_setRunning(filename);
    diags_setStatus(filename, "Script running...");

    char script_path[PATH_MAX];
    snprintf(script_path, sizeof(script_path), "%s/%s", DIAG_SCRIPT_PATH, filename);

    const char *result;
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", script_path, (char *)NULL);
        _exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        int status = 0;
        int rc;
        while ((rc = waitpid(pid, &status, 0)) < 0 && errno == EINTR)
            ;
        result = rc == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0
                     ? "Script successfully completed."
                     : "Script failed!";
    }
    else {
        result = "Failed to run script...";
    }

    diags_idleLater(filename, diags_setStatus(filename, result));

    // Also when fork() failed: no script runs any more (every other
    // script used to say "Another script is already running" until
    // Tweaks was restarted).
    diags_setRunning("");
    __sync_lock_release(&isScriptRunning);
    free(filename);
    return NULL;
}

// Main loop: show the status of each script in its item. True when one
// changed (the list is redrawn).
bool diags_applyStatus(List *menu)
{
    if (!menu->_created)
        return false;
    bool changed = false;
    for (int i = 0; i < menu->item_count; i++) {
        ListItem *item = &menu->items[i];
        if (item->payload_ptr == NULL || item->action == NULL)
            continue;
        char note[STR_MAX];
        if (diags_getStatus((const char *)item->payload_ptr, note, sizeof(note)) &&
            strcmp(note, item->sticky_note) != 0) {
            list_updateStickyNote(item, note);
            changed = true;
        }
    }
    return changed;
}

#endif
