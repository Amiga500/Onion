/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/options.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

int mainui_suite_options(void)
{
    MainUIOptions options;
    char *valid[] = {"MainUI", "--sd-root", "fixture", "--systems", "--battery", "500"};
    assert(mainui_options_parse(&options, 6, valid) == -1);
    assert(!strcmp(options.sd, "fixture") && options.start_systems);
    assert(options.battery_override && options.battery_percent == 500);
    assert(!options.informational);
    char *help[] = {"MainUI", "--help"};
    assert(mainui_options_parse(&options, 2, help) == 0 && options.informational);
    char *version[] = {"MainUI", "--version"};
    assert(mainui_options_parse(&options, 2, version) == 0 && options.informational);
    char *missing[] = {"MainUI", "--sd-root"};
    assert(mainui_options_parse(&options, 2, missing) == 2 && !options.informational);
    char *conflict[] = {"MainUI", "--sd-root", "fixture", "--list", "list"};
    assert(mainui_options_parse(&options, 5, conflict) == 2);
    char *input[] = {"MainUI", "--sd-root", "fixture", "--input", "E"};
    assert(mainui_options_parse(&options, 5, input) == 2);
    char *battery[] = {"MainUI", "--sd-root", "fixture", "--battery", "101"};
    assert(mainui_options_parse(&options, 5, battery) == 2);
    char *unknown[] = {"MainUI", "--unknown", "x"};
    assert(mainui_options_parse(&options, 3, unknown) == 2);
    return 0;
}
