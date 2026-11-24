#ifndef REDAMP_NETCLOCK_H
#define REDAMP_NETCLOCK_H
/* Start silently from the foreground UI; network consumers can wait.
 * Returns 1 if the clock is plausible, 0 on failure/cancellation. */
/* Atomic policy shared by UI and network threads. */
void netclock_set_insecure(int enabled);
int netclock_insecure(void);
void netclock_start(void);
void netclock_shutdown(void);
int netclock_valid(void);
/* Local interface/route check; not a test of Internet reachability. */
int netclock_network_available(void);
int netclock_syncing(void);
int netclock_ensure(int (*cancel)(void *),void *ctx,int force);
#endif
