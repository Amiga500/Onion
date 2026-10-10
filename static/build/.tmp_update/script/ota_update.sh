#!/bin/sh
# OTA updates for Onion.
cmd=$1
sysdir=/mnt/SDCARD/.tmp_update

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
NC='\033[0m' # No Color

# Repository name :
GITHUB_REPOSITORY=Amiga500/Onion

# channel : stable or beta
channel=$(cat "$sysdir/config/ota_channel" 2> /dev/null)
if [ "$channel" = "" ]; then
	channel="stable"
fi

main() {
	if [ "$cmd" = "check" ]; then
		IP=$(ip route get 1 | awk '{print $NF;exit}')
		if [ "$IP" != "" ]; then
			get_release_info
			case $? in
				0)
					touch "$sysdir/.updateAvailable"
					exit 0
					;;
				1)
					# Up to date: drop a flag left from an update since installed.
					rm -f "$sysdir/.updateAvailable"
					;;
			esac
			# 2: GitHub didn't answer, the flag stays as it was.
		fi
		exit 1
	fi

	rm $sysdir/cmd_to_run.sh 2> /dev/null

	# Wi-Fi turned on here goes off again however the updater ends.
	trap restore_wifi EXIT
	trap 'exit 130' INT TERM HUP

	check_available_space
	check_wifi_hardware
	enable_wifi
	check_connection
	run_bootstrap

	sleep 2
	channel_choice

	get_release_info
	case $? in
		0)
			touch "$sysdir/.updateAvailable"
			;;
		1)
			rm -f "$sysdir/.updateAvailable"
			echo -ne "${YELLOW}"
			read -n 1 -s -r -p "Press A to exit"
			exit 3
			;;
		*)
			echo -ne "${YELLOW}"
			read -n 1 -s -r -p "Press A to exit"
			exit 8
			;;
	esac

	download_update
	apply_update
}

check_available_space() {
	# Available space in MB
	mount_point=$(mount | grep -m 1 '/mnt/SDCARD' | awk '{print $1}') # it could be /dev/mmcblk0p1 or /dev/mmcblk0
	available_space=$(df -m $mount_point | awk 'NR==2{print $4}')

	# Check available space
	if [ "$available_space" -lt "1000" ]; then
		echo -e "${RED}Available space is insufficient on SD card${NC}\n"
		echo -ne "${YELLOW}"
		read -n 1 -s -r -p "Press A to exit"
		exit 1
	fi
}

# The Miyoo Mini (283) has no Wi-Fi: say so instead of waiting for a
# network that can't come.
check_wifi_hardware() {
	if [ "$(cat /tmp/deviceModel 2> /dev/null)" = "283" ]; then
		echo -e "${RED}This device has no Wi-Fi.${NC}\nTo update, copy the release to the SD card from a PC."
		echo -ne "${YELLOW}"
		read -n 1 -s -r -p "Press A to exit"
		exit 9
	fi
}

wifi_setting_on() {
	[ "$(/customer/app/jsonval wifi 2> /dev/null)" = "1" ]
}

wait_for_ip() { # seconds
	i=0
	while [ $i -lt "$1" ]; do
		IP=$(ip route get 1 2> /dev/null | awk '{print $NF;exit}')
		[ -n "$IP" ] && return 0
		sleep 1
		i=$((i + 1))
	done
	return 1
}

# Turns Wi-Fi off again when the updater turned it on and it is still off in
# Settings. Nothing else does: the network check only runs after a Wi-Fi
# change, so the radio stayed on (and connected) until the next restart.
restore_wifi() {
	[ "$wifi_started_here" = "1" ] || return 0
	wifi_started_here=0
	wifi_setting_on && return 0
	pkill -9 wpa_supplicant 2> /dev/null
	pkill -9 udhcpc 2> /dev/null
	/customer/app/axp_test wifioff > /dev/null 2>&1
}

enable_wifi() {
	# Enable wifi if necessary
	IP=$(ip route get 1 2> /dev/null | awk '{print $NF;exit}')
	if [ "$IP" = "" ] && wifi_setting_on; then
		# On in Settings but not connected yet (just after start-up, or
		# reconnecting): wait for it instead of starting a second
		# wpa_supplicant and killing the system's udhcpc.
		echo "Waiting for Wi-Fi..."
		wait_for_ip 20
		clear
	elif [ "$IP" = "" ]; then
		echo "Wifi is disabled - trying to enable it..."
		wifi_started_here=1
		insmod /mnt/SDCARD/8188fu.ko 2> /dev/null
		ifconfig lo up
		/customer/app/axp_test wifion
		sleep 2
		ifconfig wlan0 up
		# Same commands as wifi_on in update_networking.sh. wpa_supplicant is
		# not on the PATH of apps, and udhcpc in the foreground never returns
		# without a network: the updater stayed on a black screen.
		/mnt/SDCARD/miyoo/app/wpa_supplicant -B -D nl80211 -iwlan0 -c /appconfigs/wpa_supplicant.conf
		pkill -9 udhcpc 2> /dev/null
		udhcpc -i wlan0 -s /etc/init.d/udhcpc.script > /dev/null 2>&1 &
		# Up to 20 s for an address; check_connection reports a failure.
		wait_for_ip 20
		clear
	fi
}

check_connection() {
	echo -n "Checking internet connection... "
	if wget -q -T 15 --spider https://github.com > /dev/null 2>&1; then
		echo -e "${GREEN}OK${NC}"
	else
		echo -e "${RED}FAIL${NC}\nError: https://github.com not reachable.\nTurn on Wi-Fi in Settings and check that it connects."
		echo -ne "${YELLOW}"
		read -n 1 -s -r -p "Press A to exit"
		exit 2
	fi
}

run_bootstrap() {
	curl -k -s https://raw.githubusercontent.com/Amiga500/Onion/OnionPlus/static/build/.tmp_update/script/ota_bootstrap.sh | sh
}

channel_choice() {
	channel=$(echo -e "stable\nbeta" | $sysdir/script/shellect.sh -t "Select distribution channel:" -b "Press A to validate your choice.")
	clear
	# Nothing chosen (B): leave, keeping the saved channel. An empty
	# ota_channel used to be saved and read as stable.
	if [ "$channel" != "stable" ] && [ "$channel" != "beta" ]; then
		exit 0
	fi
	echo "$channel" > "$sysdir/config/ota_channel"
}

# Returns 0 when an update is available, 1 when there is none, 2 when GitHub
# didn't answer (no network, rate limit): that used to read as "up to date".
get_release_info() {
	echo -n "Retrieving release information... "

	# Github source api url
	if [ "$channel" = "beta" ]; then
		# The newest published build, prerelease or not: beta is never behind
		# stable. Taking prereleases only left beta with nothing at all, since
		# pre-release.yml publishes every build with prerelease:false.
		# Picked by publish time: GitHub doesn't list releases newest first
		# (a stable release can come before a newer beta).
		Release_list=$(curl -k -s https://api.github.com/repos/$GITHUB_REPOSITORY/releases)
		if ! echo "$Release_list" | jq -e 'type == "array"' > /dev/null 2>&1; then
			release_info_error "$Release_list"
			return 2
		fi
		Release_assets_info=$(echo "$Release_list" | jq '[.[] | select(.draft != true)] | sort_by(.published_at) | last')
		if [ -z "$Release_assets_info" ] || [ "$Release_assets_info" = "null" ]; then
			echo -e "${GREEN}DONE${NC}\n\n" \
				"No update available for $channel channel\n"
			return 1
		fi
	else
		Release_assets_info=$(curl -k -s https://api.github.com/repos/$GITHUB_REPOSITORY/releases/latest)
	fi

	if echo "$Release_assets_info" | grep -q '"message": *"Not Found"'; then
		echo -e "${GREEN}DONE${NC}\n\n" \
			"No update available for $channel channel\n"
		return 1
	fi
	if ! echo "$Release_assets_info" | jq -e '.assets' > /dev/null 2>&1; then
		release_info_error "$Release_assets_info"
		return 2
	fi

	Release_asset=$(echo "$Release_assets_info" | jq '.assets[]? | select(.name | contains("OnionPlus-v"))')

	if [ -z "$Release_asset" ]; then
		release_info_error "no OnionPlus package in the release"
		return 2
	fi

	Release_url=$(echo $Release_asset | jq '.browser_download_url' | tr -d '"')
	Release_FullVersion=$(echo $Release_asset | jq '.name' | tr -d "\"" | sed 's/^OnionPlus-v//g' | sed 's/\.zip$//g')
	Release_Version=$(echo $Release_FullVersion | sed 's/-.*$//g')
	Release_size=$(echo $Release_asset | jq -r '.size')
	Release_size_MB=$(echo "$(($Release_size / 1024 / 1024))MB")
	Release_Date=$(echo $Release_asset | jq -r '.created_at')
	Release_info=$(echo $Release_assets_info | jq '.body')

	Current_FullVersion=$(installUI --version)
	Current_Version=$(echo $Current_FullVersion | sed 's/-.*$//g')

	echo -e "${GREEN}DONE${NC}"

	echo -ne "\n\n" \
		"${BLUE}======= Installed Version ========${NC}\n" \
		" Version: $Current_FullVersion \n" \
		"${BLUE}==================================${NC}\n"
	echo -ne "\n\n" \
		"${BLUE}======== Online Version  =========${NC}\n" \
		" Version: $Release_FullVersion \n" \
		" Channel: $channel \n" \
		" Size:    $Release_size_MB \n" \
		" Date:    $Release_Date \n" \
		" URL:     $Release_url \n" \
		"${BLUE}==================================${NC}\n\n\n"

	v1=$(get_version $Current_Version)
	v2=$(get_version $Release_Version)

	if [ $v1 -gt $v2 ] || ([ $v1 -eq $v2 ] && [ "$Current_FullVersion" = "$Release_FullVersion" ]); then
		echo -e "Version is up to date\n"
		return 1
	fi

	# Same version number: builds differ by date (4.4.0-beta-YYYYMMDD-<commit>).
	# An older build, such as the stable release offered to a device on a
	# newer beta, is not an update. Same-day builds are still offered.
	current_date=$(build_date "$Current_FullVersion")
	release_date=$(build_date "$Release_FullVersion")
	if [ $v1 -eq $v2 ] && [ -n "$current_date" ] && [ -n "$release_date" ] &&
		[ "$release_date" -lt "$current_date" ]; then
		echo -e "Installed build is newer ($current_date)\n"
		return 1
	fi

	echo -e "${GREEN}Update available!${NC}\n"
	return 0
}

release_info_error() { # response or reason
	_msg=$(echo "$1" | jq -r '.message? // empty' 2> /dev/null)
	[ -n "$_msg" ] || _msg=$(echo "$1" | tr '\n' ' ' | head -c 80)
	[ -n "$_msg" ] || _msg="no answer"
	echo -e "${RED}FAIL${NC}\n\n" \
		"Error: GitHub didn't answer ($_msg).\n" \
		"Try again in a few minutes.\n"
}

download_update() {
	echo -ne "${YELLOW}"
	read -n 1 -s -r -p "Press A to continue"
	echo -ne "${NC}"

	Mychoice=$(echo -e "No\nYes" | $sysdir/script/shellect.sh -t "Download $Release_Version ($Release_size_MB) ?" -b "Press A to validate your choice.")
	clear
	if [ "$Mychoice" = "Yes" ]; then

		# No file system repair here: fsck.fat -a on the mounted SD card (the
		# system runs from it) can turn clusters of open or unsynced files into
		# FSCKxxxx.REC fragments. Flush pending writes instead; damaged cards are
		# better repaired on a PC.
		sync

		mkdir -p $sysdir/download/
		echo -ne "\n\n" \
			"${BLUE}== Downloading Onion $Release_Version ($channel channel) ==${NC}\n"
		/mnt/SDCARD/.tmp_update/bin/freemma > NUL
		sync
		wget --no-check-certificate $Release_url -O "$sysdir/download/$Release_Version.zip"
		echo -ne "\n\n" \
			"${GREEN}================== Download done ==================${NC}\n"
		sync
		sleep 2
	else
		exit 4
	fi

	Downloaded_size=$(stat -c %s "$sysdir/download/$Release_Version.zip")
	if [ "$Downloaded_size" -eq "$Release_size" ]; then
		echo -e "${GREEN}File size OK!${NC} ($Downloaded_size)"
		sleep 3
	else
		echo -ne "\n\n" \
			"${RED}Error: Wrong download size${NC} ($Downloaded_size instead of $Release_size)\n"
		rm -f "$sysdir/download/$Release_Version.zip"
		echo -ne "${YELLOW}"
		read -n 1 -s -r -p "Press A to exit"
		exit 5
	fi
}

apply_update() {
	Mychoice=$(echo -e "No\nYes" | $sysdir/script/shellect.sh -t "Apply update $Release_Version ?" -b "Press A to validate your choice.")
	clear
	if [ "$Mychoice" = "Yes" ]; then
		echo "Applying update... "

		umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
		/mnt/SDCARD/.tmp_update/bin/freemma > NUL

		# unzip -o "$sysdir/download/$Release_Version.zip" -d "/mnt/SDCARD"
		7z x -aoa -o"/mnt/SDCARD" "$sysdir/download/$Release_Version.zip"

		if [ $? -eq 0 ]; then
			echo -e "${GREEN}Decompression successful.${NC}"
			# Extracted: the package (hundreds of MB) is no longer needed and
			# used to stay on the card.
			rm -f "$sysdir/download/$Release_Version.zip"
			sync
			sleep 3
			echo -ne "\n\n" \
				"Update $Release_Version applied.\n" \
				"Rebooting to run installation...\n"
			echo -ne "${YELLOW}"
			read -n 1 -s -r -p "Press A to reboot"
			sleep 1
			reboot
		else
			echo -ne "\n\n" \
				"${RED}Error: Something wrong happened during decompression.${NC}\n" \
				"Try to run OTA update again or do a manual update.\n"
			echo -ne "${YELLOW}"
			read -n 1 -s -r -p "Press A to exit"
			exit 6
		fi
	else
		exit 7
	fi
}

# Build date (YYYYMMDD) of a version such as 4.4.0-beta-20261010-f7c522a5.
build_date() { echo "$1" | sed -n -e 's/^.*-\([0-9]\{8\}\)-[0-9a-f]\{7,\}$/\1/p' -e 's/^.*-\([0-9]\{8\}\)$/\1/p' | head -n 1; }

get_version() { echo "$@" | tr -d [:alpha:] | awk -F'[.-]' '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }'; }

main
