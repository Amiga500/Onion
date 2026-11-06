/**
 * @file test_gs_game_name.c
 * @brief The GameSwitcher's game name never ends up empty
 *
 * A name made only of tags ("(Hack) [T+Ita]") cleaned to an empty string,
 * SDL_ttf drew no surface for it and the name bar dereferenced NULL. The
 * cleaning is the real file_cleanName() (utils/file.c); the fallback is
 * gs_fillShortName() from src/gameSwitcher/gs_game_name.h.
 *
 * Build and run: make -f Makefile.unit test_gs_game_name
 */

#include "onion_test.h"
#include <string.h>

#include "utils/file.h"
#include "utils/str.h"

#include "../src/gameSwitcher/gs_game_name.h"

// What processItemMetaWork() does with a name and a ROM file name.
static void shown_name(char *out, size_t size, const char *name, const char *rom_name, const char *label)
{
    char cleaned[STR_MAX * 2];
    file_cleanName(cleaned, name);
    snprintf(out, size, "%s", cleaned);
    gs_fillShortName(out, size, name, rom_name, label);
}

TEST(cleaned_name_kept)
{
    char out[STR_MAX * 2];
    shown_name(out, sizeof(out), "Super Mario Advance (USA)", "Super Mario Advance (USA)", "x");
    ASSERT_STREQ(out, "Super Mario Advance");
}

TEST(tags_only_name_falls_back_to_full_name)
{
    char out[STR_MAX * 2];
    shown_name(out, sizeof(out), "(Hack) [T+Ita]", "(Hack) [T+Ita]", "label");
    ASSERT_STREQ(out, "(Hack) [T+Ita]");
}

TEST(bios_like_name_not_empty)
{
    char out[STR_MAX * 2];
    shown_name(out, sizeof(out), "[BIOS] (World)", "[BIOS] (World)", "");
    ASSERT_TRUE(gs_nameHasText(out));
}

TEST(numbered_tags_only_name_not_empty)
{
    char out[STR_MAX * 2];
    shown_name(out, sizeof(out), "1. (Japan)", "1. (Japan)", "");
    ASSERT_STREQ(out, "1. (Japan)");
}

TEST(blank_name_uses_rom_then_label)
{
    char out[64] = "   ";
    gs_fillShortName(out, sizeof(out), "  ", "rom", "label");
    ASSERT_STREQ(out, "rom");

    char out2[64] = "";
    gs_fillShortName(out2, sizeof(out2), "", "", "label");
    ASSERT_STREQ(out2, "label");
}

TEST(nothing_at_all_gives_question_mark)
{
    char out[8] = "";
    gs_fillShortName(out, sizeof(out), NULL, NULL, NULL);
    ASSERT_STREQ(out, "?");
}

TEST(fallback_fits_the_buffer)
{
    char out[6] = "";
    gs_fillShortName(out, sizeof(out), "A very long name", "", "");
    ASSERT_STREQ(out, "A ver");
}

int main(void)
{
    printf("\n=== gs_game_name.h ===\n\n");
    RUN_TEST(cleaned_name_kept);
    RUN_TEST(tags_only_name_falls_back_to_full_name);
    RUN_TEST(bios_like_name_not_empty);
    RUN_TEST(numbered_tags_only_name_not_empty);
    RUN_TEST(blank_name_uses_rom_then_label);
    RUN_TEST(nothing_at_all_gives_question_mark);
    RUN_TEST(fallback_fits_the_buffer);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
