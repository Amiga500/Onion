#ifndef UTILS_APPS_H__
#define UTILS_APPS_H__

#include "./file.h"
#include "./log.h"
#include "./str.h"

typedef struct {
    char dirName[STR_MAX];
    char label[STR_MAX];
    bool is_duplicate;
    int dup_id;
} InstalledApp;
static InstalledApp _installed_apps[100];
static InstalledApp _installed_apps_sorted[100];
static int installed_apps_count = 0;
static bool installed_apps_loaded = false;
#define INSTALLED_APPS_MAX ((int)(sizeof(_installed_apps) / sizeof(_installed_apps[0])))

bool _getAppDirAndConfig(const char *app_dir_name, char *out_app_dir,
                         char *out_config_path)
{
    memset(out_app_dir, 0, STR_MAX * sizeof(char));
    memset(out_config_path, 0, STR_MAX * sizeof(char));

    strcpy(out_app_dir, "/mnt/SDCARD/App/");
    strncat(out_app_dir, app_dir_name, 128);

    if (!is_dir(out_app_dir))
        return false;

    strcpy(out_config_path, out_app_dir);
    strcat(out_config_path, "/config.json");

    if (!is_file(out_config_path))
        return false;

    return true;
}

int _comp_installed_apps(const void *a, const void *b)
{
    return strcasecmp(((InstalledApp *)a)->label, ((InstalledApp *)b)->label);
}

InstalledApp *getInstalledApps(bool sort)
{
    DIR *dp;
    struct dirent *ep;
    char app_dir[STR_MAX], config_path[STR_MAX];

    if (!installed_apps_loaded) {
        if ((dp = opendir("/mnt/SDCARD/App")) == NULL)
            return NULL;

        while ((ep = readdir(dp))) {
            // The arrays hold 100 apps: the 101st was written past them.
            if (installed_apps_count >= INSTALLED_APPS_MAX) {
                printf_debug("more than %d apps, the rest is not listed\n", INSTALLED_APPS_MAX);
                break;
            }
            if (ep->d_type != DT_DIR || strcmp(ep->d_name, ".") == 0 ||
                strcmp(ep->d_name, "..") == 0)
                continue;
            int i = installed_apps_count;

            if (!_getAppDirAndConfig(ep->d_name, app_dir, config_path))
                continue;

            InstalledApp *app = &_installed_apps[i];

            strncpy(app->dirName, ep->d_name, STR_MAX - 1);
            file_parseKeyValue(config_path, "label", app->label, ':', 0);
            app->is_duplicate = false;
            app->dup_id = 0;

            // Check for duplicate labels
            for (int j = installed_apps_count - 1; j >= 0; j--) {
                InstalledApp *other = &_installed_apps[j];
                if (strcmp(other->label, app->label) == 0) {
                    other->is_duplicate = app->is_duplicate = true;
                    app->dup_id = other->dup_id + 1;
                    break;
                }
            }

            _installed_apps_sorted[i] = _installed_apps[i];

            printf_debug("app %d: %s (%s)\n", i, app->dirName, app->label);
            installed_apps_count++;
        }

        closedir(dp); // was never closed

        qsort(_installed_apps_sorted, installed_apps_count,
              sizeof(InstalledApp), _comp_installed_apps);

        installed_apps_loaded = true;
    }

    return sort ? _installed_apps_sorted : _installed_apps;
}

bool getAppPosition(const char *app_dir_name, int *currpos, int *total)
{
    bool found = false;
    *currpos = 0;
    *total = 0;

    getInstalledApps(false);

    for (int i = 0; i < installed_apps_count; i++) {
        InstalledApp *app = &_installed_apps[i];
        if (strncmp(app_dir_name, app->dirName, STR_MAX) == 0) {
            *currpos = i;
            found = true;
            break;
        }
    }
    *total = installed_apps_count;

    printf_debug("app pos: %d (total: %d)\n", *currpos, *total);

    return found;
}

// One single-quoted sh word ('\'' for a quote inside).
static bool _shellQuote(const char *s, char *out, size_t size)
{
    size_t j = 0;
    if (size < 3)
        return false;
    out[j++] = '\'';
    for (; *s != '\0'; s++) {
        if (*s == '\'') {
            if (j + 4 >= size)
                return false;
            memcpy(out + j, "'\\''", 4);
            j += 4;
        }
        else {
            if (j + 1 >= size)
                return false;
            out[j++] = *s;
        }
    }
    if (j + 2 > size)
        return false;
    out[j++] = '\'';
    out[j] = '\0';
    return true;
}

void set_cmd_app(const char *app_dir_name)
{
    char app_dir[STR_MAX], config_path[STR_MAX];

    if (!_getAppDirAndConfig(app_dir_name, app_dir, config_path))
        return;

    char launch[STR_MAX] = "";
    file_parseKeyValue(config_path, "launch", launch, ':', 0);

    if (strlen(launch) == 0)
        return;

    // Quoted: with an app folder such as "App/My App" `cd` failed, and the
    // launch script was looked for in the wrong folder.
    char q_dir[STR_MAX * 2], q_launch[STR_MAX * 2];
    if (!_shellQuote(app_dir, q_dir, sizeof(q_dir)) ||
        !_shellQuote(launch, q_launch, sizeof(q_launch)))
        return;

    FILE *fp;
    char cmd[STR_MAX * 6];
    snprintf(cmd, sizeof(cmd),
             "cd %s; chmod a+x ./%s; "
             "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so ./%s",
             q_dir, q_launch, q_launch);
    file_put_sync(fp, "/tmp/cmd_to_run.sh", "%s", cmd);
}

#endif // UTILS_APPS_H__
