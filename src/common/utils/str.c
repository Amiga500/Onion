#include "./str.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool str_getLastNumber(char *str, long *out_val)
{
    char *p = str;
    long val = -1;

    while (*p) {
        if (isdigit(*p))
            val = strtol(p, &p, 10);
        else
            p++;
    }

    if (val != -1)
        *out_val = val;

    return val != -1;
}

char *str_split(char *str, const char *delim)
{
    char *p = strstr(str, delim);
    if (p == NULL)
        return NULL;          // delimiter not found
    *p = '\0';                // terminate string after head
    size_t delim_len = strlen(delim);  // Cache length
    return p + delim_len;     // return tail substring
}

char *str_replace(char *orig, char *rep, char *with)
{
    char *ins;     // the next insert point
    char *tmp;     // varies
    int len_rep;   // length of rep (the string to remove)
    int len_with;  // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;     // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count)
        ins = tmp + len_rep;

    // Cache orig length for allocation
    size_t len_orig = strlen(orig);
    char *result = (char *)malloc(len_orig + (len_with - len_rep) * count + 1);
    tmp = result;

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        // Use memcpy instead of strncpy (faster, no null padding)
        memcpy(tmp, orig, len_front);
        tmp += len_front;
        memcpy(tmp, with, len_with);
        tmp += len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

// Fast lookup table for whitespace characters used in trim
static inline bool is_trim_char(unsigned char c)
{
    return (c == '\r' || c == '\n' || c == '\t' || c == ' ' || 
            c == '{' || c == '}' || c == ',');
}

static inline bool is_string_quote_char(unsigned char c)
{
    return (c == '\r' || c == '\n' || c == '"');
}

// Stores the trimmed input string into the given output buffer, which must be
// large enough to store the result.  If it is too small, the output is
// truncated.
size_t str_trim(char *out, size_t len, const char *str, bool first)
{
    if (len == 0)
        return 0;

    const char *end;
    size_t out_size;
    bool is_string = false;

    // Trim leading space - use lookup instead of strchr
    while (is_trim_char((unsigned char)*str))
        str++;

    end = str + 1;

    if ((unsigned char)*str == '"') {
        is_string = true;
        str++;
        while (!is_string_quote_char((unsigned char)*end))
            end++;
    }

    if (*str == 0) // All spaces?
    {
        *out = 0;
        return 1;
    }

    // Trim trailing space
    if (first) {
        while (!is_trim_char((unsigned char)*end))
            end++;
    }
    else {
        end = str + strlen(str) - 1;
        while (end > str && is_trim_char((unsigned char)*end))
            end--;
        end++;
    }

    if (is_string && (unsigned char)*(end - 1) == '"')
        end--;

    // Set output size to minimum of trimmed string length and buffer size minus 1
    out_size = (end - str) < len - 1 ? (end - str) : len - 1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

int str_endsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int str_startsWith(const char *str, const char *prefix)
{
    if (!str || !prefix)
        return 0;
    // Direct comparison without strlen for prefix - more efficient
    while (*prefix) {
        if (*str++ != *prefix++)
            return 0;
    }
    return 1;
}

void str_removeParentheses(char *str_out, const char *str_in)
{
    int len = strlen(str_in);
    int c = 0;
    bool inside = false;
    char end_char = '\0';

    // Direct write to output, avoiding temp buffer
    for (int i = 0; i < len && c < STR_MAX - 1; i++) {
        if (!inside && (str_in[i] == '(' || str_in[i] == '[')) {
            end_char = str_in[i] == '(' ? ')' : ']';
            inside = true;
            continue;
        }
        else if (inside) {
            if (str_in[i] == end_char)
                inside = false;
            continue;
        }
        str_out[c++] = str_in[i];
    }

    str_out[c] = '\0';

    // Trim in place if needed
    if (c > 0) {
        // Simple inline trim - remove trailing whitespace
        while (c > 0 && (str_out[c-1] == ' ' || str_out[c-1] == '\t' || 
                         str_out[c-1] == '\r' || str_out[c-1] == '\n')) {
            c--;
        }
        str_out[c] = '\0';
    }
}

// Helper function for fast integer to string conversion
// Note: Only handles non-negative integers, suitable for time values
static int _int_to_str(char *buf, int value)
{
    char temp[16];
    int i = 0, len = 0;
    
    // Handle negative values (shouldn't happen with time, but be safe)
    if (value < 0) {
        value = 0;
    }
    
    if (value == 0) {
        buf[0] = '0';
        return 1;
    }
    
    // Convert digits in reverse order
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Reverse the string
    len = i;
    for (int j = 0; j < i; j++) {
        buf[j] = temp[i - j - 1];
    }
    
    return len;
}

void str_serializeTime(char *dest_str, int nTime)
{
    char *p = dest_str;
    
    if (nTime >= 60) {
        int h = nTime / 3600;
        int m = (nTime - 3600 * h) / 60;
        if (h > 0) {
            // Format: "Xh Ym"
            p += _int_to_str(p, h);
            *p++ = 'h';
            *p++ = ' ';
            p += _int_to_str(p, m);
            *p++ = 'm';
            *p = '\0';
        }
        else {
            // Format: "Xm Ys"
            p += _int_to_str(p, m);
            *p++ = 'm';
            *p++ = ' ';
            p += _int_to_str(p, nTime - 60 * m);
            *p++ = 's';
            *p = '\0';
        }
    }
    else {
        // Format: "Xs"
        p += _int_to_str(p, nTime);
        *p++ = 's';
        *p = '\0';
    }
}

int str_count_char(const char *str, char ch)
{
    int count = 0;
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (str[i] == ch) {
            count++;
        }
    }
    return count;
}

bool includeCJK(char *str)
{
    // Fast path - check for ASCII-only string
    unsigned char c;
    while ((c = (unsigned char)*str) != '\0') {
        // Quick check: if high bit set, might be CJK
        if (c >= 0x80) {
            return true;  // Early exit on first non-ASCII
        }
        str++;
    }
    return false;
}