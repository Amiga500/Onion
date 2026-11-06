#ifndef KEYMON_DOUBLE_PRESS_H__
#define KEYMON_DOUBLE_PRESS_H__

// How long to wait, after a MENU release, for the second press of a double
// press: what is left of the 300 ms from the first press, 0 when that has
// gone. The wait was poll(300 - elapsed): a press held for more than 300 ms
// (a slow tap in a game, where the long press is 700 ms) gave a negative
// timeout, which poll() takes as "wait forever": the single-press action
// waited for the next key, and keymon handled no key meanwhile.

#define MENU_DOUBLE_PRESS_MS 300

static int menuButton_doublePressWait(int now_ms, int pressed_ms)
{
    int left = MENU_DOUBLE_PRESS_MS - (now_ms - pressed_ms);
    if (left < 0)
        return 0;
    if (left > MENU_DOUBLE_PRESS_MS)
        return MENU_DOUBLE_PRESS_MS; // a clock that went back
    return left;
}

#endif // KEYMON_DOUBLE_PRESS_H__
