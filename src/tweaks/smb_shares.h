#ifndef TWEAKS_SMB_SHARES_H__
#define TWEAKS_SMB_SHARES_H__

// The shares of smb.conf for Tweaks > Network > File sharing (Samba),
// without SDL so host tests run it.
//
// The old reader kept the byte offset of each share's "available = " line
// and its toggle wrote "available = N" there. A share without that line
// took the offset of the share before it (its toggle switched the other
// share), a "path = " line before the first share wrote to the share at
// index -1 (crash), and an indented line was overwritten from its first
// byte, leaving the rest of the old text as a stray line. Now the file is
// read line by line into sections, and a toggle rewrites the file
// atomically, changing (or adding) the line of the share it is for.

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "utils/file.h"
#include "utils/str.h"

typedef struct {
    char name[STR_MAX - 11]; // shown: without a leading "__", " (!)" added then
    char section[STR_MAX];   // as in the file, between [ and ]
    char path[STR_MAX];
    int available; // 1 if available = yes, 0 otherwise
} Share;

static const char *smb_skipSpace(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

// "[name]" at p (after the indent): the name's length, or -1.
static int smb_sectionName(const char *p, const char **name_out)
{
    if (*p != '[')
        return -1;
    const char *end = strchr(p + 1, ']');
    if (end == NULL)
        return -1;
    *name_out = p + 1;
    return (int)(end - (p + 1));
}

// "key = value" at p: true when the key is key (Samba ignores case and
// spaces around '='); the value, trimmed, goes to value (size bytes).
static bool smb_keyValue(const char *p, const char *key, char *value, size_t size)
{
    size_t key_len = strlen(key);
    if (strncasecmp(p, key, key_len) != 0)
        return false;
    const char *q = smb_skipSpace(p + key_len);
    if (*q != '=')
        return false;
    q = smb_skipSpace(q + 1);
    size_t len = strcspn(q, "\r\n");
    while (len > 0 && (q[len - 1] == ' ' || q[len - 1] == '\t'))
        len--;
    if (size > 0) {
        if (len >= size)
            len = size - 1;
        memcpy(value, q, len);
        value[len] = '\0';
    }
    return true;
}

static bool smb_isTrue(const char *value)
{
    return strcmp(value, "1") == 0 || strcasecmp(value, "yes") == 0 ||
           strcasecmp(value, "true") == 0;
}

// The shares of an open smb.conf ([global] is not one); *count_out of
// them. NULL when there are none or memory runs out.
static Share *smb_readShares(FILE *file, int *count_out)
{
    *count_out = 0;
    Share *shares = NULL;
    int count = 0;
    Share *current = NULL;

    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, file) >= 0) {
        const char *p = smb_skipSpace(line);
        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\n' || *p == '\r')
            continue;

        const char *name;
        int name_len = smb_sectionName(p, &name);
        if (name_len >= 0) {
            current = NULL;
            if (name_len == 0 || (name_len == 6 && strncasecmp(name, "global", 6) == 0))
                continue;
            Share *grown = realloc(shares, (size_t)(count + 1) * sizeof(Share));
            if (grown == NULL)
                break;
            shares = grown;
            current = &shares[count++];
            memset(current, 0, sizeof(*current));
            current->available = 1; // Samba's default

            int keep = name_len < (int)sizeof(current->section) - 1 ? name_len : (int)sizeof(current->section) - 1;
            memcpy(current->section, name, (size_t)keep);
            current->section[keep] = '\0';

            const char *shown = current->section;
            bool warn = strncmp(shown, "__", 2) == 0;
            if (warn)
                shown += 2;
            snprintf(current->name, sizeof(current->name), warn ? "%.*s (!)" : "%.*s",
                     (int)sizeof(current->name) - 5, shown);
            continue;
        }

        if (current == NULL)
            continue; // [global], or before any section

        char value[STR_MAX];
        if (smb_keyValue(p, "path", value, sizeof(value)))
            snprintf(current->path, sizeof(current->path), "%s", value);
        else if (smb_keyValue(p, "available", value, sizeof(value)))
            current->available = smb_isTrue(value);
    }
    free(line);

    if (count == 0) {
        free(shares);
        return NULL;
    }
    *count_out = count;
    return shares;
}

// Set "available" of the share in section of the smb.conf at path: its
// line is rewritten (indent kept), or added at the end of the section. The
// file is replaced atomically. False, with the file as it was, when the
// share is not there or the file can't be written.
static bool smb_setAvailable(const char *path, const char *section, bool available)
{
    FILE *in = fopen(path, "r");
    if (in == NULL)
        return false;
    char tmp_path[PATH_MAX], final_path[PATH_MAX];
    FILE *out = file_atomic_begin(path, tmp_path, sizeof(tmp_path), final_path, sizeof(final_path));
    if (out == NULL) {
        fclose(in);
        return false;
    }

    bool in_section = false, done = false, ends_with_newline = true;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, in)) >= 0) {
        const char *p = smb_skipSpace(line);
        const char *name;
        int name_len = *p == '#' || *p == ';' ? -1 : smb_sectionName(p, &name);
        if (name_len >= 0) {
            if (in_section && !done) {
                fprintf(out, "available = %d\n", available ? 1 : 0);
                done = true;
            }
            in_section = (size_t)name_len == strlen(section) &&
                         strncasecmp(name, section, (size_t)name_len) == 0;
        }
        else if (in_section && !done && *p != '#' && *p != ';' &&
                 smb_keyValue(p, "available", NULL, 0)) {
            fprintf(out, "%.*savailable = %d\n", (int)(p - line), line, available ? 1 : 0);
            done = true;
            continue;
        }
        fwrite(line, 1, (size_t)len, out);
        ends_with_newline = len > 0 && line[len - 1] == '\n';
    }
    free(line);

    if (in_section && !done) {
        fprintf(out, "%savailable = %d\n", ends_with_newline ? "" : "\n", available ? 1 : 0);
        done = true;
    }
    bool ok = done && !ferror(in) && !ferror(out);
    fclose(in);
    if (!ok) {
        fclose(out);
        remove(tmp_path);
        return false;
    }
    return file_atomic_commit(out, tmp_path, final_path);
}

#endif // TWEAKS_SMB_SHARES_H__
