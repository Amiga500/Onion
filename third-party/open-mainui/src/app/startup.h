/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_STARTUP_H
#define MAINUI_STARTUP_H
#include "app/app_context.h"

/* -1 continues startup; a nonnegative value is the final exit status.
 * Failed setup cleans up the resources it has opened. Teardown is for a
 * fully initialized app, after setup_render. */
int mainui_setup_session(MainUIApp *ui, int argc, char **argv);
int mainui_setup_video(MainUIApp *ui);
void mainui_restore_session(MainUIApp *ui);
void mainui_setup_render(MainUIApp *ui);
int mainui_teardown(MainUIApp *ui);
#endif
