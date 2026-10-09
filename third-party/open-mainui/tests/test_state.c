/* SPDX-License-Identifier: GPL-3.0-only */
#include "cJSON.h"
#include "core/core.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rejected(const char *json)
{
    MainUIStack state = {.count = 1, .frames = {{7, -1, 19, 12, 20}}};
    MainUIStack before = state;
    assert(!mainui_state_parse(json, &state));
    assert(!memcmp(&state, &before, sizeof state));
}

int mainui_suite_state(void)
{
    const char *bad[] = {NULL,
                         "",
                         "null",
                         "[]",
                         "{}",
                         "{\"list\":{}}",
                         "{\"list\":[null]}",
                         "{\"list\":[3]}",
                         "{\"list\":[{}]}",
                         "{\"list\":[]} trailing"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        rejected(bad[i]);
    }
    const char *keys[] = {"title", "type", "currpos", "pagestart", "pageend"};
    const char *values[] = {"2147483648", "-2147483649", "0.5",  "-0.5", "1e999",
                            "\"1\"",      "null",        "true", "[]",   "{}"};
    for (size_t key = 0; key < 5; key++) {
        for (size_t value = 0; value < sizeof values / sizeof *values; value++) {
            char json[1024];
            int length = snprintf(json, sizeof json, "{\"list\":[");
            /* A valid first frame verifies that a later failure is atomic. */
            length +=
                snprintf(json + length, sizeof json - length,
                         "{\"title\":1,\"type\":2,\"currpos\":3,\"pagestart\":4,\"pageend\":5},{");
            for (size_t i = 0; i < 5; i++) {
                length += snprintf(json + length, sizeof json - length, "%s\"%s\":%s", i ? "," : "",
                                   keys[i], i == key ? values[value] : "0");
            }
            snprintf(json + length, sizeof json - length, "}]}");
            rejected(json);
        }
    }
    MainUIStack source = {.count = MAINUI_STACK_MAX}, decoded;
    for (size_t i = 0; i < source.count; i++) {
        source.frames[i] = (MainUIFrame){INT_MIN, INT_MAX, -1, (int)i, INT_MAX};
    }
    char *json = mainui_state_json(&source);
    assert(json && mainui_state_parse(json, &decoded));
    assert(decoded.count == source.count);
    for (size_t i = 0; i < source.count; i++) {
        assert(decoded.frames[i].title == INT_MIN && decoded.frames[i].type == INT_MAX);
        assert(decoded.frames[i].selected == -1 && decoded.frames[i].start == (int)i);
        assert(decoded.frames[i].end == INT_MAX);
    }
    cJSON *root = cJSON_Parse(json);
    cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "list");
    assert(cJSON_AddItemToArray(list, cJSON_Duplicate(cJSON_GetArrayItem(list, 0), true)));
    char *overflow = cJSON_PrintUnformatted(root);
    assert(overflow);
    rejected(overflow);
    free(overflow);
    cJSON_Delete(root);
    free(json);
    source.count++;
    assert(!mainui_state_json(&source) && !mainui_state_json(NULL));
    assert(!mainui_state_parse("{\"list\":[]}", NULL));
    assert(mainui_state_parse("{\"list\":[]}", &decoded) && decoded.count == 0);
    json = mainui_state_json(&decoded);
    assert(json && mainui_state_parse(json, &source) && source.count == 0);
    free(json);
    puts("State numeric limits, malformed later frames, sentinels and stack bounds passed");
    return 0;
}
