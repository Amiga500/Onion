#ifndef PLAY_ACTIVITY_SQL_H__
#define PLAY_ACTIVITY_SQL_H__

// Closing play sessions. play_activity_stop() and play_activity_stop_all()
// close open sessions (play_time IS NULL) at their current length. A session
// longer than a day (or negative) comes from a clock jump during the session,
// e.g. the time being set from 1970 by network time sync, and is discarded.
// Only the session being closed is checked: stored rows, such as play times
// imported from the old format (one row per game with its total, created_at
// 0), are never removed for their length. Rows with a negative play time are
// removed, as in official Onion.
#define PLAY_ACTIVITY_MAX_SESSION_S 86400
#define _PA_STR2(x) #x
#define _PA_STR(x) _PA_STR2(x)

#define _PA_NOW "(strftime('%s', 'now'))"

// Every open session (runs before each suspend).
#define PLAY_ACTIVITY_CLOSE_ALL_SQL                                                                                                                                                                                                                  \
    "BEGIN;"                                                                                                                                                                                                                                         \
    "DELETE FROM play_activity WHERE play_time IS NULL AND (" _PA_NOW " - created_at < 0 OR " _PA_NOW " - created_at > " _PA_STR(PLAY_ACTIVITY_MAX_SESSION_S) ");"                                                                                   \
                                                                                                                                                              "UPDATE play_activity SET play_time = " _PA_NOW " - created_at, updated_at = " _PA_NOW \
                                                                                                                                                              " WHERE play_time IS NULL;"                                                            \
                                                                                                                                                              "DELETE FROM play_activity WHERE play_time < 0;"                                       \
                                                                                                                                                              "COMMIT;"

// One game's open session: a sqlite3_mprintf() format taking the rom id three
// times ('%' is doubled because it is a format string).
#define _PA_NOW_FMT "(strftime('%%s', 'now'))"
#define PLAY_ACTIVITY_CLOSE_ROM_FMT                                                                                                                                                            \
    "BEGIN;"                                                                                                                                                                                   \
    "DELETE FROM play_activity WHERE rom_id = %d AND play_time IS NULL AND (" _PA_NOW_FMT                                                                                                      \
    " - created_at < 0 OR " _PA_NOW_FMT " - created_at > " _PA_STR(PLAY_ACTIVITY_MAX_SESSION_S) ");"                                                                                           \
                                                                                                "UPDATE play_activity SET play_time = " _PA_NOW_FMT " - created_at, updated_at = " _PA_NOW_FMT \
                                                                                                " WHERE rom_id = %d AND play_time IS NULL;"                                                    \
                                                                                                "DELETE FROM play_activity WHERE rom_id = %d AND play_time < 0;"                               \
                                                                                                "COMMIT;"

#endif // PLAY_ACTIVITY_SQL_H__
