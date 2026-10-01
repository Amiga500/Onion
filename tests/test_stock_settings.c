/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/stock_settings.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    MainUIStockSettings settings;
    mainui_stock_settings_load(&settings, argv[1], argv[2], 0);
    if (argc == 4) {
        assert(settings.count == 8);
        assert(settings.rows[0] == SET_SHUTDOWN && settings.rows[7] == SET_ABOUT);
        const int models[] = {283, 354, 284};
        for (unsigned i = 0; i < sizeof models / sizeof *models; ++i) {
            mainui_stock_settings_load(&settings, argv[1], argv[2], models[i]);
            bool wifi = false;
            for (int row = 0; row < settings.count; ++row) {
                wifi |= settings.rows[row] == SET_WIFI;
            }
            assert(wifi == (models[i] != 283));
            assert(settings.count == (models[i] == 283 ? 7 : 8));
        }
        puts("Empty Settings allowlist restores model-appropriate defaults");
        return 0;
    }
    assert(settings.count == 3);
    assert(settings.rows[0] == SET_DISPLAY && settings.rows[1] == SET_BRIGHTNESS &&
           settings.rows[2] == SET_ABOUT);
    assert(!strcmp(mainui_stock_setting_icon(SET_SHUTDOWN), "skin/icon-Shutdown.png"));
    assert(!strcmp(mainui_stock_setting_label(SET_SOUND), "Menu sound"));
    puts("Stock Settings whitelist and reference labels verified");
    return 0;
}
