#ifndef RENDER_TEXTBOX_FIT_H__
#define RENDER_TEXTBOX_FIT_H__

// Smallest font size a message is shrunk to so it fits the screen: below
// this, text on a 640x480 screen stops being readable.
#define TEXTBOX_FIT_MIN_SIZE 14

/**
 * @brief Next font size to try for a text measured at `size` as w x h pixels
 *        so it fits in max_w x max_h. SDL-free so it can be unit tested.
 *
 * Shrinks in proportion to the overflow, by at least one point, and never
 * below min_size. Returns `size` unchanged when the text already fits or
 * cannot get smaller.
 */
static int textbox_fit_size(int size, int w, int h, int max_w, int max_h, int min_size)
{
    if ((w <= max_w && h <= max_h) || size <= min_size || w <= 0 || h <= 0)
        return size;
    double scale_w = w > max_w ? (double)max_w / w : 1.0;
    double scale_h = h > max_h ? (double)max_h / h : 1.0;
    int next = (int)(size * (scale_w < scale_h ? scale_w : scale_h));
    if (next >= size)
        next = size - 1;
    if (next < min_size)
        next = min_size;
    return next;
}

#endif // RENDER_TEXTBOX_FIT_H__
