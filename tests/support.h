/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_TEST_SUPPORT_H
#define MAINUI_TEST_SUPPORT_H

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

/* Fixture roots arrive as command-line strings, so the compiler cannot prove a
 * path fits. Assert instead of letting snprintf silently truncate and turn a
 * real failure into a confusing missing-file error. */
#define TEST_PATH(buffer, ...) test_format((buffer), sizeof(buffer), __VA_ARGS__)

static inline char *test_format(char *out, size_t size, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf(out, size, format, arguments);
    va_end(arguments);
    assert(written > 0 && (size_t)written < size);
    return out;
}

#endif
