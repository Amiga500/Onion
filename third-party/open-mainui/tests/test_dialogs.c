/* SPDX-License-Identifier: GPL-3.0-only */
#include "support.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include "ui/theme.h"
#include <stdio.h>
#include <string.h>

static MainUITheme theme;
static SDL_Surface *screen, *expected;
static int room;

/* The context menu as v1.0.9 drew it, with the given text on each row. */
static void draw_v109(SDL_Surface *target, const MainUIContext *context, const char **texts)
{
    SDL_FillRect(target, NULL, SDL_MapRGB(target->format, 40, 80, 120));
    mainui_draw_popup_footer(target, &theme);
    int rows = context->visible_count < 6 ? context->visible_count : 6;
    SDL_Surface *background = mainui_theme_popup_background(&theme, rows);
    int width = background ? background->w : 120;
    int x = 0, y = 0;
    mainui_blit(target, background, x, y);
    SDL_Surface *selection = mainui_theme_popup_selection(&theme);
    for (int i = 0; i < rows; i++) {
        int top = y + i * 60;
        SDL_Rect clip = {(Sint16)x, (Sint16)top, (Uint16)width, 60};
        SDL_SetClipRect(target, &clip);
        if (i == context->selected) {
            mainui_blit(target, selection, x, top);
        }
        mainui_label(target, theme.menu_font, theme.color, texts[i], x + 20,
                     top + (60 - TTF_FontHeight(theme.menu_font)) / 2);
    }
    SDL_SetClipRect(target, NULL);
}

static void draw_with(const MainUIContext *context, bool ellipsis)
{
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 40, 80, 120));
    mainui_draw_context(screen, &theme, context, ellipsis);
}

static void draw(const MainUIContext *context)
{
    draw_with(context, true);
}

static bool same(void)
{
    for (int y = 0; y < screen->h; y++) {
        if (memcmp((Uint8 *)screen->pixels + y * screen->pitch,
                   (Uint8 *)expected->pixels + y * expected->pitch, (size_t)screen->w * 4)) {
            return false;
        }
    }
    return true;
}

static int width_of(const char *text)
{
    int width = 0, height = 0;
    assert(TTF_SizeUTF8(theme.menu_font, text, &width, &height) == 0);
    return width;
}

static MainUIContext one(const char *label)
{
    MainUIContext context = {.count = 1, .visible_count = 1};
    context.entries[0] = (MainUIContextEntry){.action = CONTEXT_CUSTOM1, .enabled = true};
    snprintf(context.entries[0].label, sizeof context.entries[0].label, "%s", label);
    return context;
}

static const char *shown_for(const char *label)
{
    for (int i = 0; i < 6; i++) {
        if (theme.context_labels[i].used && !strcmp(theme.context_labels[i].label, label)) {
            return theme.context_labels[i].shown;
        }
    }
    return NULL;
}

/* The first prefix of label whose width is in (low, high], if any. */
static bool prefix_between(const char *label, int low, int high, char out[128])
{
    for (size_t end = 1; label[end - 1]; end++) {
        if (((unsigned char)label[end] & 0xc0) == 0x80) {
            continue;
        }
        memcpy(out, label, end);
        out[end] = 0;
        int width = width_of(out);
        if (width > low && width <= high) {
            return true;
        }
    }
    return false;
}

static const char *samples[] = {
    "Rafraîchir toutes les ROMs de la carte mémoire maintenant",
    "Ajouter aux favoris et encore quelques mots de plus",
    "Supprimer de la liste des jeux récents tout de suite",
    "Обновить все ромы на карте памяти прямо сейчас",
    "Aktualisieren Sie alle ROMs auf der Speicherkarte jetzt",
    "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii",
};

/* A label that fits in room is drawn exactly as v1.0.9 drew it. */
static void check_whole(const char *label)
{
    MainUIContext context = one(label);
    draw(&context);
    const char *texts[] = {label};
    draw_v109(expected, &context, texts);
    assert(same());
    assert(!strcmp(shown_for(label), label));
}

/* A label wider than room ends in "...", 5 px clear of the popup's edge,
 * cut after a whole character, keeping as much as fits. */
static void check_shortened(const char *label)
{
    MainUIContext context = one(label);
    draw(&context);
    const char *shown = shown_for(label);
    assert(shown);
    size_t length = strlen(shown);
    assert(length >= 3 && !strcmp(shown + length - 3, "..."));
    size_t cut = length - 3;
    assert(cut < strlen(label) && !strncmp(shown, label, cut));
    assert(((unsigned char)label[cut] & 0xc0) != 0x80);
    assert(!cut || shown[cut - 1] != ' ');
    assert(width_of(shown) <= room - 5);
    /* One more character (after any spaces left out) would not fit. */
    size_t next = cut;
    while (label[next] == ' ') {
        next++;
    }
    do {
        next++;
    } while (((unsigned char)label[next] & 0xc0) == 0x80);
    char longer[600];
    snprintf(longer, sizeof longer, "%.*s...", (int)next, label);
    assert(width_of(longer) > room - 5);
    const char *texts[] = {shown};
    draw_v109(expected, &context, texts);
    assert(same());
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: fixture-dialogs <onion-builtin-theme-dir>\n");
        return 2;
    }
    assert(SDL_Init(0) == 0 && TTF_Init() == 0);
    MainUIConfig config;
    mainui_config_parse(&config, NULL, NULL, NULL, NULL);
    assert(mainui_theme_open(&theme, argv[1], argv[1], &config));
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0xff0000, 0xff00, 0xff, 0);
    expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0xff0000, 0xff00, 0xff, 0);
    assert(screen && expected);
    SDL_Surface *background = mainui_theme_popup_background(&theme, 1);
    assert(background && background->w == 320);
    room = background->w - 20;

    check_whole("Search");
    check_whole("Rafraîchir");
    /* Within the 5 px kept clear for the dots, but shown whole in v1.0.9:
     * drawn whole, with no dots. Exactly room wide is whole too. */
    char label[128];
    int between = 0, over = 0;
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; i++) {
        if (prefix_between(samples[i], room - 5, room, label)) {
            check_whole(label);
            between++;
        }
        if (prefix_between(samples[i], room, room + 12, label)) {
            check_shortened(label);
            over++;
        }
        check_shortened(samples[i]);
    }
    assert(between >= 2 && over >= 2);

    /* Shown labels are kept: a redraw measures nothing and draws the same. */
    MainUIContext context = one(samples[0]);
    draw(&context);
    unsigned clock = theme.context_label_clock;
    char before[512];
    snprintf(before, sizeof before, "%s", shown_for(samples[0]));
    SDL_BlitSurface(screen, NULL, expected, NULL);
    draw(&context);
    assert(same() && theme.context_label_clock == clock + 1);
    assert(!strcmp(shown_for(samples[0]), before));
    /* Pushed out by six others, it is worked out again the same. */
    for (int i = 0; i < 6; i++) {
        char other[32];
        snprintf(other, sizeof other, "Other %d", i);
        MainUIContext pushed = one(other);
        draw(&pushed);
    }
    assert(!shown_for(samples[0]));
    draw(&context);
    assert(same() && !strcmp(shown_for(samples[0]), before));

    /* Three rows, the second selected: each row fitted on its own. */
    MainUIContext three = {.count = 3, .visible_count = 3, .selected = 1};
    const char *labels[] = {"Launch", samples[0], samples[3]};
    for (int i = 0; i < 3; i++) {
        three.entries[i] = (MainUIContextEntry){
            .action = (MainUIContextAction)(CONTEXT_CUSTOM1 + i), .enabled = true};
        snprintf(three.entries[i].label, sizeof three.entries[i].label, "%s", labels[i]);
        three.visible[i] = i;
    }
    draw(&three);
    const char *texts[] = {labels[0], shown_for(labels[1]), shown_for(labels[2])};
    assert(!strcmp(texts[0], "Launch") && texts[1] && texts[2]);
    draw_v109(expected, &three, texts);
    assert(same());
    /* With .noContextMenuEllipsis, every label is drawn as in v1.0.9. */
    draw_with(&three, false);
    draw_v109(expected, &three, labels);
    assert(same());

    SDL_FreeSurface(screen);
    SDL_FreeSurface(expected);
    mainui_theme_close(&theme);
    TTF_Quit();
    SDL_Quit();
    puts("Context menu labels: whole when they fit, shortened with dots when cut off, kept, "
         "and as in v1.0.9 with the ellipsis off");
    return 0;
}
