#include <assert.h>

#include "../src/common/utils/str.h"

int main(void)
{
    assert(str_count_char("", 'a') == 0);
    assert(str_count_char("abc", 'a') == 1);
    assert(str_count_char("a/b/c", '/') == 2);
    assert(str_count_char("abc", '\0') == 0);

    return 0;
}
