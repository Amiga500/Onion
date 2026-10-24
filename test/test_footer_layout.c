/**
 * @file test_footer_layout.c
 * @brief Footer of the RedOnion programs, classic and "B A, page left"
 *        (src/common/theme/render/footer.h)
 *
 * Tweaks, the GameSwitcher, the Info panel and the prompts draw their footer
 * with theme_renderStandardHint() and theme_renderFooterStatus(). With
 * .mainuiFooterAltLayout they follow Open MainUI 1.0.10: page number on the
 * left, hints in button order, B then A, ending on the right. Classic keeps
 * the positions it always had.
 *
 * SDL and the theme are replaced by stubs that record every blit, and text
 * is 10 px a character.
 *
 * Build and run: make -f Makefile.unit test_footer_layout
 */

#define FOOTER_ALT_LAYOUT_FLAG "/tmp/onion_test_footer_layout/.mainuiFooterAltLayout"

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL/SDL.h" /* test/stubs */
#include "utils/str.h"

/* ---- SDL_ttf and theme stubs (footer.h's dependencies) ---- */
#define THEME_BACKGROUND_H__
#define THEME_CONFIG_H__
#define THEME_RESOURCES_H__

typedef struct {
    int unused;
} TTF_Font;
static TTF_Font fake_font;

typedef enum { HINT } FontType;
typedef enum { BUTTON_A,
               BUTTON_B,
               BG_FOOTER } ResourceType;

typedef struct {
    struct {
        SDL_Color color;
    } hint, currentpage, total;
    struct {
        bool hints;
    } hideLabels;
} FakeTheme;
static FakeTheme fake_theme;
static FakeTheme *theme(void) { return &fake_theme; }

static double g_scale = 1.0;
static struct {
    int width, height;
} g_display = {640, 480};

static SDL_Rect theme_scaleRect(SDL_Rect rect) { return rect; }

/* Surfaces: 54x40 buttons (icon-A-54), a footer bar and a background. */
static SDL_Surface button_a = {.w = 54, .h = 40}, button_b = {.w = 54, .h = 40};
static SDL_Surface bg_footer = {.w = 640, .h = 60}, background = {.w = 640, .h = 480};
static bool no_b_button = false;

static SDL_Surface *resource_getSurface(ResourceType type)
{
    switch (type) {
    case BUTTON_A:
        return &button_a;
    case BUTTON_B:
        return no_b_button ? NULL : &button_b;
    default:
        return &bg_footer;
    }
}
static TTF_Font *resource_getFont(FontType type)
{
    (void)type;
    return &fake_font;
}
static SDL_Surface *theme_background(void) { return &background; }

/* Text surfaces remember their string. */
typedef struct {
    SDL_Surface surface;
    char text[64];
} TextSurface;

static SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font, const char *text, SDL_Color fg)
{
    (void)font;
    (void)fg;
    TextSurface *t = calloc(1, sizeof(TextSurface));
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->surface.w = 10 * (int)strlen(text);
    t->surface.h = 30;
    return &t->surface;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
    if (surface != &button_a && surface != &button_b && surface != &bg_footer &&
        surface != &background)
        free(surface);
}

typedef struct {
    SDL_Surface *src;
    int x, y, w;
    bool clipped; /* a source rect was given: a clear of the old number */
} Blit;
static Blit blits[64];
static int blit_count = 0;

static int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    (void)dst;
    if (blit_count < 64)
        blits[blit_count++] = (Blit){src, dstrect ? dstrect->x : 0, dstrect ? dstrect->y : 0,
                                     srcrect ? srcrect->w : src->w, srcrect != NULL};
    return 0;
}

#include "theme/render/footer.h"

/* ---- helpers ---- */

static const char *text_of(SDL_Surface *s)
{
    if (s == &button_a)
        return "[A]";
    if (s == &button_b)
        return "[B]";
    if (s == &bg_footer)
        return "[footer]";
    if (s == &background)
        return "[background]";
    return ((TextSurface *)s)->text;
}

static Blit *find(const char *what)
{
    for (int i = 0; i < blit_count; i++)
        if (strcmp(text_of(blits[i].src), what) == 0)
            return &blits[i];
    return NULL;
}

static int count_clears(void)
{
    int n = 0;
    for (int i = 0; i < blit_count; i++)
        if (blits[i].clipped && blits[i].src == &bg_footer)
            n++;
    return n;
}

static SDL_Surface screen;

static void reset(bool alt)
{
    blit_count = 0;
    no_b_button = false;
    theme_footerSetAltLayout(alt);
    theme_renderStandardHint_cleanup();
    old_status_width = -1;
}

/* ---- tests ---- */

/* The positions Onion always used: A at 20, B 30 px after A's label, the
 * page number ending at 620. */
TEST(classic_layout_is_unchanged)
{
    reset(false);
    theme_renderStandardHint(&screen, "SELECT", "BACK");
    theme_renderFooterStatus(&screen, 4, 9);

    ASSERT_EQ(find("[A]")->x, 20);
    ASSERT_EQ(find("[A]")->y, 450 - 20);
    ASSERT_EQ(find("SELECT")->x, 20 + 54 + 5);
    ASSERT_EQ(find("SELECT")->y, 449 - 15);
    ASSERT_EQ(find("[B]")->x, 20 + 54 + 5 + 60 + 30);
    ASSERT_EQ(find("BACK")->x, 20 + 54 + 5 + 60 + 30 + 54 + 5);
    ASSERT_EQ(find("9")->x, 620 - 10);
    ASSERT_EQ(find("4/")->x, 620 - 10 - 20);
}

TEST(alt_layout_page_left_b_then_a_right)
{
    reset(true);
    theme_renderStandardHint(&screen, "SELECT", "BACK");
    theme_renderFooterStatus(&screen, 4, 9);

    /* A's label ends at 620, as the page number did. */
    ASSERT_EQ(find("SELECT")->x + 60, 620);
    ASSERT_EQ(find("[A]")->x, 620 - 60 - 5 - 54);
    /* B before A, 30 px apart, the same spacing as classic. */
    ASSERT_EQ(find("BACK")->x + 40 + 30, find("[A]")->x);
    ASSERT_EQ(find("[B]")->x, find("BACK")->x - 5 - 54);
    ASSERT_TRUE(find("[B]")->x < find("[A]")->x);
    /* Page number from the left margin. */
    ASSERT_EQ(find("4/")->x, 20);
    ASSERT_EQ(find("9")->x, 20 + 20);
    ASSERT_EQ(find("[A]")->y, 450 - 20);
    ASSERT_EQ(find("4/")->y, 449 - 15);
}

/* A prompt that must be answered has no B hint: A alone, on the right. */
TEST(alt_layout_without_b_hint)
{
    reset(true);
    theme_renderStandardHint(&screen, "SELECT", NULL);
    ASSERT_NULL(find("[B]"));
    ASSERT_EQ(find("SELECT")->x + 60, 620);

    reset(false);
    theme_renderStandardHint(&screen, "SELECT", NULL);
    ASSERT_NULL(find("[B]"));
    ASSERT_EQ(find("[A]")->x, 20);
}

/* A theme without a B icon must not crash (classic dereferenced it). */
TEST(missing_b_icon_draws_label_only)
{
    reset(true);
    no_b_button = true;
    theme_renderStandardHint(&screen, "SELECT", "BACK");
    ASSERT_NULL(find("[B]"));
    ASSERT_EQ(find("BACK")->x + 40 + 30, find("[A]")->x);
}

/* Hidden hint labels (theme override): icons stay, in the same order. */
TEST(hidden_labels_keep_icon_order)
{
    reset(true);
    fake_theme.hideLabels.hints = true;
    theme_renderStandardHint(&screen, "SELECT", "BACK");
    fake_theme.hideLabels.hints = false;
    ASSERT_NULL(find("SELECT"));
    ASSERT_TRUE(find("[B]")->x < find("[A]")->x);
}

/* A new page number clears the old one where it was drawn. */
TEST(status_clears_its_old_place)
{
    reset(true);
    theme_renderFooterStatus(&screen, 4, 9);
    blit_count = 0;
    theme_renderFooterStatus(&screen, 5, 9);
    ASSERT_EQ(count_clears(), 1);
    for (int i = 0; i < blit_count; i++)
        if (blits[i].clipped && blits[i].src == &bg_footer) {
            ASSERT_EQ(blits[i].x, 20);
            ASSERT_EQ(blits[i].w, 30);
        }

    reset(false);
    theme_renderFooterStatus(&screen, 4, 9);
    blit_count = 0;
    theme_renderFooterStatus(&screen, 5, 9);
    ASSERT_EQ(count_clears(), 1);
    for (int i = 0; i < blit_count; i++)
        if (blits[i].clipped && blits[i].src == &bg_footer)
            ASSERT_EQ(blits[i].x, 620 - 30);
}

/* Tweaks changes the layout while it runs and redraws the whole footer: the
 * old number's place now holds hints, so it must not be cleared. */
TEST(layout_change_does_not_clear_over_hints)
{
    reset(false);
    theme_renderFooterStatus(&screen, 4, 9);
    theme_footerSetAltLayout(true);
    blit_count = 0;
    theme_renderStandardHint(&screen, "SELECT", "BACK");
    theme_renderFooterStatus(&screen, 4, 9);
    ASSERT_EQ(count_clears(), 0);
    ASSERT_EQ(find("4/")->x, 20);

    /* Then the next change of number clears on the left. */
    blit_count = 0;
    theme_renderFooterStatus(&screen, 5, 9);
    ASSERT_EQ(count_clears(), 1);
}

/* The layout is read from the flag file, once per process. */
TEST(flag_file_is_read_once)
{
    (void)!system("rm -rf /tmp/onion_test_footer_layout && mkdir -p /tmp/onion_test_footer_layout");
    _footer_alt_layout = -1;
    ASSERT_FALSE(theme_footerAltLayout());

    (void)!system("touch " FOOTER_ALT_LAYOUT_FLAG);
    ASSERT_FALSE(theme_footerAltLayout()); /* cached */
    _footer_alt_layout = -1;
    ASSERT_TRUE(theme_footerAltLayout());

    (void)!system("rm -rf /tmp/onion_test_footer_layout");
}

/* The default path is the flag Tweaks writes and Open MainUI reads. */
TEST(default_flag_path)
{
    FILE *fp = fopen("../src/common/theme/render/footer.h", "r");
    ASSERT_NOT_NULL(fp);
    static char src[1 << 16];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';
    ASSERT_NOT_NULL(strstr(src, "#define FOOTER_ALT_LAYOUT_FLAG "
                                "\"/mnt/SDCARD/.tmp_update/config/.mainuiFooterAltLayout\""));
}

int main(void)
{
    printf("\n=== Footer layout (theme/render/footer.h) ===\n\n");

    RUN_TEST(classic_layout_is_unchanged);
    RUN_TEST(alt_layout_page_left_b_then_a_right);
    RUN_TEST(alt_layout_without_b_hint);
    RUN_TEST(missing_b_icon_draws_label_only);
    RUN_TEST(hidden_labels_keep_icon_order);
    RUN_TEST(status_clears_its_old_place);
    RUN_TEST(layout_change_does_not_clear_over_hints);
    RUN_TEST(flag_file_is_read_once);
    RUN_TEST(default_flag_path);

    TEST_REPORT();
    return test_failures;
}
