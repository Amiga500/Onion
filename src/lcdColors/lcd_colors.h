#ifndef LCD_COLORS_H__
#define LCD_COLORS_H__

#include <stdint.h>
#include <stdlib.h>

// Settings › Display values (system.json lumination, hue, saturation,
// contrast), each 0 to 20, as the LCD parameters stock MainUI writes for
// them. The parameter block is MI_DISP_LcdParam_t, six 32-bit words:
// CSC matrix, luma, contrast, hue, saturation, sharpness. Only the four
// colour words change; the matrix and sharpness the panel has are kept.

#define LCD_COLORS_COUNT 4
#define LCD_PARAM_WORDS 6

enum { LCD_LUMINANCE,
       LCD_HUE,
       LCD_SATURATION,
       LCD_CONTRAST };

static const int lcdColors_defaults[LCD_COLORS_COUNT] = {7, 10, 10, 10};

// A value from the command line; the default when it is not a whole
// number from 0 to 20 (missing or damaged in system.json).
static inline int lcdColors_parse(const char *text, int fallback)
{
    char *end = NULL;
    if (text == NULL || *text == '\0')
        return fallback;
    long value = strtol(text, &end, 10);
    if (*end != '\0' || value < 0 || value > 20)
        return fallback;
    return (int)value;
}

// Stock MainUI's mapping, also used by Open MainUI's Settings › Display.
static inline void lcdColors_fill(uint32_t params[LCD_PARAM_WORDS],
                                  const int values[LCD_COLORS_COUNT])
{
    params[1] = (uint32_t)(values[LCD_LUMINANCE] + 35);
    params[2] = (uint32_t)((values[LCD_CONTRAST] + 15) * 2);
    params[3] = (uint32_t)(values[LCD_HUE] * 5);
    params[4] = (uint32_t)(values[LCD_SATURATION] * 5);
}

#endif // LCD_COLORS_H__
