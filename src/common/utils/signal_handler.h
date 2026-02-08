#ifndef SIGNAL_HANDLER_H__
#define SIGNAL_HANDLER_H__

#include <signal.h>
#include <stdbool.h>

/**
 * @brief Common signal handler setup for clean application shutdown
 * 
 * This utility provides a standardized way to handle SIGINT and SIGTERM signals
 * across all Onion applications. It eliminates code duplication by providing
 * both a macro for simple cases and a function for complex state management.
 * 
 * Usage:
 *   Simple case (sets a bool quit flag):
 *     static bool quit = false;
 *     SIGNAL_HANDLER_SETUP(quit);
 * 
 *   Complex case (custom handler):
 *     static void myHandler(int sig) { ... }
 *     signal(SIGINT, myHandler);
 *     signal(SIGTERM, myHandler);
 */

/**
 * @brief Generic signal handler that sets a boolean flag to true
 * 
 * @param quit_flag Pointer to a boolean flag to set on signal
 * @param sig Signal number received
 */
static inline void signal_handler_quit(volatile bool *quit_flag, int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        *quit_flag = true;
        break;
    default:
        break;
    }
}

/**
 * @brief Macro to setup standard signal handlers
 * 
 * Creates a local signal handler and registers it for SIGINT and SIGTERM.
 * The handler sets the provided quit_flag to true when signals are received.
 * 
 * @param quit_flag Name of the boolean variable to set (without &)
 */
#define SIGNAL_HANDLER_SETUP(quit_flag)                     \
    do {                                                    \
        static void _local_sigHandler(int sig) {            \
            signal_handler_quit(&quit_flag, sig);           \
        }                                                   \
        signal(SIGINT, _local_sigHandler);                  \
        signal(SIGTERM, _local_sigHandler);                 \
    } while (0)

#endif // SIGNAL_HANDLER_H__
