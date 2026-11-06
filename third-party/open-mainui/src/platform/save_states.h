/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_SAVE_STATES_H
#define MAINUI_SAVE_STATES_H
#include <stdbool.h>

/* A game's RetroArch save states, as Onion keeps them:
 * <sd>/Saves/CurrentProfile/states/<core name>/<ROM name>.state,
 * <ROM name>.state1, .state2 ... and .state.auto, each with an optional .png
 * beside it. The core is the one runtime.sh launches: the ROM's
 * .game_config/<ROM name>.cfg core, else RedOnion's for the system
 * (.tmp_update/config/system_cores), each when its .so is there, else the
 * launch.sh default_core or its .retroarch/cores/<core>.so; its name is the
 * corename of that core's .info, or for a few cores whose states RetroArch
 * keeps under another name (Beetle Supafaust's in Supafaust), that name
 * when the corename's folder holds no state of the game.
 * All paths here are host paths. */

#define MAINUI_SAVES_SHOWN 10      /* the highest numbered states listed */
#define MAINUI_SAVES_PREVIOUS (-1) /* the slot of the Previous Session entry */
#define MAINUI_SAVES_AUTO (-2)     /* the slot of the Auto Save entry */
#define MAINUI_SAVES_BACKUP ".state.auto.mainui-backup"

/* game-menu.json's saved_games resume_mode: what becomes of Auto. */
enum {
    MAINUI_RESUME_BACKUP, /* the default: Auto kept in one backup per game */
    MAINUI_RESUME_OVERWRITE,
    MAINUI_RESUME_SWAP
};

typedef struct {
    int slot;         /* 0 for .state, N for .stateN, or MAINUI_SAVES_AUTO/PREVIOUS */
    char state[4096]; /* the state file */
    bool shot;        /* state + ".png" exists */
} MainUISave;

typedef struct {
    char directory[4096]; /* the core's states folder */
    char stem[1024];      /* the ROM file's name without its extension */
    int count;            /* entries: Previous Session, slots ascending, then Auto Save */
    MainUISave saves[MAINUI_SAVES_SHOWN + 2];
} MainUISaves;

/* The game's states folder and ROM name: false when the game is not a
 * RetroArch one MainUI can tell the core of. Reads the ROM's .game_config,
 * the launcher and the core's .info; nothing else. */
bool mainui_saves_locate(MainUISaves *saves, const char *sd, const char *launch, const char *rom);
/* After mainui_saves_locate(): list Previous Session (the backup of Auto)
 * when requested and available, then the ten highest numbered states in
 * ascending slot order, followed by Auto Save (.state.auto) when requested.
 * Neither ordering nor selection depends on file timestamps.
 * Reads only that folder; never writes. Returns count. */
int mainui_saves_list(MainUISaves *saves, bool previous, bool autosave);

/* What puts Auto back after a resume whose game did not launch. */
enum {
    MAINUI_SAVES_UNDO_NONE,     /* nothing was changed */
    MAINUI_SAVES_UNDO_BACKUP,   /* the backup goes back into Auto */
    MAINUI_SAVES_UNDO_REMOVE,   /* there was no Auto: it goes */
    MAINUI_SAVES_UNDO_EXCHANGE, /* Auto and the backup exchange again */
    MAINUI_SAVES_UNDO_SWAP,     /* Auto and the slot exchange again */
    MAINUI_SAVES_UNDO_KEPT      /* overwrite replaced Auto: nothing to put back */
};

/* Put entry index into <ROM name>.state.auto, with its picture, as mode
 * says: backup first puts Auto, when there is one, into the game's one
 * backup, <ROM name>.state.auto.mainui-backup, and its Previous Session
 * entry exchanges Auto and the backup; overwrite replaces Auto; swap
 * exchanges Auto and the state (only copies it without an Auto), the only
 * mode that writes a numbered state. The Auto Save entry changes nothing.
 * Each file is written in full beside its target as <target>.mainui-new,
 * then renamed onto it, so each is whole, old or new. *undo says what puts
 * Auto back (mainui_saves_undo()). False, with error set, when a file
 * cannot be read or written, nothing then changed; when one cannot be
 * renamed, what was not stays as <target>.mainui-new, possibly the only
 * copy of the Auto before; and while any such file is there, naming it,
 * so it is never overwritten. */
bool mainui_saves_resume(const MainUISaves *saves, int index, int mode, int *undo, char error[256]);
/* Put Auto back as undo says, after resuming entry index when the game did
 * not launch. False, with error set, when it cannot: the earlier Auto is
 * then in the backup, the slot, or beside its target as
 * <target>.mainui-new, to restore by hand. */
bool mainui_saves_undo(const MainUISaves *saves, int index, int undo, char error[256]);
/* Tests only: refuse this rename or removal, as the card might; negative
 * never does. */
extern int mainui_saves_test_fail;
#endif
