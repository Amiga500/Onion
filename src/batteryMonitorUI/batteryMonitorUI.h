#ifndef BATTERY_MONITOR_UI_H
#define BATTERY_MONITOR_UI_H
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <math.h>
#include <signal.h>
#include <sqlite3/sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "system/display.h"
#include "system/keymap_sw.h"
#include "system/system.h"
#include "utils/file.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/str.h"
#include "utils/surfaceSetAlpha.h"

#define GRAPH_LINE_WIDTH 1

// Look for new battery log entries and a new voltage at batmon's own pace
// (CHECK_BATTERY_TIMEOUT_S), and wait a moment after a change before reading
#define DB_CHECK_INTERVAL_MS 15000
#define DB_SETTLE_MS 1000

// After a wake from sleep (a gap of more than WAKE_GAP_MS between loop passes), check
// every DB_FAST_CHECK_INTERVAL_MS for WAKE_FAST_CHECK_MS: batmon reads the battery
// within its 15 s cycle after the wake
#define WAKE_GAP_MS 2000
#define WAKE_FAST_CHECK_MS 30000
#define DB_FAST_CHECK_INTERVAL_MS 1000

// Button polling: every INPUT_POLL_IDLE_MS while idle, every INPUT_POLL_ACTIVE_MS for
// INPUT_ACTIVE_MS after the last button event
#define INPUT_POLL_IDLE_MS 50
#define INPUT_POLL_ACTIVE_MS 16
#define INPUT_ACTIVE_MS 1000

// LEFT from the history (see estimate_left_seconds): a drop of more than this many
// percent at once is a gauge jump; levels average neighbours within the window; the
// session's own load is fully trusted once it has drained this many percent
#define LEFT_MAX_STEP 5
#define LEFT_CURVE_WINDOW 2
#define LEFT_FULL_TRUST_PCT 60

// Sessions picked with L1/R1: at most this many marked; the chosen one shown by tinting
// the area under its curve with this colour (strength out of 256), its start
// kept this far inside the view
#define MAX_SESSION_MARKS 256
#define SESSION_MARK_COLOR 136, 97, 252 // Onion purple (boot screen)
#define SESSION_MARK_MARGIN 20
#define SESSION_MARK_TINT 80
// the caption under the title for the chosen one: a lighter Onion purple, readable on
// the blue background
#define SESSION_CAPTION_COLOR 190, 168, 255
// the graph while charging (was a pink red, 255, 170, 170; red reads as a low-battery
// warning, green as charging)
#define CHARGE_COLOR 125, 219, 143
// a selected charge: tinted in CHARGE_COLOR at this strength (out of 256, 30 %), and its
// caption in CHARGE_COLOR
#define CHARGE_MARK_TINT 77
#define SESSION_MARK_MIN_LENGTH 300 // s: shorter ones are charger flickers, not sessions
// A session's sleep is shown from this long in total
#define SLEEP_MIN_SHOWN 300
// A clock gap between two entries counts as sleep from this long and this share of the
// older entry's awake time (batmon's count runs a few percent slow, as each loop pass
// takes a little over a second); the sleep rate (%/h) is shown from SLEEP_RATE_MIN of sleep
#define SLEEP_GAP_MIN 120
// %: a charge's level rising more than this between two log entries means the console was off
// meanwhile (batmon logs nothing then), so without clock times the charge's length is incomplete
#define CHARGE_OFF_JUMP_MIN 3
#define SLEEP_GAP_MIN_PCT 10
#define SLEEP_RATE_MIN 1800

// "V2" after the title in the background (Arkhip at 20 matches its letters)
#define TITLE_FONT_SIZE 20
#define TITLE_V2_X 452
#define TITLE_V2_Y 11

// The zoom buttons moved to L2/R2: a "2" after the L and R of the zoom icons
#define ZOOM_R_LETTER_W 14 // the R's columns at the left of right_arrow.png
// The "2" in the title font is as tall as the icons' L and R (rows 6-20 of the icon;
// the font's glyph box starts 3 px above its letters)
#define ZOOM_DIGIT_DY 3

// Space between pixels, higher is more transparent
#define GRAPH_BACKGROUND_OPACITY 4

// The history under the curve: its colour blended in at this strength (out of 256, 30 %)
#define GRAPH_FILL_ALPHA 77

// A multiple of 4 is recommended
#define GRAPH_MAX_FULL_PAGES 8

#define GRAPH_DISPLAY_SIZE_X 583
#define GRAPH_DISPLAY_SIZE_Y 324
#define GRAPH_DISPLAY_START_X 29
#define GRAPH_DISPLAY_START_Y 79
#define GRAPH_DISPLAY_DURATION 16200
#define GRAPH_PAGE_SCROLL_SMOOTHNESS 12
#define GRAPH_MIN_SESSION_FOR_ESTIMATION 1200
#define GRAPH_MAX_PLAUSIBLE_ESTIMATION 54000
#define GRAPH_ESTIMATED_LINE_GAP 20

#define LABEL_Y 410

#define LABEL1_X 150
#define LABEL2_X 278
#define LABEL3_X 407
#define LABEL4_X 538

#define SUB_TITLE_X 255
#define SUB_TITLE_Y 32

#define LABEL_SESSION_X 100
#define LABEL_SESSION_Y 439
#define LABEL_CURRENT_X 108
#define LABEL_CURRENT_Y 454
#define LABEL_LEFT_X 630
#define LABEL_LEFT_Y 438
#define LABEL_BEST_X 630
#define LABEL_BEST_Y 453

// The voltage after CURRENT, and AVG/MIN ending just left of the "BEST :" in the background
#define LABEL_GAP 10
#define LABEL_STATS_RIGHT_X 515

#define LABEL_SIZE_X 65
#define LABEL_SIZE_Y 15

#define RIGHT_ARROW_X 591
#define RIGHT_ARROW_Y 13

#define LEFT_ARROW_X 0
#define LEFT_ARROW_Y 13

#define ARROW_LENGHT 48
#define ARROW_WIDTH 28

#endif //BATTERY_MONITOR_UI_H
