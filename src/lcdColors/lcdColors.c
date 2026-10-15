/**
 * lcdColors - put Settings › Display colours into the Mini Flip LCD
 *
 * Usage: lcdColors <luminance> <hue> <saturation> <contrast>
 *        (each 0 to 20, as in system.json; anything else uses the default)
 *
 * The Mini Flip LCD comes up with default Luminance, Hue, Saturation and
 * Contrast, and the launcher sets the saved ones only when they are changed
 * in Settings › Display. runtime.sh runs this at boot, in the background,
 * so the saved colours are back from the first screen.
 *
 * The SigmaStar libraries are loaded at run time, as the launcher does, so
 * the tool builds without their headers and simply fails on a device
 * without them. MI_SYS is started for this process and released at the end,
 * as pngScale and jpg2png do.
 *
 * Exit status: 0 applied, 1 not applied, 2 usage.
 */

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

#include "lcd_colors.h"

typedef int (*SysFunc)(void);
typedef int (*GetLcdFunc)(int, uint32_t *);
typedef int (*SetLcdFunc)(int, const uint32_t *);

static void *symbol(void *library, const char *name)
{
    void *found = library ? dlsym(library, name) : NULL;
    if (found == NULL)
        fprintf(stderr, "lcdColors: %s not found\n", name);
    return found;
}

int main(int argc, char *argv[])
{
    if (argc != 1 + LCD_COLORS_COUNT) {
        fprintf(stderr, "Usage: %s <luminance> <hue> <saturation> <contrast>\n",
                argv[0]);
        return 2;
    }
    int values[LCD_COLORS_COUNT];
    for (int i = 0; i < LCD_COLORS_COUNT; i++)
        values[i] = lcdColors_parse(argv[1 + i], lcdColors_defaults[i]);

    void *sys = dlopen("libmi_sys.so", RTLD_NOW | RTLD_GLOBAL);
    void *disp = dlopen("libmi_disp.so", RTLD_NOW);
    if (sys == NULL || disp == NULL) {
        fprintf(stderr, "lcdColors: %s\n", dlerror());
        return 1;
    }
    SysFunc sys_init, sys_exit;
    GetLcdFunc get_lcd;
    SetLcdFunc set_lcd;
    // POSIX's way to turn dlsym's object pointer into a function pointer.
    *(void **)(&sys_init) = symbol(sys, "MI_SYS_Init");
    *(void **)(&sys_exit) = symbol(sys, "MI_SYS_Exit");
    *(void **)(&get_lcd) = symbol(disp, "MI_DISP_GetLcdParam");
    *(void **)(&set_lcd) = symbol(disp, "MI_DISP_SetLcdParam");
    if (!sys_init || !sys_exit || !get_lcd || !set_lcd)
        return 1;

    if (sys_init() != 0) {
        fprintf(stderr, "lcdColors: MI_SYS_Init failed\n");
        return 1;
    }
    uint32_t params[LCD_PARAM_WORDS] = {0};
    int status = 1;
    if (get_lcd(0, params) != 0) {
        fprintf(stderr, "lcdColors: MI_DISP_GetLcdParam failed\n");
    }
    else {
        lcdColors_fill(params, values);
        if (set_lcd(0, params) == 0)
            status = 0;
        else
            fprintf(stderr, "lcdColors: MI_DISP_SetLcdParam failed\n");
    }
    sys_exit();
    return status;
}
