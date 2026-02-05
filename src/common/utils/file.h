#ifndef UTILS_FILE_H__
#define UTILS_FILE_H__

/**
 * @file file.h
 * @brief File system utilities for Onion OS (Miyoo Mini)
 * 
 * Provides common file operations optimized for embedded Linux:
 * - File/directory existence and type checking
 * - File reading and writing with proper sync
 * - Path manipulation and resolution
 * - Directory traversal utilities
 * 
 * Many functions use stat64 for large file support.
 */

#include <dirent.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifndef DT_UNKNOWN
/** d_type value when the type is not known. */
#define DT_UNKNOWN 0
/** d_type value for a FIFO. */
#define DT_FIFO 1
/** d_type value for a character device. */
#define DT_CHR 2
/** d_type value for a directory. */
#define DT_DIR 4
/** d_type value for a block device. */
#define DT_BLK 6
/** d_type value for a regular file. */
#define DT_REG 8
/** d_type value for a symbolic link. */
#define DT_LNK 10
/** d_type value for a socket. */
#define DT_SOCK 12
#define DT_WHT 14
#endif

/**
 * @brief Read a value from a file using fscanf
 * 
 * Opens file for reading, scans value using format string, closes file.
 * Usage: file_get(fp, "/path/to/file", "%d", &int_value);
 * 
 * @param fp FILE pointer variable (will be used internally)
 * @param path Path to file
 * @param format scanf format string
 * @param dest Pointer to destination variable
 */
#define file_get(fp, path, format, dest) \
    {                                    \
        if ((fp = fopen(path, "r"))) {   \
            fscanf(fp, format, dest);    \
            fclose(fp);                  \
        }                                \
    }

/**
 * @brief Write a value to a file using fprintf
 * 
 * Opens file for writing (creates/truncates), writes value, closes file.
 * Note: Does NOT sync to disk - use file_put_sync for critical data.
 * 
 * @param fp FILE pointer variable (will be used internally)
 * @param path Path to file
 * @param format printf format string
 * @param value Value to write
 */
#define file_put(fp, path, format, value) \
    {                                     \
        if ((fp = fopen(path, "w+"))) {   \
            fprintf(fp, format, value);   \
            fclose(fp);                   \
        }                                 \
    }

/**
 * @brief Write a value to a file with fsync for durability
 * 
 * Same as file_put but calls fflush() and fsync() before close.
 * Use this for critical data that must survive power loss.
 * 
 * @param fp FILE pointer variable (will be used internally)
 * @param path Path to file
 * @param format printf format string
 * @param value Value to write
 */
#define file_put_sync(fp, path, format, value) \
    {                                          \
        if ((fp = fopen(path, "w+"))) {        \
            fprintf(fp, format, value);        \
            fflush(fp);                        \
            fsync(fileno(fp));                 \
            fclose(fp);                        \
        }                                      \
    }

#ifndef PATH_MAX
/** Maximum path length */
#define PATH_MAX 4096
#endif

/**
 * @brief Fast macro to check if a filename is "." or ".."
 * Uses direct character comparison instead of strcmp() for O(1) performance.
 * This is a common operation when iterating directory entries.
 */
#define IS_DOT_OR_DOTDOT(name) \
    ((name)[0] == '.' && ((name)[1] == '\0' || ((name)[1] == '.' && (name)[2] == '\0')))

/** Format string for reading/writing integers */
#define CONTENT_INT "%d"
/** Format string for reading a string until newline */
#define CONTENT_STR "%[^\n]"

/**
 * @brief Check if a file or directory exists
 * @param file_path Path to check
 * @return true if path exists
 */
bool exists(const char *file_path);

/**
 * @brief Check if path is a regular file
 * @param file_path Path to check
 * @return true if path exists and is a regular file
 */
bool is_file(const char *file_path);

/**
 * @brief Check if path is a directory
 * @param file_path Path to check
 * @return true if path exists and is a directory
 */
bool is_dir(const char *file_path);

/**
 * @brief Check if file has been modified since last check
 * @param path Path to file
 * @param old_mtime Pointer to stored mtime (updated if modified)
 * @return true if file was modified
 */
bool file_isModified(const char *path, time_t *old_mtime);

/**
 * @brief Check if a file is locked (can't be opened)
 * @param path Path to file
 * @return true if file is locked or inaccessible
 */
bool file_isLocked(const char *path);

/**
 * @brief returns the filename component of a path
 * 
 * This is a copy of the GNU `basename` version and
 * retains all the quirks that come along with it
 * 
 * See 'Versions' here:
 * https://man7.org/linux/man-pages/man3/basename.3.html
 * 
 * Copied from: 
 * https://sourceware.org/git/?p=glibc.git;a=blob;f=string/basename.c;h=d5b5d4763dd3fa307497cc99788b0bb24c95bcf1;hb=refs/heads/master#l22
 * 
 * @param filename The full file path.
 * @return * char* 
 */
const char *file_basename(const char *filename);

/**
 * @brief Create directories in dir_path using `mkdir -p` command.
 *
 * @param dir_path The full directory path.
 * @return true If the path didn't exist (dirs were created).
 * @return false If the path exists (no dirs were created).
 */
bool mkdirs(const char *dir_path);

/**
 * @brief Read the last line of a file
 * @param filename Path to file
 * @param out_str Buffer to store the last line
 */
void file_readLastLine(const char *filename, char *out_str);

/**
 * @brief Read entire file contents into memory
 * @param path Path to file
 * @return Newly allocated string with file contents (caller must free)
 */
char *file_read(const char *path) __attribute__((malloc));

/**
 * @brief Write a string to a file
 * @param path Path to file
 * @param str String to write
 * @param len Length of string
 * @return true on success
 */
bool file_write(const char *path, const char *str, uint32_t len);

/**
 * @brief Copy a file from source to destination
 * @param src_path Source file path
 * @param dest_path Destination file path
 */
void file_copy(const char *src_path, const char *dest_path);

/**
 * @brief Remove file extension from a path
 * @param myStr File path or name
 * @return Newly allocated string without extension (caller must free)
 */
char *file_removeExtension(const char *myStr) __attribute__((malloc));

/**
 * @brief Get the directory portion of a path
 * @param absolutePath Full file path
 * @return Newly allocated string with directory path (caller must free)
 */
char *file_dirname(const char *absolutePath) __attribute__((malloc));

/**
 * @brief Clean a filename for display (remove extension and special chars)
 * @param name_out Buffer for cleaned name
 * @param file_name Input filename
 */
void file_cleanName(char *name_out, const char *file_name);

/**
 * @brief Get the file extension from a path
 * @param filename File path or name
 * @return Pointer to extension (within the original string) or empty string
 */
const char *file_getExtension(const char *filename);

/**
 * @brief Parse a key-value pair from a config file
 * @param file_path Path to config file
 * @param key_in Key to search for
 * @param value_out Buffer to store the value
 * @param divider Character separating key and value (e.g., '=')
 * @param select_index For multi-value keys, which value to select
 * @return Pointer to value_out on success, NULL if key not found
 */
char *file_parseKeyValue(const char *file_path, const char *key_in,
                         char *value_out, char divider, int select_index);

/**
 * @brief Replace a key's value in a config file
 * @param file_path Path to config file
 * @param key Key to find
 * @param replacement_line New line to replace the key's line
 */
void file_changeKeyValue(const char *file_path, const char *key,
                         const char *replacement_line);

/**
 * @brief Calculate relative path from one path to another
 * @param path_out Buffer for resulting relative path
 * @param path_from Source directory
 * @param path_to Destination path
 * @return true on success
 */
bool file_path_relative_to(char *path_out, const char *path_from, const char *path_to);

/**
 * @brief Find the most recently modified file in a directory
 * @param dir_path Directory to search
 * @param newest_file Buffer to store the filename
 * @param buffer_size Size of the buffer
 * @return true if a file was found
 */
bool file_findNewest(const char *dir_path, char *newest_file, size_t buffer_size);

/**
 * @brief Open a file, creating parent directories if needed
 * @param path File path
 * @param mode fopen mode string
 * @return FILE pointer or NULL on failure
 */
FILE *file_open_ensure_path(const char *path, const char *mode);

/**
 * @brief Read the Nth line from a file
 * @param filename Path to file
 * @param n Line number (1-based)
 * @return Newly allocated string with line contents (caller must free)
 */
char *file_read_lineN(const char *filename, int n) __attribute__((malloc));

/**
 * @brief Delete the Nth line from a file
 * @param fileName Path to file
 * @param n Line number to delete (1-based)
 */
void file_delete_line(const char *fileName, int n);

/**
 * @brief Add a line to the beginning of a file
 * @param filename Path to file
 * @param lineToAdd Line to prepend
 */
void file_add_line_to_beginning(const char *filename, const char *lineToAdd);

/**
 * @brief Resolve a path to an absolute path
 * 
 * Resolves ".." path components to create a clean absolute path.
 * Example: "/mnt/SDCARD/Emu/GBA/../../Roms/GBA/game.gba" 
 *       -> "/mnt/SDCARD/Roms/GBA/game.gba"
 *
 * @param path Path to resolve
 * @return Newly allocated string with resolved path (caller must free)
 */
char *file_resolvePath(const char *path) __attribute__((malloc));

#endif // UTILS_FILE_H__
