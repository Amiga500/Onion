#ifndef UTILS_STR_H__
#define UTILS_STR_H__

/**
 * @file str.h
 * @brief String manipulation utilities for Onion OS
 * 
 * Provides common string operations optimized for embedded use:
 * - String splitting and replacement
 * - Whitespace trimming
 * - Number extraction
 * - Character encoding detection (CJK support)
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Maximum string length for most path/string operations */
#define STR_MAX 256

/**
 * @brief Concatenate two strings into a buffer
 * 
 * Uses memcpy for efficiency when strings fit in buffer,
 * falls back to snprintf for safety when they don't.
 * 
 * @param ptr Destination buffer (must be at least STR_MAX bytes)
 * @param str1 First string
 * @param str2 Second string to append
 */
#define concat(ptr, str1, str2)                      \
    {                                                \
        size_t len1 = strlen(str1);                  \
        size_t len2 = strlen(str2);                  \
        if (len1 + len2 <= STR_MAX - 1) {            \
            memcpy(ptr, str1, len1);                 \
            memcpy(ptr + len1, str2, len2 + 1);      \
        }                                            \
        else {                                       \
            snprintf(ptr, STR_MAX, "%s%s", str1, str2); \
        }                                            \
    }

/**
 * @brief Extract the last number found in a string
 * 
 * Scans the string and returns the value of the last numeric sequence.
 * Useful for extracting version numbers, indices, etc.
 * 
 * @param str Input string to scan
 * @param out_val Pointer to store the extracted number
 * @return true if a number was found
 * @return false if no number was found
 */
bool str_getLastNumber(const char *str, long *out_val);

/**
 * @brief Split a string at the first occurrence of a delimiter
 * 
 * Modifies the original string by inserting a null terminator.
 * Returns pointer to the substring after the delimiter.
 * 
 * @param str String to split (will be modified)
 * @param delim Delimiter to search for
 * @return Pointer to substring after delimiter, or NULL if not found
 */
char *str_split(char *str, const char *delim);

/**
 * @brief Replace all occurrences of a substring
 * 
 * Allocates and returns a new string with all instances of 'rep'
 * replaced by 'with'. Caller must free the returned string.
 * 
 * @param orig Original string
 * @param rep Substring to replace
 * @param with Replacement string (NULL treated as empty string)
 * @return Newly allocated string with replacements, or NULL on error
 */
char *str_replace(char *orig, const char *rep, const char *with);

/**
 * @brief Trim leading and/or trailing whitespace from a string
 * 
 * Copies the trimmed result to the output buffer.
 * 
 * @param out Output buffer for trimmed string
 * @param len Size of output buffer
 * @param str Input string to trim
 * @param first If true, only trim leading whitespace; if false, trim both ends
 * @return Number of characters written (excluding null terminator)
 */
size_t str_trim(char *out, size_t len, const char *str, bool first);

/**
 * @brief Check if a string ends with a given suffix
 * 
 * @param str String to check
 * @param suffix Suffix to look for
 * @return 1 if str ends with suffix, 0 otherwise
 */
int str_endsWith(const char *str, const char *suffix);

/**
 * @brief Remove parentheses and their contents from a string
 * 
 * Useful for cleaning up ROM names like "Game (USA)" -> "Game"
 * 
 * @param str_out Output buffer for cleaned string
 * @param str_in Input string
 */
void str_removeParentheses(char *str_out, const char *str_in);

/**
 * @brief Format a time value as a human-readable string
 * 
 * Converts seconds to "HH:MM:SS" or similar format.
 * 
 * @param dest_str Output buffer for formatted time
 * @param nTime Time value in seconds
 */
void str_serializeTime(char *dest_str, int nTime);

/**
 * @brief Count occurrences of a character in a string
 * 
 * @param str String to search
 * @param ch Character to count
 * @return Number of occurrences
 */
int str_count_char(const char *str, char ch);

/**
 * @brief Check if a string contains CJK (Chinese/Japanese/Korean) characters
 * 
 * Detects UTF-8 encoded CJK characters for font selection purposes.
 * 
 * @param str String to check
 * @return true if CJK characters are present
 * @return false if only ASCII/Latin characters
 */
bool includeCJK(const char *str);

#endif // UTILS_STR_H__
