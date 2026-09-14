#include "onion_test.h"

#include "utils/file.h"
#include "utils/log.h"

#include <stdio.h>
#include <unistd.h>

static char g_path[256];

static void setup(void)
{
    snprintf(g_path, sizeof(g_path), "/tmp/onion_unit_log_%d.log", (int)getpid());
    remove(g_path);
}

static void teardown(void)
{
    log_setPath(NULL);
    remove(g_path);
}

TEST(debug_without_path_does_not_create_file)
{
    log_setPath(NULL);
    log_debug("t.c", 1, "hello %s\n", "x");
    ASSERT_FALSE(exists(g_path));
}

TEST(debug_appends_to_path)
{
    log_setPath(g_path);
    log_debug("t.c", 7, "one\n");
    log_debug("t.c", 8, "two\n");
    char *s = file_read(g_path);
    ASSERT_NOT_NULL(s);
    ASSERT_TRUE(strstr(s, "t.c:7>\t") != NULL);
    ASSERT_TRUE(strstr(s, "one") != NULL);
    ASSERT_TRUE(strstr(s, "t.c:8>\t") != NULL);
    free(s);
}

int main(void)
{
    printf("\n=== log.c unit tests ===\n\n");
    setup();
    RUN_TEST(debug_without_path_does_not_create_file);
    RUN_TEST(debug_appends_to_path);
    teardown();
    return onion_test_report("test_log");
}
