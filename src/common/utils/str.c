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
    return p + strlen(delim); // return tail substring
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

    size_t len_orig = strlen(orig);
    // Use size_t arithmetic to avoid signed integer overflow.
    // new_len = len_orig - len_rep*count + len_with*count + 1
    size_t remove_total = (size_t)len_rep * (size_t)count;
    size_t insert_total = (size_t)len_with * (size_t)count;
    // Guard against multiplication overflow
    if (count > 0 && insert_total / (size_t)count != (size_t)len_with)
        return NULL;
    // Guard against final addition overflow
    size_t base_len = len_orig - remove_total; // safe: we found 'count' non-overlapping matches
    if (base_len + insert_total < base_len)
        return NULL;
    size_t new_len = base_len + insert_total + 1;
    if (new_len == 0)
        return NULL;
    char *result = (char *)malloc(new_len);
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
        tmp = strncpy(tmp, orig, len_front) + len_front;
        memcpy(tmp, with, len_with);
        tmp += len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    memcpy(tmp, orig, strlen(orig) + 1);
    return result;
}

// Stores the trimmed input string into the given output buffer, which must be
// large enough to store the result.  If it is too small, the output is
// truncated.
// Returns the number of bytes written (excluding the null terminator);
// an all-whitespace or empty input yields 0 with *out set to '\0'.
size_t str_trim(char *out, size_t len, const char *str, bool first)
{
    if (len == 0)
        return 0;

    const char *end;
    size_t out_size;
    bool is_string = false;

    // Trim leading space
    while (*str != '\0' && strchr("\r\n\t {},", (unsigned char)*str) != NULL)
        str++;

    end = str + 1;

    if ((unsigned char)*str == '"') {
        is_string = true;
        str++;
        while (strchr("\r\n\"", (unsigned char)*end) == NULL)
            end++;
    }

    if (*str == 0) // All spaces?
    {
        *out = 0;
        return 0;
    }

    // Trim trailing space
    if (first)
        while (strchr("\r\n\t {},", (unsigned char)*end) == NULL)
            end++;
    else {
        end = str + strlen(str) - 1;
        while (end > str && strchr("\r\n\t {},", (unsigned char)*end) != NULL)
            end--;
        end++;
    }

    if (is_string && (unsigned char)*(end - 1) == '"')
        end--;

    // Set output size to minimum of trimmed string length and buffer size minus
    // 1
    size_t trimmed_len = (size_t)(end - str);
    out_size = trimmed_len < len - 1 ? trimmed_len : len - 1;

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

void str_removeParentheses(char *str_out, const char *str_in)
{
    char temp[STR_MAX];
    int len = strlen(str_in);
    int c = 0;
    bool inside = false;
    char end_char;

    for (int i = 0; i < len && i < STR_MAX - 1; i++) {
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
        temp[c++] = str_in[i];
    }

    temp[c] = '\0';

    str_trim(str_out, STR_MAX, temp, false);
}

void str_serializeTime(char *dest_str, int nTime)
{
    if (nTime >= 60) {
        int h = nTime / 3600;
        int m = (nTime - 3600 * h) / 60;
        if (h > 0) {
            snprintf(dest_str, STR_MAX, "%dh %dm", h, m);
        }
        else {
            snprintf(dest_str, STR_MAX, "%dm %ds", m, nTime - 60 * m);
        }
    }
    else {
        snprintf(dest_str, STR_MAX, "%ds", nTime);
    }
}

int str_count_char(const char *str, char ch)
{
    int count = 0;
    for (const char *p = str; *p; p++) {
        if (*p == ch)
            count++;
    }
    return count;
}

bool includeCJK(char *str)
{
    while (*str) {
        unsigned char c = (unsigned char)*str;
        // Check for CJK UTF-8 sequences
        // CJK Unified Ideographs: U+4E00–U+9FFF (0xE4 0xB8 0x80 to 0xE9 0xBF 0xBF)
        // Hiragana: U+3040–U+309F (0xE3 0x81 0x80 to 0xE3 0x82 0x9F)
        // Katakana: U+30A0–U+30FF (0xE3 0x82 0xA0 to 0xE3 0x83 0xBF)
        if (c >= 0xE3 && c <= 0xE9) {
            // Require a complete 3-byte UTF-8 sequence (both continuation bytes)
            if (str[1] && str[2] &&
                ((unsigned char)str[1] & 0xC0) == 0x80 &&
                ((unsigned char)str[2] & 0xC0) == 0x80) {
                return true;
            }
        }
        str++;
    }
    return false;
}