#include <assert.h>
#include <string.h>

#include "../src/common/utils/str.h"

int main(void)
{
    char out[STR_MAX];

    assert(str_count_char("", 'a') == 0);
    assert(str_count_char("abc", 'a') == 1);
    assert(str_count_char("a/b/c", '/') == 2);
    assert(str_count_char("abc", '\0') == 0);

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
