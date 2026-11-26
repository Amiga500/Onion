/**
 * @file test_proc_exited.c
 * @brief Tests for proc_exited() (src/common/system/proc_exited.h)
 *
 * keymon stops MainUI with SIGTERM for the GameSwitcher and waits for it to
 * end; MainUI is runtime.sh's child, so it stays listed as a zombie until
 * runtime.sh collects it, and has to count as ended then.
 *
 * Build and run: make -f Makefile.unit test_proc_exited
 */

#include "onion_test.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/common/system/proc_exited.h"

TEST(stat_lines) {
    ASSERT_FALSE(proc_statLineExited("1234 (MainUI) R 1 1234 0"));
    ASSERT_FALSE(proc_statLineExited("1234 (MainUI) S 1 1234 0"));
    ASSERT_TRUE(proc_statLineExited("1234 (MainUI) Z 1 1234 0"));
    ASSERT_TRUE(proc_statLineExited("1234 (MainUI) X 1 1234 0"));
    // a name with spaces and brackets: the state is after the last ')'
    ASSERT_TRUE(proc_statLineExited("77 (a (b) c) Z 1 77 0"));
    ASSERT_FALSE(proc_statLineExited("77 (a (b) c) S 1 77 0"));
    ASSERT_FALSE(proc_statLineExited(""));
    ASSERT_FALSE(proc_statLineExited(NULL));
    ASSERT_FALSE(proc_statLineExited("1234 (MainUI)"));
}

TEST(running_process_has_not_ended) {
    ASSERT_FALSE(proc_exited(getpid()));
}

TEST(zombie_counts_as_ended) {
    pid_t child = fork();
    if (child == 0)
        _exit(0);
    ASSERT_TRUE(child > 0);
    // not collected yet: a zombie, as MainUI before runtime.sh's wait
    for (int i = 0; i < 100 && !proc_exited(child); i++)
        usleep(10000);
    ASSERT_TRUE(proc_exited(child));
    ASSERT_EQ(kill(child, 0), 0); // still listed
    waitpid(child, NULL, 0);
    ASSERT_TRUE(proc_exited(child)); // and gone
}

TEST(sigterm_then_ended) {
    pid_t child = fork();
    if (child == 0) {
        for (;;)
            pause();
    }
    ASSERT_FALSE(proc_exited(child));
    kill(child, SIGTERM);
    int waited = 0;
    while (!proc_exited(child) && waited < 1000) {
        usleep(20000);
        waited += 20;
    }
    ASSERT_TRUE(proc_exited(child));
    waitpid(child, NULL, 0);
}

int main(void)
{
    printf("\n=== proc_exited.h Unit Tests ===\n\n");

    RUN_TEST(stat_lines);
    RUN_TEST(running_process_has_not_ended);
    RUN_TEST(zombie_counts_as_ended);
    RUN_TEST(sigterm_then_ended);

    TEST_REPORT();
    return test_failures;
}
