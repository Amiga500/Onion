#ifndef KEYMON_KONAMI_H__
#define KEYMON_KONAMI_H__

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// The easter egg's code (UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A in
// MainUI). Onion followed it with an index that went back to 0, or to 1 for
// UP, after a wrong key: a code typed right after an UP (often the last key
// pressed in a menu) or with one UP too many was missed, and only a second
// try started the game. The last presses are kept instead, and the code
// counts as soon as they match it, whatever came before.

#define KONAMI_MAX 16

typedef struct {
    int keys[KONAMI_MAX];
    int count;
} KonamiState;

// Feeds one key press. True when the last `len` presses are `code`; the
// presses then start over, so the same code is needed again for a second
// time.
static bool konami_feed(KonamiState *st, const int *code, int len, int key)
{
    if (st == NULL || code == NULL || len <= 0 || len > KONAMI_MAX)
        return false;
    if (st->count < 0 || st->count > len)
        st->count = 0;
    if (st->count == len)
        memmove(st->keys, st->keys + 1, (size_t)(len - 1) * sizeof(int));
    else
        st->count++;
    st->keys[st->count - 1] = key;
    if (st->count < len || memcmp(st->keys, code, (size_t)len * sizeof(int)) != 0)
        return false;
    st->count = 0;
    return true;
}

#endif // KEYMON_KONAMI_H__
