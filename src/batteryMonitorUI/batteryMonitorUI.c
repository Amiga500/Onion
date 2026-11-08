#include "./batteryMonitorUI.h"
#include "system/device_model.h"

#include "../batmon/batmonDB.h"
#include <pthread.h>
#include <sched.h>

extern int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *cpuset);
#define _LARGEFILE64_SOURCE 1
#include <sys/stat.h>

static bool quit = false;
static int current_zoom = 1;
static int current_page = 0;
static int current_index;

// Zoom level
static int segment_duration;

static char label[4][4];
static SDL_Color color_white = {255, 255, 255};
static SDL_Color color_pastel_blue = {89, 167, 255};
static int graph_max_size = GRAPH_MAX_FULL_PAGES * GRAPH_DISPLAY_SIZE_X;
static int estimation_line_size = 0;
static int begining_session_index;
static char session_duration[10];
static char current_percentage[10];
static char session_left[10];
static char session_best[10];
static char session_avg[12];
static char session_min[12];
static char battery_voltage[16];

static TTF_Font *font_small = NULL;
static TTF_Font *font_tiny = NULL;

// AXP221 Telemetry & Menu State
static bool show_axp_menu = false;
static bool axp_confirm_calib = false;
static bool axp_confirm_btest = false;
static bool axp_confirm_cap = false; // saving a capacity while the battery is still charging
enum {
    AXP_MENU_CAPACITY = 0,
    AXP_MENU_VOFF,
    AXP_MENU_SYNC,
    AXP_MENU_CALIB,
    AXP_MENU_BTEST,
    AXP_MENU_COUNT
};
static int axp_menu_selected = AXP_MENU_CAPACITY;
static int axp_edit_capacity = 3000;
static int axp_edit_voff = 3; // REG 31H bits 2-0 code: 0..7 -> 2.6..3.3 V (default 3 = 2.9 V)
#define AXP_VOFF_CODE_MAX 5   // UI limit: 3.1 V (3.2/3.3 V would waste too much usable capacity)
static Uint32 axp_last_poll = 0;
static char axp_feedback_msg[64] = "";
static Uint32 axp_feedback_until = 0;
static volatile sig_atomic_t sigcont_received = 0;

static void sigcont_handler(int sig)
{
    (void)sig;
    sigcont_received = 1;
}

typedef struct {
    bool available;
    int voltage_mv;
    int current_ma;
    bool is_charging;
    int battery_pct;
    int ocv_pct;
    int cc_pct;
    int capacity_mah;
    int cc_mah;
    bool gauge_en;
    bool cc_en;
    bool cal_en;     // REG B8H bit 5: capacity correction function enabled (persistent switch)
    bool cal_active; // REG B8H bit 4: capacity correction currently in progress
    bool pct_valid;  // REG B9H bit 7: battery percentage correctly calculated
    bool cap_set;    // REG E0H/E1H holds a capacity (bit 15 set, value > 0); factory default 0
    bool cc_valid;   // REG E5H bit 7 with a capacity set; clear on a fresh PMIC (E5H then reads 100)
    int voff_code;   // REG 31H bits 2-0: auto power-off voltage code, -1 if unknown
} axp_telemetry_t;

static int voff_code_to_mv(int code)
{
    return 2600 + code * 100;
}

static axp_telemetry_t axp_status = {.voff_code = -1};

typedef struct {
    int pixel_height;
    bool is_charging;
    bool is_estimated;
} graph_spot;

graph_spot graphic[GRAPH_MAX_FULL_PAGES * GRAPH_DISPLAY_SIZE_X];

// Sessions on battery found while building the graph, newest first, as graphic[] indices
typedef struct {
    int start_index;
    int end_index;
    int length; // seconds
    int start_level;
    int end_level;
    bool is_current;
    bool is_charge; // a charge, not a session on battery
    int tail;       // a charge: seconds on the charger after reaching end_level
    // clock times (Unix) of the start, the end and, for a charge, reaching end_level; only
    // when has_times (the log has start_time for every entry and the clock held steady)
    bool has_times;
    long long start_time, end_time, tail_time;
    long long sleep; // a session: seconds asleep within it (clock gaps between entries)
    int sleep_drop;  // a session: percent lost across those gaps
} session_mark;

static session_mark session_marks[MAX_SESSION_MARKS];
static int session_mark_count = 0;
static int selected_session = -1; // -1: none, the normal view
//int graphic_size;

static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}

// From Amiga500/Onion e1942134: a picture that failed to load is skipped, not blitted
static void safeBlitSurface(SDL_Surface *src, SDL_Rect *srcrect,
                            SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src != NULL && dst != NULL)
        SDL_BlitSurface(src, srcrect, dst, dstrect);
}

static SDL_Surface *video;
static SDL_Surface *screen;
static SDL_Surface *background;
static SDL_Surface *waiting_screen;
static SDL_Surface *right_arrow;
static SDL_Surface *left_arrow;
static SDL_Surface *end_graph;
static TTF_Font *font_Arkhip;
static TTF_Font *font_title;

void init(void)
{
    log_setName("batteryMonitorUI");
    // the dates are local time: runtime.sh passes Onion's time zone (config/.tz) as TZ to
    // what it launches; read it ourselves when started some other way
    if (getenv("TZ") == NULL) {
        char tz[64] = "";
        FILE *tzf = fopen("/mnt/SDCARD/.tmp_update/config/.tz", "r");
        if (tzf != NULL) {
            if (fgets(tz, sizeof(tz), tzf) != NULL) {
                tz[strcspn(tz, "\r\n")] = '\0';
                if (tz[0] != '\0')
                    setenv("TZ", tz, 1);
            }
            fclose(tzf);
        }
    }
    tzset();
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 50);
    TTF_Init();

    video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    waiting_screen = IMG_Load("./res/waiting_screen.png");
    background = IMG_Load("./res/background.png");
    right_arrow = IMG_Load("./res/right_arrow.png");
    left_arrow = IMG_Load("./res/left_arrow.png");
    end_graph = IMG_Load("./res/end.png");

    font_Arkhip = TTF_OpenFont("./res/Arkhip_font.ttf", 15);
    font_title = TTF_OpenFont("./res/Arkhip_font.ttf", TITLE_FONT_SIZE);
    font_small = TTF_OpenFont("./res/Arkhip_font.ttf", 12);
    font_tiny = TTF_OpenFont("./res/Arkhip_font.ttf", 10);
}

void free_resources(void)
{
    if (font_Arkhip != NULL)
        TTF_CloseFont(font_Arkhip);
    if (font_title != NULL)
        TTF_CloseFont(font_title);
    if (font_small != NULL)
        TTF_CloseFont(font_small);
    if (font_tiny != NULL)
        TTF_CloseFont(font_tiny);
    TTF_Quit();

    SDL_FreeSurface(waiting_screen);
    SDL_FreeSurface(background);
    SDL_FreeSurface(right_arrow);
    SDL_FreeSurface(left_arrow);
    SDL_FreeSurface(end_graph);

    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    SDL_Quit();
}

void secondsToHoursMinutes(int seconds, char *output)
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    sprintf(output, "%dh%02d", hours, minutes);
}

void drawLine(int x1, int y1, int x2, int y2, Uint32 color)
{
    int dx, dy, sx, sy, err, e2;

    dx = abs(x2 - x1);
    dy = abs(y2 - y1);

    if (x1 < x2) {
        sx = 1;
    }
    else {
        sx = -1;
    }

    if (y1 < y2) {
        sy = 1;
    }
    else {
        sy = -1;
    }

    err = dx - dy;

    while (1) {
        SDL_Rect pixel = {x1, y1, 1, 1};
        SDL_FillRect(screen, &pixel, color);

        if (x1 == x2 && y1 == y2) {
            break;
        }

        e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

int _renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect, bool right_align)
{
    int text_width = 0;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (textSurface != NULL) {
        text_width = textSurface->w;
        if (right_align)
            SDL_BlitSurface(textSurface, NULL, screen, &(SDL_Rect){rect->x - textSurface->w, rect->y, rect->w, rect->h});
        else
            SDL_BlitSurface(textSurface, NULL, screen, rect);
        SDL_FreeSurface(textSurface);
    }
    return text_width;
}

int renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, false);
}

int renderTextAlignRight(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, true);
}

void switch_zoom_profile(int segment_duration)
{

    switch (segment_duration) {
    case 7200:
        // A segemmt is 120 minutes
        sprintf(label[0], "%s", "4h");
        sprintf(label[1], "%s", "8h");
        sprintf(label[2], "%s", "12h");
        sprintf(label[3], "%s", "16h");
        break;
    case 3600:
        // A segemmt is 60 minutes
        sprintf(label[0], "%s", "2h");
        sprintf(label[1], "%s", "4h");
        sprintf(label[2], "%s", "6h");
        sprintf(label[3], "%s", "8h");
        break;
    case 1800:
        // A segemmt is 30 minutes
        sprintf(label[0], "%s", "1h");
        sprintf(label[1], "%s", "2h");
        sprintf(label[2], "%s", "3h");
        sprintf(label[3], "%s", "4h");
        break;

    default:
        sprintf(label[0], "%s", "");
        sprintf(label[1], "%s", "");
        sprintf(label[2], "%s", "");
        sprintf(label[3], "%s", "");
        break;
    }
}

void render_waiting_screen()
{
    safeBlitSurface(waiting_screen, NULL, screen, NULL);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
    sleep(1.5);
}

int battery_to_pixel(int battery_perc)
{
    // Converts a battery percentage to a pixel coordinate
    int y = (int)((GRAPH_DISPLAY_SIZE_Y * battery_perc) / 100) + GRAPH_DISPLAY_START_Y;

    if ((y < 0) || (y > 480)) {
        return -1;
    }
    else {
        return y;
    }
}

int duration_to_pixel(int duration)
{
    // Convert a duration to a number of pixel
    // (At a scale of zoom = 1:1, one segment = 30mn)
    // 270 mn total, 16200 seconds
    return (int)((GRAPH_DISPLAY_SIZE_X * duration) / GRAPH_DISPLAY_DURATION);
}

// compute_graph() expects a cleared graph, so a refresh starts from scratch
void reset_graph(void)
{
    memset(graphic, 0, sizeof(graphic));
    estimation_line_size = 0;
    begining_session_index = 0;
    session_duration[0] = '\0';
    current_percentage[0] = '\0';
    session_left[0] = '\0';
    session_best[0] = '\0';
    session_avg[0] = '\0';
    session_min[0] = '\0';
    session_mark_count = 0;
}

typedef struct {
    int level;    // percent (500 or more while charging on the Mini)
    int duration; // seconds awake at this level; batmon updates the newest row every 10 min
    bool charging;
} log_row;

// The whole log of this device, oldest first (batmon keeps the newest FILO_MIN_SIZE rows).
// Returns the row count; *out is malloc'ed (NULL when empty).
int load_log_rows(log_row **out)
{
    int n = 0, cap = 0;
    log_row *rows = NULL;

    if (open_battery_log_db() == 1) {
        if (bat_log_db != NULL) {
            const char *sql = "SELECT bat_level, duration, is_charging FROM bat_activity WHERE device_serial = ? ORDER BY id ASC;";
            sqlite3_stmt *stmt;

            if (sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, DEVICE_SN, -1, SQLITE_STATIC);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    if (n == cap) {
                        cap = cap ? cap * 2 : 1024;
                        log_row *grown = realloc(rows, cap * sizeof(log_row));
                        if (grown == NULL)
                            break;
                        rows = grown;
                    }
                    rows[n].level = sqlite3_column_int(stmt, 0);
                    rows[n].duration = sqlite3_column_int(stmt, 1);
                    rows[n].charging = sqlite3_column_int(stmt, 2);
                    if (rows[n].duration < 0)
                        rows[n].duration = 0;
                    n++;
                }
            }
            sqlite3_finalize(stmt);
            close_battery_log_db();
        }
    }
    *out = rows;
    return n;
}

// AVG, MIN and BEST: average, shortest and longest finished session on battery in the
// log. A session counts only when the log holds both its start (a charging entry before
// it) and its end (the charger plugged in), as batmon measures a session. BEST comes from
// the log too, not batmon's all-time record, which may date from a fresher battery.
// Short unplugs say nothing about battery life, so AVG and MIN leave out sessions under
// GRAPH_MIN_SESSION_FOR_ESTIMATION.
void compute_session_stats(const log_row *rows, int n)
{
    int count = 0, total = 0, shortest = 0, longest = 0;
    bool started = false; // seen a charging entry, so the session's start is known
    bool in_session = false;
    int length = 0;

    for (int i = 0; i < n; i++) {
        if (rows[i].charging) {
            if (in_session) {
                if (length > longest)
                    longest = length;
                if (length >= GRAPH_MIN_SESSION_FOR_ESTIMATION) {
                    total += length;
                    if (count == 0 || length < shortest)
                        shortest = length;
                    count++;
                }
            }
            started = true;
            in_session = false;
            length = 0;
        }
        else if (started) {
            in_session = true;
            length += rows[i].duration;
        }
    }

    session_avg[0] = session_min[0] = session_best[0] = '\0';
    if (count > 0) {
        secondsToHoursMinutes(total / count, session_avg);
        secondsToHoursMinutes(shortest, session_min);
    }
    if (longest > 0)
        secondsToHoursMinutes(longest, session_best);
}

// LEFT: seconds of play left in the current session, or -1 when it can't be told
// (charging, no history). Backtested on a real log (2026-11-02): mean error ~4 min
// when predicting the time down to 10 %, against 23-29 min for the old straight line
// through the session.
//
// 1. The battery's discharge curve from the history: how long each 1 % lasted, per
//    level. A row at level a followed by level b < a spreads its time over a..b+1. A
//    drop of more than LEFT_MAX_STEP % at once is a gauge jump, not a drain, and is
//    skipped. Each level averages its neighbours within LEFT_CURVE_WINDOW.
// 2. The current session's load: its real time for the levels it went through against
//    the curve's time for them. The factor is trusted in proportion to the percent
//    drained so far, fully from LEFT_FULL_TRUST_PCT on.
// 3. LEFT = the curve's time from the current level down to 0, times that factor, less
//    the time already spent at the current level.
int estimate_left_seconds(const log_row *rows, int n)
{
    if (n == 0 || rows[n - 1].charging)
        return -1;

    // the current session: the rows after the last charging entry
    int s = n - 1;
    while (s > 0 && !rows[s - 1].charging)
        s--;

    // 1. per-level seconds from the stretches on battery before this session (it is
    //    measured against them in step 2); with no history, from this session itself
    static double sum[101];
    static int cnt[101];
    double all_sum = 0;
    int all_cnt = 0;
    for (int limit = s; all_cnt == 0 && limit <= n; limit = (limit < n) ? n : n + 1) {
        memset(sum, 0, sizeof(sum));
        memset(cnt, 0, sizeof(cnt));
        for (int i = 0; i + 1 < limit; i++) {
            if (rows[i].charging || rows[i + 1].charging)
                continue;
            int a = rows[i].level, b = rows[i + 1].level;
            if (a > 100 || b >= a || a - b > LEFT_MAX_STEP)
                continue;
            double per = (double)rows[i].duration / (a - b);
            for (int L = b + 1; L <= a; L++) {
                sum[L] += per;
                cnt[L]++;
            }
        }
        for (int L = 0; L <= 100; L++) {
            all_sum += sum[L];
            all_cnt += cnt[L];
        }
    }
    if (all_cnt == 0)
        return -1;
    double curve[101];
    for (int L = 0; L <= 100; L++) {
        double ps = 0;
        int pc = 0;
        for (int k = L - LEFT_CURVE_WINDOW; k <= L + LEFT_CURVE_WINDOW; k++) {
            if (k >= 0 && k <= 100) {
                ps += sum[k];
                pc += cnt[k];
            }
        }
        curve[L] = pc ? ps / pc : all_sum / all_cnt;
    }

    int level = rows[n - 1].level;
    if (level > 100)
        level = 100;
    if (level <= 0)
        return 0;

    // 2. the session's load against the curve, from its finished rows
    int start = rows[s].level > 100 ? 100 : rows[s].level;
    double factor = 1.0;
    int drained = start - level;
    if (drained > 0) {
        double real = 0, expected = 0;
        for (int i = s; i < n - 1; i++)
            real += rows[i].duration;
        for (int L = level + 1; L <= start; L++)
            expected += curve[L];
        if (real > 0 && expected > 0) {
            double r = real / expected;
            if (r < 0.33)
                r = 0.33;
            if (r > 3.0)
                r = 3.0;
            double w = drained >= LEFT_FULL_TRUST_PCT ? 1.0 : (double)drained / LEFT_FULL_TRUST_PCT;
            factor = 1.0 + w * (r - 1.0);
        }
    }

    // 3. down to 0 %, less the time already at this level (but not below the levels under it)
    double below = 0;
    for (int L = 1; L < level; L++)
        below += curve[L];
    double here = curve[level] * factor - rows[n - 1].duration;
    if (here < 0)
        here = 0;
    return (int)(below * factor + here);
}

// Remove OnionOS low-battery shutdown flag (/tmp/.lowBat) when charging or battery level > 3%.
// OnionOS keymon sets .lowBat upon hitting <= 1%, but fails to clear it when the charger is plugged in.
static void check_and_clear_lowbat_flag(int battery_pct, bool is_charging)
{
    if (battery_pct <= 0) {
        FILE *fp = fopen("/tmp/percBat", "r");
        if (fp != NULL) {
            if (fscanf(fp, "%d", &battery_pct) != 1)
                battery_pct = 0;
            fclose(fp);
        }
    }
    if (is_charging || battery_pct > 3) {
        if (access("/tmp/.lowBat", F_OK) == 0) {
            unlink("/tmp/.lowBat");
        }
    }
}

// Battery voltage from batmon's last reading. On the Mini Plus that is axp_test's output
// (millivolts); on the Mini batmon writes the smoothed ADC value there instead, which it
// turns into volts as (adc - 148) / 100. Returns true when the text changed.
bool read_battery_voltage(void)
{
    char buf[100] = "";
    char text[sizeof(battery_voltage)] = "";
    int voltage = 0, charging = 0;
    FILE *fp = fopen("/tmp/.axp_result", "r");

    if (fp != NULL) {
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        const char *v = strstr(buf, "\"voltage\":");
        const char *c = strstr(buf, "\"charging\":");
        if (v != NULL)
            voltage = atoi(v + 10);
        if (c != NULL)
            charging = atoi(c + 11);
    }

    check_and_clear_lowbat_flag(-1, charging != 0);

    if (HAS_AXP()) {
        if (voltage > 0) {
            int cv = (voltage + 5) / 10;
            snprintf(text, sizeof(text), "%d.%02dV", cv / 100, cv % 100);
        }
    }
    else if (voltage > 148 && charging == 0) {
        // MIYOO283: while charging batmon stores no reading (the ADC value is 100)
        int centivolts = voltage - 148;
        snprintf(text, sizeof(text), "%d.%02dV", centivolts / 100, centivolts % 100);
    }

    if (strcmp(text, battery_voltage) == 0)
        return false;
    strcpy(battery_voltage, text);
    return true;
}

void compute_graph(void)
{
    int total_duration = 0;
    int previous_index = graph_max_size - 1;
    bool is_estimation_computed = false;

    log_row *rows = NULL;
    int row_count = load_log_rows(&rows);
    compute_session_stats(rows, row_count);
    int left_seconds = estimate_left_seconds(rows, row_count);
    // the newest entry is a charging one: the newest session on battery is finished
    bool rows_end_charging = row_count > 0 && rows[row_count - 1].charging;
    free(rows);

    // the stretch (on battery or charging) being walked through, newest first, for the
    // L1/R1 marks
    int run_end_index = -1, run_length = 0, run_start_level = 0, run_end_level = 0;
    int run_tail = 0;          // time at the end level, at the stretch's newest end
    bool run_tail_open = true; // still walking through rows at the end level
    bool run_charging = false;
    bool first_run = true;
    // clock times (Unix): the stretch's first entry, its end, when a charge reached its last
    // level, and the start of the entry walked just before (the newer one; now at first)
    long long run_start_time = 0, run_end_time = 0, run_tail_time = 0;
    long long newer_row_time = (long long)time(NULL);
    bool run_times_ok = false;
    // sleep found inside the stretch: seconds, and percent lost across it
    long long run_sleep = 0;
    int run_sleep_drop = 0, run_rows = 0, newer_row_level = 0;
    int estimate_shift = 0;
    (void)rows_end_charging;

    if (open_battery_log_db() == 1) {
        if (bat_log_db != NULL) {
            const char *sql = "SELECT * FROM bat_activity WHERE device_serial = ? ORDER BY id DESC;";
            sqlite3_stmt *stmt;
            int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);

            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, DEVICE_SN, -1, SQLITE_STATIC);
                bool b_quit = false;

                while ((sqlite3_step(stmt) == SQLITE_ROW) && (!b_quit)) {

                    int bat_perc = sqlite3_column_int(stmt, 2);
                    int duration = sqlite3_column_int(stmt, 3);
                    bool is_charging = sqlite3_column_int(stmt, 4);

                    // when the entry began (batmon logs it since 2026-11), 0 when unknown
                    long long row_time = 0;
                    if (sqlite3_column_count(stmt) > 5 && sqlite3_column_type(stmt, 5) != SQLITE_NULL)
                        row_time = sqlite3_column_int64(stmt, 5);

                    // On stock OnionOS, batmon does not log start_time (it is NULL in SQLite).
                    // Derive row_time from log file mtime: mtime is when batmon last wrote
                    // this row's duration, so (mtime - duration) is when this row began.
                    if (first_run && row_time <= 0) {
                        struct stat st;
                        if (stat(BATTERY_LOG_FILE, &st) == 0 && st.st_mtime > 0)
                            row_time = (long long)st.st_mtime - duration;
                    }

                    // For the newest row, batmon only writes duration upon percentage change or every 10 min.
                    // Advance duration with live elapsed awake time so SESSION updates smoothly on the fly.
                    if (first_run && !is_charging && row_time > 0 && newer_row_time > row_time) {
                        int live_elapsed = (int)(newer_row_time - row_time);
                        if (live_elapsed > duration)
                            duration = live_elapsed;
                    }

                    // CURRENT: the newest entry only. Testing total_duration == 0 also
                    // caught the entry before it, as the newest one has duration 0 until
                    // batmon updates it, so CURRENT showed the previous level.
                    if (current_percentage[0] == '\0') {
                        sprintf(current_percentage, "%d%%", bat_perc);
                    }

                    current_index = (graph_max_size - 1) - duration_to_pixel(total_duration);

                    if (current_index < 0 || current_index >= graph_max_size) {
                        // From Amiga500/Onion e1942134: corrupt or implausible duration
                        // data would write out of the graphic[] array bounds; stop
                        // processing this record set.
                        b_quit = true;
                        break;
                    }

                    graphic[current_index].is_charging = is_charging;

                    if (bat_perc > 100)
                        bat_perc = 100;

                    int current_value = battery_to_pixel(bat_perc);

                    graphic[current_index].pixel_height = current_value;

                    int segment = previous_index - current_index;
                    if (segment > 1) {
                        for (int i = 1; i < segment; i++) {
                            //  graphic[current_index+i].pixel_height = graphic[current_index].pixel_height + (int)(((graphic[previous_index].pixel_height-current_value)/segment-1)*i);
                            graphic[current_index + i].pixel_height = graphic[previous_index].pixel_height;
                            graphic[current_index + i].is_charging = graphic[previous_index].is_charging;
                        }
                    }

                    if ((is_charging) && (!is_estimation_computed)) {
                        secondsToHoursMinutes(total_duration, session_duration);
                        // LEFT from the history (estimate_left_seconds); the line runs
                        // from the current reading down to 0 % at the estimated time
                        int now_height = graphic[graph_max_size - 1].pixel_height;
                        int estimated_playtime = left_seconds;
                        estimation_line_size = duration_to_pixel(estimated_playtime);
                        if (estimated_playtime > 0 && estimation_line_size > 0 && now_height > GRAPH_DISPLAY_START_Y) {
                            float slope = -(float)(now_height - GRAPH_DISPLAY_START_Y) / (float)estimation_line_size;

                            { // (was: if (slope < 0), always true here)
                                if (estimated_playtime < GRAPH_MAX_PLAUSIBLE_ESTIMATION) {
                                    secondsToHoursMinutes(estimated_playtime, session_left);
                                    // shift of the existing logs to make room for the estimation line
                                    int room_to_make = estimation_line_size + GRAPH_ESTIMATED_LINE_GAP;
                                    if (current_index - room_to_make >= 0) {
                                        for (int i = current_index; i < graph_max_size; i++) {
                                            graphic[i - room_to_make].pixel_height = graphic[i].pixel_height;
                                            graphic[i - room_to_make].is_charging = graphic[i].is_charging;
                                            graphic[i].pixel_height = 0;
                                            graphic[i].is_charging = false;
                                        }
                                    }
                                    total_duration += estimated_playtime + (int)(GRAPH_ESTIMATED_LINE_GAP)*GRAPH_DISPLAY_DURATION / GRAPH_DISPLAY_SIZE_X;
                                    current_index -= room_to_make;
                                    previous_index -= room_to_make;
                                    estimate_shift = room_to_make;
                                    begining_session_index = previous_index;
                                    for (int x = (graph_max_size - room_to_make); x < graph_max_size; x++) {
                                        int y = now_height + (int)(slope * (x - (graph_max_size - room_to_make)));
                                        if (y > GRAPH_DISPLAY_START_Y) {
                                            graphic[x].pixel_height = y;
                                            graphic[x].is_estimated = true;
                                        }
                                        else
                                            break;
                                    }
                                }
                                else {
                                    estimation_line_size = 0;
                                    estimated_playtime = 0;
                                }
                            }
                        }

                        if (session_left[0] == '\0')
                            estimation_line_size = 0; // no line drawn: page from the newest entry
                        is_estimation_computed = true;
                    }

                    // Marks for L1/R1: sessions on battery and charges. Each row sits
                    // at the time its level ended, so a stretch ends at its newest row
                    // and starts at the newest row of the other kind before it.
                    int raw_level = sqlite3_column_int(stmt, 2); // 500+ while charging on the Mini
                    if (run_end_index >= 0 && is_charging != run_charging) {
                        // the newest stretch always (it is going on); others only when
                        // longer than a charger flicker
                        if (session_mark_count < MAX_SESSION_MARKS &&
                            (first_run || run_length >= SESSION_MARK_MIN_LENGTH ||
                             (run_times_ok && run_end_time - run_start_time >= SESSION_MARK_MIN_LENGTH))) {
                            session_mark *m = &session_marks[session_mark_count++];
                            m->start_index = current_index;
                            // the estimate line moved the current session left
                            m->end_index = run_end_index - (first_run ? estimate_shift : 0);
                            m->length = run_length;
                            m->start_level = run_start_level;
                            m->end_level = run_end_level;
                            m->is_current = first_run;
                            m->is_charge = run_charging;
                            // a charge: the time it took to reach its last level; the
                            // rest is time left on the charger at that level
                            m->tail = (run_charging && run_tail < run_length) ? run_tail : 0;
                            // clock times: kept only when every entry had one and the
                            // span is not shorter than the awake time (a clock set back,
                            // as on the Mini after a power-off, breaks that)
                            m->has_times = run_times_ok && run_start_time > 0 &&
                                           run_end_time - run_start_time >= (long long)run_length * 9 / 10;
                            m->start_time = run_start_time;
                            m->end_time = run_end_time;
                            m->tail_time = run_tail_time;
                            m->sleep = run_sleep;
                            m->sleep_drop = run_sleep_drop;
                        }
                        first_run = false;
                        run_end_index = -1;
                    }
                    if (run_end_index < 0) {
                        run_end_index = current_index;
                        run_end_level = raw_level;
                        run_charging = is_charging;
                        run_length = 0;
                        run_tail = 0;
                        run_tail_open = true;
                        // the stretch ends where the newer entry began (or now)
                        run_end_time = newer_row_time;
                        run_times_ok = newer_row_time > 0;
                        run_tail_time = 0;
                        run_rows = 0;
                        run_sleep = 0;
                        run_sleep_drop = 0;
                    }
                    // Sleep inside a session on battery: the clock gap between this entry's
                    // end and the newer entry's start (batmon is stopped while asleep, so
                    // its duration leaves the sleep out). The level lost across the gap is
                    // what the sleep cost: batmon reads the battery at once on wake.
                    if (run_rows > 0 && !is_charging && row_time > 0 && newer_row_time > 0) {
                        long long gap = newer_row_time - row_time - duration;
                        if (gap >= SLEEP_GAP_MIN && gap * 100 >= (long long)duration * SLEEP_GAP_MIN_PCT) {
                            run_sleep += gap;
                            if (raw_level <= 100 && newer_row_level <= 100 && raw_level > newer_row_level)
                                run_sleep_drop += raw_level - newer_row_level;
                        }
                    }
                    run_rows++;
                    newer_row_level = raw_level;
                    if (run_tail_open && raw_level == run_end_level) {
                        run_tail += duration;
                        run_tail_time = row_time; // when the end level was reached
                    }
                    else
                        run_tail_open = false;
                    run_start_level = raw_level;
                    run_length += duration;
                    run_start_time = row_time;
                    if (row_time <= 0 || (newer_row_time > 0 && row_time > newer_row_time))
                        run_times_ok = false;
                    newer_row_time = row_time;

                    total_duration += duration;
                    if (duration_to_pixel(total_duration) > graph_max_size)
                        b_quit = true;

                    previous_index = current_index;
                }

                // Oldest run at the end of the log
                if (run_end_index >= 0 && session_mark_count < MAX_SESSION_MARKS &&
                    (first_run || run_length >= SESSION_MARK_MIN_LENGTH ||
                     (run_times_ok && run_end_time - run_start_time >= SESSION_MARK_MIN_LENGTH))) {
                    session_mark *m = &session_marks[session_mark_count++];
                    m->start_index = previous_index >= 0 ? previous_index : 0;
                    m->end_index = run_end_index - (first_run ? estimate_shift : 0);
                    m->length = run_length;
                    m->start_level = run_start_level;
                    m->end_level = run_end_level;
                    m->is_current = first_run;
                    m->is_charge = run_charging;
                    m->tail = (run_charging && run_tail < run_length) ? run_tail : 0;
                    m->has_times = run_times_ok && run_start_time > 0 &&
                                   run_end_time - run_start_time >= (long long)run_length * 9 / 10;
                    m->start_time = run_start_time;
                    m->end_time = run_end_time;
                    m->tail_time = run_tail_time;
                    m->sleep = run_sleep;
                    m->sleep_drop = run_sleep_drop;
                }
            }

            // Merge adjacent charge sessions if separated only by a brief charger disconnect (no discharge session between them)
            for (int i = 0; i < session_mark_count - 1;) {
                if (session_marks[i].is_charge && session_marks[i + 1].is_charge) {
                    session_marks[i].start_level = session_marks[i + 1].start_level;
                    session_marks[i].start_index = session_marks[i + 1].start_index;
                    session_marks[i].length += session_marks[i + 1].length;
                    session_marks[i].start_time = session_marks[i + 1].start_time;
                    session_marks[i].has_times = session_marks[i].has_times && session_marks[i + 1].has_times;
                    session_marks[i].sleep += session_marks[i + 1].sleep;
                    session_marks[i].sleep_drop += session_marks[i + 1].sleep_drop;
                    for (int k = session_marks[i + 1].start_index; k <= session_marks[i].end_index; k++) {
                        if (k >= 0 && k < graph_max_size) {
                            graphic[k].is_charging = true;
                        }
                    }
                    for (int j = i + 1; j < session_mark_count - 1; j++) {
                        session_marks[j] = session_marks[j + 1];
                    }
                    session_mark_count--;
                }
                else {
                    i++;
                }
            }

            if (session_duration[0] == '\0')
                secondsToHoursMinutes(total_duration, session_duration);
            sqlite3_finalize(stmt);
            close_battery_log_db();
        }
    }
    if (selected_session >= session_mark_count)
        selected_session = session_mark_count - 1;
}

// graphic[] index units per screen pixel at a zoom (16 h, 8 h, 4 h view)
static int zoom_to_level(int zoom)
{
    return zoom == 0 ? 4 : (zoom == 2 ? 1 : 2);
}

static int page_max_for_zoom(int zoom)
{
    return (GRAPH_MAX_FULL_PAGES * GRAPH_PAGE_SCROLL_SMOOTHNESS) / zoom_to_level(zoom) - GRAPH_PAGE_SCROLL_SMOOTHNESS;
}

// The page that shows graphic[index] just inside the view's left edge (as renderPage pages)
static int page_for_index(int index)
{
    int zoom_level = zoom_to_level(current_zoom);
    int base = estimation_line_size == 0 ? graph_max_size - GRAPH_DISPLAY_SIZE_X * zoom_level : begining_session_index;
    int step = GRAPH_DISPLAY_SIZE_X * zoom_level / GRAPH_PAGE_SCROLL_SMOOTHNESS;
    int need = base - (index - SESSION_MARK_MARGIN * zoom_level);
    int page = need <= 0 ? 0 : (need + step - 1) / step;
    int page_max = page_max_for_zoom(current_zoom);
    if (page > page_max)
        page = page_max;
    return page < 0 ? 0 : page;
}

// The right zoom icon reads "R2" now: its R moves left to make room for the "2"
static void draw_right_zoom_icon(void)
{
    int r_x = RIGHT_ARROW_X - 15;
    safeBlitSurface(right_arrow, &(SDL_Rect){0, 0, ZOOM_R_LETTER_W, ARROW_WIDTH}, screen, &(SDL_Rect){r_x, RIGHT_ARROW_Y, ZOOM_R_LETTER_W, ARROW_WIDTH});
    renderText("2", font_title ? font_title : font_Arkhip, color_white, &(SDL_Rect){r_x + ZOOM_R_LETTER_W + 1, RIGHT_ARROW_Y + ZOOM_DIGIT_DY, 16, 24});
    safeBlitSurface(right_arrow, &(SDL_Rect){ZOOM_R_LETTER_W, 0, ARROW_LENGHT - ZOOM_R_LETTER_W, ARROW_WIDTH}, screen,
                    &(SDL_Rect){RIGHT_ARROW_X + ZOOM_R_LETTER_W, RIGHT_ARROW_Y, ARROW_LENGHT - ZOOM_R_LETTER_W, ARROW_WIDTH});
}

#define AXP_I2C_DEV "/dev/i2c-1"
#define AXP_ADDR 0x34
#define REG_AXP_VOFF 0x31
#define REG_AXP_CTL 0xB8
#define REG_AXP_PCT 0xB9
#define REG_AXP_E0 0xE0
#define REG_AXP_E1 0xE1
#define REG_AXP_E2 0xE2
#define REG_AXP_E3 0xE3
#define REG_AXP_E4 0xE4
#define REG_AXP_E5 0xE5

static int axp_i2c_read(int fd, unsigned char reg)
{
    unsigned char val = 0;
    struct i2c_msg msgs[2] = {
        {AXP_ADDR, 0, 1, &reg},
        {AXP_ADDR, I2C_M_RD, 1, &val},
    };
    struct i2c_rdwr_ioctl_data d = {msgs, 2};
    if (ioctl(fd, I2C_RDWR, &d) < 0)
        return -1;
    return val;
}

static int axp_i2c_read16(int fd, unsigned char reg)
{
    unsigned char buf[2] = {0, 0};
    struct i2c_msg msgs[2] = {
        {AXP_ADDR, 0, 1, &reg},
        {AXP_ADDR, I2C_M_RD, 2, buf},
    };
    struct i2c_rdwr_ioctl_data d = {msgs, 2};
    if (ioctl(fd, I2C_RDWR, &d) < 0)
        return -1;
    return (buf[0] << 8) | buf[1];
}

static int axp_i2c_write(int fd, unsigned char reg, unsigned char val)
{
    unsigned char buf[2] = {reg, val};
    struct i2c_msg msg = {AXP_ADDR, 0, 2, buf};
    struct i2c_rdwr_ioctl_data d = {&msg, 1};
    return ioctl(fd, I2C_RDWR, &d) < 0 ? -1 : 0;
}

static bool axp_poll_data(void)
{
    if (!HAS_AXP()) {
        axp_status.available = false;
        return false;
    }

    int fd = open(AXP_I2C_DEV, O_RDWR);
    if (fd < 0) {
        axp_status.available = false;
        return false;
    }
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);

    int b8 = axp_i2c_read(fd, REG_AXP_CTL);
    int b9 = axp_i2c_read(fd, REG_AXP_PCT);
    if (b8 < 0 || b9 < 0) {
        close(fd);
        axp_status.available = false;
        return false;
    }

    // Atomic 16-bit reads to avoid register tearing across bytes
    int e01 = axp_i2c_read16(fd, REG_AXP_E0);
    int e23 = axp_i2c_read16(fd, REG_AXP_E2);
    int e4 = axp_i2c_read(fd, REG_AXP_E4);
    int e5 = axp_i2c_read(fd, REG_AXP_E5);

    int v12 = axp_i2c_read16(fd, 0x78);
    int d12 = axp_i2c_read16(fd, 0x7C);
    int c12 = axp_i2c_read16(fd, 0x7A);
    int st01 = axp_i2c_read(fd, 0x01);
    int r31 = axp_i2c_read(fd, REG_AXP_VOFF);

    close(fd);

    axp_status.voff_code = (r31 >= 0) ? (r31 & 0x07) : -1;

    axp_status.available = true;
    axp_status.gauge_en = (b8 >> 7) & 1;
    axp_status.cc_en = (b8 >> 6) & 1;
    axp_status.cal_en = (b8 >> 5) & 1;
    axp_status.cal_active = (b8 >> 4) & 1;
    axp_status.pct_valid = (b9 >> 7) & 1;
    axp_status.battery_pct = b9 & 0x7F;

    if (v12 >= 0) {
        int v_raw = ((v12 >> 8) << 4) | (v12 & 0x0F);
        axp_status.voltage_mv = (int)(v_raw * 1.1 + 0.5);
    }
    else {
        axp_status.voltage_mv = 0;
    }

    int dis_ma = (d12 >= 0) ? (((d12 >> 8) << 4) | (d12 & 0x0F)) : 0;
    int chg_ma = (c12 >= 0) ? (((c12 >> 8) << 4) | (c12 & 0x0F)) : 0;
    bool is_charging = (st01 >= 0) ? ((st01 >> 6) & 1) : false;
    axp_status.is_charging = is_charging;
    if (is_charging && chg_ma > 0)
        axp_status.current_ma = chg_ma;
    else
        axp_status.current_ma = -dis_ma;

    if (e01 >= 0) {
        int raw_cap = e01 & 0x7FFF;
        axp_status.capacity_mah = (int)(raw_cap * 1.456 + 0.5);
    }
    if (e23 >= 0) {
        int raw_cc = e23 & 0x7FFF;
        axp_status.cc_mah = (int)(raw_cc * 1.456 + 0.5);
    }

    // Measured on a Mini Plus with a factory-reset PMIC (battery unplugged): E0H-E3H read 0 and
    // E5H reads 0x64 with bit 7 clear. Saving a capacity sets E0H bit 7, and the PMIC then seeds
    // the counter by itself and sets E5H bit 7.
    axp_status.cap_set = e01 >= 0 && (e01 & 0x8000) && (e01 & 0x7FFF) > 0;
    axp_status.ocv_pct = e4 >= 0 ? (e4 & 0x7F) : 0;
    axp_status.cc_pct = e5 >= 0 ? (e5 & 0x7F) : 0;
    axp_status.cc_valid = axp_status.cap_set && e5 >= 0 && ((e5 >> 7) & 1);

    check_and_clear_lowbat_flag(axp_status.battery_pct, axp_status.is_charging);

    return true;
}

static bool axp_set_capacity(int mah)
{
    if (!HAS_AXP() || mah <= 0 || mah > 10000)
        return false;
    int fd = open(AXP_I2C_DEV, O_RDWR);
    if (fd < 0)
        return false;
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);

    int b8 = axp_i2c_read(fd, REG_AXP_CTL);
    if (b8 >= 0 && (b8 & 0xC0) != 0xC0) {
        axp_i2c_write(fd, REG_AXP_CTL, (unsigned char)(b8 | 0xC0));
    }

    int units = (int)((mah / 1.456) + 0.5);
    unsigned char hi = 0x80 | ((units >> 8) & 0x7F);
    unsigned char lo = units & 0xFF;
    bool ok = (axp_i2c_write(fd, REG_AXP_E0, hi) == 0 &&
               axp_i2c_write(fd, REG_AXP_E1, lo) == 0);
    close(fd);
    return ok;
}

static bool axp_set_coulomb_counter(int mah)
{
    if (!HAS_AXP() || mah <= 0 || mah > 10000)
        return false;
    int fd = open(AXP_I2C_DEV, O_RDWR);
    if (fd < 0)
        return false;
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);

    int b8 = axp_i2c_read(fd, REG_AXP_CTL);
    if (b8 >= 0 && (b8 & 0xC0) != 0xC0) {
        axp_i2c_write(fd, REG_AXP_CTL, (unsigned char)(b8 | 0xC0));
    }

    int units = (int)((mah / 1.456) + 0.5);
    unsigned char hi = 0x80 | ((units >> 8) & 0x7F);
    unsigned char lo = units & 0xFF;
    bool ok = (axp_i2c_write(fd, REG_AXP_E2, hi) == 0 &&
               axp_i2c_write(fd, REG_AXP_E3, lo) == 0);
    close(fd);
    return ok;
}

// AXP221 datasheet v1.2, REG 31H (wakeup control & Voff), default 03H:
//   bit 7 PWROK pull-low on wakeup (RW, keep), bit 6 soft restart (write 1 = PMU reboots!),
//   bit 5 soft wakeup (write 1 = outputs restored), bit 4 IRQ wakeup masking (RW, keep),
//   bit 3 sleep wakeup enable (self-clearing), bits 2-0 Voff: 2.6 V + code * 0.1 V.
// Only bits 7 and 4 are preserved; bits 6/5/3 are always written as 0.
// Returns 0 on success, -1 on I2C failure, -2 when the read-back value differs.
static int axp_set_voff(int code)
{
    if (!HAS_AXP() || code < 0 || code > 7)
        return -1;
    int fd = open(AXP_I2C_DEV, O_RDWR);
    if (fd < 0)
        return -1;
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);

    int r31 = axp_i2c_read(fd, REG_AXP_VOFF);
    if (r31 < 0) {
        close(fd);
        return -1;
    }
    unsigned char new_r31 = (unsigned char)((r31 & 0x90) | (code & 0x07));
    if (axp_i2c_write(fd, REG_AXP_VOFF, new_r31) != 0) {
        close(fd);
        return -1;
    }
    int check = axp_i2c_read(fd, REG_AXP_VOFF);
    close(fd);
    if (check < 0)
        return -1;
    return ((check & 0x07) == code) ? 0 : -2;
}

// AXP221 datasheet v1.2, REG B8H:
//   bit 7 fuel gauge enable, bit 6 coulomb counter enable,
//   bit 5 battery total capacity correction enable (RW, persistent switch),
//   bit 4 correction status (0: not correcting, 1: correcting).
// Returns 0 on success, -1 on I2C failure, -2 when the PMIC did not accept the new bit 5 value.
static int axp_set_calibration(bool on)
{
    if (!HAS_AXP())
        return -1;
    int fd = open(AXP_I2C_DEV, O_RDWR);
    if (fd < 0)
        return -1;
    ioctl(fd, I2C_TIMEOUT, 5);
    ioctl(fd, I2C_RETRIES, 1);

    int b8 = axp_i2c_read(fd, REG_AXP_CTL);
    if (b8 < 0) {
        close(fd);
        return -1;
    }
    // Always keep Gauge (bit 7) and Coulomb Counter (bit 6) enabled.
    // Bits 3-0 are reserved ("do not change"); bit 4 is a status bit, written back as read.
    unsigned char new_b8 = (unsigned char)((b8 | 0xC0));
    if (on) {
        new_b8 |= 0x20;
        // If design capacity is unconfigured or zero, initialize it with selected capacity
        int e01 = axp_i2c_read16(fd, REG_AXP_E0);
        if (e01 <= 0 || (e01 & 0x7FFF) == 0) {
            int units = (int)((axp_edit_capacity / 1.456) + 0.5);
            unsigned char hi = 0x80 | ((units >> 8) & 0x7F);
            unsigned char lo = units & 0xFF;
            axp_i2c_write(fd, REG_AXP_E0, hi);
            axp_i2c_write(fd, REG_AXP_E1, lo);
        }
    }
    else
        new_b8 &= ~0x20;

    if (axp_i2c_write(fd, REG_AXP_CTL, new_b8) != 0) {
        close(fd);
        return -1;
    }

    // Read back to confirm the PMIC really accepted the change
    int check = axp_i2c_read(fd, REG_AXP_CTL);
    close(fd);
    if (check < 0)
        return -1;
    return (((check >> 5) & 1) == (on ? 1 : 0)) ? 0 : -2;
}

// ---------------------------------------------------------------------------
// Battery health test: internal resistance from voltage sag under CPU load.
//   Stage 0: idle baseline (app idle, ondemand governor, screen dimmed)
//   Stage 1: 2 worker threads at 100% load, 1200 MHz (stock)
//   Stage 2: 2 worker threads at 100% load, 1600 MHz (via Onion's cpuclock tool)
//   R_int = (V_idle - V_load) / (I_load - I_idle)
// Original CPU clock, governor and brightness are always restored (finish, abort,
// [B], MENU, SIGTERM, atexit).
// ---------------------------------------------------------------------------
#define BT_CPUCLOCK_BIN "/mnt/SDCARD/.tmp_update/bin/cpuclock"
#define BT_GOVERNOR_FILE "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
#define BT_STAGES 3
#define BT_STAGE_MS 15000 // per stage
#define BT_SETTLE_MS 7000 // samples before this are ignored (voltage relaxation)
#define BT_SAMPLE_MS 250
#define BT_START_MIN_MV 3600
#define BT_ABORT_MV 3500
#define BT_MIN_DELTA_MA 100
#define BT_DIM_RAW 3 // Onion brightness level 0
#define BT_LOAD_THREADS 2

enum { BT_OFF = 0,
       BT_RUNNING,
       BT_DONE,
       BT_ABORTED };

typedef struct {
    int mhz; // 0 = leave clock as is (idle)
    long v_sum;
    long i_sum;
    int n;
    int v_mv; // averaged battery voltage
    int i_ma; // averaged discharge current (positive)
} bt_stage_t;

static struct {
    int state;
    int stage;
    Uint32 stage_start;
    Uint32 next_sample;
    bt_stage_t st[BT_STAGES];
    int r_mohm[BT_STAGES]; // per load stage vs idle, -1 if not measurable
    int live_v, live_i;
    int saved_mhz;
    char saved_gov[32];
    bool clock_changed;
    uint32_t saved_brightness;
    bool brightness_changed;
    char msg[64];
} bt = {0};

static volatile int bt_load_run = 0;
static volatile uint32_t bt_load_sink = 0;
static pthread_t bt_threads[BT_LOAD_THREADS];
static int bt_thread_count = 0;

static void *bt_load_worker(void *arg)
{
    int core_id = (int)(uintptr_t)arg;
    cpu_set_t cpuset;
    memset(&cpuset, 0, sizeof(cpuset));
    ((unsigned long *)&cpuset)[0] = (1UL << (core_id % 2));
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    uint32_t a = (uint32_t)core_id + 1, b = 0x9E3779B9u, c = 7;
    float f = 1.0001f, g = 0.9999f;
    while (bt_load_run) {
        for (int k = 0; k < 20000; k++) {
            a = a * 1664525u + 1013904223u;
            b ^= (a >> 3) | (b << 5);
            c += a * b;
            f = f * 1.0000001f + g;
            g = g * 0.9999999f + 0.0001f;
            if (f > 1.0e6f)
                f = 1.0001f;
        }
        bt_load_sink = a ^ b ^ c ^ (uint32_t)f;
    }
    return NULL;
}

static void bt_stop_load(void)
{
    bt_load_run = 0;
    for (int i = 0; i < bt_thread_count; i++)
        pthread_join(bt_threads[i], NULL);
    bt_thread_count = 0;
}

static bool bt_start_load(void)
{
    if (bt_thread_count > 0)
        return true;
    bt_load_run = 1;
    for (int i = 0; i < BT_LOAD_THREADS; i++) {
        if (pthread_create(&bt_threads[i], NULL, bt_load_worker, (void *)(uintptr_t)i) != 0) {
            bt_stop_load();
            return false;
        }
        bt_thread_count++;
    }
    return true;
}

static const char *bt_cpuclock_cmd(void)
{
    return access(BT_CPUCLOCK_BIN, X_OK) == 0 ? BT_CPUCLOCK_BIN : "cpuclock";
}

static int bt_get_mhz(void)
{
    char cmd[96], buf[32];
    snprintf(cmd, sizeof(cmd), "%s 2>/dev/null", bt_cpuclock_cmd());
    FILE *p = popen(cmd, "r");
    if (p == NULL)
        return -1;
    int mhz = -1;
    if (fgets(buf, sizeof(buf), p) != NULL)
        mhz = atoi(buf);
    pclose(p);
    return (mhz >= 100 && mhz <= 2400) ? mhz : -1;
}

static bool bt_set_mhz(int mhz)
{
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "%s %d >/dev/null 2>&1", bt_cpuclock_cmd(), mhz);
    return system(cmd) == 0;
}

static void bt_restore(void)
{
    bt_stop_load();
    if (bt.clock_changed) {
        if (bt.saved_mhz > 0)
            bt_set_mhz(bt.saved_mhz);
        if (bt.saved_gov[0] != '\0') {
            FILE *fp = fopen(BT_GOVERNOR_FILE, "w");
            if (fp != NULL) {
                fputs(bt.saved_gov, fp);
                fclose(fp);
            }
        }
        bt.clock_changed = false;
    }
    if (bt.brightness_changed) {
        display_setBrightnessRaw(bt.saved_brightness);
        bt.brightness_changed = false;
    }
}

static void bt_abort(const char *reason)
{
    bt_restore();
    snprintf(bt.msg, sizeof(bt.msg), "%s", reason);
    bt.state = BT_ABORTED;
}

static const char *bt_rating(int r_mohm)
{
    if (r_mohm < 0)
        return "N/A";
    if (r_mohm < 160)
        return "EXCELLENT";
    if (r_mohm < 260)
        return "GOOD";
    if (r_mohm < 380)
        return "AGED";
    return "POOR / REPLACE";
}

// Best result: the 1600 MHz stage (largest current step), else 1200 MHz
static int bt_final_r(void)
{
    return bt.r_mohm[2] >= 0 ? bt.r_mohm[2] : bt.r_mohm[1];
}

static void bt_enter_stage(int stage, Uint32 now)
{
    bt.stage = stage;
    bt.stage_start = now;
    bt.next_sample = now;
    bt.st[stage].v_sum = bt.st[stage].i_sum = 0;
    bt.st[stage].n = 0;

    if (stage == 0) {
        // Stage 0: Idle baseline with screen dimmed
        display_setBrightnessRaw(BT_DIM_RAW);
    }
    else {
        // Stage 1 & 2: Load 2 cores with screen at 100% brightness
        display_setBrightness(10);
    }

    if (bt.st[stage].mhz > 0) {
        bt.clock_changed = true;
        if (!bt_set_mhz(bt.st[stage].mhz)) {
            bt_abort("cpuclock failed to set CPU clock");
            return;
        }
        if (!bt_start_load())
            bt_abort("Failed to start load threads");
    }
}

static void bt_start(Uint32 now)
{
    memset(&bt, 0, sizeof(bt));
    bt.st[0].mhz = 0;
    bt.st[1].mhz = 1200;
    bt.st[2].mhz = 1600;
    for (int i = 0; i < BT_STAGES; i++)
        bt.r_mohm[i] = -1;
    bt.state = BT_RUNNING;

    if (!axp_poll_data()) {
        bt_abort("PMIC not available");
        return;
    }
    if (axp_status.is_charging) {
        bt_abort("Unplug the charger first");
        return;
    }
    if (axp_status.voltage_mv < BT_START_MIN_MV) {
        snprintf(bt.msg, sizeof(bt.msg), "Battery too low (%.2f V < %.2f V)",
                 axp_status.voltage_mv / 1000.0, BT_START_MIN_MV / 1000.0);
        bt.state = BT_ABORTED;
        return;
    }
    bt.saved_mhz = bt_get_mhz();
    if (bt.saved_mhz < 0) {
        bt_abort("Onion cpuclock tool not found");
        return;
    }
    FILE *fp = fopen(BT_GOVERNOR_FILE, "r");
    if (fp != NULL) {
        if (fscanf(fp, "%31s", bt.saved_gov) != 1)
            bt.saved_gov[0] = '\0';
        fclose(fp);
    }

    bt.saved_brightness = display_getBrightnessRaw();
    bt.brightness_changed = true;
    display_setBrightnessRaw(BT_DIM_RAW);

    bt_enter_stage(0, now);
}

static void bt_finish(void)
{
    bt_restore();
    for (int k = 1; k < BT_STAGES; k++) {
        int d_i = bt.st[k].i_ma - bt.st[0].i_ma;
        int d_v = bt.st[0].v_mv - bt.st[k].v_mv;
        bt.r_mohm[k] = (d_i >= BT_MIN_DELTA_MA) ? (int)((long)d_v * 1000 / d_i) : -1;
        if (bt.r_mohm[k] < 0 && d_i >= BT_MIN_DELTA_MA)
            bt.r_mohm[k] = 0; // voltage rose under load: noise, clamp
    }
    bt.state = BT_DONE;
    if (bt_final_r() < 0)
        snprintf(bt.msg, sizeof(bt.msg), "Load step too small to measure (< %d mA)", BT_MIN_DELTA_MA);
}

// Returns true when the screen needs a redraw
static bool bt_tick(Uint32 now)
{
    if (bt.state != BT_RUNNING || (Sint32)(now - bt.next_sample) < 0)
        return false;
    bt.next_sample = now + BT_SAMPLE_MS;

    if (!axp_poll_data()) {
        bt_abort("I2C read failed");
        return true;
    }
    bt.live_v = axp_status.voltage_mv;
    bt.live_i = -axp_status.current_ma; // discharge as positive
    if (axp_status.is_charging) {
        bt_abort("Charger connected during test");
        return true;
    }
    if (bt.live_v > 0 && bt.live_v < BT_ABORT_MV) {
        bt_abort("Voltage dropped below 3.50 V");
        return true;
    }

    bt_stage_t *s = &bt.st[bt.stage];
    Uint32 elapsed = now - bt.stage_start;
    if (elapsed >= BT_SETTLE_MS) {
        s->v_sum += bt.live_v;
        s->i_sum += bt.live_i;
        s->n++;
    }
    if (elapsed >= BT_STAGE_MS) {
        if (s->n > 0) {
            s->v_mv = (int)(s->v_sum / s->n);
            s->i_ma = (int)(s->i_sum / s->n);
        }
        if (bt.stage + 1 < BT_STAGES)
            bt_enter_stage(bt.stage + 1, now);
        else
            bt_finish();
    }
    return true;
}

static void drawRectFill(int x, int y, int w, int h, Uint32 color)
{
    SDL_Rect r = {x, y, w, h};
    SDL_FillRect(screen, &r, color);
}

static void drawFrame1px(int x, int y, int w, int h, Uint32 color)
{
    drawRectFill(x, y, w, 1, color);
    drawRectFill(x, y + h - 1, w, 1, color);
    drawRectFill(x, y, 1, h, color);
    drawRectFill(x + w - 1, y, 1, h, color);
}

static void drawFrame2px(int x, int y, int w, int h, Uint32 color)
{
    drawRectFill(x, y, w, 2, color);
    drawRectFill(x, y + h - 2, w, 2, color);
    drawRectFill(x, y, 2, h, color);
    drawRectFill(x + w - 2, y, 2, h, color);
}

static void dimScreen(Uint8 alpha)
{
    SDL_Surface *dim = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    if (dim != NULL) {
        SDL_FillRect(dim, NULL, SDL_MapRGB(dim->format, 4, 8, 16));
        SDL_SetAlpha(dim, SDL_SRCALPHA, alpha);
        SDL_BlitSurface(dim, NULL, screen, NULL);
        SDL_FreeSurface(dim);
    }
}

static void drawCenteredText(const char *text, TTF_Font *font, SDL_Color color, int x, int y, int w)
{
    if (font == NULL || text == NULL || text[0] == '\0')
        return;
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, text, &tw, &th);
    int tx = x + (w - tw) / 2;
    if (tx < x)
        tx = x;
    renderText(text, font, color, &(SDL_Rect){tx, y, tw, th});
}

static void drawBadgePill(const char *text, TTF_Font *font, SDL_Color txt_col, Uint32 bg_col, Uint32 border_col, int x, int y, int w, int h)
{
    drawRectFill(x, y, w, h, bg_col);
    drawFrame1px(x, y, w, h, border_col);
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, text, &tw, &th);
    renderText(text, font, txt_col, &(SDL_Rect){x + (w - tw) / 2, y + (h - th) / 2, tw, th});
}

static void drawKeyValueLine(const char *key, const char *val, TTF_Font *font, SDL_Color key_col, SDL_Color val_col, int x, int y, int w)
{
    renderText(key, font, key_col, &(SDL_Rect){x, y, 140, 20});
    renderTextAlignRight(val, font, val_col, &(SDL_Rect){x + w, y, 120, 20});
}

static void axp_save_capacity(Uint32 now)
{
    if (axp_set_capacity(axp_edit_capacity))
        snprintf(axp_feedback_msg, sizeof(axp_feedback_msg), "Saved capacity: %d mAh", axp_edit_capacity);
    else
        snprintf(axp_feedback_msg, sizeof(axp_feedback_msg), "Failed to save capacity");
    axp_feedback_until = now + 3000;
    axp_poll_data();
}

static void renderAxpMenu(void)
{
    dimScreen(240);
    // Draw solid dark bar behind header to prevent background text bleeding through
    drawRectFill(0, 0, 640, 56, SDL_MapRGB(screen->format, 8, 14, 24));
    // Draw solid dark bar behind footer badges
    drawRectFill(0, 436, 640, 44, SDL_MapRGB(screen->format, 8, 14, 24));

    Uint32 col_card_bg = SDL_MapRGB(screen->format, 16, 24, 38);
    Uint32 col_card_border = SDL_MapRGB(screen->format, 42, 59, 92);
    Uint32 col_sel_bg = SDL_MapRGB(screen->format, 30, 53, 91);
    Uint32 col_sel_border = SDL_MapRGB(screen->format, 77, 132, 219);
    Uint32 col_pill_bg = SDL_MapRGB(screen->format, 14, 22, 34);
    Uint32 col_pill_border = SDL_MapRGB(screen->format, 50, 70, 105);

    SDL_Color col_title = {89, 167, 255};
    SDL_Color col_sub = {160, 185, 215};
    SDL_Color col_header = {100, 200, 255};
    SDL_Color col_lbl = {150, 175, 205};
    SDL_Color col_val = {255, 255, 255};
    SDL_Color col_green = {125, 219, 143};
    SDL_Color col_cyan = {100, 215, 255};
    SDL_Color col_warn = {240, 140, 100};

    // Header
    drawCenteredText("HARDWARE PMIC & BATTERY MONITOR", font_title ? font_title : font_Arkhip, col_title, 0, 20, 640);

    // Safeguard check
    if (!axp_status.available) {
        int mw = 520, mh = 240, mx = 60, my = 120;
        Uint32 err_bg = SDL_MapRGB(screen->format, 14, 22, 34);
        Uint32 err_border = SDL_MapRGB(screen->format, 224, 108, 117);
        drawRectFill(mx, my, mw, mh, err_bg);
        drawFrame2px(mx, my, mw, mh, err_border);

        drawCenteredText("HARDWARE FUEL GAUGE NOT AVAILABLE", font_title ? font_title : font_Arkhip, (SDL_Color){224, 108, 117}, mx, my + 25, mw);
        drawCenteredText("This menu requires a PMIC chip with hardware fuel gauge,", font_small ? font_small : font_Arkhip, col_val, mx, my + 70, mw);
        drawCenteredText("which is present on the Miyoo Mini Plus and Flip.", font_small ? font_small : font_Arkhip, col_val, mx, my + 92, mw);
        drawCenteredText("Your console model or revision does not provide", font_small ? font_small : font_Arkhip, col_sub, mx, my + 122, mw);
        drawCenteredText("I2C access to this hardware telemetry.", font_small ? font_small : font_Arkhip, col_sub, mx, my + 144, mw);

        drawBadgePill("[B / SELECT] Back to Graph", font_small ? font_small : font_Arkhip, col_val, col_sel_bg, col_sel_border, mx + 150, my + 185, 220, 32);
        return;
    }

    // Section 1: Live Status Headers
    renderText("LIVE PMIC TELEMETRY", font_Arkhip, col_header, &(SDL_Rect){25, 58, 250, 18});
    renderText("FUEL GAUGE STATUS", font_Arkhip, col_header, &(SDL_Rect){325, 58, 250, 18});

    // Left Card: Live Telemetry (enlarged table)
    int card_y = 80;
    int card_h = 186;
    int card_w = 290;
    int lx = 25;
    int rx = 325;

    drawRectFill(lx, card_y, card_w, card_h, col_card_bg);
    drawFrame1px(lx, card_y, card_w, card_h, col_card_border);

    // Right Card: Fuel Gauge Readings (enlarged table)
    drawRectFill(rx, card_y, card_w, card_h, col_card_bg);
    drawFrame1px(rx, card_y, card_w, card_h, col_card_border);

    // Subtle horizontal divider lines between table rows
    for (int i = 1; i < 5; i++) {
        int line_y = 92 + i * 34 - 8;
        drawRectFill(lx + 10, line_y, card_w - 20, 1, SDL_MapRGB(screen->format, 28, 40, 60));
        drawRectFill(rx + 10, line_y, card_w - 20, 1, SDL_MapRGB(screen->format, 28, 40, 60));
    }

    char v_str[32], c_str[32], p_str[32], rem_str[32], cap_str[32];
    snprintf(v_str, sizeof(v_str), "%.2f V", axp_status.voltage_mv / 1000.0);
    if (axp_status.is_charging && axp_status.current_ma > 0)
        snprintf(c_str, sizeof(c_str), "+%d mA (Chg)", axp_status.current_ma);
    else
        snprintf(c_str, sizeof(c_str), "%d mA", axp_status.current_ma);
    if (axp_status.pct_valid)
        snprintf(p_str, sizeof(p_str), "%d%%", axp_status.battery_pct);
    else
        snprintf(p_str, sizeof(p_str), "%d%% (est.)", axp_status.battery_pct);
    // No capacity in the PMIC (new console, battery unplugged): both read 0, which looks broken.
    // A counter above the capacity happens when the capacity was saved while charging (the PMIC
    // seeds the counter from the inflated voltage and never trims it back); syncing at 100% fixes it.
    bool cc_over = axp_status.cap_set && axp_status.cc_mah > axp_status.capacity_mah + 50;
    if (axp_status.cap_set) {
        snprintf(rem_str, sizeof(rem_str), "%d mAh", axp_status.cc_mah);
        snprintf(cap_str, sizeof(cap_str), "%d mAh", axp_status.capacity_mah);
    }
    else {
        snprintf(rem_str, sizeof(rem_str), "Not set");
        snprintf(cap_str, sizeof(cap_str), "Not set");
    }

    TTF_Font *tbl_font = font_Arkhip ? font_Arkhip : font_small;
    drawKeyValueLine("Battery Voltage:", v_str, tbl_font, col_lbl, col_val, lx + 12, 92 + 0 * 34, card_w - 24);
    drawKeyValueLine("Power Draw:", c_str, tbl_font, col_lbl, (axp_status.is_charging && axp_status.current_ma > 0) ? col_green : col_val, lx + 12, 92 + 1 * 34, card_w - 24);
    drawKeyValueLine("Battery Level:", p_str, tbl_font, col_lbl, col_val, lx + 12, 92 + 2 * 34, card_w - 24);
    drawKeyValueLine("Energy Remaining:", rem_str, tbl_font, col_lbl, !axp_status.cap_set ? col_sub : cc_over ? col_warn
                                                                                                              : col_val,
                     lx + 12, 92 + 3 * 34, card_w - 24);
    drawKeyValueLine("Total Capacity:", cap_str, tbl_font, col_lbl, col_sub, lx + 12, 92 + 4 * 34, card_w - 24);

    char ocv_str[32], cc_str[32];
    snprintf(ocv_str, sizeof(ocv_str), "%d%%", axp_status.ocv_pct);
    if (axp_status.cc_valid)
        snprintf(cc_str, sizeof(cc_str), "%d%%", axp_status.cc_pct);
    else
        snprintf(cc_str, sizeof(cc_str), "--");
    const char *gauge_str = (axp_status.gauge_en && axp_status.cc_en) ? "Active" : "Disabled";
    const char *cal_mode_str = axp_status.cal_en ? "Enabled" : "Off";
    const char *cal_stat_str = axp_status.cal_active ? "In progress" : "Idle";

    drawKeyValueLine("Voltage (OCV):", ocv_str, tbl_font, col_lbl, col_val, rx + 12, 92 + 0 * 34, card_w - 24);
    drawKeyValueLine("Coulomb Ctr (CC):", cc_str, tbl_font, col_lbl, axp_status.cc_valid ? col_val : col_sub, rx + 12, 92 + 1 * 34, card_w - 24);
    drawKeyValueLine("Fuel Gauge:", gauge_str, tbl_font, col_lbl, col_green, rx + 12, 92 + 2 * 34, card_w - 24);
    drawKeyValueLine("Cap. Learning:", cal_mode_str, tbl_font, col_lbl, axp_status.cal_en ? col_green : col_sub, rx + 12, 92 + 3 * 34, card_w - 24);
    drawKeyValueLine("Correction:", cal_stat_str, tbl_font, col_lbl, axp_status.cal_active ? col_cyan : col_sub, rx + 12, 92 + 4 * 34, card_w - 24);

    // Section 2: Settings & Actions (5 rows, 30 px pitch, fits above the footer at y=444)
    renderText("BATTERY SETTINGS & ACTIONS", font_Arkhip, col_header, &(SDL_Rect){25, 270, 300, 18});
    // What to do next, on the right of the section header
    const char *next_hint = NULL;
    if (!axp_status.cap_set)
        // Full means a finished charge, not just 99% after unplugging:
        // without a capacity the level is voltage-based and drops at once under load.
        next_hint = (!axp_status.is_charging && axp_status.battery_pct >= 100) ? "Battery full: save capacity now"
                                                                               : "Charge to 100%, then save capacity";
    else if (cc_over)
        next_hint = axp_status.is_charging ? "Counter high: sync when charging ends" : "Counter high: sync at 100%";
    if (next_hint)
        renderTextAlignRight(next_hint, font_small ? font_small : font_Arkhip, col_warn, &(SDL_Rect){615, 273, 300, 16});

    const int row_y0 = 288, row_pitch = 30, row_h = 28, txt_dy = 4;
    for (int i = 0; i < AXP_MENU_COUNT; i++) {
        int y = row_y0 + i * row_pitch;
        bool sel = (axp_menu_selected == i);
        drawRectFill(25, y, 590, row_h, sel ? col_sel_bg : col_card_bg);
        if (sel) {
            drawFrame2px(25, y, 590, row_h, col_sel_border);
            renderText(">", font_Arkhip, col_cyan, &(SDL_Rect){35, y + txt_dy, 15, 20});
        }
        else
            drawFrame1px(25, y, 590, row_h, col_card_border);
    }
#define ROW_TY(i) (row_y0 + (i) * row_pitch + txt_dy)

    // Item: Capacity
    renderText("Battery Capacity:", font_Arkhip, col_val, &(SDL_Rect){50, ROW_TY(AXP_MENU_CAPACITY), 180, 20});
    char cap_edit_str[32];
    snprintf(cap_edit_str, sizeof(cap_edit_str), "<  %d mAh  >", axp_edit_capacity);
    renderTextAlignRight(cap_edit_str, font_Arkhip, col_cyan, &(SDL_Rect){605, ROW_TY(AXP_MENU_CAPACITY), 150, 20});
    if (axp_menu_selected == AXP_MENU_CAPACITY)
        renderText("[LEFT/RIGHT] Change  [A] Save", font_small ? font_small : font_Arkhip, col_sub, &(SDL_Rect){215, ROW_TY(AXP_MENU_CAPACITY) + 2, 210, 16});

    // Item: Shutdown voltage (Voff, REG 31H bits 2-0)
    renderText("Shutdown Voltage:", font_Arkhip, col_val, &(SDL_Rect){50, ROW_TY(AXP_MENU_VOFF), 180, 20});
    char voff_edit_str[32];
    bool voff_dirty = (axp_status.voff_code >= 0 && axp_edit_voff != axp_status.voff_code);
    snprintf(voff_edit_str, sizeof(voff_edit_str), "<  %.1f V%s  >", voff_code_to_mv(axp_edit_voff) / 1000.0, voff_dirty ? "*" : "");
    renderTextAlignRight(voff_edit_str, font_Arkhip, col_cyan, &(SDL_Rect){605, ROW_TY(AXP_MENU_VOFF), 150, 20});
    if (axp_menu_selected == AXP_MENU_VOFF) {
        char voff_hint[48];
        if (axp_status.voff_code >= 0)
            snprintf(voff_hint, sizeof(voff_hint), "Now %.1f V  [A] Save", voff_code_to_mv(axp_status.voff_code) / 1000.0);
        else
            snprintf(voff_hint, sizeof(voff_hint), "[A] Save");
        renderText(voff_hint, font_small ? font_small : font_Arkhip, col_sub, &(SDL_Rect){215, ROW_TY(AXP_MENU_VOFF) + 2, 210, 16});
    }

    // Item: Sync 100%
    drawCenteredText("[ A: Set current capacity to 100% ]", font_Arkhip, axp_menu_selected == AXP_MENU_SYNC ? col_val : col_sub, 25, ROW_TY(AXP_MENU_SYNC), 590);

    // Item: Capacity learning
    const char *cal_btn_txt = axp_status.cal_en ? "[ A: Disable capacity learning ]" : "[ A: Enable capacity learning ]";
    drawCenteredText(cal_btn_txt, font_Arkhip, axp_menu_selected == AXP_MENU_CALIB ? (axp_status.cal_en ? col_warn : col_val) : col_sub, 25, ROW_TY(AXP_MENU_CALIB), 590);

    // Item: Battery test
    drawCenteredText("[ A: Battery test ]", font_Arkhip, axp_menu_selected == AXP_MENU_BTEST ? col_val : col_sub, 25, ROW_TY(AXP_MENU_BTEST), 590);
#undef ROW_TY

    // Toast feedback message (floating at top of screen)
    if (axp_feedback_until > 0 && axp_feedback_msg[0] != '\0') {
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_small ? font_small : font_Arkhip, axp_feedback_msg, &tw, &th);
        int bx = 320 - (tw + 30) / 2;
        Uint32 t_bg = SDL_MapRGB(screen->format, 20, 50, 30);
        Uint32 t_border = SDL_MapRGB(screen->format, 60, 180, 90);
        drawRectFill(bx, 44, tw + 30, 24, t_bg);
        drawFrame1px(bx, 44, tw + 30, 24, t_border);
        drawCenteredText(axp_feedback_msg, font_small ? font_small : font_Arkhip, col_green, bx, 48, tw + 30);
    }

    // Footer Badges
    drawBadgePill("[DPAD] Select / Adjust", font_small ? font_small : font_Arkhip, col_val, col_pill_bg, col_pill_border, 25, 444, 180, 26);
    drawBadgePill("[A] Confirm / Save", font_small ? font_small : font_Arkhip, col_val, col_pill_bg, col_pill_border, 220, 444, 160, 26);
    drawBadgePill("[B / SELECT] Back to Graph", font_small ? font_small : font_Arkhip, col_val, col_pill_bg, col_pill_border, 395, 444, 220, 26);

    // Modal Confirmation Dialog (Calibration)
    if (axp_confirm_calib) {
        dimScreen(190);
        int mw = 520, mh = 290, mx = 60, my = 95;
        Uint32 m_bg = SDL_MapRGB(screen->format, 12, 20, 32);
        Uint32 m_border = SDL_MapRGB(screen->format, 77, 132, 219);
        drawRectFill(mx, my, mw, mh, m_bg);
        drawFrame2px(mx, my, mw, mh, m_border);

        drawCenteredText("BATTERY CALIBRATION", font_title ? font_title : font_Arkhip, col_val, mx, my + 18, mw);
        drawCenteredText("Teaches the PMIC your battery's real capacity", font_small ? font_small : font_Arkhip, col_title, mx, my + 44, mw);

        int ib_x = mx + 20, ib_y = my + 72, ib_w = mw - 40, ib_h = 130;
        Uint32 ib_bg = SDL_MapRGB(screen->format, 18, 28, 44);
        Uint32 ib_border = SDL_MapRGB(screen->format, 40, 60, 90);
        drawRectFill(ib_x, ib_y, ib_w, ib_h, ib_bg);
        drawFrame1px(ib_x, ib_y, ib_w, ib_h, ib_border);

        if (axp_status.cal_en) {
            renderText("Capacity learning is a persistent PMIC setting.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 14, ib_w - 32, 20});
            renderText("It stays enabled and refines capacity on every cycle.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 40, ib_w - 32, 20});
            renderText("Disabling keeps the learned capacity and the counter.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 66, ib_w - 32, 20});
            renderText("Keeping it enabled is safe and recommended.", font_small ? font_small : font_Arkhip, col_green, &(SDL_Rect){ib_x + 16, ib_y + 92, ib_w - 32, 20});
        }
        else {
            // The safe order: capacity saved after a finished charge, so the counter starts right.
            // Correction only runs in the charge after a PMIC power-off at Voff (observed on a Mini Plus).
            renderText(axp_status.cap_set ? "1. Capacity is set (best saved right after a full charge)."
                                          : "1. First charge to 100%, then save Battery Capacity.",
                       font_small ? font_small : font_Arkhip, axp_status.cap_set ? col_val : col_warn, &(SDL_Rect){ib_x + 16, ib_y + 14, ib_w - 32, 20});
            char step2[80];
            if (axp_status.voff_code >= 0)
                snprintf(step2, sizeof(step2), "2. At ~10%% sleep until it powers off (%.1f V).", voff_code_to_mv(axp_status.voff_code) / 1000.0);
            else
                snprintf(step2, sizeof(step2), "2. At ~10%% sleep until it powers off by itself.");
            renderText(step2, font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 40, ib_w - 32, 20});
            renderText("3. Charge straight to 100% (Correction: In progress).", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 66, ib_w - 32, 20});
            renderText("4. The PMIC then saves the learned Total Capacity.", font_small ? font_small : font_Arkhip, col_green, &(SDL_Rect){ib_x + 16, ib_y + 92, ib_w - 32, 20});
        }

        // Two Action Buttons at Modal bottom
        int btn_w = 210, btn_h = 38, btn_y = my + 225;
        // Button 1 (Start/Stop)
        int b1_x = mx + 35;
        Uint32 b1_bg = axp_status.cal_en ? SDL_MapRGB(screen->format, 70, 30, 30) : SDL_MapRGB(screen->format, 25, 65, 35);
        Uint32 b1_border = axp_status.cal_en ? SDL_MapRGB(screen->format, 180, 70, 70) : SDL_MapRGB(screen->format, 60, 180, 90);
        drawRectFill(b1_x, btn_y, btn_w, btn_h, b1_bg);
        drawFrame2px(b1_x, btn_y, btn_w, btn_h, b1_border);
        const char *m_act_txt = axp_status.cal_en ? "[A] Disable" : "[A] Enable";
        drawCenteredText(m_act_txt, font_Arkhip, col_val, b1_x, btn_y + 8, btn_w);

        // Button 2 (Cancel)
        int b2_x = mx + mw - 35 - btn_w;
        Uint32 b2_bg = SDL_MapRGB(screen->format, 45, 45, 55);
        Uint32 b2_border = SDL_MapRGB(screen->format, 90, 90, 110);
        drawRectFill(b2_x, btn_y, btn_w, btn_h, b2_bg);
        drawFrame2px(b2_x, btn_y, btn_w, btn_h, b2_border);
        drawCenteredText("[B] Cancel", font_Arkhip, col_val, b2_x, btn_y + 8, btn_w);
    }

    // Modal Confirmation Dialog (capacity saved while charging)
    if (axp_confirm_cap) {
        dimScreen(190);
        int mw = 520, mh = 250, mx = 60, my = 115;
        Uint32 m_bg = SDL_MapRGB(screen->format, 12, 20, 32);
        Uint32 m_border = SDL_MapRGB(screen->format, 220, 140, 50);
        drawRectFill(mx, my, mw, mh, m_bg);
        drawFrame2px(mx, my, mw, mh, m_border);

        drawCenteredText("STILL CHARGING", font_title ? font_title : font_Arkhip, col_val, mx, my + 18, mw);

        int ib_x = mx + 20, ib_y = my + 54, ib_w = mw - 40, ib_h = 108;
        drawRectFill(ib_x, ib_y, ib_w, ib_h, SDL_MapRGB(screen->format, 18, 28, 44));
        drawFrame1px(ib_x, ib_y, ib_w, ib_h, SDL_MapRGB(screen->format, 40, 60, 90));
        renderText("The PMIC starts its counter from the battery voltage,", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 12, ib_w - 32, 20});
        renderText("which reads too high while charging.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 36, ib_w - 32, 20});
        renderText("Best: let it finish charging, then save.", font_small ? font_small : font_Arkhip, col_green, &(SDL_Rect){ib_x + 16, ib_y + 60, ib_w - 32, 20});
        renderText("Saving now? Press 'Set current capacity to 100%' at full.", font_small ? font_small : font_Arkhip, col_sub, &(SDL_Rect){ib_x + 16, ib_y + 84, ib_w - 32, 20});

        int btn_w = 210, btn_h = 38, btn_y = my + 190;
        int b1_x = mx + 35, b2_x = mx + mw - 35 - btn_w;
        drawRectFill(b1_x, btn_y, btn_w, btn_h, SDL_MapRGB(screen->format, 70, 50, 25));
        drawFrame2px(b1_x, btn_y, btn_w, btn_h, SDL_MapRGB(screen->format, 220, 140, 50));
        drawCenteredText("[A] Save anyway", font_Arkhip, col_val, b1_x, btn_y + 8, btn_w);
        drawRectFill(b2_x, btn_y, btn_w, btn_h, SDL_MapRGB(screen->format, 45, 45, 55));
        drawFrame2px(b2_x, btn_y, btn_w, btn_h, SDL_MapRGB(screen->format, 90, 90, 110));
        drawCenteredText("[B] Cancel", font_Arkhip, col_val, b2_x, btn_y + 8, btn_w);
    }

    // Modal Confirmation Dialog (Battery Health Test)
    if (axp_confirm_btest) {
        dimScreen(190);
        int mw = 520, mh = 296, mx = 60, my = 92;
        Uint32 m_bg = SDL_MapRGB(screen->format, 12, 20, 32);
        Uint32 m_border = SDL_MapRGB(screen->format, 220, 140, 50);
        drawRectFill(mx, my, mw, mh, m_bg);
        drawFrame2px(mx, my, mw, mh, m_border);

        drawCenteredText("BATTERY HEALTH TEST", font_title ? font_title : font_Arkhip, col_val, mx, my + 16, mw);
        drawCenteredText("Hardware stress test & internal resistance check", font_small ? font_small : font_Arkhip, col_warn, mx, my + 42, mw);

        int ib_x = mx + 20, ib_y = my + 68, ib_w = mw - 40, ib_h = 138;
        Uint32 ib_bg = SDL_MapRGB(screen->format, 18, 28, 44);
        Uint32 ib_border = SDL_MapRGB(screen->format, 40, 60, 90);
        drawRectFill(ib_x, ib_y, ib_w, ib_h, ib_bg);
        drawFrame1px(ib_x, ib_y, ib_w, ib_h, ib_border);

        renderText("NOTICE (about 45 s, battery only, >= 3.60 V):", font_small ? font_small : font_Arkhip, col_warn, &(SDL_Rect){ib_x + 16, ib_y + 12, ib_w - 32, 18});
        renderText("1. Screen dims; 15 s idle baseline is measured.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 34, ib_w - 32, 18});
        renderText("2. Both CPU cores at 100% load: 1200 MHz, then 1600 MHz.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 54, ib_w - 32, 18});
        renderText("3. Internal resistance = voltage sag / current step.", font_small ? font_small : font_Arkhip, col_val, &(SDL_Rect){ib_x + 16, ib_y + 76, ib_w - 32, 18});
        renderText("Clock & brightness restored. Turn Wi-Fi off for accuracy.", font_small ? font_small : font_Arkhip, col_green, &(SDL_Rect){ib_x + 16, ib_y + 102, ib_w - 32, 18});

        // Two Action Buttons at Modal bottom
        int btn_w = 210, btn_h = 38, btn_y = my + 232;
        // Button 1 (Start Test)
        int b1_x = mx + 35;
        Uint32 b1_bg = SDL_MapRGB(screen->format, 25, 65, 35);
        Uint32 b1_border = SDL_MapRGB(screen->format, 60, 180, 90);
        drawRectFill(b1_x, btn_y, btn_w, btn_h, b1_bg);
        drawFrame2px(b1_x, btn_y, btn_w, btn_h, b1_border);
        drawCenteredText("[A] Start Test", font_Arkhip, col_val, b1_x, btn_y + 8, btn_w);

        // Button 2 (Cancel)
        int b2_x = mx + mw - 35 - btn_w;
        Uint32 b2_bg = SDL_MapRGB(screen->format, 45, 45, 55);
        Uint32 b2_border = SDL_MapRGB(screen->format, 90, 90, 110);
        drawRectFill(b2_x, btn_y, btn_w, btn_h, b2_bg);
        drawFrame2px(b2_x, btn_y, btn_w, btn_h, b2_border);
        drawCenteredText("[B] Cancel", font_Arkhip, col_val, b2_x, btn_y + 8, btn_w);
    }

    // Battery health test progress / results
    if (bt.state != BT_OFF) {
        TTF_Font *fs = font_small ? font_small : font_Arkhip;
        dimScreen(190);
        int mw = 540, mh = 330, mx = 50, my = 75;
        Uint32 m_border = bt.state == BT_ABORTED ? SDL_MapRGB(screen->format, 224, 108, 117) : SDL_MapRGB(screen->format, 220, 140, 50);
        drawRectFill(mx, my, mw, mh, SDL_MapRGB(screen->format, 12, 20, 32));
        drawFrame2px(mx, my, mw, mh, m_border);
        drawCenteredText("BATTERY HEALTH TEST", font_title ? font_title : font_Arkhip, col_val, mx, my + 14, mw);

        static const char *stage_names[BT_STAGES] = {"Idle baseline", "Load 2 cores @ 1200 MHz", "Load 2 cores @ 1600 MHz"};
        char line[96];

        if (bt.state == BT_RUNNING) {
            Uint32 el = SDL_GetTicks() - bt.stage_start;
            if (el > BT_STAGE_MS)
                el = BT_STAGE_MS;
            snprintf(line, sizeof(line), "Stage %d/%d: %s", bt.stage + 1, BT_STAGES, stage_names[bt.stage]);
            drawCenteredText(line, fs, col_warn, mx, my + 42, mw);

            // overall progress bar
            int pb_x = mx + 30, pb_y = my + 66, pb_w = mw - 60, pb_h = 12;
            long total = (long)BT_STAGES * BT_STAGE_MS;
            long done = (long)bt.stage * BT_STAGE_MS + el;
            drawRectFill(pb_x, pb_y, pb_w, pb_h, SDL_MapRGB(screen->format, 30, 40, 58));
            drawRectFill(pb_x, pb_y, (int)(pb_w * done / total), pb_h, SDL_MapRGB(screen->format, 100, 215, 255));

            if (bt.stage > 0)
                snprintf(line, sizeof(line), "Live: %.3f V   %d mA  [2x CPU 100%%  BL 100%%]", bt.live_v / 1000.0, bt.live_i);
            else
                snprintf(line, sizeof(line), "Live: %.3f V   %d mA  [IDLE  BL 0%%]", bt.live_v / 1000.0, bt.live_i);
            drawCenteredText(line, font_Arkhip, col_val, mx, my + 86, mw);
        }
        else if (bt.state == BT_ABORTED) {
            drawCenteredText("TEST ABORTED", font_Arkhip, (SDL_Color){224, 108, 117}, mx, my + 46, mw);
            drawCenteredText(bt.msg, fs, col_val, mx, my + 72, mw);
            drawCenteredText("CPU clock and brightness restored.", fs, col_sub, mx, my + 92, mw);
        }
        else {
            int r = bt_final_r();
            if (r >= 0) {
                snprintf(line, sizeof(line), "Internal resistance: %d mOhm  -  %s", r, bt_rating(r));
                drawCenteredText(line, font_Arkhip, r < 260 ? col_green : col_warn, mx, my + 50, mw);
            }
            else
                drawCenteredText(bt.msg, fs, col_warn, mx, my + 50, mw);
            drawCenteredText("<160 excellent, <260 good, <380 aged, else replace", fs, col_sub, mx, my + 76, mw);
        }

        // per-stage table
        int tx = mx + 30, ty = my + 116, tw = mw - 60;
        drawRectFill(tx, ty, tw, 128, SDL_MapRGB(screen->format, 18, 28, 44));
        drawFrame1px(tx, ty, tw, 128, SDL_MapRGB(screen->format, 40, 60, 90));
        renderText("Stage", fs, col_lbl, &(SDL_Rect){tx + 12, ty + 8, 200, 18});
        renderTextAlignRight("Voltage", fs, col_lbl, &(SDL_Rect){tx + 300, ty + 8, 80, 18});
        renderTextAlignRight("Current", fs, col_lbl, &(SDL_Rect){tx + 390, ty + 8, 80, 18});
        renderTextAlignRight("R_int", fs, col_lbl, &(SDL_Rect){tx + tw - 12, ty + 8, 70, 18});
        for (int k = 0; k < BT_STAGES; k++) {
            int ry = ty + 36 + k * 30;
            bool measured = (k < bt.stage) || bt.state == BT_DONE || (bt.state == BT_ABORTED && k < bt.stage);
            measured = measured && bt.st[k].n > 0;
            SDL_Color c = (bt.state == BT_RUNNING && k == bt.stage) ? col_cyan : (measured ? col_val : col_sub);
            renderText(stage_names[k], fs, c, &(SDL_Rect){tx + 12, ry, 220, 18});
            if (measured) {
                snprintf(line, sizeof(line), "%.3f V", bt.st[k].v_mv / 1000.0);
                renderTextAlignRight(line, fs, c, &(SDL_Rect){tx + 300, ry, 80, 18});
                snprintf(line, sizeof(line), "%d mA", bt.st[k].i_ma);
                renderTextAlignRight(line, fs, c, &(SDL_Rect){tx + 390, ry, 80, 18});
                if (k > 0 && bt.state == BT_DONE) {
                    if (bt.r_mohm[k] >= 0)
                        snprintf(line, sizeof(line), "%d mOhm", bt.r_mohm[k]);
                    else
                        snprintf(line, sizeof(line), "-");
                    renderTextAlignRight(line, fs, c, &(SDL_Rect){tx + tw - 12, ry, 70, 18});
                }
            }
            else
                renderTextAlignRight("...", fs, c, &(SDL_Rect){tx + 390, ry, 80, 18});
        }

        const char *hint = bt.state == BT_RUNNING ? "[B] Cancel test" : "[A / B] Close";
        drawBadgePill(hint, fs, col_val, col_pill_bg, col_pill_border, mx + (mw - 200) / 2, my + mh - 46, 200, 30);
    }
}

void renderPage()
{
    char sub_title[128];
    safeBlitSurface(background, NULL, screen, NULL);

    switch (current_zoom) {
    case 0:
        sprintf(sub_title, "%s", "16 HOURS VIEW");
        segment_duration = 7200;
        draw_right_zoom_icon();
        break;
    case 1:
        sprintf(sub_title, "%s", "8 HOURS VIEW");
        segment_duration = 3600;
        draw_right_zoom_icon();
        safeBlitSurface(left_arrow, NULL, screen, &(SDL_Rect){LEFT_ARROW_X, LEFT_ARROW_Y, ARROW_LENGHT, ARROW_WIDTH});
        break;
    case 2:
        sprintf(sub_title, "%s", "4 HOURS VIEW");
        segment_duration = 1800;
        safeBlitSurface(left_arrow, NULL, screen, &(SDL_Rect){LEFT_ARROW_X, LEFT_ARROW_Y, ARROW_LENGHT, ARROW_WIDTH});
        break;
    default:
        sprintf(sub_title, "%s", "8 HOURS VIEW");
        segment_duration = 3600;
        draw_right_zoom_icon();
        safeBlitSurface(left_arrow, NULL, screen, &(SDL_Rect){LEFT_ARROW_X, LEFT_ARROW_Y, ARROW_LENGHT, ARROW_WIDTH});
        break;
    }
    if (estimation_line_size == 0)
        current_index = graph_max_size - (GRAPH_DISPLAY_SIZE_X * (int)(segment_duration / 1800));
    else
        current_index = begining_session_index;

    current_index -= (int)(current_page * (GRAPH_DISPLAY_SIZE_X * (int)(segment_duration / 1800)) / GRAPH_PAGE_SCROLL_SMOOTHNESS);

    if (current_index < 0)
        current_index = 0;

    switch_zoom_profile(segment_duration);

    renderText(label[0], font_Arkhip, color_white, &(SDL_Rect){LABEL1_X, LABEL_Y, 32, 32});
    renderText(label[1], font_Arkhip, color_white, &(SDL_Rect){LABEL2_X, LABEL_Y, 32, 32});
    renderText(label[2], font_Arkhip, color_white, &(SDL_Rect){LABEL3_X, LABEL_Y, 32, 32});
    renderText(label[3], font_Arkhip, color_white, &(SDL_Rect){LABEL4_X, LABEL_Y, 32, 32});

    // A "2" on the zoom icons (L2/R2)
    if (current_zoom > 0)
        renderText("2", font_title ? font_title : font_Arkhip, color_white, &(SDL_Rect){LEFT_ARROW_X + ARROW_LENGHT, LEFT_ARROW_Y + ZOOM_DIGIT_DY, 16, 24});

    session_mark *sel = NULL;
    if (selected_session >= 0 && selected_session < session_mark_count) {
        sel = &session_marks[selected_session];
        char length_text[12], levels_text[32] = "", tail_text[32] = "", sleep_text[64] = "", date_text[32] = "";
        // A charge: the time to reach its last level, then the time left on the charger.
        // batmon counts awake time only; with clock times a charge is timed on the clock,
        // so charging while asleep counts too.
        int main_len = sel->length - sel->tail;
        int tail_len = sel->tail;
        if (sel->has_times && sel->is_charge) {
            if (sel->tail > 0 && sel->tail_time >= sel->start_time && sel->tail_time <= sel->end_time) {
                main_len = (int)(sel->tail_time - sel->start_time);
                tail_len = (int)(sel->end_time - sel->tail_time);
            }
            else {
                main_len = (int)(sel->end_time - sel->start_time);
                tail_len = 0;
            }
        }
        secondsToHoursMinutes(main_len, length_text);
        // the Mini logs no percentage while charging (500)
        if (sel->start_level <= 100 && sel->end_level <= 100)
            snprintf(levels_text, sizeof(levels_text), " (%d%%-%d%%)", sel->start_level, sel->end_level);
        if (tail_len >= SESSION_MARK_MIN_LENGTH) {
            char t[12];
            secondsToHoursMinutes(tail_len, t);
            snprintf(tail_text, sizeof(tail_text), " +%s AT %d%%", t, sel->end_level);
        }
        // a session: the time asleep within it, the percent it cost and the rate per hour
        // (the rate from half an hour of sleep, shorter is too coarse at 1 % steps). Needs
        // clock times only around the sleep, not for the whole session.
        if (!sel->is_charge && sel->sleep >= SLEEP_MIN_SHOWN) {
            char t[12];
            secondsToHoursMinutes((int)sel->sleep, t);
            if (sel->sleep >= SLEEP_RATE_MIN && sel->sleep_drop > 0) {
                int tenths = (int)((long long)sel->sleep_drop * 36000 / sel->sleep); // %/h x10
                snprintf(sleep_text, sizeof(sleep_text), " +%s SLEEP -%d%% (%d.%d%%/H)", t,
                         sel->sleep_drop, tenths / 10, tenths % 10);
            }
            else
                snprintf(sleep_text, sizeof(sleep_text), " +%s SLEEP -%d%%", t, sel->sleep_drop);
        }
        // the date only with a hardware clock (Mini Plus / Flip): the Mini's clock restarts from
        // the time saved at shutdown
        if (sel->has_times && HAS_AXP()) {
            time_t t = (time_t)sel->start_time;
            struct tm *lt = localtime(&t);
            if (lt != NULL)
                strftime(date_text, sizeof(date_text), " %d.%m %H:%M", lt);
        }
        snprintf(sub_title, sizeof(sub_title), "%s: %s%s%s%s%s%s",
                 sel->is_charge ? "CHARGE" : "SESSION", length_text,
                 levels_text, tail_text, sleep_text, date_text, sel->is_current ? " NOW" : "");
    }
    // centred under the title
    int sub_w = 0, sub_h = 0;
    TTF_SizeUTF8(font_Arkhip, sub_title, &sub_w, &sub_h);
    renderText(sub_title, font_Arkhip, !sel ? color_pastel_blue : sel->is_charge ? (SDL_Color){CHARGE_COLOR}
                                                                                 : (SDL_Color){SESSION_CAPTION_COLOR},
               &(SDL_Rect){320 - sub_w / 2, SUB_TITLE_Y, 100, 50});

    renderText(session_duration, font_Arkhip, color_white, &(SDL_Rect){LABEL_SESSION_X, LABEL_SESSION_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    int current_width = renderText(current_percentage, font_Arkhip, color_white, &(SDL_Rect){LABEL_CURRENT_X, LABEL_CURRENT_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    renderText(battery_voltage, font_Arkhip, color_white, &(SDL_Rect){LABEL_CURRENT_X + current_width + LABEL_GAP, LABEL_CURRENT_Y, LABEL_SIZE_X, LABEL_SIZE_Y});

    // AVG over MIN, as LEFT over BEST: values right-aligned before "LEFT :"/"BEST :",
    // the labels in one column left of the wider value
    const char *avg_text = session_avg[0] ? session_avg : "-";
    const char *min_text = session_min[0] ? session_min : "-";
    int avg_w = renderTextAlignRight(avg_text, font_Arkhip, color_white, &(SDL_Rect){LABEL_STATS_RIGHT_X, LABEL_LEFT_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    int min_w = renderTextAlignRight(min_text, font_Arkhip, color_white, &(SDL_Rect){LABEL_STATS_RIGHT_X, LABEL_BEST_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    int stats_label_x = LABEL_STATS_RIGHT_X - (avg_w > min_w ? avg_w : min_w) - LABEL_GAP / 2;
    renderTextAlignRight("AVG :", font_Arkhip, color_pastel_blue, &(SDL_Rect){stats_label_x, LABEL_LEFT_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    renderTextAlignRight("MIN :", font_Arkhip, color_pastel_blue, &(SDL_Rect){stats_label_x, LABEL_BEST_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    renderTextAlignRight(session_left, font_Arkhip, color_white, &(SDL_Rect){LABEL_LEFT_X, LABEL_LEFT_Y, LABEL_SIZE_X, LABEL_SIZE_Y});
    renderTextAlignRight(session_best, font_Arkhip, color_white, &(SDL_Rect){LABEL_BEST_X, LABEL_BEST_Y, LABEL_SIZE_X, LABEL_SIZE_Y});

    // Center footer hint: Battery Menu / [SELECT]
    drawCenteredText("BATTERY MENU", font_Arkhip, color_pastel_blue, 210, LABEL_SESSION_Y, 220);
    drawCenteredText("[SELECT]", font_Arkhip, color_white, 210, LABEL_CURRENT_Y, 220);

    int half_line_width = (int)(GRAPH_LINE_WIDTH) / 2;

    Uint32 white_pixel_color = SDL_MapRGBA(screen->format, 255, 255, 255, 0);
    Uint32 charge_pixel_color = SDL_MapRGBA(screen->format, CHARGE_COLOR, 0); // was 255, 170, 170
    Uint32 blue_pixel_color = SDL_MapRGBA(screen->format, 89, 167, 255, 0);
    Uint32 pixel_color = white_pixel_color;

    int x;
    int y;
    int x_end = 0;
    int y_end = 0;
    int index;

    int zoom_level = (int)segment_duration / 1800;

    if (SDL_LockSurface(screen) == 0) {
        // a selected charge is tinted green, a session on battery Onion purple
        SDL_Color tint = (sel && sel->is_charge) ? (SDL_Color){CHARGE_COLOR} : (SDL_Color){SESSION_MARK_COLOR};
        int tint_alpha = (sel && sel->is_charge) ? CHARGE_MARK_TINT : SESSION_MARK_TINT;

        for (int i = 0; i < graph_max_size - current_index; i += zoom_level) {

            x = GRAPH_DISPLAY_START_X + (int)(i / zoom_level);
            y = graphic[i + current_index].pixel_height;

            bool is_charging = graphic[i + current_index].is_charging;
            bool is_estimated = graphic[i + current_index].is_estimated;
            bool in_selection = sel && is_charging == sel->is_charge && !is_estimated &&
                                i + current_index >= sel->start_index && i + current_index <= sel->end_index;

            // the chosen session: the area under its curve tinted, before the curve and
            // its dots are drawn over it
            if (in_selection && x < GRAPH_DISPLAY_START_X + GRAPH_DISPLAY_SIZE_X) {
                for (int k = GRAPH_DISPLAY_START_Y + 1; k < y; k++) {
                    Uint32 *p = (Uint32 *)((Uint8 *)screen->pixels + (480 - k) * screen->pitch) + x;
                    Uint8 r, g, b;
                    SDL_GetRGB(*p, screen->format, &r, &g, &b);
                    *p = SDL_MapRGB(screen->format,
                                    (r * (256 - tint_alpha) + tint.r * tint_alpha) >> 8,
                                    (g * (256 - tint_alpha) + tint.g * tint_alpha) >> 8,
                                    (b * (256 - tint_alpha) + tint.b * tint_alpha) >> 8);
                }
            }

            //if ((!is_charging)
            if ((!is_charging) && (!is_estimated))
                pixel_color = white_pixel_color; // a selection shows as the tint under the curve
            else if (is_charging)
                pixel_color = charge_pixel_color;
            else if (is_estimated) {
                pixel_color = blue_pixel_color;
                if ((y < GRAPH_DISPLAY_START_Y + 5) && (x < GRAPH_DISPLAY_SIZE_X + GRAPH_DISPLAY_START_X)) {
                    x_end = x - 12;
                    y_end = GRAPH_DISPLAY_SIZE_Y + GRAPH_DISPLAY_START_Y - 45;
                }
            }

            if (x < (GRAPH_DISPLAY_START_X + GRAPH_DISPLAY_SIZE_X)) {
                if (half_line_width >= 0) {
                    for (int k = -half_line_width; k <= half_line_width; k++) {
                        index = (480 - y + k) * screen->pitch + x * screen->format->BytesPerPixel;
                        *((Uint32 *)((Uint8 *)screen->pixels + index)) = pixel_color;
                    }
                }

                // Graph background: the logged history filled solid under the curve
                // with its colour at GRAPH_FILL_ALPHA (the dot grid hid the colours);
                // the LEFT estimate keeps the dots, to read as a prediction
                if (!is_estimated) {
                    Uint8 cr, cg, cb;
                    SDL_GetRGB(pixel_color, screen->format, &cr, &cg, &cb);
                    for (int k = y - 1; k > GRAPH_DISPLAY_START_Y; k--) {
                        Uint32 *p = (Uint32 *)((Uint8 *)screen->pixels + (480 - k) * screen->pitch) + x;
                        Uint8 r, g, b;
                        SDL_GetRGB(*p, screen->format, &r, &g, &b);
                        *p = SDL_MapRGB(screen->format,
                                        (r * (256 - GRAPH_FILL_ALPHA) + cr * GRAPH_FILL_ALPHA) >> 8,
                                        (g * (256 - GRAPH_FILL_ALPHA) + cg * GRAPH_FILL_ALPHA) >> 8,
                                        (b * (256 - GRAPH_FILL_ALPHA) + cb * GRAPH_FILL_ALPHA) >> 8);
                    }
                }
                else if ((x % GRAPH_BACKGROUND_OPACITY) == 0) {
                    for (int k = y; k > GRAPH_DISPLAY_START_Y; k--) {
                        if ((k % GRAPH_BACKGROUND_OPACITY) == 0) {
                            index = (480 - k) * screen->pitch + x * screen->format->BytesPerPixel;
                            *((Uint32 *)((Uint8 *)screen->pixels + index)) = pixel_color;
                        }
                    }
                }
            }
        }
        SDL_UnlockSurface(screen);
        if (x_end != 0)
            safeBlitSurface(end_graph, NULL, screen, &(SDL_Rect){x_end, y_end, 24, 45});
    }

    if (show_axp_menu)
        renderAxpMenu();

    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
}

int main(int argc, char *argv[])
{
    init();
    signal(SIGCONT, sigcont_handler);
    atexit(bt_restore);
    KeyState keystate[320] = {(KeyState)0};
    render_waiting_screen();

    getDeviceModel();
    getDeviceSerial();
    compute_graph();
    read_battery_voltage();
    check_and_clear_lowbat_flag(-1, false);
    renderPage();

    bool changed;

    // batmon reads the battery every 15 s and writes the log when the percentage
    // changes. Check at the same pace: redraw from the log when the file's mtime
    // has moved, and show the voltage of batmon's last reading. The SD card is FAT, whose
    // mtime counts in 2 s steps, so wait a moment after a change: batmon writes
    // two statements back to back (duration update, then the new row).
    time_t db_mtime = 0;
    file_isModified(BATTERY_LOG_FILE, &db_mtime);
    Uint32 next_check = SDL_GetTicks() + DB_CHECK_INTERVAL_MS;
    Uint32 refresh_at = 0;
    Uint32 last_input = SDL_GetTicks();
    Uint32 last_pass = SDL_GetTicks();
    Uint32 fast_checks_until = 0;

    while (!quit) {

        changed = false;

        Uint32 now = SDL_GetTicks();
        // Onion's sleep stops every process (SIGSTOP) and continues them on wake, batmon
        // included, which then reads the battery at its next 15 s check. A long gap
        // between passes or SIGCONT means we were stopped: query AXP221 immediately and recompute.
        if (sigcont_received || (Sint32)(now - last_pass) > WAKE_GAP_MS) {
            sigcont_received = 0;
            if (bt.state == BT_RUNNING)
                bt_abort("Interrupted by sleep");
            axp_poll_data();
            reset_graph();
            compute_graph();
            read_battery_voltage();
            changed = true;
            next_check = now;
            fast_checks_until = now + WAKE_FAST_CHECK_MS;
        }
        last_pass = now;
        if ((Sint32)(now - next_check) >= 0) {
            bool fast = (Sint32)(now - fast_checks_until) < 0;
            next_check = now + (fast ? DB_FAST_CHECK_INTERVAL_MS : DB_CHECK_INTERVAL_MS);
            if (file_isModified(BATTERY_LOG_FILE, &db_mtime))
                refresh_at = now + DB_SETTLE_MS;
            // a small file in /tmp (RAM), rewritten by batmon at each check
            if (read_battery_voltage())
                changed = true;
        }
        if (refresh_at != 0 && (Sint32)(now - refresh_at) >= 0) {
            refresh_at = 0;
            reset_graph();
            compute_graph();
            // a new session (charger in or out) shifts the list: keep the choice valid
            if (selected_session >= session_mark_count)
                selected_session = session_mark_count - 1;
            changed = true;
        }

        // Periodic clock advance: keep SESSION ticking live even while battery % remains constant
        static Uint32 last_clock_tick = 0;
        if ((Sint32)(now - last_clock_tick) >= 15000) {
            last_clock_tick = now;
            reset_graph();
            compute_graph();
            changed = true;
        }

        // Proactively clear Onion's false low-battery shutdown flag (/tmp/.lowBat) every 1s
        static Uint32 last_lowbat_tick = 0;
        if ((Sint32)(now - last_lowbat_tick) >= 1000) {
            last_lowbat_tick = now;
            check_and_clear_lowbat_flag(-1, false);
        }

        if (bt.state == BT_RUNNING) {
            if (bt_tick(now))
                changed = true;
            axp_last_poll = now; // the test polls the PMIC itself
        }
        else if (show_axp_menu && (Sint32)(now - axp_last_poll) >= 1000) {
            axp_last_poll = now;
            if (axp_poll_data())
                changed = true;
        }
        if (axp_feedback_until > 0 && (Sint32)(now - axp_feedback_until) >= 0) {
            axp_feedback_until = 0;
            changed = true;
        }

        if (updateKeystate(keystate, &quit, true, NULL)) {
            last_input = now;
            if (show_axp_menu && bt.state != BT_OFF) {
                // Battery test screen swallows all input except cancel / close
                if (bt.state == BT_RUNNING) {
                    if (keystate[SW_BTN_B] == PRESSED) {
                        bt_abort("Cancelled by user");
                        changed = true;
                    }
                }
                else if (keystate[SW_BTN_A] == PRESSED || keystate[SW_BTN_B] == PRESSED) {
                    bt.state = BT_OFF;
                    changed = true;
                }
            }
            else if (show_axp_menu) {
                if (axp_confirm_calib) {
                    if (keystate[SW_BTN_B] == PRESSED) {
                        axp_confirm_calib = false;
                        changed = true;
                    }
                    else if (keystate[SW_BTN_A] == PRESSED) {
                        bool new_state = !axp_status.cal_en;
                        int res = axp_set_calibration(new_state);
                        if (res == 0) {
                            snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                     new_state ? "Capacity learning enabled (CC on)" : "Capacity learning disabled (CC stays on)");
                        }
                        else if (res == -2) {
                            snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                     "PMIC rejected the change (REG B8H unchanged)");
                        }
                        else {
                            snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                     "I2C error: failed to update PMIC");
                        }
                        axp_feedback_until = now + 3000;
                        axp_confirm_calib = false;
                        axp_poll_data();
                        changed = true;
                    }
                    else if (keystate[SW_BTN_SELECT] == PRESSED) {
                        axp_confirm_calib = false;
                        changed = true;
                    }
                }
                else if (axp_confirm_cap) {
                    if (keystate[SW_BTN_B] == PRESSED || keystate[SW_BTN_SELECT] == PRESSED) {
                        axp_confirm_cap = false;
                        changed = true;
                    }
                    else if (keystate[SW_BTN_A] == PRESSED) {
                        axp_confirm_cap = false;
                        axp_save_capacity(now);
                        changed = true;
                    }
                }
                else if (axp_confirm_btest) {
                    if (keystate[SW_BTN_B] == PRESSED || keystate[SW_BTN_SELECT] == PRESSED) {
                        axp_confirm_btest = false;
                        changed = true;
                    }
                    else if (keystate[SW_BTN_A] == PRESSED) {
                        axp_confirm_btest = false;
                        bt_start(now);
                        changed = true;
                    }
                }
                else if (!axp_status.available) {
                    if (keystate[SW_BTN_B] == PRESSED || keystate[SW_BTN_SELECT] == PRESSED) {
                        show_axp_menu = false;
                        changed = true;
                    }
                }
                else {
                    if (keystate[SW_BTN_SELECT] == PRESSED || keystate[SW_BTN_B] == PRESSED) {
                        show_axp_menu = false;
                        changed = true;
                    }
                    else if (keystate[SW_BTN_UP] >= PRESSED) {
                        if (axp_menu_selected > 0) {
                            axp_menu_selected--;
                            changed = true;
                        }
                    }
                    else if (keystate[SW_BTN_DOWN] >= PRESSED) {
                        if (axp_menu_selected < AXP_MENU_COUNT - 1) {
                            axp_menu_selected++;
                            changed = true;
                        }
                    }
                    else if (axp_menu_selected == AXP_MENU_CAPACITY) {
                        if (keystate[SW_BTN_LEFT] >= PRESSED) {
                            int next = ((axp_edit_capacity - 1) / 50) * 50;
                            if (next >= 1500) {
                                axp_edit_capacity = next;
                                changed = true;
                            }
                        }
                        else if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                            int next = (axp_edit_capacity / 50 + 1) * 50;
                            if (next <= 4000) {
                                axp_edit_capacity = next;
                                changed = true;
                            }
                        }
                        else if (keystate[SW_BTN_A] == PRESSED) {
                            axp_poll_data();
                            if (axp_status.is_charging)
                                axp_confirm_cap = true; // ask first: the counter would start too high
                            else
                                axp_save_capacity(now);
                            changed = true;
                        }
                    }
                    else if (axp_menu_selected == AXP_MENU_VOFF) {
                        if (keystate[SW_BTN_LEFT] >= PRESSED) {
                            if (axp_edit_voff > 0) {
                                axp_edit_voff--;
                                changed = true;
                            }
                        }
                        else if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                            if (axp_edit_voff < AXP_VOFF_CODE_MAX) {
                                axp_edit_voff++;
                                changed = true;
                            }
                        }
                        else if (keystate[SW_BTN_A] == PRESSED) {
                            int res = axp_set_voff(axp_edit_voff);
                            if (res == 0)
                                snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                         "Shutdown voltage set to %.1f V", voff_code_to_mv(axp_edit_voff) / 1000.0);
                            else if (res == -2)
                                snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                         "PMIC rejected the change (REG 31H unchanged)");
                            else
                                snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                         "I2C error: failed to set shutdown voltage");
                            axp_feedback_until = now + 3000;
                            axp_poll_data();
                            changed = true;
                        }
                    }
                    else if (axp_menu_selected == AXP_MENU_SYNC) {
                        if (keystate[SW_BTN_A] == PRESSED) {
                            int cap = (axp_status.capacity_mah > 0) ? axp_status.capacity_mah : axp_edit_capacity;
                            if (axp_set_coulomb_counter(cap)) {
                                snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                         "Synced counter to 100%% (%d mAh)", cap);
                            }
                            else {
                                snprintf(axp_feedback_msg, sizeof(axp_feedback_msg),
                                         "Failed to sync counter");
                            }
                            axp_feedback_until = now + 3000;
                            axp_poll_data();
                            changed = true;
                        }
                    }
                    else if (axp_menu_selected == AXP_MENU_CALIB) {
                        if (keystate[SW_BTN_A] == PRESSED) {
                            axp_confirm_calib = true;
                            changed = true;
                        }
                    }
                    else if (axp_menu_selected == AXP_MENU_BTEST) {
                        if (keystate[SW_BTN_A] == PRESSED) {
                            axp_confirm_btest = true;
                            changed = true;
                        }
                    }
                }
            }
            else {
                if (keystate[SW_BTN_SELECT] == PRESSED) {
                    show_axp_menu = true;
                    axp_confirm_calib = false;
                    axp_confirm_btest = false;
                    axp_confirm_cap = false;
                    axp_menu_selected = AXP_MENU_CAPACITY;
                    axp_poll_data();
                    if (axp_status.available && axp_status.capacity_mah >= 1500 && axp_status.capacity_mah <= 4000) {
                        int rounded = ((axp_status.capacity_mah + 25) / 50) * 50;
                        if (abs(axp_status.capacity_mah - rounded) <= 1)
                            axp_edit_capacity = rounded;
                        else
                            axp_edit_capacity = 3000;
                    }
                    else {
                        axp_edit_capacity = 3000;
                    }
                    if (axp_status.available && axp_status.voff_code >= 0)
                        axp_edit_voff = axp_status.voff_code;
                    changed = true;
                }
                else if (keystate[SW_BTN_B] == PRESSED)
                    quit = true;
                else if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                    if (current_page > 0)
                        current_page--;
                    changed = true;
                }
                else if (keystate[SW_BTN_LEFT] >= PRESSED) {
                    int page_max = (int)(((GRAPH_MAX_FULL_PAGES) * (GRAPH_PAGE_SCROLL_SMOOTHNESS)) / (segment_duration / 1800));
                    page_max -= GRAPH_PAGE_SCROLL_SMOOTHNESS;

                    if (page_max > current_page)
                        current_page++;
                    changed = true;
                }
                // L2/R2 zoom (L1/R1 did too before they went to the sessions); a chosen
                // session stays in view
                else if (keystate[SW_BTN_R2] >= PRESSED) {
                    if (current_zoom < 2) {
                        current_zoom++;
                        current_page = selected_session >= 0 ? page_for_index(session_marks[selected_session].start_index) : 0;
                        changed = true;
                    }
                }
                else if (keystate[SW_BTN_L2] >= PRESSED) {
                    if (current_zoom > 0) {
                        current_zoom--;
                        current_page = selected_session >= 0 ? page_for_index(session_marks[selected_session].start_index) : 0;
                        changed = true;
                    }
                }
                // L1: the next older session's start, R1: the next newer one; R1 from the
                // newest goes back to the normal view
                else if (keystate[SW_BTN_L1] >= PRESSED) {
                    if (selected_session + 1 < session_mark_count) {
                        selected_session++;
                        current_page = page_for_index(session_marks[selected_session].start_index);
                        changed = true;
                    }
                }
                else if (keystate[SW_BTN_R1] >= PRESSED) {
                    if (selected_session >= 0) {
                        selected_session--;
                        current_page = selected_session >= 0 ? page_for_index(session_marks[selected_session].start_index) : 0;
                        changed = true;
                    }
                }
            }
        }

        if (!changed) {
            // Without a pause this loop kept one core busy the whole time. Poll slower
            // while no button moves, and at full pace for a moment after one, for smooth
            // scrolling and key repeat. Measured idle on a Mini Plus (2026-11-02): about
            // 0.1 % of a core.
            bool active = (Sint32)(now - last_input) < INPUT_ACTIVE_MS;
            SDL_Delay(active ? INPUT_POLL_ACTIVE_MS : INPUT_POLL_IDLE_MS);
            continue;
        }

        renderPage();
    }
    check_and_clear_lowbat_flag(-1, false);
    bt_restore(); // MENU / SIGTERM during a test: put clock & brightness back
    free_resources();
    return EXIT_SUCCESS;
}
