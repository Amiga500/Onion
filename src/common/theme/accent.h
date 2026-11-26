#ifndef THEME_ACCENT_H__
#define THEME_ACCENT_H__

// The accent colour chosen in Tweaks > Look and feel > Accent colour, for
// the programs that draw it without theme images (canvas_ui.h, DisplayCal,
// Battery Monitor...). script/accent_color.sh writes the colour in use to
// config/display/accentRGB as "accent light dark" (RRGGBB): the light one
// for text on the dark panels, the dark one behind a selected slider. It is
// red (the Silky RedOnion theme's own) when the file is missing or another
// theme is active. Read once per program.

#include <stdint.h>
#include <stdio.h>

#ifndef ACCENT_RGB_FILE
#define ACCENT_RGB_FILE "/mnt/SDCARD/.tmp_update/config/display/accentRGB"
#endif

enum { ACCENT_MAIN, ACCENT_LIGHT, ACCENT_DARK };

// 0xRRGGBB
static inline uint32_t accent_rgb(int which)
{
    static uint32_t c[3] = {0x9E2C45, 0xD65670, 0x782036};
    static int loaded = 0;
    if (!loaded) {
        loaded = 1;
        FILE *f = fopen(ACCENT_RGB_FILE, "r");
        if (f) {
            unsigned int a, l, d;
            if (fscanf(f, "%6x %6x %6x", &a, &l, &d) == 3) {
                c[0] = a & 0xFFFFFF;
                c[1] = l & 0xFFFFFF;
                c[2] = d & 0xFFFFFF;
            }
            fclose(f);
        }
    }
    return c[which >= 0 && which <= 2 ? which : 0];
}

#define ACCENT_R(which) ((int)((accent_rgb(which) >> 16) & 0xFF))
#define ACCENT_G(which) ((int)((accent_rgb(which) >> 8) & 0xFF))
#define ACCENT_B(which) ((int)(accent_rgb(which) & 0xFF))

#endif // THEME_ACCENT_H__
