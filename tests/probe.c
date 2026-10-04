/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "state") == 0) {
        MainUIStack state;
        if (!mainui_state_parse(argv[2], &state)) {
            return 2;
        }
        char *json = mainui_state_json(&state);
        if (!json) {
            return 3;
        }
        puts(json);
        free(json);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "config") == 0) {
        const char *input = strcmp(argv[3], "<missing>") ? argv[3] : NULL;
        MainUIConfig c;
        mainui_config_parse(&c, input, input, input, input);
        if (!strcmp(argv[2], "rows")) {
            printf("[%d,%d]\n", c.rows, c.row_height);
        }
        else if (!strcmp(argv[2], "font")) {
            printf("%d\n", c.font_size);
        }
        else if (!strcmp(argv[2], "repeat")) {
            printf("[%d,%d]\n", c.repeat_delay, c.repeat_interval);
        }
        else if (!strcmp(argv[2], "scroll")) {
            printf("[%d,%d,%d]\n", c.scroll_status, c.scroll_delay, c.scroll_speed);
        }
        else {
            return 2;
        }
        return 0;
    }
    fprintf(stderr, "Development harness: config rows|font|repeat|scroll VALUE, or state JSON\n");
    return 2;
}
