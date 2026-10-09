/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_TEST_SUITES_H
#define MAINUI_TEST_SUITES_H

/* Self-contained suites. Each returns 0 on success and needs no fixture on
 * disk, so every one of them also runs unchanged on the device build. Add a
 * declaration here and an entry in the table in main.c. */
int mainui_suite_context(void);
int mainui_suite_core(void);
int mainui_suite_input(void);
int mainui_suite_launch(void);
int mainui_suite_letter_jump(void);
int mainui_suite_menu(void);
int mainui_suite_name_input(void);
int mainui_suite_options(void);
int mainui_suite_screen_events(void);
int mainui_suite_state(void);
int mainui_suite_timing(void);

#endif
