#!/bin/sh
scriptlabel="Scraper (%LIST%)"
scriptinfo="Launches the scraper\nfor the selected system."
require_networking=1
echo $0 $*

sysdir=/mnt/SDCARD/.tmp_update
rm -f /tmp/scraper_script.sh

# MENU closes the terminal. pressMenu2Kill runs "pkill -f", so the pattern
# must match the terminal only: "st" alone also matched
# game_list_options.sh ("list") and killed the GLO with it.
pressMenu2Kill bin/st &

cd $sysdir
#./bin/st -q -e 	"/mnt/SDCARD/scrap_screenscraper.sh" "MD"   # quick alternative
./bin/st -q -e "$sysdir/script/scraper/menu.sh" "$1" "$2"

pkill -9 pressMenu2Kill


# background scraping :
if [ -f /tmp/scraper_script.sh ]; then
    chmod a+x /tmp/scraper_script.sh
    sh /tmp/scraper_script.sh &
fi

# Non-zero: GLO goes back to the game list. Zero would start the game.
exit 1
