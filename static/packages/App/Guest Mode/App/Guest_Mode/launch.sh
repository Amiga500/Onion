#!/bin/sh

json_file="/mnt/SDCARD/system.json"
currentThemefile="/mnt/SDCARD/Saves/CurrentProfile/theme/currentTheme"
defautTheme="/mnt/SDCARD/Themes/Silky RedOnion/"

progdir=`cd -- "$(dirname "$0")" >/dev/null 2>&1; pwd -P`
cd $progdir

if [ ! -f /tmp/rtc_available ]; then
  # Save current time
  ./saveTime.sh
fi
sync

# Save current Favourites + RecentList lists
cp /mnt/SDCARD/Roms/*.json /mnt/SDCARD/Saves/CurrentProfile/lists
rm /mnt/SDCARD/Roms/*.json

# Save current theme
jq -r .theme "$json_file" > "$currentThemefile"

# Saves/MainProfile exists only while the guest profile is the current one
# (Kids Mode, the Backup app and this app's install.sh decide the same way).
# Deciding by Saves/GuestProfile went wrong after an update made with Guest
# Mode on, which brought back an empty GuestProfile: the guest's profile was
# moved inside MainProfile and the empty one became the current profile.
if [ -d /mnt/SDCARD/Saves/MainProfile ] ; then
	# The guest profile is the current one
	if [ -d /mnt/SDCARD/Saves/GuestProfile ] ; then
		# Left by such an update (the default settings only): kept aside,
		# or the guest's profile would be moved inside it.
		mv /mnt/SDCARD/Saves/GuestProfile "/mnt/SDCARD/Saves/GuestProfile-unused-$(date +%Y%m%d-%H%M%S)"
	fi
	cp ./data/configOFF.json ./config.json
	mv /mnt/SDCARD/Saves/CurrentProfile /mnt/SDCARD/Saves/GuestProfile
	mv /mnt/SDCARD/Saves/MainProfile /mnt/SDCARD/Saves/CurrentProfile

elif [ -d /mnt/SDCARD/Saves/GuestProfile ] ; then
	# The main profile is the current one
	cp ./data/configON.json ./config.json
	mv /mnt/SDCARD/Saves/CurrentProfile /mnt/SDCARD/Saves/MainProfile
	mv /mnt/SDCARD/Saves/GuestProfile /mnt/SDCARD/Saves/CurrentProfile

else
	# No guest profile on the card (deleted on a computer): switching
	# would leave no current profile at all. Nothing is changed.
	cp /mnt/SDCARD/Saves/CurrentProfile/lists/*.json /mnt/SDCARD/Roms 2> /dev/null
	infoPanel --title "Guest Mode" --message "No guest profile found\n(Saves/GuestProfile).\nNothing was changed."
	exit 1
fi

# Favourites + RecentList restoration
cp /mnt/SDCARD/Saves/CurrentProfile/lists/*.json /mnt/SDCARD/Roms

# Default theme for a profile without one (a new Guest profile), or whose
# theme is gone (Silky by DiMo, the default before RedOnion). This wrote
# "$themeString", never set: the theme became "", and MainUI, the boot logo
# and Tweaks fell back to the stock Onion skin in /mnt/SDCARD/miyoo/app/.
mkdir -p "$(dirname "$currentThemefile")"
saved_theme="$(cat "$currentThemefile" 2>/dev/null)"
if [ -z "$saved_theme" ] || [ ! -d "$saved_theme" ]; then
	echo "$defautTheme" > "$currentThemefile"
fi

# Theme restoration
if [ -e "$currentThemefile" ]; then
	jq --arg theme "$(cat "$currentThemefile")" '.theme = $theme' "$json_file" > temp.json
	mv temp.json "$json_file"
fi
themeSwitcher --reapply_icons

if [ ! -f /tmp/rtc_available ]; then
  # Load current time
  ./loadTime.sh
fi
