#!/bin/sh
mydir=$(dirname "$0")
cd $mydir

export HOME=$mydir
export PATH=$mydir:$PATH
export LD_LIBRARY_PATH=$mydir/libs:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

. /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh

CUST_LOGO=0
CUST_CPUCLOCK=1

# Uncomment this section if you need to customize the Drastic menu logo :
# if [ "$CUST_LOGO" == "1" ]; then
# echo "convert resources/logo/0.png to drastic_logo_0.raw"
# echo "convert resources/logo/1.png to drastic_logo_1.raw"
# ./png2raw
# fi

# True when the framebuffer is 752x560, two pages, 32 bpp (what DraStic
# expects on the Mini v4 and Flip).
fb_is_752x560() {
    fbset 2> /dev/null | grep -q "geometry 752 560 752 1120 32"
}

ensure_752x560_mode() {
    fb_is_752x560 && return 0
    before=$(fbset 2> /dev/null | grep -o "geometry.*")
    fbmode=/mnt/SDCARD/.tmp_update/bin/fbmode
    if [ ! -x "$fbmode" ] || ! "$fbmode" 752x560 --pages 2 --preclear --no-clear --timeout 500; then
        fbset -g 752 560 752 1120 32
    fi
    tries=0
    while ! fb_is_752x560 && [ $tries -lt 20 ]; do
        sleep 0.05
        tries=$((tries + 1))
    done
    # Kept small: one line each time the mode had to be fixed.
    echo "$(date '+%F %T') DraStic start: was '${before:-unknown}', now '$(fbset 2> /dev/null | grep -o "geometry.*")' after $tries waits" \
        >> /mnt/SDCARD/.tmp_update/logs/drastic_mode.log 2> /dev/null
}

resolution=$(head -n 1 /tmp/screen_resolution)
if [ "$resolution" = "752x560" ]; then
    USE_752x560_RES=1
else
    USE_752x560_RES=0
fi

sv=$(cat /proc/sys/vm/swappiness)

echo 10 >/proc/sys/vm/swappiness # 60 by default

cd $mydir

if [ "$CUST_CPUCLOCK" == "1" ]; then
    cpuclock 1500
fi

l_triggers_swapped=$(jq ".swap_l1l2" "/mnt/SDCARD/Emu/NDS/resources/settings.json")

if [ $l_triggers_swapped -eq 1 ]; then
    touch /tmp/drastic_swap_l1l2
else
    rm -f /tmp/drastic_swap_l1l2
fi

# DraStic's SDL picks its screen layout once, at start, from `fbset`: the
# 752x560 layout only when the framebuffer already reports 752. On the Mini
# v4 and Flip, if the mode is not (or not yet) 752x560 with two pages at
# that moment, DraStic draws its 640x480 layout into it and the screen is
# garbled until it is restarted. Check the mode here and set it if needed.
if [ "$USE_752x560_RES" = "1" ]; then
    ensure_752x560_mode
fi

./drastic "$1"

sync

echo $sv >/proc/sys/vm/swappiness

# The Mini v4 and Flip go back to 640x480 in runtime.sh (fbmode --preclear),
# which clears the screen first. An fbset here switched the mode before that
# and showed DraStic's last 752x560 frame garbled on exit.
