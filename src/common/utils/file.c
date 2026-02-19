#define _LARGEFILE64_SOURCE

#include "file.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

/* Maximum number of path components supported by file_resolvePath().
 * 64 is far more than any real path on this device requires and keeps
 * the stack frame small (256 bytes vs 16 KB for components[PATH_MAX]). */
#define MAX_PATH_COMPONENTS 64

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
    char *p = strrchr(filename, '/');
    return p ? p + 1 : (char *)filename;
}

/**
 * @brief Create directories in dir_path using `mkdir -p` command.
 *
 * @param dir_path The full directory path.
 * @return true If the path didn't exist (dirs were created).
 * @return false If the path exists (no dirs were created).
 */
bool mkdirs(const char *dir_path)
{
    if (!exists(dir_path)) {
        char dir_cmd[STR_MAX + 16];
        int cmd_len = snprintf(dir_cmd, sizeof(dir_cmd), "mkdir -p \"%s\"", dir_path);
        if (cmd_len < 0 || cmd_len >= (int)sizeof(dir_cmd)) {
            printf_debug("mkdirs: path too long, skipping mkdir for: %s\n", dir_path);
            return false;
        }
        system(dir_cmd);
        return true;
    }
    return false;
}

void file_readLastLine(const char *filename, char *out_str, size_t out_size)
{
    FILE *fd;
    long size;
    char buff[256];
    char *token = NULL;

    if (out_size == 0)
        return;

    if ((fd = fopen(filename, "rb")) != NULL) {
        // get file size
        fseek(fd, 0L, SEEK_END);
        size = ftell(fd);
        fseek(fd, 0L, SEEK_SET);

        int max_len = size < 255 ? size + 1 : 255;
        if (max_len <= 1) {
            fclose(fd);
            return;
        }

        // get the last line
        fseek(fd, -max_len, SEEK_END);
        if (fread(buff, max_len - 1, 1, fd) == 0) {
            fclose(fd);
            return;
        }

        // cleanup
        fclose(fd);
        buff[max_len - 1] = '\0';

        token = strtok(buff, "\n");
        while (token != NULL) {
            if (strlen(token) > 0)
                snprintf(out_str, out_size, "%s", token);
            token = strtok(NULL, "\n");
        }
    }
}

char *file_read(const char *path)
{
    FILE *f = NULL;
    char *buffer = NULL;
    long length = 0;

    if (!exists(path))
        return NULL;

    if ((f = fopen(path, "rb"))) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (length < 0) {
            fclose(f);
        }
        else {
            buffer = (char *)malloc((length + 1) * sizeof(char));
            if (buffer) {
                size_t bytes_read = fread(buffer, sizeof(char), length, f);
                buffer[bytes_read] = '\0';
                if (ferror(f)) {
                    free(buffer);
                    buffer = NULL;
                }
            }
            fclose(f);
        }
    }

    return buffer;
}

bool file_write(const char *path, const char *str, uint32_t len)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0)
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
    if (src_fd < 0) {
        printf_debug("file_copy: cannot open src '%s': %s\n", src_path, strerror(errno));
        return;
    }

    struct stat st;
    mode_t mode = 0644;
    if (fstat(src_fd, &st) == 0)
        mode = st.st_mode & 0777;

    int dst_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dst_fd < 0) {
        printf_debug("file_copy: cannot open dst '%s': %s\n", dest_path, strerror(errno));
        close(src_fd);
        return;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        if (write(dst_fd, buf, n) != n) {
            printf_debug("file_copy: write error to '%s': %s\n", dest_path, strerror(errno));
            break;
        }
    }

    close(src_fd);
    close(dst_fd);
}

char *file_removeExtension(const char *myStr)
{
    if (myStr == NULL)
        return NULL;
    size_t myStr_len = strlen(myStr);
    char *retStr = (char *)malloc(myStr_len + 1);
    char *lastExt;
    if (retStr == NULL)
        return NULL;
    memcpy(retStr, myStr, myStr_len + 1);
    if ((lastExt = strrchr(retStr, '.')) != NULL && *(lastExt + 1) != ' ' && *(lastExt + 2) != '\0')
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
    if (name_without_ext == NULL) {
        name_out[0] = '\0';
        return;
    }
    char *no_underscores = str_replace(name_without_ext, "_", " ");
    if (no_underscores == NULL) {
        str_removeParentheses(name_out, name_without_ext);
        free(name_without_ext);
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
            if (!(f = sscanf(line, search_str, key, val))) {
                if (fscanf(fp, "%*[^\n]\n") == EOF)
                    break;
                else
                    continue;
            }
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

    fp = fopen(file_path, "r");
    if (fp == NULL)
        return;

    char tempPath[STR_MAX + 16];
    char *dir = file_dirname(file_path);
    snprintf(tempPath, sizeof(tempPath), "%s/.tmp_ckv", dir ? dir : ".");
    free(dir);

    cp = fopen(tempPath, "w+");
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
        if (line_len > 0 && line[line_len - 1] != '\n')
            fprintf(cp, "%s\n", line);
        else
            fprintf(cp, "%s", line);
    }

    if (!found) {
        printf_debug("Append: %s\n", replacement_line);
        fprintf(cp, "%s\n", replacement_line);
    }

    fclose(fp);
    fclose(cp);
    if (line)
        free(line);

    remove(file_path);
    rename(tempPath, file_path);
}

bool file_path_relative_to(char *path_out, size_t dest_size, const char *dir_from, const char *file_to)
{
    path_out[0] = '\0';

    /* SD card paths are at most STR_MAX*2 (~510 B); realpath() returns NULL with
     * ENAMETOOLONG if the resolved path exceeds the buffer, which is already handled. */
    char abs_from[STR_MAX * 2];
    char abs_to[STR_MAX * 2];
    if (realpath(dir_from, abs_from) == NULL || realpath(file_to, abs_to) == NULL) {
        return false;
    }

    char *p1 = abs_from;
    char *p2 = abs_to;
    while (*p1 && (*p1 == *p2)) {
        ++p1, ++p2;
    }

    if (*p2 == '/') {
        ++p2;
    }

    if (strlen(p1) > 0) {
        int num_parens = str_count_char(p1, '/') + 1;
        for (int i = 0; i < num_parens; i++) {
            strncat(path_out, "../", dest_size - strlen(path_out) - 1);
        }
    }
    strncat(path_out, p2, dest_size - strlen(path_out) - 1);

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
            char full_path[STR_MAX * 2];
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
    /* JSON recents lines reach 1086 chars (JsonGameEntry_toJson max output + newline).
     * STR_MAX*4+130 = 1154 matches JSON_RECENTS_LINE_MAX and avoids fgets truncation. */
    char line[STR_MAX * 4 + 130];
    int lineNumber = 1;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_debug("Error opening the file");
        return NULL;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (lineNumber == n) {
            fclose(file);
            size_t line_len = strlen(line);
            char *lineN = malloc(line_len + 1);
            if (lineN == NULL) {
                print_debug("Memory allocation error");
                return NULL;
            }
            memcpy(lineN, line, line_len + 1);
            return lineN;
        }
        lineNumber++;
    }

    fclose(file);
    return NULL;
}

void file_delete_line(const char *fileName, int n)
{

    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        print_debug("Error opening file");
        return;
    }

    char tempPath[STR_MAX + 16];
    char *dir = file_dirname(fileName);
    snprintf(tempPath, sizeof(tempPath), "%s/.tmp_dl", dir ? dir : ".");
    free(dir);

    FILE *tempFile = fopen(tempPath, "w");
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating temporary file");
        return;
    }

    /* JSON recents lines reach 1086 chars (JsonGameEntry_toJson max output + newline).
     * STR_MAX*4+130 = 1154 matches JSON_RECENTS_LINE_MAX and avoids fgets truncation. */
    char line[STR_MAX * 4 + 130];
    int lineNumber = 1;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (lineNumber != n) {
            fputs(line, tempFile);
        }
        lineNumber++;
    }

    fclose(file);
    fclose(tempFile);

    if (remove(fileName) != 0) {
        print_debug("Error deleting original file");
        return;
    }

    if (rename(tempPath, fileName) != 0) {
        print_debug("Error renaming temporary file");
        return;
    }

    printf_debug("Line %d has been successfully deleted.\n", n);
}

void file_add_line_to_beginning(const char *filename, const char *lineToAdd)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_debug("Error opening the file");
        return;
    }
    char tempPath[STR_MAX];
    char *path = file_dirname(filename);
    snprintf(tempPath, sizeof(tempPath), "%s/.tmp_alb", path);
    free(path);

    FILE *tempFile = fopen(tempPath, "w");
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating the temporary file");
        return;
    }
    fputs(lineToAdd, tempFile);

    /* JSON recents lines reach 1086 chars (JsonGameEntry_toJson max output + newline).
     * STR_MAX*4+130 = 1154 matches JSON_RECENTS_LINE_MAX and avoids fgets truncation. */
    char line[STR_MAX * 4 + 130];
    while (fgets(line, sizeof(line), file) != NULL) {
        fputs(line, tempFile);
    }
    fclose(file);
    fclose(tempFile);
    if (remove(filename) != 0) {
        print_debug("Error removing the original file");
        return;
    }
    if (rename(tempPath, filename) != 0) {
        print_debug("Error renaming the temporary file");
        return;
    }
    print_debug("Line added to the beginning of the file successfully.\n");
}

char *file_resolvePath(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    // Allocate memory for the resolved path
    char *resolvedPath = (char *)malloc(STR_MAX * 2);
    if (resolvedPath == NULL) {
        perror("Error allocating memory for resolved path");
        return NULL;
    }

    // Copy the input path to a temporary buffer
    char tempPath[STR_MAX * 2];
    strncpy(tempPath, path, sizeof(tempPath) - 1);
    tempPath[sizeof(tempPath) - 1] = '\0';

    /* PATH_MAX/2 is the theoretical max component count, but MAX_PATH_COMPONENTS is
     * more than enough for any real path on this device and avoids a 16 KB stack frame. */
    char *components[MAX_PATH_COMPONENTS];
    int componentCount = 0;

    // Split the path into components
    char *token = strtok(tempPath, "/");
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            // Handle ".." by removing the last component if there is one
            if (componentCount > 0) {
                componentCount--;
            }
        }
        else if (strcmp(token, ".") != 0) {
            // Ignore "." and add other components to the array
            if (componentCount < MAX_PATH_COMPONENTS) {
                components[componentCount++] = token;
            }
            else {
                print_debug("file_resolvePath: path has too many components, truncating");
            }
        }
        token = strtok(NULL, "/");
    }

    // Reconstruct the resolved path
    resolvedPath[0] = '\0';
    for (int i = 0; i < componentCount; i++) {
        strncat(resolvedPath, "/", STR_MAX * 2 - strlen(resolvedPath) - 1);
        strncat(resolvedPath, components[i], STR_MAX * 2 - strlen(resolvedPath) - 1);
    }

    // Handle the case where the path is empty
    if (resolvedPath[0] == '\0') {
        strncpy(resolvedPath, "/", STR_MAX * 2 - 1);
        resolvedPath[STR_MAX * 2 - 1] = '\0';
    }

    return resolvedPath;
}
