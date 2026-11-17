/* SPDX-License-Identifier: GPL-3.0-only */
/* One-time LCD CSC restore for Miyoo's SigmaStar SDK.
 * Stock/Open MainUI algorithm: keep the 24-byte LCD parameters, replacing
 * only lumination, contrast, hue and saturation.
 *
 * usage: restore_lcd_csc <lumination 0..20> <hue 0..20>
 *                        <saturation 0..20> <contrast 0..20>
 */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int read_level(const char *s, int *out)
{
    char *end = NULL;
    long number;
    if (!s || !*s)
        return -1;
    errno = 0;
    number = strtol(s, &end, 10);
    if (errno || end == s || *end || number < 0 || number > 20)
        return -1;
    *out = (int)number;
    return 0;
}

int main(int argc, char **argv)
{
    int lumination, hue, saturation, contrast;
    int result = 1;
    int (*sys_init)(void);
    int (*sys_exit)(void);
    int (*get_lcd)(int, uint32_t *);
    int (*set_lcd)(int, const uint32_t *);
    void *sys_lib = NULL;
    void *disp_lib = NULL;
    uint32_t params[6] = {0};

    if (argc != 5 || read_level(argv[1], &lumination) ||
        read_level(argv[2], &hue) || read_level(argv[3], &saturation) ||
        read_level(argv[4], &contrast)) {
        fprintf(stderr, "usage: %s LUMA HUE SATURATION CONTRAST (each 0..20)\n", argv[0]);
        return 2;
    }

    sys_lib = dlopen("libmi_sys.so", RTLD_NOW | RTLD_GLOBAL);
    if (!sys_lib) {
        fprintf(stderr, "CSC: cannot load libmi_sys.so: %s\n", dlerror());
        return 1;
    }
    sys_init = (int (*)(void))dlsym(sys_lib, "MI_SYS_Init");
    sys_exit = (int (*)(void))dlsym(sys_lib, "MI_SYS_Exit");
    if (!sys_init || !sys_exit) {
        fprintf(stderr, "CSC: MI_SYS_Init/MI_SYS_Exit unavailable\n");
        goto close_sys;
    }
    disp_lib = dlopen("libmi_disp.so", RTLD_NOW | RTLD_LOCAL);
    if (!disp_lib) {
        fprintf(stderr, "CSC: cannot load libmi_disp.so: %s\n", dlerror());
        goto close_sys;
    }
    get_lcd = (int (*)(int, uint32_t *))dlsym(disp_lib, "MI_DISP_GetLcdParam");
    set_lcd = (int (*)(int, const uint32_t *))dlsym(disp_lib, "MI_DISP_SetLcdParam");
    if (!get_lcd || !set_lcd) {
        fprintf(stderr, "CSC: LCD parameter functions unavailable\n");
        goto close_disp;
    }
    if (sys_init() != 0) {
        fprintf(stderr, "CSC: MI_SYS_Init failed; no display changes\n");
        goto close_disp;
    }
    if (get_lcd(0, params) != 0) {
        fprintf(stderr, "CSC: MI_DISP_GetLcdParam failed; no display changes\n");
        goto exit_sys;
    }

    /* Match src/app/settings_page.c:apply_display(), exactly. */
    params[1] = (uint32_t)(lumination + 35);
    params[3] = (uint32_t)(hue * 5);
    params[4] = (uint32_t)(saturation * 5);
    params[2] = (uint32_t)((contrast + 15) * 2);
    if (set_lcd(0, params) != 0) {
        fprintf(stderr, "CSC: MI_DISP_SetLcdParam failed\n");
        goto exit_sys;
    }
    fprintf(stderr, "CSC: restored: luma=%u hue=%u saturation=%u contrast=%u\n",
            params[1], params[3], params[4], params[2]);
    result = 0;

exit_sys:
    /* SDK requires MI_SYS_Init and MI_SYS_Exit in pairs. */
    if (sys_exit() != 0)
        fprintf(stderr, "CSC: warning: MI_SYS_Exit returned an error\n");
close_disp:
    dlclose(disp_lib);
close_sys:
    dlclose(sys_lib);
    return result;
}
