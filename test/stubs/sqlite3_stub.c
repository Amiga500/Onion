/* Minimal sqlite3 link stubs for host tests that include production headers
 * (e.g. cacheDB.h) but only exercise their non-database logic. Every call
 * fails cleanly: opening reports SQLITE_CANTOPEN, so no statement is ever
 * produced. Prototypes come from the vendored include/sqlite3/sqlite3.h. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3/sqlite3.h>

int sqlite3_open(const char *filename, sqlite3 **db)
{
    (void)filename;
    if (db != NULL)
        *db = NULL;
    return SQLITE_CANTOPEN;
}
int sqlite3_close(sqlite3 *db) { (void)db; return SQLITE_OK; }
int sqlite3_close_v2(sqlite3 *db) { (void)db; return SQLITE_OK; }
const char *sqlite3_errmsg(sqlite3 *db) { (void)db; return "stub"; }
int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int n, sqlite3_stmt **stmt, const char **tail)
{
    (void)db; (void)sql; (void)n; (void)tail;
    if (stmt != NULL)
        *stmt = NULL;
    return SQLITE_ERROR;
}
int sqlite3_step(sqlite3_stmt *stmt) { (void)stmt; return SQLITE_DONE; }
const unsigned char *sqlite3_column_text(sqlite3_stmt *stmt, int col)
{
    (void)stmt; (void)col;
    return NULL;
}
int sqlite3_finalize(sqlite3_stmt *stmt) { (void)stmt; return SQLITE_OK; }
/* %q/%Q are sqlite-only conversions: return an empty query instead. */
char *sqlite3_mprintf(const char *fmt, ...)
{
    (void)fmt;
    char *s = malloc(1);
    if (s != NULL)
        s[0] = '\0';
    return s;
}
void sqlite3_free(void *p) { free(p); }
