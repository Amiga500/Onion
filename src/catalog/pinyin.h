/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_PINYIN_H
#define MAINUI_PINYIN_H
#include <stddef.h>
/* Stock Search initials: ASCII unchanged, first py.dat pronunciation initial,
 * unknown Unicode characters become spaces. Output is always terminated.
 * The SD's miyoo/app/py.dat is a process snapshot; changing SD roots reloads it. */
void mainui_pinyin(const char *sd, const char *name, char *out, size_t capacity);
#endif
