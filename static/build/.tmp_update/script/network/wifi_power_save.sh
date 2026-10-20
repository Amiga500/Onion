#!/bin/sh
# Wi-Fi power save on the interfaces that are up: wifi_power_save.sh on|off
#
# Wi-Fi starts with power save on (wifi_on in update_networking.sh): idle
# with Wi-Fi connected, a Mini Plus drained 0.62 %/min instead of 0.75. It
# delays packets that arrive while the radio sleeps: a ping to an idle Mini
# Plus averaged 40 ms (peaks of 220 ms) against 3.5 ms (peak 15 ms) with it
# off. Netplay turns it off while RetroArch runs and on again afterwards.
sysdir=${sysdir:-/mnt/SDCARD/.tmp_update}
net=${WIFI_NET_DIR:-/sys/class/net}

case "$1" in
    on | off) ;;
    *)
        echo "usage: $0 on|off" >&2
        exit 2
        ;;
esac

# iw needs libnl (lib) and libatomic (lib/parasyte), whoever calls this.
export LD_LIBRARY_PATH="$sysdir/lib:$sysdir/lib/parasyte:${LD_LIBRARY_PATH:-/lib}"
for dev in wlan0 wlan1; do
    [ -e "$net/$dev" ] && "$sysdir/bin/iw" dev "$dev" set power_save "$1" 2> /dev/null
done
exit 0
