#ifndef THEME_RESOURCE_IDS_H__
#define THEME_RESOURCE_IDS_H__

// Theme image and font identifiers, and the mapping from a pop menu size or
// a brightness level to its image, without SDL, so host tests include it.
// theme/resources.h includes it.

#define HIDDEN_ITEM_ALPHA 60
#define RES_MAX_REQUESTS 200

typedef enum theme_images {
    NULL_IMAGE,
    BG_TITLE,
    LOGO,
    BATTERY_0,
    BATTERY_20,
    BATTERY_50,
    BATTERY_80,
    BATTERY_100,
    BATTERY_CHARGING,
    BG_LIST_S,
    BG_LIST_L,
    HORIZONTAL_DIVIDER,
    PROGRESS_DOT,
    TOGGLE_ON,
    TOGGLE_OFF,
    BG_FOOTER,
    BUTTON_A,
    BUTTON_B,
    LEFT_ARROW,
    RIGHT_ARROW,
    LEFT_ARROW_WB,
    RIGHT_ARROW_WB,
    POP_BG,
    EMPTY_BG,
    PREVIEW_BG,
    BRIGHTNESS_0,
    BRIGHTNESS_1,
    BRIGHTNESS_2,
    BRIGHTNESS_3,
    BRIGHTNESS_4,
    BRIGHTNESS_5,
    BRIGHTNESS_6,
    BRIGHTNESS_7,
    BRIGHTNESS_8,
    BRIGHTNESS_9,
    BRIGHTNESS_10,
    LEGEND_GAMESWITCHER,
    BG_POP_MENU_1,
    BG_POP_MENU_2,
    BG_POP_MENU_3,
    BG_POP_MENU_4,
    DOT_ACTIVE,
    DOT_NEUTRAL,
    BOOT_SCREEN,
    SCREEN_OFF,
    SCREEN_OFF_SAVE,
    LOW_BAT,
    images_count
} ThemeImages;

typedef enum theme_fonts {
    NULL_FONT,
    TITLE,
    HINT,
    GRID1x4,
    GRID3x4,
    LIST,
    BATTERY,
    fonts_count
} ThemeFonts;

// Background image for a pop menu of 1-4 items; NULL_IMAGE otherwise.
static inline ThemeImages resource_popMenuBgId(int size)
{
    switch (size) {
    case 1:
        return BG_POP_MENU_1;
    case 2:
        return BG_POP_MENU_2;
    case 3:
        return BG_POP_MENU_3;
    case 4:
        return BG_POP_MENU_4;
    default:
        return NULL_IMAGE;
    }
}

// Slider image for brightness 0-10; NULL_IMAGE otherwise.
static inline ThemeImages resource_brightnessId(int brightness)
{
    if (brightness < 0 || brightness > 10)
        return NULL_IMAGE;
    return (ThemeImages)(BRIGHTNESS_0 + brightness);
}

#endif // THEME_RESOURCE_IDS_H__
