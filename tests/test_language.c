/* SPDX-License-Identifier: GPL-3.0-only */
#include "localization/language.h"
#include "platform/system_config.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    assert(!strcmp(mainui_translate(15, "Settings"), "Settings"));
    MainUILanguages list = {0};
    assert(mainui_languages_open(&list, argv[1], argv[2]));
    assert(list.count == 1);
    assert(!strcmp(list.entries[0].name, "Custom test"));
    assert(mainui_language_select(&list, argv[1]));
    assert(!strcmp(mainui_translate(15, "Settings"), "Custom settings"));
    assert(!strcmp(mainui_translate(407, "Tweaks"), "Custom tweaks"));
    assert(!strcmp(mainui_translate(9999, "fallback"), "fallback"));
    cJSON *saved = mainui_system_read(argv[1]);
    assert(!strcmp(cJSON_GetObjectItemCaseSensitive(saved, "language")->valuestring, "en.lang"));
    assert(cJSON_GetObjectItemCaseSensitive(saved, "preserve")->valueint == 123);
    cJSON_Delete(saved);
    mainui_languages_close(&list);
    mainui_language_close();
    assert(!strcmp(mainui_translate(15, "Settings"), "Settings"));
    puts("On-demand language, custom IDs, fallback and settings preservation passed");
    return 0;
}
