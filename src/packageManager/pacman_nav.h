#ifndef PACMAN_NAV_H__
#define PACMAN_NAV_H__

// Moving through a package list and scrolling the summary, without SDL so
// host tests run it.
//
// A list shows 7 rows: the selected one is pos + sel. The old code assumed
// at least 7 packages: with fewer, DOWN moved the selection onto empty rows
// and UP at the top set the first row to count - 7, a negative index the
// list was then drawn from (a read before the package table). The summary
// of a short list (under one screen) was scrolled to a negative offset by
// UP/DOWN and drawn shifted out of view.

#include <stdbool.h>

#define PACMAN_ROWS 7

static int pacman_visibleRows(int count)
{
    return count < PACMAN_ROWS ? count : PACMAN_ROWS;
}

// DOWN; first_press: not a key repeat (only a press wraps to the top).
static void pacman_down(int count, bool first_press, int *sel, int *pos)
{
    int rows = pacman_visibleRows(count);
    if (rows == 0)
        return;
    if (*sel < rows - 1)
        (*sel)++;
    else if (*pos + *sel < count - 1)
        (*pos)++;
    else if (first_press)
        *sel = *pos = 0;
}

// UP; first_press: not a key repeat (only a press wraps to the bottom).
static void pacman_up(int count, bool first_press, int *sel, int *pos)
{
    int rows = pacman_visibleRows(count);
    if (rows == 0)
        return;
    if (*sel > 0)
        (*sel)--;
    else if (*pos > 0)
        (*pos)--;
    else if (first_press) {
        *sel = rows - 1;
        *pos = count - rows;
    }
}

// R2: a page down.
static void pacman_pageDown(int count, int *sel, int *pos)
{
    if (*pos + 2 * PACMAN_ROWS < count)
        *pos += PACMAN_ROWS;
    else if (*pos + PACMAN_ROWS < count) {
        *pos = count - PACMAN_ROWS;
        *sel = PACMAN_ROWS - 1;
    }
}

// L2: a page up.
static void pacman_pageUp(int *sel, int *pos)
{
    if (*pos - PACMAN_ROWS > 0)
        *pos -= PACMAN_ROWS;
    else {
        *pos = 0;
        *sel = 0;
    }
}

// Scroll offset y of content_h pixels shown view_h at a time, moved by
// offset and kept within the content (0 when it fits). True when it moved.
static bool pacman_scrollBy(int *y, int offset, int content_h, int view_h)
{
    int y_max = content_h > view_h ? content_h - view_h : 0;
    int old = *y;
    int moved = *y + offset;
    if (moved > y_max)
        moved = y_max;
    if (moved < 0)
        moved = 0;
    *y = moved;
    return moved != old;
}

#endif // PACMAN_NAV_H__
