#!/bin/sh
scriptlabel="Scraper (%LIST%)"
scriptinfo="Launches the scraper\nfor the selected system."
require_networking=1
echo $0 $*

sysdir=/mnt/SDCARD/.tmp_update
rm -f /tmp/scraper_script.sh

cd $sysdir
# New UI: the menus, the messages and the scraping are drawn natively
# (menu.sh with SCRAPER_NATIVE=1: prompt and progressUI); a terminal opens
# only for the on-screen keyboard. SCRAPER_TERMINAL=1: the terminal menus.
if [ -z "$SCRAPER_TERMINAL" ]; then
    SCRAPER_NATIVE=1 sh "$sysdir/script/scraper/menu.sh" "$1" "$2"
else
    # MENU closes the terminal. pressMenu2Kill runs "pkill -f", so the
    # pattern must match the terminal only: "st" alone also matched
    # game_list_options.sh ("list") and killed the GLO with it.
    pressMenu2Kill bin/st &
    ./bin/st -q -e "$sysdir/script/scraper/menu.sh" "$1" "$2"
    pkill -9 pressMenu2Kill
fi

# However the menu ended. menu.sh creates /tmp/stay_awake when a scraping
# starts and removed it only at its normal end, and the scrapers set the
# CPU governor to performance and back to ondemand only at theirs. No
# Wi-Fi, MENU, the terminal closed as soon as the cover was found, a
# console a source does not know: both stayed, and since the scraper is
# started from the game list, not as an app, runtime.sh did not clean up
# until a game or an app had been closed. Until then the console never
# went to standby by itself, and a standby never ended in a power-off.
rm -f /tmp/stay_awake
echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# background scraping :
if [ -f /tmp/scraper_script.sh ]; then
    chmod a+x /tmp/scraper_script.sh
    # The governor again once the job has ended, however it ended
    (
        sh /tmp/scraper_script.sh
        echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
    ) &
fi

# Non-zero: GLO goes back to the game list. Zero would start the game.
exit 1
