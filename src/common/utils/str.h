#ifndef UTILS_STR_H__
#define UTILS_STR_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_MAX 256
#define concat(ptr, str1, str2) \
    {                           \
        strcpy(ptr, str1);      \
        strcat(ptr, str2);      \
    }

bool str_getLastNumber(char *str, long *out_val);
char *str_split(char *str, const char *delim);

/**
 * @brief Replace all occurrences of a substring with another
 *
 * @param orig Original string (restrict: no aliasing)
 * @param rep Substring to find and replace (restrict: no aliasing)
 * @param with Replacement string (restrict: no aliasing)
 * @return Newly allocated string with replacements (caller must free)
 */
char *str_replace(char * restrict orig, char * restrict rep, char * restrict with)
    __attribute__((malloc));

// Stores the trimmed input string into the given output buffer, which must be
// large enough to store the result.  If it is too small, the output is
// truncated.
size_t str_trim(char *out, size_t len, const char *str, bool first);

int str_endsWith(const char *str, const char *suffix);

void str_removeParentheses(char *str_out, const char *str_in);
void str_serializeTime(char *dest_str, int nTime);

int str_count_char(const char *str, char ch);
bool includeCJK(char *str);

#endif // UTILS_STR_H__
