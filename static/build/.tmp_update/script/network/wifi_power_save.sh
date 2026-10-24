#!/bin/sh
# Wi-Fi power save on the Wi-Fi interfaces present: wifi_power_save.sh on|off|auto
#
# Power save saves battery: idle with Wi-Fi connected, a Mini Plus drained
# 0.62 %/min instead of 0.75. It delays packets that arrive while the radio
# sleeps: a ping to an idle Mini Plus averaged 40 ms (peaks of 220 ms)
# against 3.5 ms (peak 15 ms) with it off, and file transfers stutter.
#
# auto: off while a file-transfer service is switched on in Tweaks > Network
# (FTP, SSH/SFTP, HTTP file server, Samba, Telnet), on once they are all
# off, or paused for a game ("Disable services in game"). Called when
# Wi-Fi starts, when a service is toggled and when services start or stop
# around a game. Netplay turns it off while RetroArch runs, then auto.
sysdir=${sysdir:-/mnt/SDCARD/.tmp_update}
net=${WIFI_NET_DIR:-/sys/class/net}
services_paused=${SERVICES_PAUSED_FLAG:-/tmp/services_paused_in_game}

mode=$1
case "$mode" in
    on | off) ;;
    auto)
        mode=on
        if [ ! -f "$services_paused" ]; then
            for service in ftp ssh http smbd telnet; do
                if [ -f "$sysdir/config/.${service}State" ]; then
                    mode=off
                    break
                fi
            done
        fi
        ;;
    *)
        echo "usage: $0 on|off|auto" >&2
        exit 2
        ;;
esac

# iw needs libnl (lib) and libatomic (lib/parasyte), whoever calls this.
export LD_LIBRARY_PATH="$sysdir/lib:$sysdir/lib/parasyte:${LD_LIBRARY_PATH:-/lib}"
# wlan0 is the station, wlan1 the Easy Netplay hotspot (hotspot_create.sh).
for dev in wlan0 wlan1; do
    [ -e "$net/$dev" ] && "$sysdir/bin/iw" dev "$dev" set power_save "$mode" 2> /dev/null
done
exit 0
