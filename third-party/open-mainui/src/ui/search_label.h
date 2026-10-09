/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_SEARCH_LABEL_H
#define MAINUI_SEARCH_LABEL_H
#include "ui/theme.h"
/* Owned optional unselected label; the selected row keeps its normal surface. */
SDL_Surface *mainui_search_label(MainUITheme *theme, const char *label, const char *query);
#endif
