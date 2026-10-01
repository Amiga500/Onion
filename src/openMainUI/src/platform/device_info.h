/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DEVICE_INFO_H
#define MAINUI_DEVICE_INFO_H
#include "platform/device_adapter.h"
#include <stdint.h>

typedef struct {
    char serial[128], cpu[64], memory[64], storage[128];
} MainUIDeviceInfo;

/* Optional borrowed read-only probes for fixture tests. Text readers must return
 * bounded NUL-terminated text; storage returns byte counts. NULL uses native I/O. */
typedef struct {
    void *context;
    bool (*text)(void *, const char *, char *, size_t);
    bool (*storage)(void *, const char *, uint64_t *, uint64_t *);
    bool (*cpu)(void *, uint64_t *);
} MainUIDeviceInfoSource;

/* Fresh readings on each call; independently unavailable fields say Unknown.
 * Simulated mode reads only runtime fixtures. Real mode reads Onion/kernel data.
 * No shell commands, writes, network calls, or cached previous readings. */
void mainui_device_info(const MainUIDeviceAdapter *, const char *sd, const MainUIDeviceInfoSource *,
                        MainUIDeviceInfo *);
/* Read-only counterpart of Onion cpuclock's PLL formula. Invalid values return 0. */
uint64_t mainui_device_pll_mhz(uint16_t low, uint16_t high, uint16_t post, uint32_t fallback);
#endif
