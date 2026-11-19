#ifndef KEYMON_SUSPEND_TIMER_H__
#define KEYMON_SUSPEND_TIMER_H__

// Standby on the Mini and Mini Plus: the wait before the power-off counts
// from the start of the standby. It was poll(timeout) again after every
// input event, ignored ones included (a key other than POWER, a release),
// so events less than a minute apart kept a Mini Plus in standby for hours.
// The Flip already counts from the start (500 ms polls for the lid).

// The poll timeout left: -1 with no timeout (stay awake), 0 once elapsed.
static inline int suspendTimer_remaining(int timeout_ms, long elapsed_ms)
{
    if (timeout_ms < 0)
        return -1;
    if (elapsed_ms < 0) // clock gone back: count from now
        elapsed_ms = 0;
    if (elapsed_ms >= timeout_ms)
        return 0;
    return (int)(timeout_ms - elapsed_ms);
}

#endif // KEYMON_SUSPEND_TIMER_H__
