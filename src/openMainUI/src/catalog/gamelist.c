/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/gamelist.h"
#include "catalog/catalog.h"
#include "catalog/pinyin.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define XML_LIMIT (16u * 1024u * 1024u)
#define XML_DEPTH 32

typedef struct {
    uint32_t hash, offset, length;
} MetadataRecord;

#define IMPORT_FOLDER_BUCKETS 256
#define IMPORT_INDEX_BYTES (32u * 1024u * 1024u)

typedef struct ImportFile {
    struct ImportFile *next;
    unsigned char type;
    char name[];
} ImportFile;

typedef struct ImportFolder {
    struct ImportFolder *next;
    ImportFile **files;
    size_t buckets, count;
    bool fallback;
    char path[];
} ImportFolder;

typedef struct {
    sqlite3 *database;
    sqlite3_stmt *insert, *folder;
    const char *sd, *root, *saved_root;
    char values[6][MAINUI_PATH_MAX];
    size_t lengths[6];
    bool seen[6];
    MainUIMetadata *metadata;
    MetadataRecord *index;
    uint32_t record_start, record_end;
    const char *filename;
    unsigned records;
    MainUICancel cancel;
    ImportFolder *folders[IMPORT_FOLDER_BUCKETS];
    size_t index_bytes;
} Import;

static bool xml_character(unsigned value)
{
    return value == 9 || value == 10 || value == 13 || (value >= 32 && value <= 0xd7ff) ||
           (value >= 0xe000 && value <= 0xfffd) || (value >= 0x10000 && value <= 0x10ffff);
}

/* Validate source UTF-8 before tokenization, including ignored metadata. This
 * excludes overlong encodings, surrogates and forbidden XML control characters. */
static bool valid_utf8(const unsigned char *text, size_t size)
{
    for (size_t i = 0; i < size;) {
        unsigned value = text[i++];
        unsigned extra = 0, minimum = 0;
        if (value >= 0xc2 && value <= 0xdf) {
            extra = 1;
            minimum = 0x80;
            value &= 0x1f;
        }
        else if (value >= 0xe0 && value <= 0xef) {
            extra = 2;
            minimum = 0x800;
            value &= 0x0f;
        }
        else if (value >= 0xf0 && value <= 0xf4) {
            extra = 3;
            minimum = 0x10000;
            value &= 7;
        }
        else if (value >= 0x80) {
            return false;
        }
        if (extra > size - i) {
            return false;
        }
        while (extra--) {
            unsigned next = text[i++];
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || !xml_character(value)) {
            return false;
        }
    }
    return true;
}

/* Decode only predefined entities and numeric references. DTDs and external
 * entities are unsupported, so XML cannot trigger additional file reads. */
static bool append_text(const char *text, size_t size, bool entities, char *out, size_t *length)
{
    for (size_t i = 0; i < size; i++) {
        unsigned value = (unsigned char)text[i];
        char bytes[4] = {(char)value};
        size_t count = 1;
        if (entities && value == '&') {
            size_t start = ++i;
            while (i < size && text[i] != ';' && i - start < 16) {
                i++;
            }
            if (i == size || text[i] != ';') {
                return false;
            }
            size_t n = i - start;
            const char *entity = text + start;
            if (n && entity[0] == '#') {
                size_t digit = 1;
                unsigned base = 10;
                if (digit < n && entity[digit] == 'x') {
                    base = 16;
                    digit++;
                }
                if (digit == n) {
                    return false;
                }
                value = 0;
                for (; digit < n; digit++) {
                    unsigned char c = (unsigned char)entity[digit];
                    unsigned d = 99;
                    if (c >= '0' && c <= '9') {
                        d = c - '0';
                    }
                    else if (c >= 'a' && c <= 'f') {
                        d = c - 'a' + 10;
                    }
                    else if (c >= 'A' && c <= 'F') {
                        d = c - 'A' + 10;
                    }
                    if (d >= base || value > (0x10ffff - d) / base) {
                        return false;
                    }
                    value = value * base + d;
                }
            }
            else if (n == 3 && !memcmp(entity, "amp", n)) {
                value = '&';
            }
            else if (n == 2 && !memcmp(entity, "lt", n)) {
                value = '<';
            }
            else if (n == 2 && !memcmp(entity, "gt", n)) {
                value = '>';
            }
            else if (n == 4 && !memcmp(entity, "quot", n)) {
                value = '"';
            }
            else if (n == 4 && !memcmp(entity, "apos", n)) {
                value = '\'';
            }
            else {
                return false;
            }
            if (!xml_character(value)) {
                return false;
            }
            if (value < 0x80) {
                bytes[0] = (char)value;
            }
            else {
                count = value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
                unsigned remaining = value;
                for (size_t j = count - 1; j; j--) {
                    bytes[j] = (char)(0x80 | (remaining & 63));
                    remaining >>= 6;
                }
                unsigned prefix = count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0;
                bytes[0] = (char)(prefix | remaining);
            }
        }
        else if (value < 32 && !xml_character(value)) {
            return false;
        }
        if (out) {
            if (*length + count >= MAINUI_PATH_MAX) {
                return false;
            }
            memcpy(out + *length, bytes, count);
            *length += count;
            out[*length] = 0;
        }
    }
    return true;
}

static bool insert_row(Import *import, const char *label, const char *path, const char *image,
                       int type, const char *parent)
{
    char pinyin[MAINUI_PATH_MAX];
    pinyin[0] = 0;
    if (!type) {
        mainui_pinyin(import->sd, label, pinyin, sizeof pinyin);
    }
    sqlite3_stmt *statement = import->insert;
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    return sqlite3_bind_text(statement, 1, label, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_bind_text(statement, 2, path, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_bind_text(statement, 3, image, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_bind_int(statement, 4, type) == SQLITE_OK &&
           sqlite3_bind_text(statement, 5, parent, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_bind_text(statement, 6, pinyin, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_bind_text(statement, 7, pinyin, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
           sqlite3_step(statement) == SQLITE_DONE;
}

static bool regular_file(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

/* Exact filename keys preserve case-sensitive host semantics. */
static uint32_t file_hash(const char *name)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        hash = (hash ^ *p) * 16777619u;
    }
    return hash;
}

static void clear_folder_index(Import *import, ImportFolder *folder)
{
    for (size_t i = 0; i < folder->buckets; ++i) {
        ImportFile *entry = folder->files[i];
        while (entry) {
            ImportFile *next = entry->next;
            import->index_bytes -= sizeof *entry + strlen(entry->name) + 1;
            free(entry);
            entry = next;
        }
    }
    import->index_bytes -= folder->buckets * sizeof *folder->files;
    free(folder->files);
    folder->files = NULL;
    folder->buckets = folder->count = 0;
}

static void close_folder_indexes(Import *import)
{
    for (size_t i = 0; i < IMPORT_FOLDER_BUCKETS; ++i) {
        ImportFolder *folder = import->folders[i];
        while (folder) {
            ImportFolder *next = folder->next;
            clear_folder_index(import, folder);
            free(folder);
            folder = next;
        }
    }
}

static bool index_file(Import *import, ImportFolder *folder, const struct dirent *entry)
{
    if (!folder->buckets || folder->count >= folder->buckets * 2) {
        size_t count = folder->buckets ? folder->buckets * 2 : 64;
        size_t bytes = count * sizeof *folder->files;
        if (bytes > IMPORT_INDEX_BYTES - import->index_bytes) {
            return false;
        }
        ImportFile **files = calloc(count, sizeof *files);
        if (!files) {
            return false;
        }
        for (size_t i = 0; i < folder->buckets; ++i) {
            ImportFile *item = folder->files[i];
            while (item) {
                ImportFile *next = item->next;
                size_t bucket = file_hash(item->name) % count;
                item->next = files[bucket];
                files[bucket] = item;
                item = next;
            }
        }
        import->index_bytes += bytes - folder->buckets * sizeof *files;
        free(folder->files);
        folder->files = files;
        folder->buckets = count;
    }
    size_t bytes = sizeof(ImportFile) + strlen(entry->d_name) + 1;
    if (bytes > IMPORT_INDEX_BYTES - import->index_bytes) {
        return false;
    }
    ImportFile *item = malloc(bytes);
    if (!item) {
        return false;
    }
    strcpy(item->name, entry->d_name);
    item->type = entry->d_type;
    size_t bucket = file_hash(item->name) % folder->buckets;
    item->next = folder->files[bucket];
    folder->files[bucket] = item;
    folder->count++;
    import->index_bytes += bytes;
    return true;
}

/* Keep one directory snapshot per import. On enumeration/allocation failure,
 * use the old stat path rather than silently dropping valid XML records. */
static bool indexed_regular_file(Import *import, const char *path)
{
    char parent[MAINUI_PATH_MAX];
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        return regular_file(path);
    }
    size_t length = (size_t)(slash - path);
    memcpy(parent, path, length);
    parent[length] = 0;
    size_t bucket = file_hash(parent) % IMPORT_FOLDER_BUCKETS;
    ImportFolder *folder = import->folders[bucket];
    while (folder && strcmp(folder->path, parent)) {
        folder = folder->next;
    }
    if (!folder) {
        size_t bytes = sizeof *folder + length + 1;
        if (bytes > IMPORT_INDEX_BYTES - import->index_bytes) {
            return regular_file(path);
        }
        folder = calloc(1, bytes);
        if (!folder) {
            return regular_file(path);
        }
        strcpy(folder->path, parent);
        folder->next = import->folders[bucket];
        import->folders[bucket] = folder;
        import->index_bytes += bytes;
        DIR *dir = opendir(parent);
        folder->fallback = !dir;
        if (dir) {
            for (;;) {
                if (mainui_cancelled(import->cancel)) {
                    folder->fallback = true;
                    break;
                }
                errno = 0;
                struct dirent *entry = readdir(dir);
                if (!entry) {
                    folder->fallback = errno != 0;
                    break;
                }
                if (!index_file(import, folder, entry)) {
                    folder->fallback = true;
                    break;
                }
            }
            if (closedir(dir)) {
                folder->fallback = true;
            }
        }
        if (folder->fallback) {
            clear_folder_index(import, folder);
        }
    }
    if (mainui_cancelled(import->cancel)) {
        return false;
    }
    if (folder->fallback) {
        return regular_file(path);
    }
    const char *name = slash + 1;
    ImportFile *entry = folder->buckets ? folder->files[file_hash(name) % folder->buckets] : NULL;
    for (; entry; entry = entry->next) {
        if (!strcmp(entry->name, name)) {
            if (entry->type == DT_UNKNOWN || entry->type == DT_LNK) {
                return regular_file(path);
            }
            return entry->type == DT_REG;
        }
    }
    /* A case-insensitive filesystem can resolve a differently spelled name.
     * Preserve that behavior on misses; exact regular-file hits avoid stat. */
    return regular_file(path);
}

/* Compare the key relative to this XML directory, accepting the reference
 * ./ or / prefix and ASCII case folding. Root fallback keys include subfolders. */
static uint32_t metadata_hash(const char *path)
{
    if (!strncmp(path, "./", 2)) {
        path += 2;
    }
    else if (*path == '/') {
        path++;
    }
    uint32_t hash = 2166136261u;
    while (*path) {
        hash = (hash ^ (unsigned char)tolower((unsigned char)*path++)) * 16777619u;
    }
    return hash;
}

static bool metadata_game(Import *import)
{
    if (import->index) {
        import->index[import->records - 1] =
            (MetadataRecord){metadata_hash(import->values[0]), import->record_start,
                             import->record_end - import->record_start};
        return true;
    }
    const char *path = import->values[0], *filename = import->filename;
    if (!strncmp(path, "./", 2)) {
        path += 2;
    }
    else if (*path == '/') {
        path++;
    }
    while (*path && *filename &&
           tolower((unsigned char)*path) == tolower((unsigned char)*filename)) {
        path++;
        filename++;
    }
    if (*path || *filename || import->metadata->found) {
        return true;
    }
    MainUIMetadata *result = import->metadata;
    result->found = true;
    strcpy(result->genre, import->values[3]);
    strcpy(result->description, import->values[5]);
    const char *rating = import->values[4];
    while (isspace((unsigned char)*rating)) {
        rating++;
    }
    if (*rating >= '0' && *rating <= '9') {
        unsigned whole = 0, fraction = 0, divisor = 1, digits = 0;
        /* Preserve the reference numeric-prefix rule without integer overflow. */
        while (*rating >= '0' && *rating <= '9') {
            whole = whole || *rating != '0';
            rating++;
        }
        if (*rating == '.') {
            rating++;
            while (*rating >= '0' && *rating <= '9' && digits++ < 3) {
                fraction = fraction * 10 + (unsigned)(*rating++ - '0');
                divisor *= 10;
            }
        }
        unsigned score = whole ? 10 : (fraction * 10 + divisor / 2) / divisor;
        snprintf(result->rating, sizeof result->rating, "%u/10", score);
    }
    return true;
}

static bool import_game(Import *import)
{
    if (++import->records > (import->metadata ? 32768u : 1000000u)) {
        return false;
    }
    if (import->metadata) {
        return metadata_game(import);
    }
    const char *path = import->values[0], *label = import->values[1];
    if (!*path || !*label) {
        return true;
    }
    char resolved[MAINUI_PATH_MAX], relative[MAINUI_PATH_MAX], parent[MAINUI_PATH_MAX] = ".";
    if (!mainui_catalog_path(resolved, import->sd, import->root, path)) {
        return true;
    }
    size_t root_length = strlen(import->root);
    /* Outside-root records have no navigable parent in this catalog. Skip them
     * along with missing ROM files; this boundary is documented explicitly. */
    if (strncmp(resolved, import->root, root_length) || resolved[root_length] != '/' ||
        !indexed_regular_file(import, resolved)) {
        return true;
    }
    int n = snprintf(relative, sizeof relative, "%s", resolved + root_length + 1);
    if (n < 0 || n >= (int)sizeof relative) {
        return false;
    }
    int depth = 0;
    for (const char *slash = strchr(relative, '/'); slash; slash = strchr(slash + 1, '/')) {
        if (++depth >= MAINUI_STACK_MAX - 1) {
            return true;
        }
    }
    for (char *slash = strchr(relative, '/'); slash; slash = strchr(slash + 1, '/')) {
        *slash = 0;
        sqlite3_reset(import->folder);
        if (sqlite3_bind_text(import->folder, 1, relative, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(import->folder) != SQLITE_DONE) {
            return false;
        }
        /* XML subfolders and deeper nesting use inferred stock scan rules. */
        char folder_path[MAINUI_PATH_MAX];
        int size = snprintf(folder_path, sizeof folder_path, "%s/%s", import->saved_root, relative);
        const char *name = strrchr(relative, '/');
        if (size < 0 || size >= (int)sizeof folder_path ||
            (sqlite3_changes(import->database) &&
             !insert_row(import, name ? name + 1 : relative, folder_path, folder_path, 1,
                         parent))) {
            return false;
        }
        strcpy(parent, relative);
        *slash = '/';
    }
    /* The patcher requires an empty image for missing/null text, never the
     * previous game's image. Each game starts with zeroed field storage. */
    char stored[MAINUI_PATH_MAX], image[MAINUI_PATH_MAX] = "";
    /* Preserve decoded XML text, including ./; absolute XML paths already
     * carry their prefix and must remain launchable. Validation above uses host paths. */
    n = snprintf(stored, sizeof stored, "%s%s%s", *path == '/' ? "" : import->saved_root,
                 *path == '/' ? "" : "/", path);
    if (n < 0 || n >= (int)sizeof stored) {
        return false;
    }
    const char *art = import->values[2];
    if (*art) {
        n = snprintf(image, sizeof image, "%s%s%s", *art == '/' ? "" : import->saved_root,
                     *art == '/' ? "" : "/", art);
        if (n < 0 || n >= (int)sizeof image) {
            return false;
        }
    }
    return insert_row(import, label, stored, image, 0, parent);
}

static void whitespace(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n') {
        (*cursor)++;
    }
}

static bool name(const char **cursor, char out[64])
{
    const char *start = *cursor;
    if (!isalpha((unsigned char)**cursor) && **cursor != '_') {
        return false;
    }
    while (**cursor && (isalnum((unsigned char)**cursor) || strchr("_:-.", **cursor))) {
        if (*cursor - start >= 63) {
            return false;
        }
        (*cursor)++;
    }
    size_t size = (size_t)(*cursor - start);
    memcpy(out, start, size);
    out[size] = 0;
    return true;
}

/* Bounded event parser retains one record, not a DOM. Unknown metadata is
 * ignored after syntax checks; nested markup in imported fields is rejected. */
static bool parse(Import *import, const char *cursor)
{
    const char *base = cursor;
    char stack[XML_DEPTH][64];
    int depth = 0, field = -1;
    bool root_seen = false;
    if (!strncmp(cursor, "\xef\xbb\xbf", 3)) {
        cursor += 3;
    }
    while (*cursor) {
        if (mainui_cancelled(import->cancel)) {
            return false;
        }
        if (*cursor != '<') {
            const char *end = strchr(cursor, '<');
            if (!end) {
                end = cursor + strlen(cursor);
            }
            for (const char *check = cursor; end - check >= 3; check++) {
                if (!memcmp(check, "]]>", 3)) {
                    return false;
                }
            }
            if (!depth) {
                whitespace(&cursor);
                if (cursor != end) {
                    return false;
                }
            }
            else if (!append_text(cursor, (size_t)(end - cursor), true,
                                  field >= 0 ? import->values[field] : NULL,
                                  field >= 0 ? &import->lengths[field] : NULL)) {
                return false;
            }
            cursor = end;
            continue;
        }
        if (!strncmp(cursor, "<!--", 4) || !strncmp(cursor, "<?", 2)) {
            bool comment = cursor[1] == '!';
            const char *end = strstr(cursor + (comment ? 4 : 2), comment ? "--" : "?>");
            if (!end || (comment && end[2] != '>')) {
                return false;
            }
            cursor = end + (comment ? 3 : 2);
            continue;
        }
        if (!strncmp(cursor, "<![CDATA[", 9)) {
            const char *end = strstr(cursor + 9, "]]>");
            if (!depth || !end ||
                !append_text(cursor + 9, (size_t)(end - cursor - 9), false,
                             field >= 0 ? import->values[field] : NULL,
                             field >= 0 ? &import->lengths[field] : NULL)) {
                return false;
            }
            cursor = end + 3;
            continue;
        }
        const char *tag_start = cursor;
        cursor++;
        bool closing = *cursor == '/';
        if (closing) {
            cursor++;
        }
        char tag[64];
        if (!name(&cursor, tag)) {
            return false;
        }
        if (closing) {
            whitespace(&cursor);
            if (*cursor++ != '>' || !depth || strcmp(tag, stack[depth - 1])) {
                return false;
            }
        }
        else {
            if (depth == XML_DEPTH || field >= 0) {
                return false;
            }
            if (!depth && (root_seen || strcmp(tag, "gameList"))) {
                return false;
            }
            if (!depth) {
                root_seen = true;
            }
            if (depth == 1 && !strcmp(tag, "game")) {
                import->record_start = (uint32_t)(tag_start - base);
                memset(import->values, 0, sizeof import->values);
                memset(import->lengths, 0, sizeof import->lengths);
                memset(import->seen, 0, sizeof import->seen);
            }
            if (depth == 2 && !strcmp(stack[1], "game")) {
                if (!strcmp(tag, "path")) {
                    field = 0;
                }
                else if (!strcmp(tag, "name")) {
                    field = 1;
                }
                else if (!strcmp(tag, "image")) {
                    field = 2;
                }
                else if (import->metadata && !strcmp(tag, "genre")) {
                    field = 3;
                }
                else if (import->metadata && !strcmp(tag, "rating")) {
                    field = 4;
                }
                else if (import->metadata && !strcmp(tag, "desc")) {
                    field = 5;
                }
                if (field >= 0) {
                    if (import->seen[field]) {
                        return false;
                    }
                    import->seen[field] = true;
                }
            }
            strcpy(stack[depth++], tag);
            while (*cursor && *cursor != '>' && *cursor != '/') {
                const char *before = cursor;
                whitespace(&cursor);
                if (*cursor == '>' || *cursor == '/') {
                    break;
                }
                char attribute[64];
                if (cursor == before || !name(&cursor, attribute)) {
                    return false;
                }
                whitespace(&cursor);
                if (*cursor++ != '=') {
                    return false;
                }
                whitespace(&cursor);
                char quote = *cursor++;
                if (quote != '\'' && quote != '"') {
                    return false;
                }
                const char *end = strchr(cursor, quote);
                if (!end || memchr(cursor, '<', (size_t)(end - cursor)) ||
                    !append_text(cursor, (size_t)(end - cursor), true, NULL, NULL)) {
                    return false;
                }
                cursor = end + 1;
            }
            closing = *cursor == '/';
            if (closing) {
                cursor++;
            }
            if (*cursor++ != '>') {
                return false;
            }
        }
        if (closing) {
            if (depth == 2 && !strcmp(stack[1], "game") &&
                (import->record_end = (uint32_t)(cursor - base), !import_game(import))) {
                return false;
            }
            if (depth == 3) {
                field = -1;
            }
            depth--;
        }
    }
    return root_seen && !depth;
}

bool mainui_gamelist_import_control(sqlite3 *database, sqlite3_stmt *insert, const char *sd,
                                    const char *root, const char *saved_root, bool *present,
                                    MainUICancel cancel)
{
    *present = true;
    char path[MAINUI_PATH_MAX];
    if (!mainui_catalog_path(path, sd, root, "miyoogamelist.xml")) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        *present = errno != ENOENT;
        return !*present;
    }
    char *data = malloc(XML_LIMIT + 1);
    if (!data) {
        fclose(file);
        return false;
    }
    size_t size = fread(data, 1, XML_LIMIT, file);
    bool ok = !ferror(file) && fgetc(file) == EOF && valid_utf8((const unsigned char *)data, size);
    fclose(file);
    data[size] = 0;
    Import *import = calloc(1, sizeof *import);
    if (!import) {
        free(data);
        return false;
    }
    import->database = database;
    import->insert = insert;
    import->sd = sd;
    import->root = root;
    import->saved_root = saved_root;
    import->cancel = cancel;
    /* Folder deduplication lives in SQLite instead of a growing heap model.
     * This temporary table never reaches the published cache file. */
    if (ok) {
        ok = sqlite3_exec(database, "CREATE TEMP TABLE mainui_xml_folders(path TEXT PRIMARY KEY)",
                          NULL, NULL, NULL) == SQLITE_OK &&
             sqlite3_prepare_v2(database, "INSERT OR IGNORE INTO mainui_xml_folders VALUES (?1)",
                                -1, &import->folder, NULL) == SQLITE_OK;
    }
    if (ok) {
        ok = parse(import, data);
    }
    sqlite3_finalize(import->folder);
    close_folder_indexes(import);
    free(import);
    free(data);
    return ok;
}

/* UI-thread cache: sixteen XML paths, at most 2 MiB of compact record locations.
 * Full buffers exist only while building; selected records are read on demand. */
#define METADATA_BYTES (2u * 1024u * 1024u)
#define METADATA_RECORDS 32768u
#define METADATA_READ 32768u

typedef struct {
    char path[MAINUI_PATH_MAX];
    MainUIFileStamp stamp;
    MetadataRecord *records;
    size_t count;
    uint64_t used;
    bool valid;
} MetadataIndex;

static MetadataIndex metadata_cache[16];
static uint64_t metadata_clock;
static size_t metadata_bytes;
#ifdef MAINUI_METADATA_TEST
static size_t metadata_builds, metadata_reads;

void mainui_gamelist_metadata_stats(size_t *builds, size_t *reads, size_t *bytes)
{
    *builds = metadata_builds;
    *reads = metadata_reads;
    *bytes = metadata_bytes;
}
#endif

static void discard_index(MetadataIndex *index)
{
    metadata_bytes -= index->count * sizeof *index->records;
    free(index->records);
    *index = (MetadataIndex){0};
}

void mainui_gamelist_metadata_close(void)
{
    for (size_t i = 0; i < 16; i++) {
        discard_index(&metadata_cache[i]);
    }
    metadata_clock = 0;
}

static int record_compare(const void *a, const void *b)
{
    const MetadataRecord *x = a, *y = b;
    if (x->hash != y->hash) {
        return x->hash < y->hash ? -1 : 1;
    }
    return (x->offset > y->offset) - (x->offset < y->offset);
}

static MetadataIndex *get_index(const char *path, MainUIFileStamp stamp)
{
    MetadataIndex *slot = &metadata_cache[0];
    for (size_t i = 0; i < 16; i++) {
        MetadataIndex *item = &metadata_cache[i];
        if (!strcmp(item->path, path)) {
            if (mainui_file_stamp_equal(item->stamp, stamp)) {
                item->used = ++metadata_clock;
                return item;
            }
            slot = item;
            break;
        }
        if (item->used < slot->used) {
            slot = item;
        }
    }
    discard_index(slot);
    strcpy(slot->path, path);
    slot->stamp = stamp;
    slot->used = ++metadata_clock;
    if (!stamp.exists || stamp.size > 8u * 1024u * 1024u) {
        return slot;
    }
#ifdef MAINUI_METADATA_TEST
    metadata_builds++;
#endif
    char *text = mainui_read_text(path, 8u * 1024u * 1024u);
    MetadataRecord *records = malloc(METADATA_RECORDS * sizeof *records);
    MainUIMetadata *unused = calloc(1, sizeof *unused);
    Import *import = calloc(1, sizeof *import);
    if (import) {
        import->metadata = unused;
        import->index = records;
    }
    bool ok = text && records && import && unused &&
              valid_utf8((const unsigned char *)text, strlen(text)) && parse(import, text) &&
              mainui_file_stamp_equal(stamp, mainui_file_stamp(path));
    size_t record_count = import ? import->records : 0;
    free(import);
    free(unused);
    free(text);
    if (!ok) {
        free(records);
        return slot;
    }
    size_t bytes = record_count * sizeof *records;
    if (!bytes) {
        free(records);
        records = NULL;
    }
    else {
        MetadataRecord *compact = realloc(records, bytes);
        if (!compact) {
            free(records);
            return slot;
        }
        records = compact;
        qsort(records, record_count, sizeof *records, record_compare);
    }
    while (metadata_bytes + bytes > METADATA_BYTES) {
        MetadataIndex *oldest = NULL;
        for (size_t i = 0; i < 16; i++) {
            MetadataIndex *item = &metadata_cache[i];
            if (item != slot && item->records && (!oldest || item->used < oldest->used)) {
                oldest = item;
            }
        }
        if (!oldest) {
            free(records);
            return slot;
        }
        discard_index(oldest);
    }
    slot->records = records;
    slot->count = record_count;
    slot->valid = true;
    metadata_bytes += bytes;
    return slot;
}

static bool read_metadata(const char *directory, size_t directory_length, const char *key,
                          MainUIMetadata *result, bool *missing)
{
    *missing = false;
    *result = (MainUIMetadata){0};
    char path[MAINUI_PATH_MAX];
    if (directory_length >= MAINUI_PATH_MAX) {
        return false;
    }
    int length = snprintf(path, sizeof path, "%.*s/gamelist.xml", (int)directory_length, directory);
    if (length < 0 || length >= (int)sizeof path) {
        return false;
    }
    MainUIFileStamp stamp = mainui_file_stamp(path);
    *missing = !stamp.exists;
    MetadataIndex *index = get_index(path, stamp);
    if (!index->valid) {
        return false;
    }
    uint32_t hash = metadata_hash(key);
    size_t low = 0, high = index->count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (index->records[mid].hash < hash) {
            low = mid + 1;
        }
        else {
            high = mid;
        }
    }
    for (; low < index->count && index->records[low].hash == hash; low++) {
        MetadataRecord record = index->records[low];
        if (record.length > METADATA_READ) {
            return false;
        }
        FILE *file = fopen(path, "rb");
        if (!file) {
            return false;
        }
#ifdef MAINUI_METADATA_TEST
        metadata_reads += record.length;
#endif
        char *text = malloc(record.length + 22);
        bool ok = text && !fseek(file, (long)record.offset, SEEK_SET);
        if (ok) {
            memcpy(text, "<gameList>", 10);
            ok = fread(text + 10, 1, record.length, file) == record.length && !ferror(file);
            memcpy(text + 10 + record.length, "</gameList>", 12);
        }
        fclose(file);
        MainUIMetadata pending = {0};
        Import *import = calloc(1, sizeof *import);
        if (import) {
            import->metadata = &pending;
            import->filename = key;
        }
        ok = ok && import && valid_utf8((const unsigned char *)text, record.length + 21) &&
             parse(import, text) && mainui_file_stamp_equal(stamp, mainui_file_stamp(path));
        free(import);
        free(text);
        if (!ok) {
            discard_index(index);
            return false;
        }
        if (pending.found) {
            *result = pending;
            return true;
        }
    }
    return true;
}

bool mainui_gamelist_metadata(const char *rom, const char *root, MainUIMetadata *result)
{
    *result = (MainUIMetadata){0};
    const char *filename = strrchr(rom, '/');
    if (!filename) {
        return false;
    }
    bool missing = false;
    bool ok = read_metadata(rom, (size_t)(filename - rom), filename + 1, result, &missing);
    if (result->found || (!ok && !missing) || !root || !*root) {
        return ok;
    }
    size_t length = strlen(root);
    while (length && root[length - 1] == '/') {
        length--;
    }
    /* Never search above the configured console, or use a sibling's basename. */
    if (!length || length >= (size_t)(filename - rom) || strncmp(rom, root, length) ||
        rom[length] != '/') {
        return ok;
    }
    return read_metadata(root, length, rom + length + 1, result, &missing);
}

bool mainui_gamelist_import(sqlite3 *database, sqlite3_stmt *insert, const char *sd,
                            const char *root, const char *saved_root, bool *present)
{
    return mainui_gamelist_import_control(database, insert, sd, root, saved_root, present,
                                          (MainUICancel){0});
}
