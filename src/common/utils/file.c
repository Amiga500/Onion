#define _LARGEFILE64_SOURCE
#define _XOPEN_SOURCE 700

#include "file.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "str.h"

bool exists(const char *file_path)
{
    struct stat64 buffer;
    return stat64(file_path, &buffer) == 0;
}

bool is_file(const char *file_path)
{
    struct stat64 buffer;
    return stat64(file_path, &buffer) == 0 && S_ISREG(buffer.st_mode);
}

bool is_dir(const char *file_path)
{
    struct stat64 buffer;
    return stat64(file_path, &buffer) == 0 && S_ISDIR(buffer.st_mode);
}

bool file_isModified(const char *path, time_t *old_mtime)
{
    struct stat64 file_stat;
    if (stat64(path, &file_stat) == 0 && file_stat.st_mtime > *old_mtime) {
        *old_mtime = file_stat.st_mtime;
        return true;
    }
    return false;
}

bool file_isLocked(const char *path)
{
    int fd = open(path, O_RDONLY | O_CREAT, 0666);
    if (fd == -1)
        return true;
    close(fd);
    return false;
}

const char *file_basename(const char *filename)
{
    const char *p = strrchr(filename, '/');
    return p ? p + 1 : filename;
}

/**
 * @brief Create directories in dir_path (mkdir -p semantics, no shell).
 *
 * @param dir_path The full directory path.
 * @return true If the path didn't exist (dirs were created).
 * @return false If the path exists (no dirs were created).
 */
bool mkdirs(const char *dir_path)
{
    if (exists(dir_path))
        return false;

    char tmp[PATH_MAX];
    size_t len = strlen(dir_path);
    if (len == 0 || len >= sizeof(tmp))
        return false;
    memcpy(tmp, dir_path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

void file_readLastLine(const char *filename, char *out_str)
{
    FILE *fd;
    long size;
    char buff[256];
    char *token = NULL;

    if ((fd = fopen(filename, "rb")) != NULL) {
        // get file size
        fseek(fd, 0L, SEEK_END);
        size = ftell(fd);

        // Read the last (up to) 255 bytes. This used to read 254 bytes
        // starting 255 bytes from the end, dropping the final byte of any
        // file of 255 bytes or more (the last character of a line without
        // a trailing newline).
        long max_len = size < (long)sizeof(buff) - 1 ? size : (long)sizeof(buff) - 1;
        if (max_len <= 0) {
            fclose(fd);
            return;
        }

        if (fseek(fd, -max_len, SEEK_END) != 0 ||
            fread(buff, (size_t)max_len, 1, fd) != 1) {
            fclose(fd);
            return;
        }

        // cleanup
        fclose(fd);
        buff[max_len] = '\0';

        char *saveptr;
        token = strtok_r(buff, "\n", &saveptr);
        while (token != NULL) {
            if (strlen(token) > 0)
                snprintf(out_str, 255, "%s", token);
            token = strtok_r(NULL, "\n", &saveptr);
        }
    }
}

char *file_read(const char *path)
{
    struct stat64 st;
    if (stat64(path, &st) != 0 || st.st_size < 0)
        return NULL;

    // Safety check: limit file size to 100MB to prevent excessive memory allocation
    if (st.st_size > 100 * 1024 * 1024)
        return NULL;

    /* Empty file: match OnionUI/Onion (malloc(1) + NUL), not NULL. */
    if (st.st_size == 0) {
        char *empty = (char *)malloc(1);
        if (empty == NULL)
            return NULL;
        empty[0] = '\0';
        return empty;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    char *buffer = (char *)malloc(st.st_size + 1);
    if (buffer == NULL) {
        close(fd);
        return NULL;
    }

    ssize_t total = 0;
    while (total < st.st_size) {
        ssize_t nread = read(fd, buffer + total, st.st_size - total);
        if (nread <= 0)
            break;
        total += nread;
    }
    close(fd);

    if (total <= 0) {
        free(buffer);
        return NULL;
    }

    buffer[total] = '\0';
    return buffer;
}

bool file_write(const char *path, const char *str, uint32_t len)
{
    int fd;
    if ((fd = open(path, O_WRONLY)) < 0)
        return false;
    if (write(fd, str, len) == -1) {
        close(fd);
        return false;
    }
    close(fd);
    return true;
}

void file_copy(const char *src_path, const char *dest_path)
{
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0)
        return;

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        close(src_fd);
        return;
    }

    int dst_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        close(src_fd);
        return;
    }

    char buf[4096];
    ssize_t nread;
    while ((nread = read(src_fd, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        while (nread > 0) {
            ssize_t nwritten = write(dst_fd, p, nread);
            if (nwritten < 0) {
                close(src_fd);
                close(dst_fd);
                return;
            }
            nread -= nwritten;
            p += nwritten;
        }
    }

    close(src_fd);
    close(dst_fd);
}

char *file_removeExtension(const char *myStr)
{
    if (myStr == NULL)
        return NULL;
    size_t len = strlen(myStr);
    char *retStr = (char *)malloc(len + 1);
    char *lastExt;
    if (retStr == NULL)
        return NULL;
    memcpy(retStr, myStr, len + 1);
    if ((lastExt = strrchr(retStr, '.')) != NULL && *(lastExt + 1) != ' ' && *(lastExt + 1) != '\0' && *(lastExt + 2) != '\0')
        *lastExt = '\0';
    return retStr;
}

char *file_dirname(const char *absolutePath)
{
    const char *lastSlash = strrchr(absolutePath, '/');
    if (lastSlash != NULL) {
        char *path;
        size_t pathLength = lastSlash - absolutePath;
        path = (char *)malloc(pathLength + 1);
        if (path != NULL) {
            strncpy(path, absolutePath, pathLength);
            path[pathLength] = '\0';
        }
        return path;
    }
    return NULL;
}

void file_cleanName(char *name_out, const char *file_name)
{
    char *name_without_ext = file_removeExtension(file_name);
    char *no_underscores = str_replace(name_without_ext, "_", " ");
    if (no_underscores == NULL) {
        // Handle allocation failure
        if (name_without_ext != NULL) {
            strncpy(name_out, name_without_ext, STR_MAX - 1);
            name_out[STR_MAX - 1] = '\0';
            free(name_without_ext);
        }
        else {
            name_out[0] = '\0';
        }
        return;
    }
    char *dot_ptr = strstr(no_underscores, ".");
    if (dot_ptr != NULL) {
        char *s = no_underscores;
        while (isdigit(*s) && s < dot_ptr)
            s++;
        if (s != dot_ptr)
            dot_ptr = no_underscores;
        else {
            dot_ptr++;
            if (dot_ptr[0] == ' ')
                dot_ptr++;
        }
    }
    else {
        dot_ptr = no_underscores;
    }
    str_removeParentheses(name_out, dot_ptr);
    free(name_without_ext);
    free(no_underscores);
}

const char *file_getExtension(const char *filename)
{
    if (filename == NULL)
        return "";
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

char *file_parseKeyValue(const char *file_path, const char *key_in,
                         char *value_out, char divider, int select_index)
{
    FILE *fp;
    int f;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char key[256], val[256];
    char key_search[STR_MAX];
    char search_str[STR_MAX];
    snprintf(search_str, sizeof(search_str), "%%255[^%c]%c%%255[^\n]\n", divider, divider);
    int match_index = 0;

    *value_out = 0;
    if ((fp = fopen(file_path, "r"))) {
        key[0] = 0;
        val[0] = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
            if (!(f = sscanf(line, search_str, key, val)))
                continue;
            if (str_trim(key_search, 256, key, true)) {
                if (strcmp(key_search, key_in) == 0) {
                    str_trim(value_out, 256, val, false);
                    if ((match_index++) == select_index)
                        break;
                }
            }
            key[0] = 0;
            val[0] = 0;
        }
        free(line);
        fclose(fp);
    }

    if (*value_out == 0)
        return NULL;
    return value_out;
}

void file_changeKeyValue(const char *file_path, const char *key,
                         const char *replacement_line)
{
    FILE *fp, *cp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Write a sibling and rename it over the target (file_atomic_*). The
    // previous remove() + rename() left a moment with no file at all, so a
    // power cut while Tweaks edited retroarch.cfg could lose it. Any write
    // error sets the stream error flag, which makes the commit discard the
    // temp file and keep the original untouched.
    char temp_path[PATH_MAX];
    char final_path[PATH_MAX];

    fp = fopen(file_path, "r");
    if (fp == NULL)
        return;
    cp = file_atomic_begin(file_path, temp_path, sizeof(temp_path),
                           final_path, sizeof(final_path));
    if (cp == NULL) {
        fclose(fp);
        return;
    }

    int key_len = strlen(key);
    int line_idx = 0, line_len;
    bool found = false;

    printf_debug("Changing '%s' in '%s'\n", key, file_path);

    while ((read = getline(&line, &len, fp)) != -1) {
        for (line_idx = 0;
             line_idx < read &&
             strchr("\r\n\t {},", (unsigned char)line[line_idx]) != NULL;
             line_idx++)
            ;
        if (strncmp(line + line_idx, key, key_len) == 0) {
            fprintf(cp, "%s\n", replacement_line);
            printf_debug("Replace: %s\n", replacement_line);
            found = true;
            continue;
        }

        line_len = strlen(line);
        fprintf(cp, "%s", line);
        if (line_len > 0 && line[line_len - 1] != '\n')
            fputc('\n', cp);
    }

    // A read error means the copy is incomplete: never commit it.
    bool read_failed = ferror(fp) != 0;
    fclose(fp);
    free(line);

    if (read_failed) {
        fclose(cp);
        remove(temp_path);
        return;
    }

    if (!found) {
        printf_debug("Append: %s\n", replacement_line);
        fprintf(cp, "%s\n", replacement_line);
    }

    if (!file_atomic_commit(cp, temp_path, final_path)) {
        print_debug("file_changeKeyValue: write failed, original kept");
    }
}

bool file_path_relative_to(char *path_out, const char *dir_from, const char *file_to)
{
    path_out[0] = '\0';

    char abs_from[PATH_MAX];
    char abs_to[PATH_MAX];
    if (realpath(dir_from, abs_from) == NULL || realpath(file_to, abs_to) == NULL) {
        return false;
    }

    char *p1 = abs_from;
    char *p2 = abs_to;
    while (*p1 && (*p1 == *p2)) {
        ++p1, ++p2;
    }

    // Ensure we're at a directory boundary.
    // Back up to the last '/' if the match ended mid-component.
    if (*p1 != '\0' || (*p2 != '\0' && *p2 != '/')) {
        while (p1 > abs_from && *(p1 - 1) != '/') {
            --p1;
            --p2;
        }
    }

    if (*p2 == '/') {
        ++p2;
    }

    size_t offset = 0;
    if (*p1 != '\0') {
        int up_levels = 0;
        for (const char *cursor = p1; *cursor; cursor++) {
            if (*cursor == '/') {
                up_levels++;
            }
        }
        up_levels++;
        for (int i = 0; i < up_levels && offset + 3 < PATH_MAX; i++) {
            memcpy(path_out + offset, "../", 3);
            offset += 3;
        }
    }
    size_t p2_len = strlen(p2);
    if (offset + p2_len + 1 < PATH_MAX) {
        memcpy(path_out + offset, p2, p2_len);
        offset += p2_len;
    }
    path_out[offset] = '\0';

    return true;
}

FILE *file_open_ensure_path(const char *path, const char *mode)
{
    char *_path = strdup(path);
    if (_path != NULL) {
        mkdirs(dirname(_path));
        free(_path);
    }
    return fopen(path, mode);
}

bool file_findNewest(const char *dir_path, char *newest_file, size_t buffer_size)
{
    DIR *d;
    struct dirent *dir;
    struct stat64 file_stat;
    time_t newest_mtime = 0;

    d = opendir(dir_path);
    if (d == NULL) {
        return false;
    }

    bool found = false;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);

            if (stat64(full_path, &file_stat) == 0) {
                if (!found || file_stat.st_mtime > newest_mtime) {
                    newest_mtime = file_stat.st_mtime;
                    strncpy(newest_file, dir->d_name, buffer_size);
                    newest_file[buffer_size - 1] = '\0';
                    found = true;
                }
            }
        }
    }

    closedir(d);
    return found;
}
char *file_read_lineN(const char *filename, int n)
{
    // Real line numbers (getline), consistent with file_delete_line().
    int lineNumber = 1;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_debug("Error opening the file");
        return NULL;
    }

    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, file) != -1) {
        if (lineNumber == n) {
            fclose(file);
            return line; // caller frees
        }
        lineNumber++;
    }

    free(line);
    fclose(file);
    return NULL;
}

// Delete several lines (1-based numbers in the ORIGINAL file, ascending)
// in a single rewrite. getline() counts real lines of any length; the old
// fixed 1 KB fgets() buffer counted a longer line twice and could delete the
// wrong entry. The file is replaced atomically.
bool file_delete_lines(const char *fileName, const int *lines, int count)
{
    if (fileName == NULL || lines == NULL || count <= 0)
        return false;

    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        print_debug("Error opening file");
        return false;
    }

    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];
    FILE *tempFile = file_atomic_begin(fileName, tmp_path, sizeof(tmp_path),
                                       final_path, sizeof(final_path));
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating temporary file");
        return false;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int lineNumber = 1;
    int next = 0;

    while ((len = getline(&line, &cap, file)) != -1) {
        while (next < count && lines[next] < lineNumber)
            next++; // tolerate unsorted/duplicate input
        if (next < count && lines[next] == lineNumber)
            next++;
        else
            fwrite(line, 1, (size_t)len, tempFile);
        lineNumber++;
    }

    free(line);
    fclose(file);

    if (!file_atomic_commit(tempFile, tmp_path, final_path)) {
        print_debug("Error replacing file");
        return false;
    }

    printf_debug("%d line(s) deleted from %s\n", count, fileName);
    return true;
}

void file_delete_line(const char *fileName, int n)
{
    file_delete_lines(fileName, &n, 1);
}

void file_add_line_to_beginning(const char *filename, const char *lineToAdd)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_debug("Error opening the file");
        return;
    }

    // Atomic replace (was: remove + rename, which left a moment with no
    // file at all).
    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];
    FILE *tempFile = file_atomic_begin(filename, tmp_path, sizeof(tmp_path),
                                       final_path, sizeof(final_path));
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating the temporary file");
        return;
    }
    fputs(lineToAdd, tempFile);

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, file)) != -1)
        fwrite(line, 1, (size_t)len, tempFile);
    free(line);
    fclose(file);

    if (!file_atomic_commit(tempFile, tmp_path, final_path)) {
        print_debug("Error replacing the file");
        return;
    }
    print_debug("Line added to the beginning of the file successfully.\n");
}

// Move line n (1-based) to the top of the file in ONE atomic rewrite
// (was: add-to-top rewrite + delete-line rewrite). A moved last line
// without a trailing newline gets one, so it cannot merge with the next.
bool file_move_line_to_top(const char *fileName, int n)
{
    if (fileName == NULL || n < 1)
        return false;
    if (n == 1)
        return true;

    char *moved = file_read_lineN(fileName, n);
    if (moved == NULL)
        return false;

    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        free(moved);
        return false;
    }

    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];
    FILE *tempFile = file_atomic_begin(fileName, tmp_path, sizeof(tmp_path),
                                       final_path, sizeof(final_path));
    if (tempFile == NULL) {
        fclose(file);
        free(moved);
        return false;
    }

    size_t moved_len = strlen(moved);
    fwrite(moved, 1, moved_len, tempFile);
    if (moved_len == 0 || moved[moved_len - 1] != '\n')
        fputc('\n', tempFile);
    free(moved);

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int lineNumber = 1;
    while ((len = getline(&line, &cap, file)) != -1) {
        if (lineNumber != n)
            fwrite(line, 1, (size_t)len, tempFile);
        lineNumber++;
    }
    free(line);
    fclose(file);

    return file_atomic_commit(tempFile, tmp_path, final_path);
}

char *file_resolvePath(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    // Allocate memory for the resolved path
    char *resolvedPath = (char *)malloc(PATH_MAX);
    if (resolvedPath == NULL) {
        perror("Error allocating memory for resolved path");
        return NULL;
    }

    // Copy the input path to a temporary buffer
    char tempPath[PATH_MAX];
    strncpy(tempPath, path, PATH_MAX - 1);
    tempPath[PATH_MAX - 1] = '\0';

    // Initialize an array to hold the path components
    char *components[PATH_MAX];
    int componentCount = 0;

    // Split the path into components
    char *saveptr;
    char *token = strtok_r(tempPath, "/", &saveptr);
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            // Handle ".." by removing the last component if there is one
            if (componentCount > 0) {
                componentCount--;
            }
        }
        else if (strcmp(token, ".") != 0) {
            // Ignore "." and add other components to the array
            components[componentCount++] = token;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    // Reconstruct the resolved path
    size_t offset = 0;
    resolvedPath[0] = '\0';
    for (int i = 0; i < componentCount; i++) {
        size_t comp_len = strlen(components[i]);
        if (offset + 1 + comp_len >= PATH_MAX)
            break;
        resolvedPath[offset++] = '/';
        memcpy(resolvedPath + offset, components[i], comp_len);
        offset += comp_len;
    }
    resolvedPath[offset] = '\0';

    // Handle the case where the path is empty
    if (resolvedPath[0] == '\0') {
        resolvedPath[0] = '/';
        resolvedPath[1] = '\0';
    }

    return resolvedPath;
}

static int _remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;
    int ret = (typeflag == FTW_DP) ? rmdir(fpath) : remove(fpath);
    if (ret != 0 && errno != ENOENT)
        printf_debug("file_remove_recursive: failed to remove %s: %s\n", fpath, strerror(errno));
    return (ret != 0 && errno == ENOENT) ? 0 : ret;
}

int file_remove_recursive(const char *path)
{
    if (path == NULL)
        return -1;
    if (!exists(path))
        return 0;
    return nftw(path, _remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

FILE *file_atomic_begin(const char *path, char *tmp_path, size_t tmp_size,
                        char *final_path, size_t final_size)
{
    if (path == NULL || tmp_path == NULL || final_path == NULL ||
        tmp_size == 0 || final_size == 0)
        return NULL;

    // Resolve symlinks so that rename() replaces the real file, not the link.
    char resolved[PATH_MAX];
    const char *target = path;
    if (realpath(path, resolved) != NULL)
        target = resolved;

    int n = snprintf(final_path, final_size, "%s", target);
    if (n < 0 || (size_t)n >= final_size)
        return NULL;

    n = snprintf(tmp_path, tmp_size, "%s.tmp", final_path);
    if (n < 0 || (size_t)n >= tmp_size)
        return NULL;

    return fopen(tmp_path, "w");
}

bool file_atomic_commit(FILE *fp, const char *tmp_path, const char *final_path)
{
    if (fp == NULL)
        return false;

    bool ok = !ferror(fp);
    if (fflush(fp) != 0)
        ok = false;
    if (ok && fsync(fileno(fp)) != 0)
        ok = false;
    if (fclose(fp) != 0)
        ok = false;

    if (ok && rename(tmp_path, final_path) != 0)
        ok = false;

    if (!ok) {
        remove(tmp_path);
        return false;
    }

    // Best effort: persist the directory entry as well.
    char dir_path[PATH_MAX];
    int n = snprintf(dir_path, sizeof(dir_path), "%s", final_path);
    if (n > 0 && (size_t)n < sizeof(dir_path)) {
        char *slash = strrchr(dir_path, '/');
        if (slash != NULL && slash != dir_path) {
            *slash = '\0';
            int dfd = open(dir_path, O_RDONLY);
            if (dfd >= 0) {
                fsync(dfd);
                close(dfd);
            }
        }
    }

    return true;
}

bool file_atomic_write(const char *path, const char *data, size_t len)
{
    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];

    FILE *fp = file_atomic_begin(path, tmp_path, sizeof(tmp_path),
                                 final_path, sizeof(final_path));
    if (fp == NULL)
        return false;

    if (len > 0 && fwrite(data, 1, len, fp) != len) {
        fclose(fp);
        remove(tmp_path);
        return false;
    }

    return file_atomic_commit(fp, tmp_path, final_path);
}
