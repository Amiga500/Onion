#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "system/axp.h"
#include "system/battery.h"
#include "system/device_model.h"
#include "system/display.h"
#include "system/keymap_hw.h"
#include "system/osd.h"
#include "system/rumble.h"
#include "system/screenshot.h"
#include "system/settings.h"
#include "system/settings_sync.h"
#include "system/state.h"
#include "system/system.h"
#include "system/system_utils.h"
#include "system/volume.h"
#include "theme/config.h"
#include "theme/render.h"
#include "theme/resources.h"
#include "utils/config.h"
#include "utils/file.h"
#include "utils/flags.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/process.h"
#include "utils/str.h"

#include "./flip_suspend.h"
#include "./input_fd.h"
#include "./konami.h"
#include "./low_battery.h"
#include "./menuButtonAction.h"
#include "./state_scan.h"
#include "./suspend_timer.h"

#define FAVORITES_PATH "/mnt/SDCARD/Roms/favourite.json"

// for proc_stat flags
#define PF_KTHREAD 0x00200000

// for suspend / hibernate
#define CHECK_SEC 15   // Interval sec to check hibernate/adc
#define SHUTDOWN_MIN 1 // Minutes to power off after hibernate
#define REPEAT_SEC(val) ((val * 1000 - 250) / 50)
// Kids Mode's video player (KidsPlay) runs many processes and threads.
#define PIDMAX 128

// MMF headphone jack GPIO detection
// Credit to Tenlevels for original patch, minor modifications made for device model check.
#define HEADPHONE_DETECT_GPIO "/sys/class/gpio/gpio45/value"
#define AUDIO_SWITCH_GPIO "/sys/class/gpio/gpio44/value"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO45_DIRECTION "/sys/class/gpio/gpio45/direction"
#define GPIO44_DIRECTION "/sys/class/gpio/gpio44/direction"

// "playActivity resume" started at the last wake-up (0: none pending)
static pid_t pa_resume_pid = 0;
static bool headphone_jack_available = false;
static int last_jack_state = -1;

static void gpioWriteInt(const char *path, int value)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d", value);
        fclose(f);
    }
}

static void gpioWriteStr(const char *path, const char *str)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", str);
        fclose(f);
    }
}

static int gpioReadInt(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    int val = -1;
    if (fscanf(f, "%d", &val) != 1)
        val = -1;
    fclose(f);
    return val;
}

static void initHeadphoneJack(void)
{
    if (DEVICE_ID != MIYOO285) {
        headphone_jack_available = false;
        return;
    }

    // Export GPIO 45 (headphone detect) if not already exported
    if (access(HEADPHONE_DETECT_GPIO, F_OK) != 0) {
        gpioWriteInt(GPIO_EXPORT_PATH, 45);
        usleep(50000);
    }

    // Export GPIO 44 (audio switch) if not already exported
    if (access(AUDIO_SWITCH_GPIO, F_OK) != 0) {
        gpioWriteInt(GPIO_EXPORT_PATH, 44);
        usleep(50000);
    }

    // Set GPIO 45 as input (headphone detect)
    if (access(GPIO45_DIRECTION, F_OK) == 0)
        gpioWriteStr(GPIO45_DIRECTION, "in");

    // Set GPIO 44 as output (audio switch)
    if (access(GPIO44_DIRECTION, F_OK) == 0)
        gpioWriteStr(GPIO44_DIRECTION, "out");

    headphone_jack_available = (access(HEADPHONE_DETECT_GPIO, F_OK) == 0);

    // Initialize audio routing based on current jack state
    if (headphone_jack_available) {
        last_jack_state = gpioReadInt(HEADPHONE_DETECT_GPIO);
        if (last_jack_state != -1)
            gpioWriteInt(AUDIO_SWITCH_GPIO, last_jack_state);
    }
}

static void checkHeadphoneJack(void)
{
    if (!headphone_jack_available)
        return;

    int current_state = gpioReadInt(HEADPHONE_DETECT_GPIO);
    if (current_state == -1)
        return;

    if (current_state != last_jack_state) {
        last_jack_state = current_state;
        // Switch audio routing: 1 = headphones, 0 = speaker
        gpioWriteInt(AUDIO_SWITCH_GPIO, current_state);

        // Restore volume when headphones connected
        if (current_state == 1)
            setVolume(settings.mute ? 0 : settings.volume);
    }
}

uint32_t suspendpid[PIDMAX];

const int KONAMI_CODE[] = {HW_BTN_UP, HW_BTN_UP, HW_BTN_DOWN, HW_BTN_DOWN,
                           HW_BTN_LEFT, HW_BTN_RIGHT, HW_BTN_LEFT, HW_BTN_RIGHT,
                           HW_BTN_B, HW_BTN_A};
const int KONAMI_CODE_LENGTH = sizeof(KONAMI_CODE) / sizeof(KONAMI_CODE[0]);

// Forward declarations
int read_lid_state(void);

#define BLUE_LIGHT_SCRIPT "/mnt/SDCARD/.tmp_update/script/blue_light.sh"
#define SCREEN_RECORDER_SCRIPT "/mnt/SDCARD/.tmp_update/script/screen_recorder.sh"
#define SETTINGS_SAVE_DELAY_MS 500 // coalesce bursts of volume/brightness steps

// Config flags read on hot paths, cached in memory instead of a stat() on
// the SD card for every key press. Refreshed on settings change and on the
// periodic tick.
static bool flag_alt_brightness = false;
static bool flag_cpu_clock_hotkey = false;

static void refresh_cached_flags(void)
{
    flag_alt_brightness = config_flag_get(".altBrightness");
    flag_cpu_clock_hotkey = config_flag_get(".cpuClockHotkey");
}

// Last CPU clock read from / set by the cpuclock tool (0 = unknown).
// Invalidated on every system state change, since game launches change it.
static int cached_cpu_clock = 0;

static void run_script_detached(const char *script, const char *arg)
{
    char *const argv[] = {"sh", (char *)script, (char *)arg, NULL};
    process_spawn_detached(argv);
}

//
//    Blue light filter schedule, evaluated in-process.
//    Replaces running blue_light.sh check every 15 s (2 global syncs and
//    ~20 fork/exec per run, synchronous, so keymon ignored keys meanwhile).
//    The script is now started, in background, only on a transition.
//

// "HH:MM" -> minutes since midnight (same fallback to 0 as the script)
static int blf_parse_minutes(const char *hhmm)
{
    int h = 0, m = 0;
    if (hhmm == NULL || sscanf(hhmm, " %d : %d", &h, &m) < 1)
        return 0;
    if (h < 0 || h > 23)
        h = 0;
    if (m < 0 || m > 59)
        m = 0;
    return h * 60 + m;
}

// Local time in minutes, using the same TZ as the script (config/.tz).
// keymon itself runs without TZ, so it is applied only for this call.
static int blf_current_minutes(void)
{
    static char tz_value[64] = "";
    static time_t tz_mtime = 0;
    struct stat st;

    if (stat(CONFIG_PATH ".tz", &st) == 0) {
        if (st.st_mtime != tz_mtime) {
            tz_mtime = st.st_mtime;
            tz_value[0] = '\0';
            FILE *fp = fopen(CONFIG_PATH ".tz", "r");
            if (fp) {
                if (fscanf(fp, "%63[^\n]", tz_value) != 1)
                    tz_value[0] = '\0';
                fclose(fp);
            }
        }
    }
    else {
        tz_value[0] = '\0';
        tz_mtime = 0;
    }

    char old_tz[64] = "";
    const char *env_tz = getenv("TZ");
    bool had_tz = env_tz != NULL;
    if (had_tz)
        snprintf(old_tz, sizeof(old_tz), "%s", env_tz);

    if (tz_value[0] != '\0')
        setenv("TZ", tz_value, 1);
    tzset();

    time_t now = time(NULL);
    struct tm local;
    int minutes = 0;
    if (localtime_r(&now, &local) != NULL)
        minutes = local.tm_hour * 60 + local.tm_min;

    if (had_tz)
        setenv("TZ", old_tz, 1);
    else
        unsetenv("TZ");
    tzset();

    return minutes;
}

// Mirrors check_blf() in blue_light.sh
static void blue_light_schedule_check(void)
{
    if (!config_flag_get(".blf") || temp_flag_get(".blfIgnoreSchedule"))
        return;

    bool active = temp_flag_get(".blfOn");

    if (!active && config_flag_get(".blfOn"))
        remove(CONFIG_PATH ".blfOn");

    char time_on[STR_MAX] = "";
    char time_off[STR_MAX] = "";
    if (!config_getString("display/blueLightTime", time_on, sizeof(time_on)) ||
        !config_getString("display/blueLightTimeOff", time_off, sizeof(time_off)))
        return;

    int on = blf_parse_minutes(time_on);
    int off = blf_parse_minutes(time_off);
    int now = blf_current_minutes();

    bool inside;
    if (off < on)
        inside = now >= on || now < off; // window crosses midnight
    else
        inside = now >= on && now < off;

    if (inside && !active)
        run_script_detached(BLUE_LIGHT_SCRIPT, "enable");
    else if (!inside && active)
        run_script_detached(BLUE_LIGHT_SCRIPT, "disable");
}

// Kids Mode (App/KidsMode) is armed: its launcher owns the screen
// (/tmp/kidsmode, set and removed by kid_mode_loop.sh).
static bool kidsmode_active(void)
{
    return temp_flag_get("kidsmode");
}

void takeScreenshot(void)
{
    super_short_pulse();
    display_setBrightnessRaw(0);
    msleep(10);
    osd_hideBar();
    screenshot_recent();
    settings_setBrightness(settings.brightness, true, false);
}

// The processes stopped for standby, kept in /tmp while they are: a keymon
// that starts again meanwhile (crash_restart.h) had no list, and they all
// stayed stopped, runtime.sh included.
#define SUSPEND_STATE_PATH "/tmp/.keymon_suspended"

static void suspend_state_save(void)
{
    FILE *fp = fopen(SUSPEND_STATE_PATH, "w");
    if (fp == NULL)
        return;
    for (uint32_t i = 1; i <= suspendpid[0]; i++)
        fprintf(fp, "%u\n", (unsigned int)suspendpid[i]);
    fclose(fp);
}

//
//    Suspend / Kill processes
//        mode: 0:STOP 1:TERM 2:KILL
//
int suspend(uint32_t mode)
{
    DIR *procdp;
    struct dirent *dir;
    char fname[32];
    pid_t suspend_pid = getpid();
    pid_t pid;
    pid_t ppid;
    char state;
    uint32_t flags;
    char comm[128];
    char line[512];
    int ret = 0;

    // terminate retroarch before kill
    if (mode == 2) {
        screenshot_system();
        terminate_retroarch();
        terminate_drastic();
    }

    sync();
    procdp = opendir("/proc");

    // Pick active processes to suspend and send SIGSTOP
    // Cond:1. PID is greater than 2(kthreadd) and not myself
    //    2. PPID is greater than 2(kthreadd)
    //    3. state is "R" or "S" or "D"
    //    4. comm is not "(sh)" / "(MainUI)" when AudioFix:OFF
    //    5. flags does not contain PF_KTHREAD (0x200000) (just in case)
    //    6. comm is not "(updater)" "(MainUI)" "(tee)" "(audioserver*" when
    //    kill mode
    if (!mode)
        suspendpid[0] = 0;
    while ((dir = readdir(procdp))) {
        if (dir->d_type == DT_DIR) {
            pid = atoi(dir->d_name);
            if ((pid > 2) && (pid != suspend_pid)) {
                snprintf(fname, sizeof(fname), "/proc/%d/stat", pid);
                FILE *fp = fopen(fname, "r");
                if (fp == NULL)
                    continue; // process already gone
                bool parsed = fgets(line, sizeof(line), fp) != NULL;
                fclose(fp);
                if (!parsed)
                    continue;

                // comm is enclosed in parentheses and may contain spaces or
                // ')' itself: the fields after it start at the LAST ')'.
                char *open_paren = strchr(line, '(');
                char *close_paren = strrchr(line, ')');
                if (open_paren == NULL || close_paren == NULL || close_paren < open_paren)
                    continue;
                size_t comm_len = (size_t)(close_paren - open_paren) + 1;
                if (comm_len >= sizeof(comm))
                    comm_len = sizeof(comm) - 1;
                memcpy(comm, open_paren, comm_len); // keep "(name)" format
                comm[comm_len] = '\0';

                if (sscanf(close_paren + 1, " %c %d %*d %*d %*d %*d %u",
                           &state, &ppid, &flags) != 3)
                    continue;

                if ((ppid > 2) &&
                    ((state == 'R') || (state == 'S') || (state == 'D')) &&
                    (strcmp(comm, "(sh)")) && (!(flags & PF_KTHREAD))) {
                    if (mode) {
                        if ((strcmp(comm, "(runtime.sh)")) &&
                            (strcmp(comm, "(updater)")) &&
                            (strcmp(comm, "(MainUI)")) &&
                            (strcmp(comm, "(tee)")) &&
                            (strncmp(comm, "(audioserver", 12)) &&
                            (strcmp(comm, "(batmon)"))) {
                            kill(pid, (mode == 1) ? SIGTERM : SIGKILL);
                            ret++;
                        }
                    }
                    else {
                        // suspendpid[0] is the counter: entries are 1..PIDMAX-1
                        if (suspendpid[0] < PIDMAX - 1) {
                            suspendpid[++suspendpid[0]] = pid;
                            kill(pid, SIGSTOP);
                            ret++;
                        }
                    }
                }
            }
        }
    }
    closedir(procdp);

    if (!mode)
        suspend_state_save();

    // reset display when anything killed
    if (mode == 2 && ret)
        display_reset();

    return ret;
}

//
//    Resume
//
void resume(void)
{
    // Send SIGCONT to suspended processes
    if (suspendpid[0]) {
        for (uint32_t i = 1; i <= suspendpid[0]; i++)
            kill(suspendpid[i], SIGCONT);
        suspendpid[0] = 0;
    }
    remove(SUSPEND_STATE_PATH);
}

//
//    A standby that keymon did not end (it crashed or was restarted):
//    CPU, stopped processes, screen and volume go back as on a wake-up.
//
static void suspend_recover(void)
{
    if (!exists(SUSPEND_STATE_PATH) && !exists(POWERSAVE_STATE_PATH))
        return;

    system_powersave_recover();

    FILE *fp = fopen(SUSPEND_STATE_PATH, "r");
    if (fp != NULL) {
        unsigned int pid;
        while (fscanf(fp, "%u", &pid) == 1) {
            if (pid > 2)
                kill((pid_t)pid, SIGCONT);
        }
        fclose(fp);
    }
    remove(SUSPEND_STATE_PATH);

    // As on a wake-up: Kids Mode kept the LCD on, only its backlight black.
    if (!kidsmode_active())
        display_on();
    display_setBrightness(settings.brightness);
    setVolume(settings.mute ? 0 : settings.volume);
    print_debug("Recovered from an interrupted standby");
}

//
//    Quit
//
void quit(int exitcode)
{
    display_close();
    if (input_fd > 0)
        close(input_fd);
    system_clock_get();
    system_rtc_set();
    system_clock_save();
    exit(exitcode);
}

//
//    Shutdown
//
// True once a shutdown has been ordered (.offOrder: keymon deepsleep(),
// MainUI power off, low battery, Kids Mode) or runtime.sh has started it
// (shutting_down). Both are only cleared at boot.
static bool shutdown_in_progress(void)
{
    return temp_flag_get(".offOrder") || temp_flag_get("shutting_down");
}

void force_shutdown(void)
{
    set_system_shutdown();
    screenshot_system();
    terminate_retroarch();
    terminate_drastic();
    system_clock_get();
    system_clock_save();
    sync();
    system("shutdown");
    while (1)
        pause();
    exit(0);
}

void showBootScreen(const char *type)
{
    char *const argv[] = {"bootScreen", (char *)type, NULL};
    process_spawn_detached(argv);
}

//
//    [onion] deepsleep if MainUI/gameSwitcher/retroarch is running
//
void deepsleep(void)
{
    // A power-off is already under way (this function returns once
    // runtime.sh has started it): nothing more to do. Called again then,
    // it found no game or MainUI left (MODE_APPS), deleted cmd_to_run.sh
    // (the game was not resumed at the next boot), vibrated and sent
    // SIGTERM to the processes of the shutdown itself.
    if (shutdown_in_progress())
        return;

    system_state_update();

    if (system_state == MODE_GAME && !check_autosave()) {
        // Kids Mode blacked the backlight when POWER went down, and nothing
        // is shut down here: put it back, or the game goes on in the dark.
        if (kidsmode_active())
            display_setBrightness(settings.brightness);
        return;
    }

    short_pulse();
    set_system_shutdown();

    if (system_state == MODE_MAIN_UI) {
        // Not kill_mainUI(): its display_reset() right after display
        // colours were changed could hang the device on shutdown
        // (robcodedev, Mini Flip). runtime.sh shows the shutdown screen.
        settings_shm_read();
        kill(system_state_pid, SIGKILL);
    }
    else if (system_state == MODE_SWITCHER) {
        kill(system_state_pid, SIGTERM);
    }
    else if (system_state == MODE_GAME) {
        screenshot_system();
        retroarch_pause();
        showBootScreen("End_Save");
        terminate_retroarch();
    }
    else if (system_state == MODE_ADVMENU) {
        kill(system_state_pid, SIGQUIT);
    }
    else if (system_state == MODE_APPS) {
        remove(CMD_TO_RUN_PATH);
        suspend(1);
    }
    else if (system_state == MODE_DRASTIC) {
        screenshot_system();
        terminate_drastic();
    }

    // Wait up to 30 s for runtime.sh to start the shutdown, then force it.
    // An app can exit by itself while POWER is held (prompt quits after
    // 500 ms, before this runs at about 600 ms): runtime.sh then starts
    // MainUI again before .offOrder exists, and above it was not running yet
    // (taken as an app), so it was left on screen until the forced shutdown.
    // It is closed as soon as it shows up.
    time_t start = time(NULL);
    while ((time(NULL) - start) < 30 && !temp_flag_get("shutting_down")) {
        msleep(500);
        system_state_update();
        if (system_state == MODE_MAIN_UI) {
            settings_shm_read();
            kill(system_state_pid, SIGKILL);
        }
    }
    if (!temp_flag_get("shutting_down")) {
        force_shutdown();
    }
}

//
//    Standby log: why a standby did or did not end in a power-off.
//    Only with Tweaks > Advanced > "Enable logging" (config/.logging);
//    a few lines per standby, in logs/keymon.log.
//
static void standby_log(const char *format, ...)
{
    if (!settings.enable_logging)
        return;
    mkdirs("/mnt/SDCARD/.tmp_update/logs");
    FILE *fp = fopen("/mnt/SDCARD/.tmp_update/logs/keymon.log", "a");
    if (fp == NULL)
        return;
    fprintf(fp, "[%ld s] ", getMilliseconds() / 1000);
    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fputc('\n', fp);
    fclose(fp);
}

// Standby timeout: a RetroArch game without auto save state is not powered
// off (deepsleep() refuses it, or its progress would be lost). Decided here,
// before resume(): resumed and then refused, the game went on running with
// the screen off, the sound at 0 and the CPU at full speed until POWER was
// pressed. It now stays stopped, and the next timeout asks again. The state
// is read from /proc and files, which a stopped RetroArch does not change.
static bool standby_game_without_autosave(void)
{
    system_state_update();
    if (system_state == MODE_GAME && !check_autosave()) {
        standby_log("standby: power-off refused (game without auto save state)");
        return true;
    }
    return false;
}

//
//    Suspend interface
//
void suspend_exec(int timeout)
{
    keyinput_disable();

    // Kids Mode: black the backlight before anything is stopped, and keep
    // the LCD controller on (display_off()/display_on() save and restore
    // the framebuffer, which flashed with the launcher's double buffer).
    bool kids = kidsmode_active();
    if (kids) {
        display_setBrightnessRaw(0);
        msleep(20);
    }

    // The GameSwitcher in front of a paused game has already closed the play
    // session, and opens it again itself when the game goes on: no resume
    // after this sleep then, or the time left in the GameSwitcher counts.
    system_state_update();
    bool in_switcher = system_state == MODE_SWITCHER;

    // The resume started at the last wake-up must have written its row
    // first: a resume that ends after this stop_all leaves a session open
    // through the whole standby (two quick POWER taps).
    if (pa_resume_pid > 0) {
        process_reap(pa_resume_pid, 3000);
        pa_resume_pid = 0;
    }

    // pause playActivity (synchronous: the session must be closed before
    // processes are stopped; no shell needed)
    {
        char *const argv[] = {"playActivity", "stop_all", NULL};
        process_run_wait(argv);
    }

    if (temp_flag_get("stay_awake")) {
        // stay awake (keep processes running and volume on)
        timeout = -1; // no timeout
    }
    else {
        // suspend processes
        suspend(0);
        setVolume(0);
    }
    rumble(0);
    display_setBrightnessRaw(0);
    if (!kids)
        display_off();
    system_powersave_on();

    uint32_t repeat_power = 0;
    uint32_t killexit = 0;
    int suspend_lid_state = read_lid_state(); // Store initial lid state
    long suspend_start = getMilliseconds();
    const long standby_begin = suspend_start;
    // Input events read in this standby (for the log): any of them used
    // to start the wait before the power-off again (suspend_timer.h).
    int standby_events = 0;
    int last_type = -1, last_code = -1, last_value = -1;
    bool charging_logged = false;
    standby_log("standby: start, power-off after %d s (-1: none, stay awake)",
                timeout < 0 ? -1 : timeout / 1000);

    while (1) {
        // Flip: 500 ms polls for the lid. Mini and Mini Plus: the time left
        // of the timeout, counted from the start of the standby.
        int poll_timeout = (DEVICE_ID == MIYOO285)
                               ? 500
                               : suspendTimer_remaining(timeout, getMilliseconds() - suspend_start);
        int ready = poll(fds, 1, poll_timeout);

        if (ready > 0) {
            read(input_fd, &ev, sizeof(ev));
            standby_events++;
            last_type = ev.type;
            last_code = ev.code;
            last_value = ev.value;
            if ((ev.type != EV_KEY) || (ev.value > REPEAT))
                continue;
            if (ev.code == HW_BTN_POWER) {
                if (ev.value == RELEASED)
                    break;
                else if (ev.value == PRESSED)
                    repeat_power = 0;
                else if (ev.value == REPEAT) {
                    if (++repeat_power >= REPEAT_SEC(5)) {
                        short_pulse();
                        killexit = 1;
                        break;
                    }
                }
            }
            else if (ev.value == RELEASED) {
                if (ev.code == HW_BTN_MENU) {
                    // screenshot
                    system_powersave_off();
                    if (!kids)
                        display_on();
                    takeScreenshot();
                    break;
                }
            }
        }
        else if (!ready) {
            // Check lid state on flip devices before shutdown
            if (DEVICE_ID == MIYOO285) {
                int current_lid = read_lid_state();
                bool timed_out = timeout != -1 &&
                                 (getMilliseconds() - suspend_start) >= timeout;
                // Opening the lid wakes the Flip also while it charges; the
                // charger only holds off the timeout power-off, checked
                // (axp_test) only once the timeout has elapsed. Same as
                // OnionUI v4.5-dev: once it has elapsed off the charger,
                // power off whatever the lid state, like the other models.
                FlipSuspendStep step = flipSuspend_step(
                    current_lid, &suspend_lid_state, timed_out, battery_isCharging);
                if (step == FLIP_SUSPEND_WAKE) {
                    print_debug("Lid opened during suspend, waking up");
                    break;
                }
                if (step == FLIP_SUSPEND_POWER_OFF) {
                    if (standby_game_without_autosave()) {
                        suspend_start = getMilliseconds();
                        continue;
                    }
                    system_powersave_off();
                    resume();
                    usleep(150000);
                    deepsleep();
                    // Back here: the power-off has started, or a game with
                    // no auto save state was kept. Count a new timeout, as
                    // below: the old one stayed elapsed, and the power-off
                    // was run again every 500 ms.
                    suspend_start = getMilliseconds();
                }
                continue;
            }

            long standby_sec = (getMilliseconds() - standby_begin) / 1000;
            if (battery_isCharging()) {
                // As before: checked again after another timeout
                if (!charging_logged)
                    standby_log("standby: %ld s, charging: no power-off while it charges", standby_sec);
                charging_logged = true;
                suspend_start = getMilliseconds();
                continue;
            }

            // Original timeout shutdown behavior (non-flip devices)
            if (standby_game_without_autosave()) {
                suspend_start = getMilliseconds();
                continue;
            }
            standby_log("standby: %ld s, %d input events (last: type %d code %d value %d), powering off",
                        standby_sec, standby_events, last_type, last_code, last_value);
            system_powersave_off();
            resume();
            usleep(150000);
            deepsleep();
            // Back here when the power-off has started, or when deepsleep()
            // kept a game that has no auto save: wait another timeout, not
            // a poll(0) loop.
            if (!shutdown_in_progress())
                standby_log("standby: power-off refused (game without auto save state)");
            suspend_start = getMilliseconds();
        }
    }

    standby_log("standby: woken after %ld s, %d input events",
                (getMilliseconds() - standby_begin) / 1000, standby_events);

    // resume
    system_powersave_off();

    if (killexit) {
        resume();
        usleep(150000);
        suspend(2);
        usleep(400000);
    }
    if (!kids)
        display_on();
    display_setBrightness(settings.brightness);
    setVolume(settings.mute ? 0 : settings.volume);
    if (!killexit) {
        resume();
        if (!in_switcher) {
            // In background, but keep its pid: the next standby waits for it
            // before its stop_all (reaped on the 15 s tick otherwise).
            char *const argv[] = {"playActivity", "resume", NULL};
            process_reap(pa_resume_pid, 0);
            pa_resume_pid = process_spawn(argv);
        }
    }

    keyinput_enable();
}

void turnOffScreen(void)
{
    // Never suspend once a shutdown has begun: suspend(0) would stop
    // runtime.sh mid-shutdown and leave the device in standby instead of
    // off (Mini Flip: lid closed right after Power off).
    if (shutdown_in_progress()) {
        print_debug("Shutdown in progress, standby ignored");
        return;
    }
    if (settings.sleep_timer == 0) {
        suspend_exec(-1); // no sleep timeout
    }
    else {
        int timeout = (settings.sleep_timer + SHUTDOWN_MIN) * 60000;
        suspend_exec(timeout);
    }
}

/**
 * @brief Adjusts the CPU clock speed based on the given adjustment value and triggers an OSD message
 *
 * This function reads the current CPU clock speed, adjusts it by the specified
 * amount, and sets the new CPU clock speed if it falls within the allowed range.
 * It then triggers an OSD message to display the new CPU clock speed.
 * If the new CPU clock speed is outside the allowed range, a short pulse is triggered
 * and the function returns without making any changes.
 *
 * @param adjust The amount to adjust the CPU clock speed by (in MHz).
 *
 * @return void
 */
void cpuClockHotkey(int adjust)
{
    if (!flag_cpu_clock_hotkey) {
        return;
    }
    printf_debug("cpuClockHotkey: %d\n", adjust);
    int min_cpu_clock = 500; // ?
    int max_cpu_clock;
    switch (DEVICE_ID) {
    case MIYOO285:
    case MIYOO354:
        max_cpu_clock = 1800;
        break;
    case MIYOO283:
        max_cpu_clock = 1600;
        break;
    default:
        // Unknown device
        return;
    }
    // process_start_read_return() copies up to STR_MAX bytes: the buffer
    // must be STR_MAX (it was 5 bytes, i.e. a stack overflow on every use).
    char cpuclockstr[STR_MAX];
    int ret;

    // Read current CPU clock (only when unknown: saves one process spawn)
    if (cached_cpu_clock <= 0) {
        cpuclockstr[0] = '\0';
        process_start_read_return("cpuclock", cpuclockstr);
        cached_cpu_clock = atoi(cpuclockstr);
    }
    int cpuclock = cached_cpu_clock;
    printf_debug("Current CPU clock: %d\n", cpuclock);
    cpuclock += adjust;
    printf_debug("Desired CPU clock: %d\n", cpuclock);
    // Bounds check
    if (cpuclock < min_cpu_clock || cpuclock > max_cpu_clock) {
        printf_debug("Desired CPU clock %d out of range (%d, %d)\n", cpuclock, min_cpu_clock, max_cpu_clock);
        SDL_Surface *surface = theme_createTextOverlay("CPU clock out of range", (SDL_Color){255, 255, 255}, (SDL_Color){0, 0, 0}, 1.0, 10);
        if (surface && overlay_surface(surface, 10, 10, 1000, true) != 0) {
            SDL_FreeSurface(surface);
        }
        short_pulse();
        return;
    }

    // Set new CPU clock
    char cmd[STR_MAX];
    snprintf(cmd, STR_MAX, "cpuclock %d", cpuclock);
    ret = process_start_read_return(cmd, cpuclockstr);
    cached_cpu_clock = (ret == 0) ? atoi(cpuclockstr) : 0;
    if (ret == 0) {
        printf_debug("Updated CPU clock: %s\n", cpuclockstr);
        char osd_txt[STR_MAX];
        snprintf(osd_txt, STR_MAX, "CPU clock set to %s MHz", cpuclockstr);
        SDL_Surface *surface = theme_createTextOverlay(osd_txt, (SDL_Color){255, 255, 255}, (SDL_Color){0, 0, 0}, 1.0, 10);
        if (surface && overlay_surface(surface, 10, 10, 1000, true) != 0) {
            SDL_FreeSurface(surface);
        }
    }
}

static void signal_refresh(int sig)
{
    display_getRenderResolution();
}

//
//    Read lid state for Miyoo Mini Flip
//    Returns: 1 if lid is open, 0 if lid is closed, -1 on error
//
int read_lid_state(void)
{
    if (DEVICE_ID != MIYOO285) {
        return -1; // Not a flip
    }

    FILE *fp = fopen("/sys/devices/soc0/soc/soc:hall-mh248/hallvalue", "r");
    if (!fp) {
        return -1; // Error reading lid state
    }

    char buf[2];
    size_t read_bytes = fread(buf, 1, 1, fp);
    fclose(fp);

    if (read_bytes != 1) {
        return -1;
    }

    return (buf[0] == '1') ? 1 : 0; // '1' = open, '0' = closed
}

//
//    Main
//
int main(void)
{
    // Initialize
    signal(SIGTERM, quit);
    signal(SIGSEGV, quit);
    signal(SIGUSR1, signal_refresh);
    log_setName("keymon");

    getDeviceModel();

    printf_debug("Device detected: DEVICE_ID=%d (283=MM, 285=Flip, 354=Plus)", DEVICE_ID);

    if (HAS_AXP()) {
        // set hardware poweroff time to 10s. A failed read is -1, which
        // ORed and written gave 0xFF: every field of the power key register
        // at its highest value. Nothing is written then.
        int pek = axp_read(0x36);
        if (pek >= 0)
            axp_write(0x36, pek | 3);
    }

    settings_init();
    refresh_cached_flags();
    initHeadphoneJack();

    // Set Initial Volume / Brightness
    setVolume(settings.mute ? 0 : settings.volume);
    display_setBrightness(settings.brightness);
    printf_debug("Settings loaded. Brightness set to: %d\n",
                 settings.brightness);

    display_init(true);

    // Does nothing unless a standby was left unfinished.
    suspend_recover();

    // Prepare for Poll button input
    input_fd = open("/dev/input/event0", O_RDONLY);
    memset(&fds, 0, sizeof(fds));
    fds[0].fd = input_fd;
    fds[0].events = POLLIN;

    // Main Loop
    uint32_t button_flag = 0;
    uint32_t repeat_LR = 0;
    uint32_t repeat_power = 0;
    uint32_t val;
    KonamiState konami = {0};
    bool b_BTN_Not_Menu_Pressed = false;
    bool b_BTN_Menu_Pressed = false;
    bool power_pressed = false;
    bool kids_dimmed_at_power_press = false;
    bool volUp_state = false;
    bool volUp_active = false;
    bool volDown_state = false;
    bool volDown_active = false;
    bool comboKey_volume = false;
    bool comboKey_menu = false;
    bool comboKey_select = false;
    bool menuAndAPressed = false; // screen recorder
    bool menuAndBPressed = false; // blue light filter
    int menuAndAPressedTime = 0;
    int menuAndBPressedTime = 0;

    int ticks = getMilliseconds();
    int hibernate_start = ticks;
    int hibernate_time;
    int elapsed_sec = 0;

    // Lid state tracking for Miyoo Mini Flip
    int last_lid_state = -1;
    int current_lid_state = -1;

    if (DEVICE_ID == MIYOO285) {
        last_lid_state = read_lid_state();
        current_lid_state = last_lid_state;
        if (last_lid_state == -1) {
            // Fallback: assume open if read fails
            print_debug("Warning: Unable to read lid state, assuming open");
            last_lid_state = 1;
            current_lid_state = 1;
        }
        else {
            printf_debug("Initial lid state: %s", last_lid_state == 1 ? "open" : "closed");
        }
    }

    bool delete_flag = false;
    struct timespec state_flag_mtime = {0, 0};
    uint32_t last_state_scan = 0;
    bool settings_changed = false;

    int save_settings_timestamp = 0;
    volatile bool needWriteSettings = false;

    time_t fav_last_modified = time(NULL);

    while (1) {
        // Debounced settings save. Runs whether or not a key arrives, so the
        // last change of a burst is written SETTINGS_SAVE_DELAY_MS after it
        // (before: only on the first key press after the next 15 s tick).
        if (needWriteSettings &&
            getMilliseconds() - save_settings_timestamp >= SETTINGS_SAVE_DELAY_MS) {
            settings_save_local();
            needWriteSettings = false;
        }

        int poll_timeout_ms = (DEVICE_ID == MIYOO285) ? 500 : ((CHECK_SEC - elapsed_sec) * 1000);
        if (poll_timeout_ms < 0)
            poll_timeout_ms = 0;
        if (needWriteSettings) {
            int save_wait = SETTINGS_SAVE_DELAY_MS - (int)(getMilliseconds() - save_settings_timestamp);
            if (save_wait < 0)
                save_wait = 0;
            if (save_wait < poll_timeout_ms)
                poll_timeout_ms = save_wait;
        }
        int poll_result = poll(fds, 1, poll_timeout_ms);

        if (poll_result > 0) {
            if (!keyinput_isValid())
                continue;
            val = ev.value;

            printf_debug("Keymon input: code=%d, value=%d\n", ev.code,
                         ev.value);

            // /tmp is tmpfs: removing a flag there needs no sync() (which
            // flushed every filesystem, SD card included, on a key press).
            if (exists("/tmp/settings_changed")) {
                remove("/tmp/settings_changed");
                if (needWriteSettings) {
                    // write our pending change first so it is not lost
                    settings_save_local();
                    needWriteSettings = false;
                }
                settings_reload();
                refresh_cached_flags();
            }

            struct stat state_flag;
            if (stat("/tmp/state_changed", &state_flag) == 0) {
                // The flag's mtime changes each time runtime.sh touches it.
                bool touched = state_flag.st_mtim.tv_sec != state_flag_mtime.tv_sec ||
                               state_flag.st_mtim.tv_nsec != state_flag_mtime.tv_nsec;
                uint32_t now = (uint32_t)getMilliseconds();
                if (stateScan_due(touched, delete_flag, now, last_state_scan)) {
                    state_flag_mtime = state_flag.st_mtim;
                    last_state_scan = now;
                    system_state_update();
                }
                cached_cpu_clock = 0; // launches/exits change the CPU clock

                if (delete_flag) {
                    remove("/tmp/state_changed");
                    delete_flag = false;
                }

                if (system_state == MODE_MAIN_UI)
                    display_setBrightness(settings.brightness);
            }

            settings_changed = false;
            osd_bar_activated = false;

            if (val != REPEAT) {

                if (ev.code == HW_BTN_MENU)
                    b_BTN_Menu_Pressed = val == PRESSED;
                else
                    b_BTN_Not_Menu_Pressed = val == PRESSED;

                if (b_BTN_Menu_Pressed && b_BTN_Not_Menu_Pressed)
                    comboKey_menu = true;
            }

            if (system_state == MODE_MAIN_UI && (ev.code == HW_BTN_B || ev.code == HW_BTN_X) && val == RELEASED) {
                // Check if favorite file changed
                if (file_isModified(FAVORITES_PATH, &fav_last_modified)) {
                    char *const argv[] = {"tools", "favfix", NULL};
                    process_run_wait(argv);
                    sync(); // favfix rewrote a file on the SD card
                }
            }

            switch (ev.code) {
            case HW_BTN_POWER:
                if (val == PRESSED) {
                    power_pressed = true;
                    // Kids Mode: a press on a screen its idle timer dimmed
                    // or blanked only wakes it; otherwise the backlight goes
                    // black at once, so no framebuffer change can flash.
                    kids_dimmed_at_power_press = false;
                    if (kidsmode_active()) {
                        kids_dimmed_at_power_press =
                            temp_flag_get("kidsmode_carousel_dimmed") ||
                            temp_flag_get("kidsmode_media_dimmed");
                        if (!kids_dimmed_at_power_press)
                            display_setBrightnessRaw(0);
                    }
                }
                if (!comboKey_menu && val == REPEAT) {
                    repeat_power++;
                    if (repeat_power == 7) {
                        deepsleep(); // 0.5sec deepsleep
                    }
                    else if (repeat_power >= REPEAT_SEC(5)) {
                        short_pulse();
                        remove(CMD_TO_RUN_PATH);
                        force_shutdown(); // 10sec force shutdown
                    }
                    break;
                }
                if (val == RELEASED) {
                    // suspend
                    if (power_pressed && repeat_power < 7) {
                        if (comboKey_menu) {
                            // KidsPlay ignores the MENU release that follows
                            if (kidsmode_active())
                                temp_flag_set("kidsmode_power_screenshot", true);
                            takeScreenshot();
                        }
                        else if (kids_dimmed_at_power_press) {
                            display_setBrightness(settings.brightness);
                            temp_flag_set("kidsmode_carousel_dimmed", false);
                            temp_flag_set("kidsmode_media_dimmed", false);
                        }
                        else {
                            if (settings.disable_standby) {
                                deepsleep();
                            }
                            else {
                                turnOffScreen();
                            }
                        }
                    }
                    power_pressed = false;
                    kids_dimmed_at_power_press = false;
                }
                repeat_power = 0;
                break;
            case HW_BTN_SELECT:
                // MainUI only runs while cmd_to_run.sh is absent: with it
                // present (a game or an app is running) SELECT cannot be
                // meant for MainUI, so skip the /proc scan (games use
                // SELECT a lot).
                if (!comboKey_select && val == RELEASED && !exists(CMD_TO_RUN_PATH)) {
                    // The cached state is only refreshed on /tmp/state_changed,
                    // a MENU press or deepsleep(), so it can still be
                    // MODE_UNKNOWN after boot or stale after returning from a
                    // game. Re-read it here; this fires once per SELECT tap.
                    system_state_update();

                    if (system_state == MODE_MAIN_UI) {
                        keyinput_send(HW_BTN_MENU, PRESSED);
                        keyinput_send(HW_BTN_MENU, RELEASED);
                    }
                }
                if (val != REPEAT)
                    button_flag =
                        (button_flag & (~SELECT)) | (val << SELECT_BIT);
                if (val == RELEASED)
                    comboKey_select = false;
                break;
            case HW_BTN_START:
                if (val != REPEAT)
                    button_flag = (button_flag & (~START)) | (val << START_BIT);
                break;
            case HW_BTN_R1:
                if (val == PRESSED && (button_flag & (SELECT | START)) == (SELECT | START)) {
                    cpuClockHotkey(100);
                }
                break;
            case HW_BTN_L1:
                if (val == PRESSED && (button_flag & (SELECT | START)) == (SELECT | START)) {
                    cpuClockHotkey(-100);
                }
                break;
            case HW_BTN_L2:
                if (val == REPEAT) {
                    // Adjust repeat speed to 1/2
                    val = repeat_LR;
                    repeat_LR ^= PRESSED;
                }
                else {
                    button_flag = (button_flag & (~L2)) | (val << L2_BIT);
                    repeat_LR = 0;
                }
                if (val == PRESSED) {
                    switch (button_flag & (SELECT | START)) {
                    case START:
                        // START + L2 : audio boost down / + R2 : reset volume
                        if (button_flag & R2)
                            setVolume(settings.volume);
                        else
                            setVolumeRaw(0, -3);
                        break;
                    case SELECT:
                        if (IS_MIYOO_PLUS_OR_FLIP())
                            break; // disable this shortcut for MMP/MMF
                        // SELECT + L2 : brightness down
                        if (flag_alt_brightness)
                            break;
                        if (settings.brightness > 0) {
                            settings_setBrightness(settings.brightness - 1,
                                                   true, false);
                            settings_changed = true;
                        }
                        osd_showBrightnessBar(settings.brightness);
                        comboKey_select = true;
                        break;
                    default:
                        break;
                    }
                }
                break;
            case HW_BTN_R2:
                if (val == REPEAT) {
                    // Adjust repeat speed to 1/2
                    val = repeat_LR;
                    repeat_LR ^= PRESSED;
                }
                else {
                    button_flag = (button_flag & (~R2)) | (val << R2_BIT);
                    repeat_LR = 0;
                }
                if (val == PRESSED) {
                    switch (button_flag & (SELECT | START)) {
                    case START:
                        // START + R2 : audio boost up / + L2 : reset volume
                        if (button_flag & L2)
                            setVolume(settings.volume);
                        else
                            setVolumeRaw(0, +3);
                        break;
                    case SELECT:
                        if (IS_MIYOO_PLUS_OR_FLIP())
                            break; // disable this shortcut for MMP/MMF
                        // SELECT + R2 : brightness up
                        if (flag_alt_brightness)
                            break;
                        if (settings.brightness < MAX_BRIGHTNESS) {
                            settings_setBrightness(settings.brightness + 1,
                                                   true, false);
                            settings_changed = true;
                        }
                        osd_showBrightnessBar(settings.brightness);
                        comboKey_select = true;
                        break;
                    default:
                        break;
                    }
                }
                break;
            case HW_BTN_MENU:

                if (!temp_flag_get("disable_menu_button")) {
                    // Key repeats outside a combo only compare timestamps
                    // with the state saved on the press: no need to rescan
                    // /proc about 20 times a second while MENU is held.
                    if (val != REPEAT || comboKey_menu)
                        system_state_update();
                    comboKey_menu = menuButtonAction(val, comboKey_menu);
                }
                else if (val == RELEASED) {
                    // MENU belongs to the port: no combo outlives it (only
                    // menuButtonAction() cleared it; POWER took a screenshot
                    // and the power-off by POWER was skipped until the next
                    // MENU press, also after the port).
                    comboKey_menu = false;
                }

                break;
            case HW_BTN_X:
            case HW_BTN_Y:
                // The cached state can still be the app or game just left:
                // runtime.sh touches /tmp/state_changed before MainUI starts,
                // and a key read in between scanned (no MainUI yet) and
                // removed the flag. X then did nothing on the home screen
                // until the next state change, "not always" (4.5.1,
                // Amiga500). Read it again on the press, as SELECT does; not
                // in a game or app (cmd_to_run.sh), where X and Y are played.
                if (val == PRESSED && !exists(CMD_TO_RUN_PATH))
                    system_state_update();
                if (ev.code == HW_BTN_X && val == PRESSED && system_state == MODE_MAIN_UI)
                    temp_flag_set("launch_alt", false);
                if (val == PRESSED)
                    applyExtraButtonShortcut(ev.code == HW_BTN_X ? 0 : 1);
                break;
            case HW_BTN_A:
                if (val == PRESSED) {
                    if (b_BTN_Menu_Pressed) {
                        menuAndAPressed = true;
                        menuAndAPressedTime = getMilliseconds();
                    }
                }
                else if (val == RELEASED) {
                    menuAndAPressed = false;
                }
                break;
            case HW_BTN_B:
                if (val == PRESSED) {
                    if (b_BTN_Menu_Pressed) {
                        menuAndBPressed = true;
                        menuAndBPressedTime = getMilliseconds();
                    }
                }
                else if (val == RELEASED) {
                    menuAndBPressed = false;
                }
                if (val == PRESSED && system_state == MODE_MAIN_UI)
                    temp_flag_set("launch_alt", false);
                break;
            case HW_BTN_VOLUME_DOWN:
                if (comboKey_menu) {
                    // MENU + VOL DOWN : brightness down
                    if (val != RELEASED && settings.brightness > 0) {
                        settings_setBrightness(settings.brightness - 1, true,
                                               false);
                        settings_changed = true;
                    }
                    osd_showBrightnessBar(settings.brightness);
                    // Pressed before MENU, it is released here: without
                    // recording it, it stayed "pressed", and the next VOL+
                    // looked like both keys together (mute toggled, VOL+
                    // dead until VOL- was pressed alone)
                    if (val == RELEASED) {
                        volDown_state = false;
                        volDown_active = false;
                    }
                    break;
                }
                if (val == PRESSED)
                    volDown_active = true;
                volDown_state = val;
                if (!volDown_active || comboKey_volume)
                    break;
                if (val == RELEASED || val == REPEAT) {
                    // Kids Mode: while muted, only the level to come back to
                    // changes (applying it would unmute the media player).
                    if (settings_setVolume(settings.volume - 1,
                                           !(settings.mute && kidsmode_active())))
                        settings_changed = true;
                }
                if (val == RELEASED)
                    volDown_active = false;
                osd_showVolumeBar(settings.volume, settings.mute);
                break;
            case HW_BTN_DOWN:
                if (DEVICE_ID == MIYOO283) {
                    if (comboKey_menu) {
                        if (flag_alt_brightness) {
                            // MENU + BTN DOWN : brightness down
                            if (val != RELEASED && settings.brightness > 0) {
                                settings_setBrightness(settings.brightness - 1, true,
                                                       false);
                                settings_changed = true;
                            }
                            osd_showBrightnessBar(settings.brightness);
                        }
                    }
                }
                break;
            case HW_BTN_VOLUME_UP:
                if (comboKey_menu) {
                    // MENU + VOL UP : brightness up
                    if (val != RELEASED &&
                        settings.brightness < MAX_BRIGHTNESS) {
                        settings_setBrightness(settings.brightness + 1, true,
                                               false);
                        settings_changed = true;
                    }
                    osd_showBrightnessBar(settings.brightness);
                    // (as for VOL- above)
                    if (val == RELEASED) {
                        volUp_state = false;
                        volUp_active = false;
                    }
                    break;
                }
                if (val == PRESSED)
                    volUp_active = true;
                volUp_state = val;
                if (!volUp_active || comboKey_volume)
                    break;
                if (val == RELEASED || val == REPEAT) {
                    if (settings.mute && kidsmode_active()) {
                        // Kids Mode: VOL+ does not unmute (VOL- with VOL+
                        // does); only the level to come back to changes.
                        if (settings_setVolume(settings.volume + 1, false))
                            settings_changed = true;
                    }
                    else if (settings_setMute(0, true)) {
                        settings_changed = true;
                    }
                    else if (settings_setVolume(settings.volume + 1, true))
                        settings_changed = true;
                }
                if (val == RELEASED)
                    volUp_active = false;
                osd_showVolumeBar(settings.volume, settings.mute);
                break;
            case HW_BTN_UP:
                if (DEVICE_ID == MIYOO283) {
                    if (comboKey_menu) {
                        if (flag_alt_brightness) {
                            // MENU + BTN UP : brightness up
                            if (val != RELEASED &&
                                settings.brightness < MAX_BRIGHTNESS) {
                                settings_setBrightness(settings.brightness + 1, true,
                                                       false);
                                settings_changed = true;
                            }
                            osd_showBrightnessBar(settings.brightness);
                        }
                    }
                }
                break;
            default:
                break;
            }

            // start screen recording after holding for >2secs
            if (menuAndAPressed && (getMilliseconds() - menuAndAPressedTime >= 2000)) {
                // Read the flag itself, as Onion does: Tweaks writes it at
                // once, while settings.rec_hotkey is only reloaded when
                // Tweaks exits. One stat() on a 2 s key hold.
                if (config_flag_get(".recHotkey")) {
                    run_script_detached(SCREEN_RECORDER_SCRIPT, "toggle");
                }

                menuAndAPressed = false;
                menuAndAPressedTime = 0;
            }

            // toggle blue light filter
            if (menuAndBPressed && (getMilliseconds() - menuAndBPressedTime >= 2000) &&
                kidsmode_active()) {
                // Kids Mode: no blue light toggle from the buttons
                menuAndBPressed = false;
                menuAndBPressedTime = 0;
            }
            if (menuAndBPressed && (getMilliseconds() - menuAndBPressedTime >= 2000)) {
                if (temp_flag_get(".blfOn")) {
                    run_script_detached(BLUE_LIGHT_SCRIPT, "disable");
                }
                else {
                    run_script_detached(BLUE_LIGHT_SCRIPT, "enable");
                }
                temp_flag_set(".blfIgnoreSchedule", true);

                menuAndBPressed = false;
                menuAndBPressedTime = 0;
            }

            if (val == PRESSED && !osd_bar_activated) {
                osd_hideBar();
            }

            // Mute toggle
            if (volDown_state != RELEASED && volUp_state != RELEASED &&
                !comboKey_volume) {
                comboKey_volume = true;
                if (settings_setMute(!settings.mute, true))
                    settings_changed = true;
                osd_showVolumeBar(settings.volume, settings.mute);
            }
            else if (volDown_state == RELEASED && volUp_state == RELEASED)
                comboKey_volume = false;

            if (settings_changed) {
                settings_shm_write();
                needWriteSettings = true;
                // was `ticks`, which only advances every 15 s: the save
                // then waited for an unrelated later key press
                save_settings_timestamp = getMilliseconds();
            }

            // Konami code: the easter egg, from MainUI (konami.h). The
            // state is read when the code is complete: the cached one can
            // still be the game's just after coming back to MainUI.
            if (val == PRESSED &&
                konami_feed(&konami, KONAMI_CODE, KONAMI_CODE_LENGTH, ev.code)) {
                system_state_update();
                if (system_state == MODE_MAIN_UI) {
                    FILE *file =
                        fopen("/mnt/SDCARD/.tmp_update/cmd_to_run.sh", "w");
                    if (file != NULL) {
                        fputs("cd /mnt/SDCARD/.tmp_update/bin; ./easter", file);
                        fclose(file);
                        kill_mainUI();
                    }
                }
            }

            if (system_state == MODE_MAIN_UI) {
                // Debounced like keymon's own changes: MainUI saves
                // system.json itself, and a save here on every key event
                // held the lock Open MainUI needs for its next change.
                if (settings_shm_pull()) {
                    needWriteSettings = true;
                    save_settings_timestamp = getMilliseconds();
                }
                display_setBrightness(settings.brightness);
            }

            hibernate_start = getMilliseconds();
        }

        // Check lid state for Miyoo Mini Flip - poll every iteration (500ms)
        if (DEVICE_ID == MIYOO285) {
            current_lid_state = read_lid_state();

            if (current_lid_state != -1 && last_lid_state != -1 &&
                current_lid_state != last_lid_state) {

                printf_debug("Lid state change: %d -> %d", last_lid_state, current_lid_state);

                if (current_lid_state == 0) {
                    printf_debug("Lid closed detected, action: %d", settings.lid_close_action);

                    switch (flipLid_onClose(settings.lid_close_action, settings.disable_standby,
                                            shutdown_in_progress())) {
                    case FLIP_LID_SUSPEND:
                        /* Lid suspend must not inherit Power-button disable_standby
                         * (#228). That flag is Power-only. */
                        turnOffScreen();
                        hibernate_start = getMilliseconds();
                        // Back from standby: stored below as the last state,
                        // "open" lets the next pass see a closed lid as a
                        // change. Woken by POWER with the lid closed, or with
                        // the lid opened and closed again during the wake-up,
                        // the Flip stayed on inside the closed lid (only a
                        // change was acted on). An open lid changes nothing.
                        current_lid_state = 1;
                        break;
                    case FLIP_LID_SHUTDOWN:
                        print_debug("Shutting down due to lid close");
                        deepsleep();
                        break;
                    case FLIP_LID_NOTHING:
                        print_debug("Lid close action: Nothing");
                        break;
                    }
                }
                else {
                    print_debug("Lid opened detected");
                }

                last_lid_state = current_lid_state;
            }
            // Update last_lid_state even if it hasn't changed to handle initial -1 case
            else if (current_lid_state != -1 && last_lid_state == -1) {
                last_lid_state = current_lid_state;
            }
        }

        checkHeadphoneJack();

        elapsed_sec = (getMilliseconds() - ticks) / 1000;
        if (elapsed_sec < CHECK_SEC)
            continue;

        if (delete_flag) {
            if (exists("/tmp/state_changed")) {
                system_state_update();
                cached_cpu_clock = 0;
                remove("/tmp/state_changed");
            }
            delete_flag = false;
        }
        else {
            delete_flag = true;
        }

        // Update ticks
        ticks = getMilliseconds();

        if (pa_resume_pid > 0 && process_reap(pa_resume_pid, 0))
            pa_resume_pid = 0;

        // Check Hibernate
        if (battery_isCharging())
            hibernate_time = 0;
        else
            hibernate_time = settings.sleep_timer;

        // A video or song playing in Kids Mode, or a video in the Video
        // Player, is not inactivity. Not stay_awake: that one also keeps
        // everything running in standby (POWER, Flip lid closed), and the
        // video went on playing with the screen off (#350).
        if (temp_flag_get("kidsmode_media_playing") || temp_flag_get("video_playing")) {
            hibernate_start = ticks;
        }
        else if (hibernate_time && !temp_flag_get("stay_awake")) {
            if (ticks - hibernate_start > hibernate_time * 60 * 1000) {
                suspend_exec(SHUTDOWN_MIN * 60000);
                hibernate_start = ticks;
            }
        }

        // Safety net for flags changed without a settings_changed notice
        refresh_cached_flags();

        // Check bluelight filter (in-process; the script only runs on a
        // transition, in background)
        if (settings.blue_light_schedule) {
            blue_light_schedule_check();
        }

        // Quit RetroArch / auto-save when battery too low. Not on the
        // charger (plugged in at the level, the game went on to be closed
        // anyway), nor with no reading (low_battery.h), and only with a game
        // in front: system_state is otherwise refreshed only on a state
        // change, so it is read again here.
        if (settings.low_battery_autosave_at &&
            lowBattery_levelReached(battery_chargingLevel(), settings.low_battery_autosave_at) &&
            !battery_isCharging()) {
            system_state_update();
            if (lowBattery_inGame(system_state) && check_autosave()) {
                temp_flag_set(".lowBat", true);
                screenshot_system();
                terminate_retroarch();
                terminate_drastic();
            }
        }

        elapsed_sec = 0;
    }
}
