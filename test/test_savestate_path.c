/**
 * @file test_savestate_path.c
 * @brief Unit tests for createSaveStatePath() from gs_popMenu.h
 *
 * Tests the save state file path construction logic used by the
 * game switcher's popup menu. The function builds paths based on:
 * - core_name: RetroArch core directory name
 * - rom_name: ROM filename (no extension)  
 * - slot: Save state slot number (-1=auto, 0=default, 1-9=numbered)
 *
 * Build and run: make -f Makefile.unit test_savestate_path
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STR_MAX 256
#define STATES_DIR "/mnt/SDCARD/Saves/CurrentProfile/states"

/* ---- Minimal Game_s for tests ---- */

typedef struct {
    char core_name[STR_MAX * 2];
    char rom_name[STR_MAX * 2];
} TestGame;

/* ---- Inline the function under test ---- */

static bool createSaveStatePath(TestGame *game, int slot, char *out_path, size_t out_path_size)
{
    if (strlen(game->core_name) == 0) {
        return false;
    }

    if (slot == -1) {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state.auto",
                 game->core_name, game->rom_name);
    }
    else if (slot == 0) {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state",
                 game->core_name, game->rom_name);
    }
    else {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state%d",
                 game->core_name, game->rom_name, slot);
    }

    return true;
}

/* ---- Helpers ---- */

static TestGame make_game(const char *core, const char *rom)
{
    TestGame g;
    strncpy(g.core_name, core, sizeof(g.core_name) - 1);
    g.core_name[sizeof(g.core_name) - 1] = '\0';
    strncpy(g.rom_name, rom, sizeof(g.rom_name) - 1);
    g.rom_name[sizeof(g.rom_name) - 1] = '\0';
    return g;
}

/* ==== Tests: auto save state (slot -1) ==== */

TEST(savestate_auto_slot) {
    TestGame g = make_game("mgba", "Pokemon Fire Red");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, -1, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/mgba/Pokemon Fire Red.state.auto");
}

TEST(savestate_auto_slot_different_core) {
    TestGame g = make_game("snes9x2005_plus", "Super Mario World");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, -1, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/snes9x2005_plus/Super Mario World.state.auto");
}

/* ==== Tests: default slot (slot 0) ==== */

TEST(savestate_default_slot) {
    TestGame g = make_game("mgba", "Pokemon Fire Red");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 0, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/mgba/Pokemon Fire Red.state");
}

TEST(savestate_default_slot_nes) {
    TestGame g = make_game("fceumm", "Mega Man 2");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 0, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/fceumm/Mega Man 2.state");
}

/* ==== Tests: numbered slots (1-9) ==== */

TEST(savestate_slot_1) {
    TestGame g = make_game("mgba", "Pokemon Fire Red");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 1, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/mgba/Pokemon Fire Red.state1");
}

TEST(savestate_slot_5) {
    TestGame g = make_game("snes9x2005_plus", "Super Mario World");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 5, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/snes9x2005_plus/Super Mario World.state5");
}

TEST(savestate_slot_9) {
    TestGame g = make_game("mgba", "Game");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 9, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/mgba/Game.state9");
}

/* ==== Tests: empty core_name returns false ==== */

TEST(savestate_empty_core_returns_false) {
    TestGame g = make_game("", "Some Game");
    char path[2048];
    path[0] = '\0';
    ASSERT_FALSE(createSaveStatePath(&g, 0, path, sizeof(path)));
}

TEST(savestate_empty_core_auto_returns_false) {
    TestGame g = make_game("", "Some Game");
    char path[2048];
    ASSERT_FALSE(createSaveStatePath(&g, -1, path, sizeof(path)));
}

TEST(savestate_empty_core_numbered_returns_false) {
    TestGame g = make_game("", "Some Game");
    char path[2048];
    ASSERT_FALSE(createSaveStatePath(&g, 3, path, sizeof(path)));
}

/* ==== Tests: special characters in ROM names ==== */

TEST(savestate_rom_with_parentheses) {
    TestGame g = make_game("mgba", "Pokemon - Fire Red (USA)");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 1, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/mgba/Pokemon - Fire Red (USA).state1");
}

TEST(savestate_rom_with_brackets) {
    TestGame g = make_game("fceumm", "[BIOS] NES");
    char path[2048];
    ASSERT_TRUE(createSaveStatePath(&g, 0, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/fceumm/[BIOS] NES.state");
}

/* ==== Tests: all slots produce unique paths ==== */

TEST(savestate_all_slots_unique) {
    TestGame g = make_game("mgba", "TestRom");
    char paths[11][2048];

    createSaveStatePath(&g, -1, paths[0], sizeof(paths[0]));
    for (int i = 0; i <= 9; i++) {
        createSaveStatePath(&g, i, paths[i + 1], sizeof(paths[i + 1]));
    }

    /* Verify all 11 paths are unique */
    for (int i = 0; i < 11; i++) {
        for (int j = i + 1; j < 11; j++) {
            ASSERT_STRNE(paths[i], paths[j]);
        }
    }
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_popMenu.h createSaveStatePath Unit Tests ===\n\n");

    /* Auto save state */
    RUN_TEST(savestate_auto_slot);
    RUN_TEST(savestate_auto_slot_different_core);

    /* Default slot */
    RUN_TEST(savestate_default_slot);
    RUN_TEST(savestate_default_slot_nes);

    /* Numbered slots */
    RUN_TEST(savestate_slot_1);
    RUN_TEST(savestate_slot_5);
    RUN_TEST(savestate_slot_9);

    /* Empty core */
    RUN_TEST(savestate_empty_core_returns_false);
    RUN_TEST(savestate_empty_core_auto_returns_false);
    RUN_TEST(savestate_empty_core_numbered_returns_false);

    /* Special characters */
    RUN_TEST(savestate_rom_with_parentheses);
    RUN_TEST(savestate_rom_with_brackets);

    /* Uniqueness */
    RUN_TEST(savestate_all_slots_unique);

    TEST_REPORT();
    return test_failures;
}
