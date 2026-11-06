/**
 * @file test_gs_state_ratio.c
 * @brief Shape of a save state picture in the GameSwitcher's Load preview
 *        (src/gameSwitcher/gs_state_ratio.h)
 *
 * The preview takes the shape the game has on screen: the whole 4:3 screen
 * when RetroArch's "keep aspect" is off, else the console's screen by ROM
 * folder (Open MainUI 1.1.0 rc14's table plus the other 4:3 consoles and
 * computers), else the picture's own.
 *
 * Build and run: make -f Makefile.unit test_gs_state_ratio
 */

#include "onion_test.h"
#include <math.h>
#include <stdio.h>

#include "../src/gameSwitcher/gs_state_ratio.h"

#define NEAR(a, b) (fabs((a) - (b)) < 1e-9)

TEST(console_folders)
{
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/FC/Mario.nes", true), 4.0 / 3.0));
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/PS/Game.chd", true), 4.0 / 3.0));
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/GB/Tetris.gb", true), 10.0 / 9.0));
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/GG/Sonic.gg", true), 10.0 / 9.0));
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/GBA/Zelda.gba", true), 3.0 / 2.0));
    /* Case-insensitive, and a game in a subfolder of the console. */
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/gba/RPG/Game.gba", true), 3.0 / 2.0));
}

TEST(other_4_3_consoles_and_computers)
{
    // Every TV console and computer folder of RedOnion's emulators.
    const char *folders[] = {"SEGACD", "THIRTYTWOX", "SEGASGONE", "NEOGEO", "NEOCD",
                             "SGFX", "SATELLAVIEW", "SUFAMI", "COLECO", "INTELLIVISION",
                             "ODYSSEY", "VIDEOPAC", "FAIRCHILD", "ATARI", "FIFTYTWOHUNDRED",
                             "SEVENTYEIGHTHUNDRED", "MSX", "CPC", "ZXS", "COMMODORE",
                             "VIC20", "AMIGA", "DOS"};
    char path[128];
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
        snprintf(path, sizeof(path), "/mnt/SDCARD/Roms/%s/Game.zip", folders[i]);
        ASSERT_TRUE(NEAR(gs_stateRatio(path, true), 4.0 / 3.0));
    }
}

TEST(other_folders_keep_the_picture)
{
    // Arcade (vertical games), handhelds, Super Game Boy borders, Vectrex.
    const char *folders[] = {"ARCADE", "CPS1", "CPS2", "CPS3", "FBNEO", "SGB",
                             "VECTREX", "LYNX", "NGP", "WS", "PICO", "NDS"};
    char path[128];
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
        snprintf(path, sizeof(path), "/mnt/SDCARD/Roms/%s/Game.zip", folders[i]);
        ASSERT_TRUE(gs_stateRatio(path, true) == 0);
    }
    ASSERT_TRUE(gs_stateRatio("/mnt/SDCARD/Roms/GBAX/x.gba", true) == 0);
    ASSERT_TRUE(gs_stateRatio("/mnt/SDCARD/Emu/FC/x.nes", true) == 0);
    ASSERT_TRUE(gs_stateRatio("/mnt/SDCARD/Roms/FC", true) == 0);
    ASSERT_TRUE(gs_stateRatio("", true) == 0);
    ASSERT_TRUE(gs_stateRatio(NULL, true) == 0);
}

TEST(keep_aspect_off_fills_the_screen)
{
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/GB/Tetris.gb", false), 4.0 / 3.0));
    ASSERT_TRUE(NEAR(gs_stateRatio("/mnt/SDCARD/Roms/NEOGEO/mslug.zip", false), 4.0 / 3.0));
}

TEST(rom_folder)
{
    char folder[8];
    ASSERT_TRUE(gs_romFolder("/mnt/SDCARD/Roms/SFC/a.sfc", folder, sizeof(folder)));
    ASSERT_STREQ(folder, "SFC");
    ASSERT_FALSE(gs_romFolder("/mnt/SDCARD/Roms/VERYLONGNAME/a", folder, sizeof(folder)));
    ASSERT_FALSE(gs_romFolder("/mnt/SDCARD/Roms//a", folder, sizeof(folder)));
}

int main(void)
{
    printf("GameSwitcher Load preview shape\n");
    RUN_TEST(console_folders);
    RUN_TEST(other_4_3_consoles_and_computers);
    RUN_TEST(other_folders_keep_the_picture);
    RUN_TEST(keep_aspect_off_fills_the_screen);
    RUN_TEST(rom_folder);

    TEST_REPORT();
    return test_failures;
}
