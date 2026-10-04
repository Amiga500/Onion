/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/device_info.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <unistd.h>

static bool native_text(void *context, const char *path, char *out, size_t capacity)
{
    (void)context;
    FILE *file = fopen(path, "rb");
    if (!file || capacity < 2) {
        if (file) {
            fclose(file);
        }
        return false;
    }
    /* procfs/sysfs sizes are not normal file sizes. Read the bounded stream. */
    size_t count = fread(out, 1, capacity - 1, file);
    bool ok = count && !ferror(file) && fgetc(file) == EOF && !memchr(out, 0, count);
    fclose(file);
    out[count] = 0;
    return ok;
}

static bool native_storage(void *context, const char *sd, uint64_t *used, uint64_t *total)
{
    (void)context;
    struct statvfs info;
    if (statvfs(sd, &info) != 0) {
        return false;
    }
    uint64_t block = info.f_frsize ? info.f_frsize : info.f_bsize;
    if (!block || !info.f_blocks || info.f_blocks > UINT64_MAX / block ||
        info.f_bfree > info.f_blocks) {
        return false;
    }
    *total = (uint64_t)info.f_blocks * block;
    *used = (uint64_t)(info.f_blocks - info.f_bfree) * block;
    return true;
}

uint64_t mainui_device_pll_mhz(uint16_t low, uint16_t high, uint16_t post, uint32_t fallback)
{
    uint64_t lpf = (uint32_t)low | ((uint32_t)high << 16);
    if (!lpf) {
        lpf = fallback;
    }
    if (!lpf) {
        return 0;
    }
    uint64_t mhz = (432000000ull * 524288 / lpf * 2 / ((uint64_t)post + 1) * 16) / 1000000;
    return mhz >= 100 && mhz <= 3000 ? mhz : 0;
}

static bool native_cpu(void *context, uint64_t *mhz)
{
    (void)context;
    /* Onion src/cpuclock/cpuclock.c: same registers and arithmetic, but no
     * writable mapping, governor changes, or frequency-setting code. */
    int fd = open("/dev/mem", O_RDONLY);
    if (fd < 0) {
        return false;
    }
    void *map = mmap(NULL, 0x1000, PROT_READ, MAP_SHARED, fd, 0x1f206000);
    close(fd);
    if (map == MAP_FAILED) {
        return false;
    }
    volatile uint16_t *words = map;
    volatile uint8_t *bytes = map;
    uint32_t fallback =
        ((uint32_t)bytes[0x2c2 * 2] << 16) | ((uint32_t)bytes[0x2c1 * 2] << 8) | bytes[0x2c0 * 2];
    *mhz = mainui_device_pll_mhz(words[0x2a4], words[0x2a6], words[0x232], fallback);
    munmap(map, 0x1000);
    return *mhz != 0;
}

static void trim(char *text)
{
    size_t start = 0, end = strlen(text);
    while (start < end && isspace((unsigned char)text[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end--;
    }
    memmove(text, text + start, end - start);
    text[end - start] = 0;
}

static bool positive(const char *text, const char *unit, uint64_t *value)
{
    while (isspace((unsigned char)*text)) {
        text++;
    }
    if (!isdigit((unsigned char)*text)) {
        return false;
    }
    char *end;
    errno = 0;
    unsigned long long number = strtoull(text, &end, 10);
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (errno || !number || strcmp(end, unit)) {
        return false;
    }
    *value = number;
    return true;
}

static bool runtime_text(const MainUIDeviceAdapter *adapter, const MainUIDeviceInfoSource *source,
                         const char *name, char *out, size_t capacity)
{
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/%s", adapter->runtime, name);
    if (n <= 0 || n >= (int)sizeof path || !source->text(source->context, path, out, capacity)) {
        return false;
    }
    trim(out);
    return *out != 0;
}

void mainui_device_info(const MainUIDeviceAdapter *adapter, const char *sd,
                        const MainUIDeviceInfoSource *injected, MainUIDeviceInfo *out)
{
    *out = (MainUIDeviceInfo){"Unknown", "Unknown", "Unknown", "Unknown"};
    if (!adapter || !sd) {
        return;
    }
    MainUIDeviceInfoSource source = {NULL, native_text, native_storage, native_cpu};
    if (injected) {
        source = *injected;
    }
    if (!source.text) {
        return;
    }
    if (adapter->backend == DEVICE_SIMULATED) {
        const char *names[] = {"serial-number.txt", "cpu-frequency.txt", "memory-size.txt",
                               "storage-usage.txt"};
        char *values[] = {out->serial, out->cpu, out->memory, out->storage};
        size_t sizes[] = {sizeof out->serial, sizeof out->cpu, sizeof out->memory,
                          sizeof out->storage};
        for (int i = 0; i < 4; i++) {
            char text[128];
            if (runtime_text(adapter, &source, names[i], text, sizes[i])) {
                strcpy(values[i], text);
            }
        }
        return;
    }
    char text[16384];
    /* Onion runtime.sh generates deviceSN with read_uuid at startup. */
    if (runtime_text(adapter, &source, "deviceSN", text, 128) && strlen(text) == 12) {
        bool valid = true;
        for (int i = 0; i < 12; i++) {
            valid &= isxdigit((unsigned char)text[i]) != 0;
        }
        if (valid) {
            strcpy(out->serial, text);
        }
    }
    uint64_t model = 0, mhz = 0, khz = 0;
    if (runtime_text(adapter, &source, "deviceModel", text, 32)) {
        positive(text, "", &model);
    }
    bool cpu = false;
    /* Stock 0x120154 reads cpuinfo_max_freq. Preserve that About meaning;
     * use current-frequency sources only when the advertised maximum is absent. */
    const char *paths[] = {"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                           "/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_max_freq",
                           "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq",
                           "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"};
    for (int i = 0; !cpu && i < 4; i++) {
        if (source.text(source.context, paths[i], text, 128)) {
            trim(text);
            if (positive(text, "", &khz) && khz >= 100000 && khz <= 3000000) {
                mhz = khz / 1000;
                cpu = true;
            }
        }
    }
    if (!cpu && (model == 283 || model == 354) && source.cpu) {
        cpu = source.cpu(source.context, &mhz) && mhz >= 100 && mhz <= 3000;
    }
    if (cpu) {
        snprintf(out->cpu, sizeof out->cpu, "%lluMHz", (unsigned long long)mhz);
    }
    if (source.text(source.context, "/proc/meminfo", text, sizeof text)) {
        for (char *line = text; line && *line;) {
            char *next = strchr(line, '\n');
            if (next) {
                *next++ = 0;
            }
            if (!strncmp(line, "MemTotal:", 9)) {
                trim(line);
                uint64_t kib = 0;
                if (positive(line + 9, "kB", &kib)) {
                    /* Stock 0x120244..0x120278 reconstructs nominal RAM in
                     * 32-MiB groups after kernel/reserved memory is excluded. */
                    uint64_t nominal = (kib / 1024 / 32 + 1) * 32;
                    snprintf(out->memory, sizeof out->memory, "%lluM", (unsigned long long)nominal);
                }
                break;
            }
            line = next;
        }
    }
    uint64_t used = 0, total = 0;
    if (source.storage && source.storage(source.context, sd, &used, &total) && total &&
        used <= total) {
        snprintf(out->storage, sizeof out->storage, "%lluG/%lluG",
                 (unsigned long long)(used / 1073741824), (unsigned long long)(total / 1073741824));
    }
}
