#include <assert.h>
#include <string.h>

#include "../src/common/utils/str.h"

int main(void)
{
    char out[STR_MAX];
    char hidden_suffix[] = {'a', '\0', 'b', '\0'};
    char long_name[STR_MAX + 32];
    size_t trimmed;

    assert(str_count_char("", 'a') == 0);
    assert(str_count_char("abc", 'a') == 1);
    assert(str_count_char("a/b/c", '/') == 2);
    assert(str_count_char("abc", '\0') == 0);
    assert(str_count_char(hidden_suffix, 'b') == 0);

    trimmed = str_trim(out, sizeof(out), "", false);
    assert(trimmed == 0);
    assert(strcmp(out, "") == 0);

    trimmed = str_trim(out, sizeof(out), "   ", false);
    assert(trimmed == 0);
    assert(strcmp(out, "") == 0);

    trimmed = str_trim(out, sizeof(out), "  abc  ", false);
    assert(trimmed == 3);
    assert(strcmp(out, "abc") == 0);

    trimmed = str_trim(out, sizeof(out), "\"abc\"", false);
    assert(trimmed == 3);
    assert(strcmp(out, "abc") == 0);

    str_removeParentheses(out, "Game Title (USA) [Hack]");
    assert(strcmp(out, "Game Title") == 0);

    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    str_removeParentheses(out, long_name);
    assert(strlen(out) == STR_MAX - 2);
    for (size_t i = 0; i < strlen(out); i++)
        assert(out[i] == 'x');

    str_serializeTime(out, -1);
    assert(strcmp(out, "0s") == 0);

    str_serializeTime(out, 0);
    assert(strcmp(out, "0s") == 0);

    str_serializeTime(out, 59);
    assert(strcmp(out, "59s") == 0);

    str_serializeTime(out, 60);
    assert(strcmp(out, "1m 0s") == 0);

    str_serializeTime(out, 3600);
    assert(strcmp(out, "1h 0m") == 0);

    return 0;
}
