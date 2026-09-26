#ifndef KEYMON_INPUT_INJECT_H__
#define KEYMON_INPUT_INJECT_H__

#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

// One EV_KEY event, as the sendkeys tool writes it (no EV_SYN). Returns
// false unless the whole event was written.
static bool input_injectKey(int fd, unsigned short code, signed int value)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = EV_KEY;
    event.code = code;
    event.value = value;
    return fd >= 0 && write(fd, &event, sizeof(event)) == (ssize_t)sizeof(event);
}

#endif // KEYMON_INPUT_INJECT_H__
