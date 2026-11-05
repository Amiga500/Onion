#ifndef KEYMON_NET_ACTIVITY_H__
#define KEYMON_NET_ACTIVITY_H__

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// File transfers keep the device awake. keymon's sleep timer only counted
// button presses, so a long FTP/HTTP/Samba/SFTP transfer with nobody
// touching the console went to standby (and the Wi-Fi with it) after the
// sleep time. While one of the file-transfer services is switched on in
// Tweaks > Network (the same ones wifi_power_save.sh watches), Wi-Fi
// traffic above NET_ACTIVITY_MIN_BYTES_PER_SEC since the last check counts
// as activity. Background chatter (broadcasts, an idle session) stays well
// below it, so an idle device still goes to standby as before.

#ifndef NET_ACTIVITY_SYSFS
#define NET_ACTIVITY_SYSFS "/sys/class/net"
#endif
#ifndef NET_ACTIVITY_CONFIG_DIR
#define NET_ACTIVITY_CONFIG_DIR "/mnt/SDCARD/.tmp_update/config"
#endif
#define NET_ACTIVITY_MIN_BYTES_PER_SEC 2048ULL

typedef struct {
    unsigned long long bytes; // rx + tx at the last check
    int ms;                   // when it was read
    bool valid;
} NetActivity;

// One sysfs counter; false when missing or unreadable.
static bool netActivity_readCounter(const char *path, unsigned long long *out)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    bool ok = fscanf(fp, "%llu", out) == 1;
    fclose(fp);
    return ok;
}

// rx + tx bytes of wlan0 (station) and wlan1 (Easy Netplay hotspot); false
// when neither is there (a Mini without Wi-Fi, Wi-Fi off).
static bool netActivity_bytes(const char *sysfs, unsigned long long *total)
{
    static const char *const devs[] = {"wlan0", "wlan1"};
    static const char *const stats[] = {"rx_bytes", "tx_bytes"};
    bool found = false;
    *total = 0;
    for (int d = 0; d < 2; d++) {
        for (int s = 0; s < 2; s++) {
            char path[256];
            unsigned long long value;
            snprintf(path, sizeof(path), "%s/%s/statistics/%s", sysfs, devs[d], stats[s]);
            if (netActivity_readCounter(path, &value)) {
                *total += value;
                found = true;
            }
        }
    }
    return found;
}

// A file-transfer service is on in Tweaks > Network.
static bool netActivity_servicesOn(const char *config_dir)
{
    static const char *const services[] = {"ftp", "ssh", "http", "smbd", "telnet"};
    for (int i = 0; i < 5; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/.%sState", config_dir, services[i]);
        if (access(path, F_OK) == 0)
            return true;
    }
    return false;
}

// Records this reading and says whether the traffic since the previous one
// was a transfer. The first reading, a counter that went back (Wi-Fi
// restarted, a 32-bit counter wrapped) or no time elapsed only resync.
static bool netActivity_busy(NetActivity *na, bool have, unsigned long long bytes, int now_ms)
{
    if (!have) {
        na->valid = false;
        return false;
    }
    bool busy = false;
    if (na->valid && bytes >= na->bytes && now_ms - na->ms > 0)
        busy = (bytes - na->bytes) * 1000ULL >=
               NET_ACTIVITY_MIN_BYTES_PER_SEC * (unsigned long long)(now_ms - na->ms);
    na->bytes = bytes;
    na->ms = now_ms;
    na->valid = true;
    return busy;
}

// keymon's check: a transfer is going on with a file-transfer service on.
static bool netActivity_keepAwake(NetActivity *na, const char *sysfs,
                                  const char *config_dir, int now_ms)
{
    if (!netActivity_servicesOn(config_dir)) {
        na->valid = false;
        return false;
    }
    unsigned long long bytes = 0;
    bool have = netActivity_bytes(sysfs, &bytes);
    return netActivity_busy(na, have, bytes, now_ms);
}

#endif // KEYMON_NET_ACTIVITY_H__
