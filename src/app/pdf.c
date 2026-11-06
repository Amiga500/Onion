/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/pdf.h"
#include "app/render.h"
#include "app/startup.h"
#include "platform/audio.h"
#include "platform/menu_button.h"
#include "platform/system_config.h"
#include "ui/panels.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Green runs as a separate application: never decode a PDF in MainUI.
 * Preflight before releasing the display, and never change CPU governor. */
static bool green_paths(const char *sd, const char *pdf, char exe[4096], char dir[4096],
                        char libs[4096])
{
    struct stat info;
    if (!sd || !*sd || !pdf || !*pdf || stat(pdf, &info) || !S_ISREG(info.st_mode)) {
        return false;
    }
    int n = snprintf(dir, 4096, "%s/.tmp_update", sd);
    if (n < 0 || n >= 4096) {
        return false;
    }
    n = snprintf(exe, 4096, "%s/bin/green/green", dir);
    if (n < 0 || n >= 4096 || access(exe, X_OK)) {
        return false;
    }
    n = snprintf(libs, 4096, "%s/lib/parasyte", dir);
    return n >= 0 && n < 4096 && !stat(libs, &info) && S_ISDIR(info.st_mode);
}

static bool run_green(const char *exe, const char *dir, const char *libs, const char *pdf)
{
    /* Prepare the environment before fork: only async-signal-safe operations
     * run in the child, even while catalog/preview threads may exist. */
    const char *old = getenv("LD_LIBRARY_PATH");

    enum {
        VALUE_SIZE = 8192
    };

    char *value = malloc(VALUE_SIZE);
    int n = value ? snprintf(value, VALUE_SIZE, "LD_LIBRARY_PATH=%s%s%s", libs,
                             old && *old ? ":" : "", old && *old ? old : "")
                  : -1;
    if (n < 0 || n >= VALUE_SIZE) {
        free(value);
        return false;
    }
    size_t count = 0;
    while (environ[count]) {
        count++;
    }
    char **env = calloc(count + 2, sizeof *env);
    if (!env) {
        free(value);
        return false;
    }
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(environ[i], "LD_LIBRARY_PATH=", 16)) {
            env[used++] = environ[i];
        }
    }
    env[used] = value;
    /* Follow Onion's convention while Green owns the screen. Only remove
     * the stay-awake marker if this launcher created it. */
    int awake = open("/tmp/stay_awake", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (awake >= 0) {
        close(awake);
    }
    fflush(NULL);
    /* Determine the descriptor ceiling before fork (sysconf is not
     * async-signal-safe while background workers exist). */
    long descriptor_limit = sysconf(_SC_OPEN_MAX);
    if (descriptor_limit < 0 || descriptor_limit > 4096) {
        descriptor_limit = 4096;
    }
    pid_t parent = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        if (awake >= 0) {
            unlink("/tmp/stay_awake");
        }
        free(env);
        free(value);
        return false;
    }
    if (!pid) {
#ifdef __linux__
        /* An Onion shortcut kills MainUI alone (keymon's SIGKILL) and starts
         * another app: Green must not stay on the screen behind it. It ends
         * with MainUI, also when MainUI ended before this line. */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent) {
            _exit(0);
        }
#else
        (void)parent;
#endif
        /* Keep the PDF reader away from MainUI's SQLite, input and theme
         * descriptors; only stdin/stdout/stderr belong to the child. */
        for (int fd = 3; fd < descriptor_limit; ++fd) {
            close(fd);
        }
        /* Do not inherit MainUI's signal handlers or masks into Green. */
        struct sigaction normal = {.sa_handler = SIG_DFL};
        sigemptyset(&normal.sa_mask);
        const int signals[] = {SIGINT,  SIGTERM, SIGCONT, SIGHUP,  SIGCHLD,  SIGQUIT,   SIGABRT,
                               SIGPIPE, SIGALRM, SIGUSR1, SIGUSR2, SIGWINCH, SIGVTALRM, SIGPROF};
        for (size_t i = 0; i < sizeof signals / sizeof *signals; ++i) {
            sigaction(signals[i], &normal, NULL);
        }
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        if (chdir(dir)) {
            _exit(127);
        }
        char *const args[] = {(char *)exe, (char *)pdf, NULL};
        execve(exe, args, env);
        _exit(127);
    }
    int status = 0;
    pid_t done;
    do {
        done = waitpid(pid, &status, 0);
    } while (done < 0 && errno == EINTR);
    free(env);
    free(value);
    if (awake >= 0) {
        unlink("/tmp/stay_awake");
    }
    return done == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool restore_display(MainUIApp *ui)
{
#ifdef MAINUI_ONION
    mainui_reset_framebuffer_pan("before PDF return");
#endif
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return false;
    }
    ui->display = SDL_SetVideoMode(640, 480, 32, ui->real_device ? SDL_HWSURFACE : SDL_SWSURFACE);
#ifdef MAINUI_ONION
    if (ui->display && ui->real_device) {
        mainui_reset_framebuffer_pan("PDF return");
    }
#endif
    if (!ui->display) {
        return false;
    }
    ui->screen = ui->display;
    if (ui->real_device) {
        ui->screen = SDL_CreateRGBSurface(
            SDL_SWSURFACE, 640, 480, ui->display->format->BitsPerPixel, ui->display->format->Rmask,
            ui->display->format->Gmask, ui->display->format->Bmask, 0);
    }
    if (!ui->screen) {
        return false;
    }
    SDL_WM_SetCaption("Open MainUI", NULL);
    SDL_EnableKeyRepeat(ui->config.repeat_delay, ui->config.repeat_interval);
    SDL_EnableUNICODE(1);
    SDL_Event stale[16];
    while (SDL_PeepEvents(stale, 16, SDL_GETEVENT, SDL_KEYDOWNMASK | SDL_KEYUPMASK) > 0) {
    }
    /* Menu monitoring continues while Green runs. Remove only its stale
     * release events; preserve unrelated worker notifications in order. */
    SDL_Event *kept = NULL;
    size_t saved = 0, capacity = 0;
    bool overflow = false;
    while (!overflow) {
        int count = SDL_PeepEvents(stale, 16, SDL_GETEVENT, SDL_EVENTMASK(SDL_USEREVENT));
        if (count <= 0) {
            break;
        }
        for (int i = 0; i < count; i++) {
            if (stale[i].user.code == MAINUI_MENU_RELEASE_CODE) {
                continue;
            }
            if (saved == capacity) {
                size_t next = capacity ? capacity * 2 : 16;
                SDL_Event *more = next <= 256 ? realloc(kept, next * sizeof *kept) : NULL;
                if (!more) {
                    /* Low-memory fallback: don't lose a worker event. */
                    SDL_PushEvent(&stale[i]);
                    overflow = true;
                    continue;
                }
                kept = more;
                capacity = next;
            }
            kept[saved++] = stale[i];
        }
        /* SDL 1.2 queues at most 128 events; this also bounds unusual
         * concurrent producers during reader shutdown. */
        if (saved >= 128) {
            break;
        }
    }
    for (size_t i = 0; i < saved; ++i) {
        SDL_PushEvent(&kept[i]);
    }
    free(kept);
    ui->menu_events_seen = mainui_menu_button_events();
    ui->quiet_l1_armed = ui->quiet_l1_held = false;
    memset(ui->held, 0, sizeof ui->held);
    ui->x_armed = ui->y_armed = ui->select_armed = ui->start_armed = false;
    ui->select_brightness = ui->return_latched = false;
    return true;
}

void mainui_pdf_launch(MainUIApp *ui, const char *path)
{
    /* The paths on the heap: the UI thread's stack stays small. */
    struct {
        char exe[4096], dir[4096], libs[4096];
    } *green = malloc(sizeof *green);

    if (!green || !ui->real_device || ui->snapshot ||
        !green_paths(ui->sd, path, green->exe, green->dir, green->libs)) {
        free(green);
        snprintf(ui->message_title, sizeof ui->message_title, "Manual (PDF)");
        snprintf(ui->message_body, sizeof ui->message_body, "Green PDF reader unavailable.");
        return;
    }
    bool had_audio = mainui_audio_available();
    /* Only tear down the previous backdrop when Green is actually available.
     * A missing executable must leave the media menu untouched. */
    mainui_media_suspend_for_pdf(&ui->details.media);
    /* The theme's Loading panel while Green starts, which takes seconds. It
     * is only drawn on the screen once, not kept anywhere: the first frame
     * after Green, drawn whole on a new screen, never shows it. */
    if (ui->screen) {
        mainui_draw_catalog_loading(ui->screen, &ui->theme, true);
        mainui_present_whole(ui);
    }
    if (ui->screen && ui->screen != ui->display) {
        SDL_FreeSurface(ui->screen);
    }
    ui->screen = ui->display = NULL;
    mainui_audio_close();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    bool succeeded = run_green(green->exe, green->dir, green->libs, path);
    free(green);
    bool display = restore_display(ui);
    if (display && had_audio) {
        cJSON *system = mainui_system_read(ui->sd);
        int volume = mainui_system_monitor_value(system, "bgmvol");
        int audiofix = -1;
        if (!mainui_device_setting_value(&ui->device_adapter, "audiofix", &audiofix)) {
            audiofix = mainui_system_monitor_value(system, "audiofix");
        }
        mainui_audio_open(ui->theme.directory, ui->theme.fallback, volume,
                          audiofix == 0 ? 4096 : 1024);
        mainui_audio_pause(ui->device_enabled && ui->device_status.sleeping == 1);
        cJSON_Delete(system);
    }
    if (!display) {
        fprintf(stderr, "[pdf] Failed to restore SDL display: %s\n", SDL_GetError());
        ui->running = false;
        ui->status = 3;
        return;
    }
    ui->cached_start = -1;
    ui->idle_tick = false;
    mainui_media_back_from_pdf(&ui->details.media, ui->screen);
    if (!succeeded) {
        snprintf(ui->message_title, sizeof ui->message_title, "Manual (PDF)");
        snprintf(ui->message_body, sizeof ui->message_body, "PDF reader failed or was closed.");
    }
}
