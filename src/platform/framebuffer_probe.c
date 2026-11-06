/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/framebuffer_probe.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

void mainui_framebuffer_probe(const SDL_Surface *display)
{
    static bool done;
    if (done || !display || !display->format) {
        return;
    }
    done = true;
    const SDL_PixelFormat *format = display->format;
    fprintf(stdout,
            "[display] SDL %dx%d pitch=%u bpp=%u bytes=%u flags=0x%08lx "
            "doublebuf=%d hw=%d masks=%08lx/%08lx/%08lx/%08lx\n",
            display->w, display->h, (unsigned)display->pitch, (unsigned)format->BitsPerPixel,
            (unsigned)format->BytesPerPixel, (unsigned long)display->flags,
            !!(display->flags & SDL_DOUBLEBUF), !!(display->flags & SDL_HWSURFACE),
            (unsigned long)format->Rmask, (unsigned long)format->Gmask,
            (unsigned long)format->Bmask, (unsigned long)format->Amask);
#ifdef __linux__
    int fd = open("/dev/fb0", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stdout, "[display] /dev/fb0: %s\n", strerror(errno));
        fflush(stdout);
        return;
    }
    struct fb_fix_screeninfo fixed;
    struct fb_var_screeninfo variable;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fixed) == 0) {
        fprintf(stdout,
                "[display] fb0 id=%.*s line_length=%lu smem_len=%lu "
                "ypanstep=%u ywrapstep=%u type=%u visual=%u\n",
                (int)sizeof fixed.id, fixed.id, (unsigned long)fixed.line_length,
                (unsigned long)fixed.smem_len, (unsigned)fixed.ypanstep, (unsigned)fixed.ywrapstep,
                (unsigned)fixed.type, (unsigned)fixed.visual);
    }
    else {
        fprintf(stdout, "[display] FBIOGET_FSCREENINFO: %s\n", strerror(errno));
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &variable) == 0) {
        fprintf(stdout,
                "[display] fb0 xres=%u yres=%u virtual=%ux%u "
                "offset=%u,%u bpp=%u rotate=%u\n",
                variable.xres, variable.yres, variable.xres_virtual, variable.yres_virtual,
                variable.xoffset, variable.yoffset, variable.bits_per_pixel, variable.rotate);
    }
    else {
        fprintf(stdout, "[display] FBIOGET_VSCREENINFO: %s\n", strerror(errno));
    }
    close(fd);
#else
    fprintf(stdout, "[display] fbdev probe unavailable on this platform\n");
#endif
    fflush(stdout);
}
