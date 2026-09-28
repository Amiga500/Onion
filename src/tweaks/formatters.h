#ifndef TWEAKS_FORMATTERS_H__
#define TWEAKS_FORMATTERS_H__

#include <stdio.h>

#include "components/list.h"
#include "utils/apps.h"
#include "utils/str.h"

#include "./formatters_basic.h"
#include "./tools.h"

#define BUTTON_MAINUI_LABELS                          \
    {                                                 \
        "Context menu", "GameSwitcher", "Resume game" \
    }
#define BUTTON_INGAME_LABELS                                                \
    {                                                                       \
        "Off", "GameSwitcher", "Exit to menu", "Quick switch", "Quick menu" \
    }

#define THEME_TOGGLE_LABELS \
    {                       \
        "-", "Off", "On"    \
    }

#define BLUELIGHT_LABELS                                                          \
    {                                                                             \
        "Subtle 1/5", "Moderate 2/5", "Balanced 3/5", "Strong 4/5", "Intense 5/5" \
    }

#define PWM_FREQUENCIES                                                                                                \
    {                                                                                                                  \
        "100 Hz", "200 Hz", "300 Hz", "400 Hz", "500 Hz", "600 Hz", "700 Hz", "800  Hz (Default)", "900 Hz", "1000 Hz" \
    }

void formatter_appShortcut(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    int value = item->value;
    InstalledApp *apps = getInstalledApps(true);
    int max_value = installed_apps_count + NUM_TOOLS + item->action_id;

    if (value <= 0 || value > max_value) {
        strcpy(out_label, item->action_id == 0 ? "B button" : "A button");
        return;
    }

    // apps
    value -= 1;
    if (value < installed_apps_count) {
        InstalledApp *app = &apps[value];
        strcpy(out_label, app->is_duplicate ? app->dirName : app->label);
        return;
    }

    // tools
    value -= installed_apps_count;
    if (value < NUM_TOOLS) {
        sprintf(out_label, "Tool: %s", tools_short_names[value]);
        return;
    }

    if (item->action_id == 1) {
        strcpy(out_label, "GLO");
    }
}

void formatter_startupTab(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    switch (item->value) {
    case 0:
        strcpy(out_label, "Main menu");
        break;
    case 1:
        strncpy(out_label,
                lang_get(LANG_RECENTS_TAB, LANG_FALLBACK_RECENTS_TAB),
                STR_MAX - 1);
        break;
    case 2:
        strncpy(out_label,
                lang_get(LANG_FAVORITES_TAB, LANG_FALLBACK_FAVORITES_TAB),
                STR_MAX - 1);
        break;
    case 3:
        strncpy(out_label, lang_get(LANG_GAMES_TAB, LANG_FALLBACK_GAMES_TAB),
                STR_MAX - 1);
        break;
    case 4:
        strncpy(out_label, lang_get(LANG_EXPERT_TAB, LANG_FALLBACK_EXPERT_TAB),
                STR_MAX - 1);
        break;
    case 5:
        strncpy(out_label, lang_get(LANG_APPS_TAB, LANG_FALLBACK_APPS_TAB),
                STR_MAX - 1);
        break;
    default:
        break;
    }
}

#endif // TWEAKS_FORMATTERS_H__
