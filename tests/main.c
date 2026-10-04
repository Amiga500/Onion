/* SPDX-License-Identifier: GPL-3.0-only */
/* Single entry point for the self-contained suites.
 *
 *   build/unit-tests            run every suite
 *   build/unit-tests core menu  run only the named suites
 *
 * SDL redefines main to SDL_main on some platforms; the suites no longer carry
 * that guard individually, so it is undone once here. */
#include "suites.h"
#ifdef main
#undef main
#endif
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    int (*run)(void);
} Suite;

static const Suite suites[] = {
    {"context", mainui_suite_context},
    {"core", mainui_suite_core},
    {"input", mainui_suite_input},
    {"launch", mainui_suite_launch},
    {"letter_jump", mainui_suite_letter_jump},
    {"menu", mainui_suite_menu},
    {"name_input", mainui_suite_name_input},
    {"options", mainui_suite_options},
    {"screen_events", mainui_suite_screen_events},
    {"state", mainui_suite_state},
    {"timing", mainui_suite_timing},
};

static const size_t count = sizeof suites / sizeof suites[0];

static int selected(int argc, char **argv, const char *name)
{
    if (argc < 2) {
        return 1;
    }
    for (int index = 1; index < argc; index++) {
        if (!strcmp(argv[index], name)) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    /* Reject an unknown filter rather than silently running nothing. */
    for (int index = 1; index < argc; index++) {
        size_t position = 0;
        while (position < count && strcmp(suites[position].name, argv[index])) {
            position++;
        }
        if (position == count) {
            fprintf(stderr, "unknown suite: %s\navailable:", argv[index]);
            for (position = 0; position < count; position++) {
                fprintf(stderr, " %s", suites[position].name);
            }
            fputc('\n', stderr);
            return 2;
        }
    }
    int failures = 0, ran = 0;
    for (size_t index = 0; index < count; index++) {
        if (!selected(argc, argv, suites[index].name)) {
            continue;
        }
        ran++;
        printf("[ run  ] %s\n", suites[index].name);
        fflush(stdout);
        int status = suites[index].run();
        if (status) {
            printf("[ FAIL ] %s (status %d)\n", suites[index].name, status);
            failures++;
        }
        else {
            printf("[  ok  ] %s\n", suites[index].name);
        }
        fflush(stdout);
    }
    printf("%d suite(s) run, %d failed\n", ran, failures);
    return failures ? 1 : 0;
}
