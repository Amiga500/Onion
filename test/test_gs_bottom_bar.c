/**
 * @file test_gs_bottom_bar.c
 * @brief GameSwitcher bottom bar with Footer layout "B A, page left"
 *        (src/gameSwitcher/gs_bottom_bar.h, gameSwitcher.c, Silky RedOnion)
 *
 * The bar is theme art. Silky RedOnion has a second one, with SELECT and
 * START on the left, Y and X in the middle and B then A on the right. It is
 * used only with the B A layout, and only when it comes from the same place
 * as the normal bar or a closer one.
 *
 * Build and run: make -f Makefile.unit test_gs_bottom_bar
 */

#include "onion_test.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/gameSwitcher/gs_bottom_bar.h"

#define NORMAL "extra/gs-bottom-bar"
#define ALT "extra/gs-bottom-bar-alt"
#define SILKY "../static/build/Themes/Silky RedOnion/skin/extra/"

/* theme_getImagePath load modes */
enum { SYSTEM = 0,
       THEME = 1,
       OVERRIDE = 2 };

TEST(classic_keeps_the_normal_bar)
{
    ASSERT_STREQ(gs_bottomBarName(false, THEME, THEME), NORMAL);
    ASSERT_STREQ(gs_bottomBarName(false, OVERRIDE, THEME), NORMAL);
}

TEST(b_a_uses_the_theme_alternative)
{
    ASSERT_STREQ(gs_bottomBarName(true, THEME, THEME), ALT);
    ASSERT_STREQ(gs_bottomBarName(true, OVERRIDE, OVERRIDE), ALT);
    ASSERT_STREQ(gs_bottomBarName(true, OVERRIDE, THEME), ALT);
    ASSERT_STREQ(gs_bottomBarName(true, THEME, SYSTEM), ALT);
}

/* A theme without an alternative keeps its own bar, never another's. */
TEST(b_a_without_alternative_keeps_the_normal_bar)
{
    ASSERT_STREQ(gs_bottomBarName(true, SYSTEM, THEME), NORMAL);
    ASSERT_STREQ(gs_bottomBarName(true, SYSTEM, SYSTEM), NORMAL);
}

/* A bar the user put in the theme overrides wins over the theme's
 * alternative. */
TEST(user_override_bar_is_not_replaced)
{
    ASSERT_STREQ(gs_bottomBarName(true, THEME, OVERRIDE), NORMAL);
}

static bool png_size(const char *path, uint32_t *w, uint32_t *h)
{
    unsigned char hdr[24];
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    size_t n = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);
    if (n != sizeof(hdr) || memcmp(hdr, "\x89PNG\r\n\x1a\n", 8) != 0 || memcmp(hdr + 12, "IHDR", 4) != 0)
        return false;
    *w = (uint32_t)hdr[16] << 24 | hdr[17] << 16 | hdr[18] << 8 | hdr[19];
    *h = (uint32_t)hdr[20] << 24 | hdr[21] << 16 | hdr[22] << 8 | hdr[23];
    return true;
}

/* Silky RedOnion ships both bars, the same size. */
TEST(silky_redonion_has_both_bars)
{
    uint32_t w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    ASSERT_TRUE(png_size(SILKY "gs-bottom-bar.png", &w1, &h1));
    ASSERT_TRUE(png_size(SILKY "gs-bottom-bar-alt.png", &w2, &h2));
    ASSERT_EQ(w1, 640);
    ASSERT_EQ(h1, 40);
    ASSERT_EQ(w2, w1);
    ASSERT_EQ(h2, h1);
}

/* gameSwitcher.c picks the bar with the footer layout and both load
 * modes. */
TEST(gameswitcher_uses_the_choice)
{
    FILE *fp = fopen("../src/gameSwitcher/gameSwitcher.c", "r");
    ASSERT_NOT_NULL(fp);
    static char src[1 << 18];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';
    ASSERT_NOT_NULL(strstr(src, "appState.custom_footer = loadOptionalImage(gs_bottomBarName(\n"
                                "        theme_footerAltLayout(),\n"
                                "        theme_getImagePath(theme()->path, \"extra/gs-bottom-bar-alt\", NULL),\n"
                                "        theme_getImagePath(theme()->path, \"extra/gs-bottom-bar\", NULL)));"));
}

int main(void)
{
    printf("\n=== GameSwitcher bottom bar (gs_bottom_bar.h) ===\n\n");

    RUN_TEST(classic_keeps_the_normal_bar);
    RUN_TEST(b_a_uses_the_theme_alternative);
    RUN_TEST(b_a_without_alternative_keeps_the_normal_bar);
    RUN_TEST(user_override_bar_is_not_replaced);
    RUN_TEST(silky_redonion_has_both_bars);
    RUN_TEST(gameswitcher_uses_the_choice);

    TEST_REPORT();
    return test_failures;
}
