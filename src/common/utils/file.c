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
    // Check existence using stat64 which is already cached by the kernel
    if (!exists(dir_path)) {
        char dir_cmd[512];
        // Use size-limited snprintf for safety
        int len = snprintf(dir_cmd, sizeof(dir_cmd), "mkdir -p \"%s\"", dir_path);
        if (len > 0 && len < (int)sizeof(dir_cmd)) {
            system(dir_cmd);
        }
        return true;
    }
    return false;
}

void file_readLastLine(const char *filename, char *out_str)
{
    FILE *fd;
    long size;
    char buff[256];

    out_str[0] = '\0';  // Initialize output

    if ((fd = fopen(filename, "rb")) != NULL) {
        // get file size
        fseek(fd, 0L, SEEK_END);
        size = ftell(fd);

        if (size <= 0) {
            fclose(fd);
            return;
        }

        // Safely calculate max_len considering long-to-int conversion
        int max_len = (size < 255L) ? (int)(size + 1) : 255;
        if (max_len <= 1) {
            fclose(fd);
            return;
        }

        // get the last line
        fseek(fd, -max_len, SEEK_END);
        size_t bytes_read = fread(buff, 1, max_len - 1, fd);
        fclose(fd);

        if (bytes_read == 0)
            return;

        buff[bytes_read] = '\0';

        // Find last non-empty line by scanning backwards
        char *last_line_start = NULL;
        char *current = buff + bytes_read - 1;

        // Skip trailing newlines
        while (current >= buff && (*current == '\n' || *current == '\r'))
            current--;

        if (current < buff)
            return;

        // Mark end of last line
        char *line_end = current + 1;

        // Find start of last line
        while (current >= buff && *current != '\n')
            current--;

        last_line_start = current + 1;

        // Copy the last line
        size_t line_len = line_end - last_line_start;
        if (line_len > 254)
            line_len = 254;
        memcpy(out_str, last_line_start, line_len);
        out_str[line_len] = '\0';
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
        buffer = (char *)malloc((length + 1) * sizeof(char));
        if (buffer) {
            fread(buffer, sizeof(char), length, f);
            buffer[length] = '\0';
        }
        fclose(f);
    }

    return buffer;
}

bool file_write(const char *path, const char *str, uint32_t len)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1)
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
    char system_cmd[4128];
    snprintf(system_cmd, sizeof(system_cmd), "cp -f \"%s\" \"%s\"", src_path, dest_path);
    system(system_cmd);
}

char *file_removeExtension(const char *myStr)
{
    if (myStr == NULL)
        return NULL;
    char *retStr = (char *)malloc(strlen(myStr) + 1);
    char *lastExt;
    if (retStr == NULL)
        return NULL;
    strcpy(retStr, myStr);
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
    char *no_underscores = str_replace(name_without_ext, "_", " ");
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
    snprintf(search_str, STR_MAX, "%%255[^%c]%c%%255[^\n]\n", divider, divider);
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
    cp = fopen("temp", "w+");
    if (fp == NULL)
        exit(EXIT_FAILURE);

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
        if (line[line_len - 1] != '\n') {
            line[line_len - 1] = '\n';
            line[line_len] = '\0';
        }
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
    rename("temp", file_path);
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

    if (*p2 == '/') {
        ++p2;
    }

    char *pathPtr = path_out;
    if (strlen(p1) > 0) {
        int num_parens = str_count_char(p1, '/') + 1;
        for (int i = 0; i < num_parens; i++) {
            memcpy(pathPtr, "../", 3);
            pathPtr += 3;
        }
    }
    strcpy(pathPtr, p2);

    return true;
}

FILE *file_open_ensure_path(const char *path, const char *mode)
{
    char *_path = strdup(path);
    mkdirs(dirname(_path));
    free(_path);
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
    char line[STR_MAX * 4];
    int lineNumber = 1;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_debug("Error opening the file");
        return NULL;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (lineNumber == n) {
            fclose(file);
            char *lineN = malloc(strlen(line) + 1);
            if (lineN == NULL) {
                print_debug("Memory allocation error");
                return NULL;
            }
            strcpy(lineN, line);
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

    FILE *tempFile = fopen("temp.txt", "w");
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating temporary file");
        return;
    }

    char line[STR_MAX * 4];
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

    if (rename("temp.txt", fileName) != 0) {
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
    snprintf(tempPath, STR_MAX, "%s/temp.txt", path);
    free(path);

    FILE *tempFile = fopen(tempPath, "w");
    if (tempFile == NULL) {
        fclose(file);
        print_debug("Error creating the temporary file");
        return;
    }
    fputs(lineToAdd, tempFile);

    char line[STR_MAX * 4];
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
    char *resolvedPath = (char *)malloc(PATH_MAX);
    if (resolvedPath == NULL) {
        perror("Error allocating memory for resolved path");
        return NULL;
    }

    // Use a single buffer for processing - avoid allocating components array
    char tempPath[PATH_MAX];
    size_t pathLen = strlen(path);
    if (pathLen >= PATH_MAX) {
        pathLen = PATH_MAX - 1;
    }
    memcpy(tempPath, path, pathLen);
    tempPath[pathLen] = '\0';

    // Process path in-place using two pointers
    char *read = tempPath;
    char *write = resolvedPath;

    // Handle leading slash for absolute paths
    if (*read == '/') {
        *write++ = '/';
        read++;
    }

    while (*read) {
        // Skip consecutive slashes
        while (*read == '/')
            read++;

        if (*read == '\0')
            break;

        // Find end of current component
        char *compStart = read;
        while (*read && *read != '/')
            read++;

        size_t compLen = read - compStart;

        if (compLen == 1 && compStart[0] == '.') {
            // Skip "." component
            continue;
        }
        else if (compLen == 2 && compStart[0] == '.' && compStart[1] == '.') {
            // Handle ".." - go back to previous component
            if (write > resolvedPath + 1) {
                write--;  // Remove trailing slash
                while (write > resolvedPath && *(write - 1) != '/')
                    write--;
            }
        }
        else {
            // Add component
            memcpy(write, compStart, compLen);
            write += compLen;
            *write++ = '/';
        }
    }

    // Remove trailing slash (except for root)
    if (write > resolvedPath + 1 && *(write - 1) == '/') {
        write--;
    }

    // Handle empty path case
    if (write == resolvedPath) {
        *write++ = '/';
    }

    *write = '\0';

    return resolvedPath;
}
