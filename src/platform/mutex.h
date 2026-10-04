/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MUTEX_H
#define MAINUI_MUTEX_H
/* Static initialization avoids racing lazy lock creation before SDL startup. */
#include <pthread.h>
typedef pthread_mutex_t MainUIMutex;
#define MAINUI_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void mainui_mutex_lock(MainUIMutex *lock)
{
    pthread_mutex_lock(lock);
}

static inline void mainui_mutex_unlock(MainUIMutex *lock)
{
    pthread_mutex_unlock(lock);
}
#endif
