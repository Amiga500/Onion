#ifndef UTILS_SECURITY_H__
#define UTILS_SECURITY_H__

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/**
 * @file security.h
 * @brief Security utilities for embedded Linux (Miyoo Mini)
 * 
 * Provides functions to:
 * - Validate paths against directory traversal attacks
 * - Check if paths are within allowed directories
 * - Escape shell arguments to prevent command injection
 */

/**
 * @brief Check if a path contains directory traversal sequences
 * 
 * Detects dangerous patterns like:
 * - "../" or "..\" (parent directory traversal)
 * - Leading "/" when expecting relative path
 * - Null bytes embedded in path
 * 
 * @param path The path to validate
 * @return true if path is safe (no traversal detected)
 * @return false if path contains traversal sequences or is NULL
 */
static inline bool path_isSafe(const char *path)
{
    if (path == NULL) {
        return false;
    }
    
    // Check for null bytes embedded in the path (truncation attack)
    size_t len = strlen(path);
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\0') {
            return false;  // Should not happen with strlen, but defensive
        }
    }
    
    // Check for directory traversal patterns
    const char *p = path;
    while (*p) {
        // Check for ".." followed by "/" or "\" or end of string
        if (p[0] == '.' && p[1] == '.') {
            char next = p[2];
            if (next == '/' || next == '\\' || next == '\0') {
                return false;  // Directory traversal detected
            }
        }
        p++;
    }
    
    return true;
}

/**
 * @brief Check if a path starts with an allowed prefix
 * 
 * Use this to ensure paths stay within allowed directories like:
 * - /mnt/SDCARD/
 * - /tmp/
 * 
 * @param path The path to check
 * @param allowed_prefix The required prefix (e.g., "/mnt/SDCARD/")
 * @return true if path starts with allowed_prefix
 * @return false if path doesn't start with prefix or either is NULL
 */
static inline bool path_isWithinDirectory(const char *path, const char *allowed_prefix)
{
    if (path == NULL || allowed_prefix == NULL) {
        return false;
    }
    
    size_t prefix_len = strlen(allowed_prefix);
    if (prefix_len == 0) {
        return false;  // Empty prefix is not allowed
    }
    
    return strncmp(path, allowed_prefix, prefix_len) == 0;
}

/**
 * @brief Validate a path for use in shell commands
 * 
 * Combines multiple checks:
 * - No directory traversal
 * - No shell metacharacters that could enable injection
 * - Within allowed directory (if specified)
 * 
 * @param path The path to validate
 * @param allowed_prefix Optional prefix the path must start with (NULL to skip)
 * @return true if path is safe for shell use
 * @return false if path contains dangerous patterns
 */
static inline bool path_isValidForShell(const char *path, const char *allowed_prefix)
{
    if (path == NULL) {
        return false;
    }
    
    // Check for directory traversal
    if (!path_isSafe(path)) {
        return false;
    }
    
    // Check allowed prefix if specified
    if (allowed_prefix != NULL && !path_isWithinDirectory(path, allowed_prefix)) {
        return false;
    }
    
    // Check for shell metacharacters that could enable command injection
    // Note: Paths should be quoted with double quotes when passed to system()
    // These characters are dangerous even in double quotes or can break out
    const char *p = path;
    while (*p) {
        char c = *p;
        // Backticks enable command substitution even in double quotes
        // Dollar sign enables variable expansion
        // Backslash can escape quotes
        // Newlines can break command structure
        if (c == '`' || c == '$' || c == '\n' || c == '\r') {
            return false;
        }
        // Double quotes would break out of quoting
        if (c == '"') {
            return false;
        }
        p++;
    }
    
    return true;
}

/**
 * @brief Escape a string for safe use in double-quoted shell arguments
 * 
 * This function escapes characters that have special meaning within
 * double-quoted strings in POSIX shells: $, `, ", \, and newlines.
 * 
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string to escape
 * @return Number of characters written (excluding null), or -1 if buffer too small
 */
static inline int shell_escape(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || src == NULL || dest_size == 0) {
        return -1;
    }
    
    size_t dest_pos = 0;
    const char *p = src;
    
    while (*p) {
        char c = *p;
        bool needs_escape = false;
        
        // Characters that need escaping in double quotes
        if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
            needs_escape = true;
        }
        
        if (needs_escape) {
            // Need room for backslash + char + null
            if (dest_pos + 2 >= dest_size) {
                dest[dest_pos] = '\0';
                return -1;  // Buffer too small
            }
            dest[dest_pos++] = '\\';
        } else {
            // Need room for char + null
            if (dest_pos + 1 >= dest_size) {
                dest[dest_pos] = '\0';
                return -1;  // Buffer too small
            }
        }
        
        dest[dest_pos++] = c;
        p++;
    }
    
    dest[dest_pos] = '\0';
    return (int)dest_pos;
}

/**
 * @brief Validate a filename (no path separators or dangerous chars)
 * 
 * Use for validating user-provided filenames like theme names, rom names, etc.
 * Does NOT allow path separators, only pure filenames.
 * 
 * @param filename The filename to validate
 * @return true if filename is safe
 * @return false if filename contains path separators or dangerous chars
 */
static inline bool filename_isValid(const char *filename)
{
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }
    
    const char *p = filename;
    while (*p) {
        char c = *p;
        // No path separators
        if (c == '/' || c == '\\') {
            return false;
        }
        // No shell metacharacters
        if (c == '`' || c == '$' || c == '"' || c == '\'' || 
            c == ';' || c == '&' || c == '|' || c == '\n' || c == '\r') {
            return false;
        }
        // No null bytes (shouldn't be possible with strlen)
        if (c == '\0') {
            return false;
        }
        p++;
    }
    
    // Don't allow "." or ".." as filenames
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }
    
    return true;
}

#endif // UTILS_SECURITY_H__
