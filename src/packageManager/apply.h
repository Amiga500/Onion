#ifndef PACMAN_APPLY_H__
#define PACMAN_APPLY_H__

#include "utils/msleep.h"

#include "./fileActions.h"
#include "./globals.h"
#include "./pacman_run.h"

#define PACMAN_FAILED_SHOWN 6

static void showApplyMessage(SDL_Surface *background, const char *lines[], int count)
{
    SDL_BlitSurface(background, NULL, screen, NULL);
    SDL_Rect rect = {10, 420 - (count - 1) * 40, 603, 48};
    for (int i = 0; i < count; i++) {
        SDL_Surface *text = TTF_RenderUTF8_Blended(i == 0 ? font35 : font25, lines[i], color_white);
        if (text != NULL) {
            SDL_BlitSurface(text, NULL, screen, &rect);
            SDL_FreeSurface(text);
        }
        rect.y += 40;
    }
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
}

void applyAllChanges(bool auto_update)
{
    // Package Manager's own copy follows the theme (script/system_skin.sh);
    // the one in .tmp_update/res belongs to the installer.
    SDL_Surface *surfaceBackground = IMG_Load("/mnt/SDCARD/App/PackageManager/res/waitingBG.png");
    if (!surfaceBackground)
        surfaceBackground = IMG_Load("/mnt/SDCARD/.tmp_update/res/waitingBG.png");

    // The packages that failed: their install script (copying the files)
    // or their own install.sh/uninstall.sh returned an error.
    int failed = 0;
    char failed_names[PACMAN_FAILED_SHOWN][STR_MAX];

    for (int nT = 0; nT < tab_count; nT++) {
        const char *data_path = layer_dirs[nT];

        if (strlen(data_path) == 0 || !exists(data_path))
            continue;

        for (int nLayer = 0; nLayer < package_count[nT]; nLayer++) {
            Package *package = &packages[nT][nLayer];

            bool should_apply =
                auto_update || (package->installed && !package->complete) ||
                package->changed;
            bool should_install = package->installed != package->changed ||
                                  (package->installed &&
                                   !package->complete && !package->changed);

            if (!should_apply)
                continue;

            bool ok = true;
            if (should_install) {
                printf_debug("Installing %s...\n", package->name);
                const char *lines[] = {package->name};
                showApplyMessage(surfaceBackground, lines, 1);

                ok = pacman_runInstall(PACMAN_INSTALL_SCRIPT, data_path, package->name);
                sync();

                if (!callPackageInstaller(data_path, package->name, true))
                    ok = false;
            }
            else if (package->installed) {
                printf_debug("Removing %s...\n", package->name);
                ok = callPackageInstaller(data_path, package->name, false);

                // app removal
                char pathAppUninstall[1000];
                if (snprintf(pathAppUninstall, sizeof(pathAppUninstall), "%s/%s", data_path, package->name) < (int)sizeof(pathAppUninstall))
                    appUninstall(pathAppUninstall, strlen(pathAppUninstall));
            }

            if (!ok) {
                printf_debug("Failed: %s\n", package->name);
                if (failed < PACMAN_FAILED_SHOWN) {
                    strncpy(failed_names[failed], package->name, STR_MAX - 1);
                    failed_names[failed][STR_MAX - 1] = '\0';
                }
                failed++;
            }
        }
    }

    // Say so, instead of closing as if all went well (a copy that failed,
    // with the SD card full, left a half-installed package).
    if (failed > 0) {
        const char *lines[PACMAN_FAILED_SHOWN + 2];
        char title[64];
        snprintf(title, sizeof(title), failed == 1 ? "1 package failed:" : "%d packages failed:", failed);
        int count = 0;
        lines[count++] = title;
        for (int i = 0; i < failed && i < PACMAN_FAILED_SHOWN; i++)
            lines[count++] = failed_names[i];
        if (failed > PACMAN_FAILED_SHOWN)
            lines[count++] = "...";
        showApplyMessage(surfaceBackground, lines, count);
        msleep(4000);
    }

    if (surfaceBackground != NULL)
        SDL_FreeSurface(surfaceBackground);
}

#endif // PACMAN_APPLY_H__
