/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/saved_actions.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        return 2;
    }
    cJSON *record = cJSON_Parse(argv[2]);
    if (!record) {
        return 2;
    }
    bool ok = mainui_recent_add(argv[1], record);
    cJSON_Delete(record);
    return ok ? 0 : 1;
}
