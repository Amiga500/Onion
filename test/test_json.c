#include "onion_test.h"

#include "utils/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_tmp[256];

static void setup_tmp(void)
{
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/onion_unit_json_%d", (int)getpid());
    mkdir(g_tmp, 0755);
}

static void teardown_tmp(void)
{
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_tmp);
    system(cmd);
}

static void join(char *out, size_t n, const char *name)
{
    snprintf(out, n, "%s/%s", g_tmp, name);
}

TEST(getString_and_missing)
{
    cJSON *o = cJSON_Parse("{\"name\":\"onion\"}");
    ASSERT_NOT_NULL(o);
    char dest[JSON_STRING_LEN];
    dest[0] = '\0';
    ASSERT_TRUE(json_getString(o, "name", dest));
    ASSERT_STREQ(dest, "onion");
    ASSERT_FALSE(json_getString(o, "nope", dest));
    cJSON_Delete(o);
}

TEST(getInt_getBool_getDouble)
{
    cJSON *o = cJSON_Parse("{\"n\":7,\"ok\":true,\"off\":false,\"pi\":3.5}");
    ASSERT_NOT_NULL(o);
    int n = 0;
    bool ok = false;
    bool off = true;
    double pi = 0;
    ASSERT_TRUE(json_getInt(o, "n", &n));
    ASSERT_EQ(n, 7);
    ASSERT_TRUE(json_getBool(o, "ok", &ok));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(json_getBool(o, "off", &off));
    ASSERT_FALSE(off);
    ASSERT_TRUE(json_getDouble(o, "pi", &pi));
    ASSERT_TRUE(pi > 3.4 && pi < 3.6);
    ASSERT_FALSE(json_getInt(o, "missing", &n));
    cJSON_Delete(o);
}

TEST(setString_existing_only)
{
    cJSON *o = cJSON_Parse("{\"k\":\"old\"}");
    ASSERT_NOT_NULL(o);
    ASSERT_TRUE(json_setString(o, "k", "new"));
    ASSERT_FALSE(json_setString(o, "nope", "x"));
    char dest[JSON_STRING_LEN];
    ASSERT_TRUE(json_getString(o, "k", dest));
    ASSERT_STREQ(dest, "new");
    cJSON_Delete(o);
}

TEST(forceSetString_adds_key)
{
    cJSON *o = cJSON_Parse("{}");
    ASSERT_NOT_NULL(o);
    ASSERT_TRUE(json_forceSetString(o, "k", "v"));
    char dest[JSON_STRING_LEN];
    ASSERT_TRUE(json_getString(o, "k", dest));
    ASSERT_STREQ(dest, "v");
    cJSON_Delete(o);
}

TEST(load_and_save_roundtrip)
{
    char path[512];
    join(path, sizeof(path), "cfg.json");
    FILE *fp = fopen(path, "w");
    ASSERT_NOT_NULL(fp);
    fputs("{\"core\":\"gpsp\"}", fp);
    fclose(fp);

    cJSON *o = json_load(path);
    ASSERT_NOT_NULL(o);
    char dest[JSON_STRING_LEN];
    ASSERT_TRUE(json_getString(o, "core", dest));
    ASSERT_STREQ(dest, "gpsp");

    ASSERT_TRUE(json_setString(o, "core", "mgba"));
    char path2[512];
    join(path2, sizeof(path2), "out.json");
    json_save(o, path2);
    cJSON_Delete(o);

    cJSON *o2 = json_load(path2);
    ASSERT_NOT_NULL(o2);
    ASSERT_TRUE(json_getString(o2, "core", dest));
    ASSERT_STREQ(dest, "mgba");
    cJSON_Delete(o2);
}

TEST(load_missing_is_null)
{
    char path[512];
    join(path, sizeof(path), "missing.json");
    cJSON *o = json_load(path);
    ASSERT_NULL(o);
}

TEST(save_null_is_noop)
{
    json_save(NULL, "/tmp/should_not_matter.json");
    cJSON *o = cJSON_Parse("{}");
    json_save(o, NULL);
    cJSON_Delete(o);
}

int main(void)
{
    printf("\n=== json.h unit tests ===\n\n");
    setup_tmp();
    RUN_TEST(getString_and_missing);
    RUN_TEST(getInt_getBool_getDouble);
    RUN_TEST(setString_existing_only);
    RUN_TEST(forceSetString_adds_key);
    RUN_TEST(load_and_save_roundtrip);
    RUN_TEST(load_missing_is_null);
    RUN_TEST(save_null_is_noop);
    teardown_tmp();
    return onion_test_report("test_json");
}
