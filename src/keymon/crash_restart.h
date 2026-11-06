#ifndef KEYMON_CRASH_RESTART_H__
#define KEYMON_CRASH_RESTART_H__

// keymon has to keep running: without it the MENU and power buttons, the
// volume and the brightness do nothing. On a SIGSEGV it ran quit() from the
// signal handler (closing the display, saving the clock): calls that can
// lock up after a crash (a malloc or stdio lock held by the code that
// crashed), leaving a hung keymon that runtime.sh still sees as running;
// otherwise it exited, and runtime.sh only starts it again at its next
// state change (a game ending). Now a crashed keymon starts itself again on
// the spot with execve(), which is async-signal-safe, at most
// KEYMON_RESTART_MAX times (a counter passed in the environment), so a
// keymon that crashes as it starts doesn't loop.
//
// Everything the handler uses is prepared beforehand. Descriptors that
// must not survive the restart (the input device: its grab would keep
// every key from the new keymon) are opened with FD_CLOEXEC.

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

#define KEYMON_RESTART_ENV "KEYMON_RESTARTS"
#define KEYMON_RESTART_MAX 5
#define KEYMON_RESTART_ENV_MAX 256

static char crash_exe[256] = "/proc/self/exe";
static char *crash_argv[8];
static char *crash_envp[KEYMON_RESTART_ENV_MAX + 2];
static char crash_counter[64];
static volatile sig_atomic_t crash_can_restart = 0;
static pid_t crash_pid = 0; // keymon itself, not a child forked to run a program

// The restarts so far (from the environment).
static int crashRestart_count(char **envp)
{
    size_t len = strlen(KEYMON_RESTART_ENV "=");
    for (char **e = envp; e != NULL && *e != NULL; e++)
        if (strncmp(*e, KEYMON_RESTART_ENV "=", len) == 0)
            return atoi(*e + len);
    return 0;
}

static void crashRestart_handler(int sig)
{
    // A child of keymon that crashed between fork() and exec() must not
    // become a second keymon.
    if (crash_can_restart && getpid() == crash_pid)
        execve(crash_exe, crash_argv, crash_envp);
    // No restart left, or execve() failed: crash as before, with the
    // default action (runtime.sh starts keymon at its next state change).
    signal(sig, SIG_DFL);
    raise(sig);
}

// Prepare the restart and install the handler for the crash signals.
// exe: the program to start ("/proc/self/exe"); argv/envp: as main() got
// them. Returns the restarts done so far.
static int crashRestart_install(const char *exe, char **argv, char **envp)
{
    int count = crashRestart_count(envp);

    snprintf(crash_exe, sizeof(crash_exe), "%s", exe);
    int n = 0;
    for (; argv != NULL && argv[n] != NULL && n < (int)(sizeof(crash_argv) / sizeof(crash_argv[0])) - 1; n++)
        crash_argv[n] = argv[n];
    crash_argv[n] = NULL;

    int k = 0;
    size_t len = strlen(KEYMON_RESTART_ENV "=");
    for (char **e = envp; e != NULL && *e != NULL && k < KEYMON_RESTART_ENV_MAX; e++)
        if (strncmp(*e, KEYMON_RESTART_ENV "=", len) != 0)
            crash_envp[k++] = *e;
    snprintf(crash_counter, sizeof(crash_counter), KEYMON_RESTART_ENV "=%d", count + 1);
    crash_envp[k++] = crash_counter;
    crash_envp[k] = NULL;

    crash_can_restart = n > 0 && count < KEYMON_RESTART_MAX;
    crash_pid = getpid();

    // A handler that crashed again must not find the signal blocked: the
    // mask is kept across execve() (SA_NODEFER), and a blocked crash signal
    // just kills the process.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crashRestart_handler;
    sa.sa_flags = SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    sigset_t unblock;
    sigemptyset(&unblock);
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        sigaction(signals[i], &sa, NULL);
        sigaddset(&unblock, signals[i]);
    }
    sigprocmask(SIG_UNBLOCK, &unblock, NULL);
    return count;
}

#endif // KEYMON_CRASH_RESTART_H__
