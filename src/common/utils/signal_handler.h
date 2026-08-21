#ifndef SIGNAL_HANDLER_H__
#define SIGNAL_HANDLER_H__

#include <signal.h>
#include <stdbool.h>

/**
 * @brief Common signal handler setup for clean application shutdown
 * 
 * This utility provides a standardized way to handle SIGINT and SIGTERM signals
 * across all Onion applications. It eliminates code duplication by providing
 * a reusable inline function for signal handling.
 * 
 * Usage:
 *   static bool quit = false;
 *   
 *   static void sigHandler(int sig) {
 *       signal_handler_quit(&quit, sig);
 *   }
 *   
 *   // In main():
 *   signal(SIGINT, sigHandler);
 *   signal(SIGTERM, sigHandler);
 */

/**
 * @brief Generic signal handler that sets a boolean flag to true
 * 
 * This inline function handles SIGINT and SIGTERM by setting the provided
 * quit flag to true, allowing for clean application shutdown.
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

#endif // SIGNAL_HANDLER_H__
