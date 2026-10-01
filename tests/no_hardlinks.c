/* SPDX-License-Identifier: GPL-3.0-only */
#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>

int link(const char *source, const char *destination)
{
    (void)source;
    (void)destination;
    errno = EPERM;
    return -1;
}

int linkat(int source_fd, const char *source, int destination_fd, const char *destination,
           int flags)
{
    (void)source_fd;
    (void)source;
    (void)destination_fd;
    (void)destination;
    (void)flags;
    errno = EPERM;
    return -1;
}
