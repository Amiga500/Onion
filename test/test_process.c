#include "onion_test.h"

#include "utils/file.h"
#include "utils/process.h"

#include <stdio.h>
#include <unistd.h>

TEST(searchpid_self)
{
    char comm[128] = {0};
    FILE *fp = fopen("/proc/self/comm", "r");
    ASSERT_NOT_NULL(fp);
    ASSERT_TRUE(fscanf(fp, "%127s", comm) == 1);
    fclose(fp);

    pid_t pid = process_searchpid(comm);
    ASSERT_TRUE(pid > 2);
}

TEST(searchpid_missing)
{
    ASSERT_EQ(process_searchpid("definitely_not_an_onion_proc_xyz"), 0);
}

TEST(isRunning_self)
{
    char comm[128] = {0};
    FILE *fp = fopen("/proc/self/comm", "r");
    ASSERT_NOT_NULL(fp);
    fscanf(fp, "%127s", comm);
    fclose(fp);
    ASSERT_TRUE(process_isRunning(comm));
    ASSERT_FALSE(process_isRunning("definitely_not_an_onion_proc_xyz"));
}

TEST(start_missing_is_false)
{
    ASSERT_FALSE(process_start("no_such_onion_bin_xyz", NULL, "/tmp", true));
}

TEST(start_true_await)
{
    ASSERT_TRUE(process_start("true", NULL, "/bin", true));
}

TEST(run_true_argv)
{
    ASSERT_TRUE(process_run("true", NULL, "/bin", true));
}

TEST(exec_path_true)
{
    char *argv[] = {"true", NULL};
    ASSERT_TRUE(process_exec_path("/bin/true", argv, true));
}

TEST(start_read_return_echo)
{
    char out[256];
    ASSERT_EQ(process_start_read_return("echo onion-refactor", out), 0);
    ASSERT_STREQ(out, "onion-refactor");
}

TEST(start_read_return_null)
{
    char out[8];
    ASSERT_EQ(process_start_read_return(NULL, out), -1);
}

int main(void)
{
    printf("\n=== process.h unit tests ===\n\n");
    RUN_TEST(searchpid_self);
    RUN_TEST(searchpid_missing);
    RUN_TEST(isRunning_self);
    RUN_TEST(start_missing_is_false);
    RUN_TEST(start_true_await);
    RUN_TEST(run_true_argv);
    RUN_TEST(exec_path_true);
    RUN_TEST(start_read_return_echo);
    RUN_TEST(start_read_return_null);
    return onion_test_report("test_process");
}
