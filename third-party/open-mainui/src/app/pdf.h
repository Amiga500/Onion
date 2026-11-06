/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_PDF_H
#define MAINUI_PDF_H
#include "app/app_context.h"
/* Optional Green reader; keeps the media menu alive across SDL restoration. */
void mainui_pdf_launch(MainUIApp *ui, const char *path);
#endif
