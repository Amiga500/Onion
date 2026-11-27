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

# New UI: what the updater is doing, while there is nothing to answer
# (OTA_NATIVE: an info panel; in the terminal the lines say it).
ui_busy() { :; }

# Repository name :
GITHUB_REPOSITORY=Amiga500/Onion

# The testers' builds have a repository of their own: the beta channel takes
# the newest release of GITHUB_REPOSITORY, in the updaters already installed
# too, so a testers' build released there would reach every beta user. The
# "testers" channel shows only with config/.otaTesters (an empty file).
TESTERS_REPOSITORY=Amiga500/RedOnion-testers

# Packages are named <project>-v<version>.zip. The project is becoming
# RedOnion; earlier releases are OnionPlus, and releases will carry both for a
# while, since updaters before this one look for OnionPlus-v only. Prefixes in
# order of preference.
PACKAGE_PREFIXES="RedOnion-v OnionPlus-v"

# channel : stable, beta or testers (only with config/.otaTesters: without
# it, beta)
channel=$(cat "$sysdir/config/ota_channel" 2> /dev/null)
case $channel in
	stable | beta) ;;
	testers) [ -f "$sysdir/config/.otaTesters" ] || channel=beta ;;
	*) channel=stable ;;
esac

main() {
	if [ "$cmd" = "check" ]; then
		IP=$(ip route get 1 2> /dev/null | awk '{print $NF;exit}')
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
	trap 'stop_download; restore_wifi; restore_keys' EXIT
	trap 'exit 130' INT TERM HUP
	quiet_keys

	ui_busy "Checking the Wi-Fi and the connection..."
	check_available_space
	check_wifi_hardware
	wait_boot_network
	enable_wifi
	check_connection
	run_bootstrap

	sleep 2
	channel_choice

	ui_busy "Looking for updates on the $channel channel..."
	get_release_info
	case $? in
		0)
			touch "$sysdir/.updateAvailable"
			;;
		1)
			rm -f "$sysdir/.updateAvailable"
			echo -ne "${YELLOW}"
			press_a "Press A to exit"
			exit 3
			;;
		*)
			echo -ne "${YELLOW}"
			press_a "Press A to exit"
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
		press_a "Press A to exit"
		exit 1
	fi
}

# The Miyoo Mini (283) has no Wi-Fi: say so instead of waiting for a
# network that can't come.
check_wifi_hardware() {
	if [ "$(cat /tmp/deviceModel 2> /dev/null)" = "283" ]; then
		echo -e "${RED}This device has no Wi-Fi.${NC}\nTo update, copy the release to the SD card from a PC.\n"
		echo -ne "${YELLOW}"
		press_a "Press A to exit"
		exit 9
	fi
}

wifi_setting_on() {
	# Same test as wifi_enabled in update_networking.sh.
	[ "$(/customer/app/jsonval wifi 2> /dev/null)" -eq 1 ] 2> /dev/null
}

# The system's Wi-Fi is up or connecting. Decided by the process, not by
# the setting in system.json: that can say on while Wi-Fi is off (a stale
# copy saved back by another program), and the updater then waited for a
# connection that never came instead of turning Wi-Fi on.
# wlan0 must exist too: just after the temporary Wi-Fi of the time sync is
# turned off, the killed wpa_supplicant can still be listed for a moment
# (as "[wpa_supplicant]", without a command line).
wifi_radio_running() {
	wlan0_present && pgrep -f "wpa_supplicant.*wlan0" > /dev/null 2>&1
}

wlan0_present() {
	[ -e /sys/class/net/wlan0 ]
}

# After "axp_test wifion" the Wi-Fi chip appears on USB after a variable
# delay. A fixed 2 s was sometimes too short on the Mini Plus: wlan0 did not
# exist yet, wpa_supplicant failed ("No such device") and no address came.
wait_for_wlan0() { # seconds
	i=0
	while ! wlan0_present && [ $i -lt $(($1 * 2)) ]; do
		sleep 0.5
		i=$((i + 1))
	done
	wlan0_present
}

start_wpa_supplicant() {
	/mnt/SDCARD/miyoo/app/wpa_supplicant -B -D nl80211 -iwlan0 -c /appconfigs/wpa_supplicant.conf
}

wlan0_linked() {
	[ "$(cat /sys/class/net/wlan0/operstate 2> /dev/null)" = "up" ]
}

# udhcpc started before wpa_supplicant has joined the network sends its
# first requests into the void and then pauses 20 s before trying again.
# Start it once the link is up.
wait_for_link() { # seconds
	i=0
	while ! wlan0_linked && [ $i -lt $(($1 * 2)) ]; do
		sleep 0.5
		i=$((i + 1))
	done
	wlan0_linked
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

# Just after start-up the network comes up in the background
# (update_networking.sh: Wi-Fi, time sync, "Enable Wi-Fi temporarily" that
# turns Wi-Fi off again at the end). Wait for it to finish before touching
# Wi-Fi: started meanwhile, the updater ran a second wpa_supplicant, and the
# temporary Wi-Fi could be switched off in the middle of the download.
wait_boot_network() {
	pgrep -f update_networking.sh > /dev/null 2>&1 || return 0
	network_ready && return 0
	echo "Waiting for the network start-up..."
	i=0
	while [ $i -lt 45 ] && pgrep -f update_networking.sh > /dev/null 2>&1 &&
		! network_ready; do
		sleep 1
		i=$((i + 1))
	done
	clear
}

# Connected, with Wi-Fi on in Settings: nothing left to wait for, even if
# update_networking.sh still runs (the time sync after Wi-Fi came on).
# Connected with Wi-Fi off in Settings is the temporary Wi-Fi of the time
# sync, which update_networking.sh turns off again: that one is waited for.
network_ready() {
	IP=$(ip route get 1 2> /dev/null | awk '{print $NF;exit}')
	[ -n "$IP" ] && wifi_setting_on
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
	if [ "$IP" = "" ] && wifi_radio_running; then
		# On but not connected yet (just after start-up, or reconnecting):
		# wait for it instead of starting a second wpa_supplicant and
		# killing the system's udhcpc.
		echo "Waiting for Wi-Fi..."
		wait_for_ip 20
		clear
	elif [ "$IP" = "" ]; then
		echo "Wifi is disabled - trying to enable it..."
		wifi_started_here=1
		insmod /mnt/SDCARD/8188fu.ko 2> /dev/null
		ifconfig lo up
		/customer/app/axp_test wifion
		wait_for_wlan0 10
		# wlan0 appears before the driver is ready: wpa_supplicant started
		# straight away ran but never joined the network. The system waits
		# 2 s after the power-on (wifi_on in update_networking.sh).
		sleep 2
		ifconfig wlan0 up
		# Power save as wifi_on in update_networking.sh sets it.
		$sysdir/script/network/wifi_power_save.sh auto
		# Same commands as wifi_on in update_networking.sh. wpa_supplicant is
		# not on the PATH of apps, and udhcpc in the foreground never returns
		# without a network: the updater stayed on a black screen.
		echo "Connecting (this can take up to a minute)..."
		pkill -9 wpa_supplicant 2> /dev/null
		start_wpa_supplicant
		# Joining took 30-60 s on a Mini Plus 3 m from the router: the
		# driver scans for a while before it associates.
		wait_for_link 50
		pkill -9 udhcpc 2> /dev/null
		udhcpc -i wlan0 -s /etc/init.d/udhcpc.script > /dev/null 2>&1 &
		# Once joined the address comes in a few seconds; up to 20 s,
		# then check_connection reports a failure.
		wait_for_ip 20
		clear
	fi
}

# curl, as for the release list: the bundled wget (uclient-fetch on mbed TLS
# 2.2.1, from 2015) timed out on github.com while curl (OpenSSL 3) reached it.
# The certificate is not checked (-k), as it is not for the release list or
# the download. Checked, it failed on a working network when the clock was
# wrong (a device without a clock battery, before the time sync): "not
# reachable" stopped the update. The reason curl gives is shown with the error.
# Headers only (-I): on some home networks the github.com page itself stalled
# after the HTTP 200 until the 30 s limit (curl error 28), while a phone
# hotspot worked (#311, thanks Nyiz). The check only needs the answer.
check_connection() {
	echo -n "Checking internet connection... "
	_why=$(curl -k -sS -I -L -f --connect-timeout 15 --max-time 30 -o /dev/null https://github.com 2>&1)
	_rc=$?
	# A server answering HEAD with an HTTP error (22): ask for the page, compressed.
	if [ $_rc -eq 22 ]; then
		_why=$(curl -k -sS -L -f --compressed --connect-timeout 15 --max-time 30 -o /dev/null https://github.com 2>&1)
		_rc=$?
	fi
	if [ $_rc -eq 0 ]; then
		echo -e "${GREEN}OK${NC}"
	else
		_why=$(echo "$_why" | grep -v '^[[:space:]]*$' | head -n 1 | cut -c 1-60)
		echo -e "${RED}FAIL${NC}\nError: https://github.com not reachable${_why:+ ($_why)}.\nTurn on Wi-Fi in Settings and check that it connects.\n"
		echo -ne "${YELLOW}"
		press_a "Press A to exit"
		exit 2
	fi
}

# The bootstrap is read from the OnionPlus branch, which every updater uses:
# keep that branch and file. Run only if a script came back: an error page
# (a renamed repository without -L, a missing file) used to be run by sh.
run_bootstrap() {
	_bootstrap=$(curl -k -s -L -f --compressed --connect-timeout 15 --max-time 30 "https://raw.githubusercontent.com/$GITHUB_REPOSITORY/OnionPlus/static/build/.tmp_update/script/ota_bootstrap.sh")
	case "$_bootstrap" in
		'#!/bin/sh'*) echo "$_bootstrap" | sh ;;
	esac
}

channel_choice() {
	_channels="stable\nbeta"
	[ -f "$sysdir/config/.otaTesters" ] && _channels="$_channels\ntesters"
	# The cursor starts on the saved channel: it always started on stable,
	# so a tester pressing A at once got the stable channel.
	case $channel in
		beta) _start=2 ;;
		testers) [ -f "$sysdir/config/.otaTesters" ] && _start=3 || _start=1 ;;
		*) _start=1 ;;
	esac
	ota_menu "$_channels" "Update channel" "$_start"
	channel=$menu_choice
	clear
	# Nothing chosen (B): leave, keeping the saved channel. An empty
	# ota_channel used to be saved and read as stable.
	case $channel in
		stable | beta) ;;
		testers) [ -f "$sysdir/config/.otaTesters" ] || exit 0 ;;
		*) exit 0 ;;
	esac
	echo "$channel" > "$sysdir/config/ota_channel"
}

# Returns 0 when an update is available, 1 when there is none, 2 when GitHub
# didn't answer (no network, rate limit): that used to read as "up to date".
get_release_info() {
	echo -n "Retrieving release information... "

	# Github source api url
	if [ "$channel" = "beta" ] || [ "$channel" = "testers" ]; then
		# The newest published build, prerelease or not: beta is never behind
		# stable. Taking prereleases only left beta with nothing at all, since
		# pre-release.yml publishes every build with prerelease:false.
		# Picked by publish time: GitHub doesn't list releases newest first
		# (a stable release can come before a newer beta).
		# testers: the same, from the testers' repository.
		# -L: a renamed repository answers with a redirect.
		_repo=$GITHUB_REPOSITORY
		[ "$channel" = "testers" ] && _repo=$TESTERS_REPOSITORY
		Release_list=$(curl -k -s -L --compressed --connect-timeout 15 --max-time 60 https://api.github.com/repos/$_repo/releases)
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
		Release_assets_info=$(curl -k -s -L --compressed --connect-timeout 15 --max-time 60 https://api.github.com/repos/$GITHUB_REPOSITORY/releases/latest)
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

	# The first prefix the release has a package for (jq 1.6 on the device:
	# no regular expressions needed).
	Release_asset=
	for _prefix in $PACKAGE_PREFIXES; do
		Release_asset=$(echo "$Release_assets_info" | jq --arg p "$_prefix" '[.assets[]? | select(.name | startswith($p) and endswith(".zip"))] | .[0] // empty')
		[ -n "$Release_asset" ] && break
	done

	if [ -z "$Release_asset" ]; then
		release_info_error "no RedOnion package in the release"
		return 2
	fi

	Release_url=$(echo $Release_asset | jq '.browser_download_url' | tr -d '"')
	Release_FullVersion=$(echo $Release_asset | jq -r '.name' | sed -e "s/^$_prefix//" -e 's/\.zip$//')
	Release_Version=$(echo $Release_FullVersion | sed 's/-.*$//g')
	Release_size=$(echo $Release_asset | jq -r '.size')
	Release_size_MB=$(echo "$(($Release_size / 1024 / 1024))MB")
	Release_Date=$(echo $Release_asset | jq -r '.created_at')
	Release_info=$(echo $Release_assets_info | jq '.body')

	Current_FullVersion=$(installUI --version)
	Current_Version=$(echo $Current_FullVersion | sed 's/-.*$//g')

	echo -e "${GREEN}DONE${NC}"

	echo -ne "\n\n" \
		"${RED}======= Installed Version ========${NC}\n" \
		" Version: $Current_FullVersion \n" \
		"${RED}==================================${NC}\n"
	echo -ne "\n\n" \
		"${RED}======== Online Version  =========${NC}\n" \
		" Version: $Release_FullVersion \n" \
		" Channel: $channel \n" \
		" Size:    $Release_size_MB \n" \
		" Date:    $Release_Date \n" \
		" URL:     $Release_url \n" \
		"${RED}==================================${NC}\n\n\n"

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

# Buttons pressed while the updater works wait in the terminal. They used to
# show as ^[ and answer the next prompt: a MENU pressed for a screenshot
# closed the updater at the next menu, A pressed a few times skipped screens
# (thanks kicksoda). Echo stays off, every prompt drops them first, and MENU
# asks before leaving, with START to leave and A to stay, so a second MENU
# (another screenshot) doesn't answer.
quiet_keys() {
	[ -n "$keys_stty" ] || keys_stty=$(stty -g 2> /dev/null)
	stty -echo 2> /dev/null
}

restore_keys() {
	[ -n "$keys_stty" ] && stty "$keys_stty" 2> /dev/null
}

drop_keys() {
	_dk=$(stty -g 2> /dev/null) || return 0
	stty -icanon min 0 time 0 2> /dev/null
	while [ -n "$(dd ibs=64 count=1 2> /dev/null)" ]; do :; done
	stty "$_dk" 2> /dev/null
}

# Read one button into $button: A, MENU, START, other, or gone (no input:
# the terminal is gone, or a read that waits only a while timed out). MENU
# reaches the terminal as a lone Esc, the D-pad as Esc [ A..D in one go, so a
# byte following within 0.1 s means it was not MENU.
read_button() {
	_key=$(dd ibs=1 count=1 2> /dev/null; echo .)
	_key=${_key%.}
	case $_key in
		'') button=gone ;;
		' ') button=A ;;
		"$NL" | "$CR") button=START ;;
		"$ESC")
			_rb=$(stty -g 2> /dev/null)
			[ -n "$_rb" ] && stty min 0 time 1 2> /dev/null
			_rest=$(dd ibs=1 count=4 2> /dev/null)
			[ -n "$_rb" ] && stty "$_rb" 2> /dev/null
			if [ -z "$_rest" ]; then button=MENU; else button=other; fi
			;;
		*) button=other ;;
	esac
}
ESC=$(printf '\033')
CR=$(printf '\r')
NL='
'

# Ask a yes/no question about leaving: START answers yes (returns 0), A no
# (returns 1); other buttons, MENU too, are ignored. No input at all (the
# terminal is gone) is yes.
ask_leave() { # question
	drop_keys
	# While the onion is on screen ($ask_row set) the question goes in its
	# row under the onion and is cleared after, so the drawing stays put.
	if [ -n "$ask_row" ]; then
		printf "\033[%d;1H\033[K" "$ask_row"
	else
		printf "\n"
	fi
	keys_red "$1 START: yes, A: no"
	_al=$(stty -g 2> /dev/null)
	stty -icanon -echo min 1 time 0 2> /dev/null
	while :; do
		read_button
		case $button in
			A) _al_ret=1 && break ;;
			START | gone) _al_ret=0 && break ;;
		esac
	done
	[ -n "$_al" ] && stty "$_al" 2> /dev/null
	if [ -n "$ask_row" ]; then
		printf "\033[%d;1H\033[K" "$ask_row"
	else
		printf '\n'
	fi
	return $_al_ret
}

# Prompts in the menus' style (shellect.sh): the button names (A, MENU,
# START: ...) in red, the rest as it is. One line of single-spaced words.
keys_red() { # text
	_kr_sep=
	# No globbing while splitting the words; the caller's setting after.
	case $- in *f*) _kr_glob= ;; *) _kr_glob=1 ;; esac
	set -f
	for _kr_w in $1; do
		printf '%s' "$_kr_sep"
		_kr_sep=' '
		_kr_key=${_kr_w%[,.]}
		case $_kr_key in
			A | A: | B | B: | X | X: | Y | Y: | MENU | MENU: | START | START: | SELECT | SELECT: | L1 | L1: | R1 | R1: | Enter | Enter:)
				printf '\033[31;1m%s\033[0m%s' "$_kr_key" "${_kr_w#"$_kr_key"}" ;;
			*) printf '%s' "$_kr_w" ;;
		esac
	done
	[ -n "$_kr_glob" ] && set +f
	return 0
}

# "Press A to exit": wait for A only (any button used to do; a D-pad press
# left the updater). No input at all (the terminal is gone) goes on too.
press_a() { # prompt
	drop_keys
	keys_red "$1"
	_pa=$(stty -g 2> /dev/null)
	stty -icanon -echo min 1 time 0 2> /dev/null
	while :; do
		read_button
		case $button in A | gone) break ;; esac
	done
	[ -n "$_pa" ] && stty "$_pa" 2> /dev/null
}

# Show a prompt and wait for A (continue, returns 0). MENU asks whether to
# leave (returns 1 if so, else shows the prompt again); other buttons are
# ignored. No input at all (the terminal is gone) leaves.
wait_key() { # prompt
	drop_keys
	keys_red "$1"
	_stty=$(stty -g 2> /dev/null)
	stty -icanon -echo min 1 time 0 2> /dev/null
	_ret=
	while [ -z "$_ret" ]; do
		read_button
		case $button in
			A) _ret=0 ;;
			gone) _ret=1 ;;
			MENU)
				if ask_leave "Exit the updater?"; then
					_ret=1
				else
					keys_red "$1"
				fi
				;;
		esac
	done
	[ -n "$_stty" ] && stty "$_stty" 2> /dev/null
	return $_ret
}

# A menu of the items (\n between them) in $menu_choice. MENU asks before
# leaving the updater; A to stay shows the menu again. shellect turns echo
# back on when it ends.
ota_menu() { # items, title[, the item to start on: 1 when not given]
	while :; do
		drop_keys
		menu_choice=$(echo -e "$1" | $sysdir/script/shellect.sh -m -s "${3:-1}" -t "--== $(echo "$2" | tr a-z A-Z) ==--" -b "Menu: Exit      A: Validate")
		_rc=$?
		quiet_keys
		[ $_rc -eq 2 ] || return 0
		clear
		ask_leave "Exit the updater?" && exit 0
	done
}

# Download a URL to a file with curl in the background, reading the buttons
# meanwhile: MENU pauses the download and asks, START then stops it (returns
# 130, the partial file removed), A goes on. Other buttons do nothing, and
# none are left over for the next prompt (see quiet_keys). Without a
# terminal (no stty) it just waits. Returns curl's exit code.
# ---- the download progress: an onion filling up ----
# The onion of the RedOnion logo (sprouts, a narrow neck, a round body,
# roots), 20 columns wide and centred, filled in red from the bottom as the
# download goes. Under it: a blank row, the percent, a blank row and 4 rows
# for the question (MENU), the result or an error with its "Press A" (the
# updater's messages take up to 3: one more is kept free, so nothing scrolls
# the onion off the top). The tall onion (18 rows) takes the rows left over,
# up to 4 more body rows, or gives up to 2 (16 rows: the 25 rows of the
# terminal's 16 px font); a compact one (12 rows) on a shorter screen. The
# Miyoo terminal is 29 x 53: a 20-row onion. Without stty: 29 x 53.
onion_layout() {
	onion_cols=$(stty size 2> /dev/null | awk '{ print $2 }')
	onion_lines=$(stty size 2> /dev/null | awk '{ print $1 }')
	case $onion_cols in '' | *[!0-9]*) onion_cols=53 ;; esac
	case $onion_lines in '' | *[!0-9]*) onion_lines=29 ;; esac
	onion_pad=$(((onion_cols - 20) / 2))
	[ "$onion_pad" -lt 0 ] && onion_pad=0
	onion_top=3
	onion_tall=1
	onion_mid=5
	onion_extra=$((onion_lines - onion_top - 18 - 6))
	if [ "$onion_extra" -lt -2 ]; then
		onion_tall=0
		onion_rows=12
	else
		[ "$onion_extra" -gt 4 ] && onion_extra=4
		onion_mid=$((5 + onion_extra))
		onion_rows=$((18 + onion_extra))
	fi
	onion_status_row=$((onion_top + onion_rows + 1)) # percent and MB
	onion_msg_row=$((onion_status_row + 2))          # first of the 4 rows
}

# Draws the onion filled from the bottom up to $1 percent.
onion_draw() { # percent (0-100)
	awk -v pct="$1" -v tall="$onion_tall" -v mid="$onion_mid" -v pad="$onion_pad" -v top="$onion_top" \
		-v red="$(printf '\033[1;31m')" -v green="$(printf '\033[1;32m')" \
		-v white="$(printf '\033[1;37m')" -v root="$(printf '\033[0;33m')" \
		-v nc="$(printf '\033[0m')" -v q="'" '
	BEGIN {
		n = 0
		a[++n] = "          ,/"
		a[++n] = "        \\ |/"
		a[++n] = "        (:)"
		if (tall) {
			a[++n] = "        /::\\"
			a[++n] = "       /::::\\"
			a[++n] = "     .`::::::`."
			a[++n] = "   .`::::::::::`."
			a[++n] = "  /::::::::::::::\\"
			for (r = 0; r < mid; r++) a[++n] = " |::::::::::::::::|"
		}
		else {
			a[++n] = "       /::::\\"
			a[++n] = "   .`::::::::::`."
			a[++n] = "  /::::::::::::::\\"
			for (r = 0; r < 2; r++) a[++n] = " |::::::::::::::::|"
		}
		a[++n] = "  \\::::::::::::::/"
		a[++n] = "   `.::::::::::.`"
		a[++n] = "     `-.::::.-`"
		a[++n] = "       /|||\\"
		if (tall) a[++n] = "     / /|||\\ \\"
		# ` stands for the quote (no quote inside this shell string)
		for (i = 1; i <= n; i++) gsub(/`/, q, a[i])
		inside = 0
		first = 0
		for (i = 1; i <= n; i++) if (index(a[i], ":")) { inside++; if (!first) first = i; last = i }
		if (pct < 0) pct = 0
		if (pct > 100) pct = 100
		filled = int(pct * inside / 100)
		k = 0
		sp = sprintf("%" pad "s", "")
		for (i = 1; i <= n; i++) {
			line = a[i]
			if (i < first) colour = green
			else if (i > last) colour = root
			else colour = white
			if (index(line, ":")) {
				k++
				full = (k > inside - filled)
				out = ""
				for (j = 1; j <= length(line); j++) {
					c = substr(line, j, 1)
					# No "out (...)": BusyBox awk reads a name followed by
					# "(" as a function call ("call to undefined function").
					if (c == ":") {
						if (full) out = out red "#"
						else out = out " "
					}
					else out = out colour c
				}
				line = out
			}
			else line = colour line
			printf "\033[%d;1H\033[K%s%s%s", top + i - 1, sp, line, nc
		}
	}'
}

# onion_pct: the percent of done bytes in total bytes, 0-100 (0 for a
# value that is not a number). Counted in KB: bytes * 100 passes 2^31
# after 21 MB, and the shell's arithmetic may be 32-bit.
onion_percent() { # done bytes, total bytes
	onion_pct=0
	case "$1$2" in '' | *[!0-9]*) return ;; esac
	[ -n "$1" ] && [ -n "$2" ] || return
	_op_t=$(($2 / 1024))
	[ "$_op_t" -gt 0 ] || return
	onion_pct=$(($1 / 1024 * 100 / _op_t))
	[ "$onion_pct" -gt 100 ] && onion_pct=100
	return 0
}

# The percent and MB under the onion, centred.
onion_status() { # done bytes, total bytes
	onion_percent "$1" "$2"
	_pct=$onion_pct
	_mb=$(awk -v d="$1" -v t="$2" 'BEGIN { printf "%.1f / %.1f MB", d / 1048576, t / 1048576 }')
	_line=$(printf '%3d%%   %s' "$_pct" "$_mb")
	_sp=$(((onion_cols - ${#_line}) / 2))
	[ "$_sp" -lt 0 ] && _sp=0
	printf "\033[%d;1H\033[K%${_sp}s%s" "$onion_status_row" "" "$_line"
}

download_file() { # url, file, expected size (bytes)
	# Quiet: the onion shows the progress (curl's own bar scrolled it).
	curl -k -L -f -s --connect-timeout 15 --retry 3 --retry-delay 2 \
		-o "$2" "$1" < /dev/null &
	_dl_pid=$!
	_dl_last=-1
	_dl_stty=$(stty -g 2> /dev/null)
	# No echo (the buttons garbled the progress bar); a read gives up after
	# 0.5 s, so the loop sees the download end.
	[ -n "$_dl_stty" ] && stty -icanon -echo min 0 time 5 2> /dev/null
	_dl_keys=1
	while [ -n "$_dl_keys" ] && kill -0 "$_dl_pid" 2> /dev/null; do
		_dl_done=$(stat -c %s "$2" 2> /dev/null || echo 0)
		onion_percent "$_dl_done" "$3"
		_dl_fill=$onion_pct
		if [ "$_dl_fill" -ne "$_dl_last" ]; then
			onion_draw "$_dl_fill"
			_dl_last=$_dl_fill
		fi
		onion_status "$_dl_done" "$3"
		read_button
		case $button in
			gone)
				# A terminal timed out; anything else is at the end of its input.
				[ -n "$_dl_stty" ] || _dl_keys=
				continue
				;;
			MENU) ;;
			*) continue ;;
		esac
		kill -STOP "$_dl_pid" 2> /dev/null
		if ask_leave "Stop the download?"; then
			stop_download
			rm -f "$2"
			[ -n "$_dl_stty" ] && stty "$_dl_stty" 2> /dev/null
			return 130
		fi
		kill -CONT "$_dl_pid" 2> /dev/null
	done
	wait "$_dl_pid"
	_dl_rc=$?
	_dl_pid=
	if [ "$_dl_rc" -eq 0 ]; then
		_dl_done=$(stat -c %s "$2" 2> /dev/null || echo 0)
		onion_draw 100
		onion_status "$_dl_done" "$3"
	fi
	if [ -n "$_dl_stty" ]; then
		# Drop the buttons pressed meanwhile.
		stty min 0 time 0 2> /dev/null
		while [ -n "$(dd ibs=64 count=1 2> /dev/null)" ]; do :; done
		stty "$_dl_stty" 2> /dev/null
	fi
	return $_dl_rc
}

# End a download still running (stopped, or the updater leaving).
stop_download() {
	[ -n "$_dl_pid" ] || return 0
	kill "$_dl_pid" 2> /dev/null
	kill -CONT "$_dl_pid" 2> /dev/null
	wait "$_dl_pid" 2> /dev/null
	_dl_pid=
}

download_update() {
	echo -ne "${YELLOW}"
	wait_key "Press A to continue, MENU to exit" || exit 0
	echo -ne "${NC}"

	ota_menu "No\nYes" "Download $Release_Version ($Release_size_MB)?"
	Mychoice=$menu_choice
	clear
	if [ "$Mychoice" = "Yes" ]; then

		# No file system repair here: fsck.fat -a on the mounted SD card (the
		# system runs from it) can turn clusters of open or unsynced files into
		# FSCKxxxx.REC fragments. Flush pending writes instead; damaged cards are
		# better repaired on a PC.
		sync

		mkdir -p $sysdir/download/
		/mnt/SDCARD/.tmp_update/bin/freemma > /dev/null
		sync
		# The terminal's screen (in the new UI the download has its own, and
		# these lines went on to show up in its error messages)
		if [ -z "$OTA_NATIVE" ]; then
			onion_layout
			clear
			printf '\033[?25l' # no cursor over the onion
			printf "${RED}== Downloading RedOnion $Release_Version ($channel) ==${NC}"
			printf "\033[%d;1H" "$((onion_msg_row + 2))"
			keys_red "MENU: stop the download"
		fi
		ask_row=$onion_msg_row
		# curl, which reaches GitHub where the bundled wget may not (see
		# check_connection); wget only if curl itself cannot run.
		download_file "$Release_url" "$sysdir/download/$Release_Version.zip" "$Release_size"
		_dl_result=$?
		ask_row=
		# The rows under the onion, cleared for the result.
		[ -z "$OTA_NATIVE" ] && printf "\033[%d;1H\033[J\033[?25h" "$onion_msg_row"
		case $_dl_result in
			126 | 127)
				clear
				wget --no-check-certificate "$Release_url" -O "$sysdir/download/$Release_Version.zip"
				;;
			130)
				echo -e "${YELLOW}Download stopped.${NC}"
				sleep 1
				exit 0
				;;
		esac
		sync
	else
		exit 4
	fi

	Downloaded_size=$(stat -c %s "$sysdir/download/$Release_Version.zip" 2> /dev/null || echo 0)
	# Up to 3 rows: they fit under the onion.
	if [ "$Downloaded_size" -eq "$Release_size" ]; then
		echo -e "${GREEN}Download done, file size OK.${NC}"
		sleep 3
	elif [ "${_dl_result:-0}" -ne 0 ]; then
		# curl runs quietly (-s), so its error is told here.
		echo -e "${RED}Error: download failed (curl error $_dl_result).${NC}"
		echo -e "Check the Wi-Fi connection and try again.\n"
		rm -f "$sysdir/download/$Release_Version.zip"
		echo -ne "${YELLOW}"
		press_a "Press A to exit"
		exit 5
	else
		echo -e "${RED}Error: wrong download size${NC}"
		echo -e "($Downloaded_size instead of $Release_size bytes)\n"
		rm -f "$sysdir/download/$Release_Version.zip"
		echo -ne "${YELLOW}"
		press_a "Press A to exit"
		exit 5
	fi
}

apply_update() {
	ota_menu "No\nYes" "Apply update $Release_Version?"
	Mychoice=$menu_choice
	clear
	if [ "$Mychoice" = "Yes" ]; then
		echo "Applying update... "

		umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null
		/mnt/SDCARD/.tmp_update/bin/freemma > /dev/null

		# Not the two boot stubs at the top of the zip (.tmp_update/updater
		# and runtime.sh, the first files extracted): they replaced the
		# installed ones at once, so an extraction cut short (battery,
		# power off, 7z error) left a card that no longer started RedOnion.
		# Without them the installed system still boots until the trigger,
		# miyoo/app/MainUI (near the end), is there; then check_installer
		# in runtime.sh starts the installer, which installs the real ones.
		if [ -n "$OTA_NATIVE" ]; then
			apply_extract "$sysdir/download/$Release_Version.zip"
		else
			7z x -aoa -o"/mnt/SDCARD" "$sysdir/download/$Release_Version.zip" \
				-x!.tmp_update/updater -x!.tmp_update/runtime.sh
		fi

		if [ $? -eq 0 ]; then
			echo -e "${GREEN}Decompression successful.${NC}"
			# Extracted: the package (hundreds of MB) is no longer needed and
			# used to stay on the card.
			rm -f "$sysdir/download/$Release_Version.zip"
			sync
			sleep 3
			echo -ne "\n\n" \
				"Update $Release_Version applied.\n" \
				"Rebooting to run installation...\n\n"
			echo -ne "${YELLOW}"
			press_a "Press A to reboot"
			sleep 1
			# A plain reboot, as in 4.4.0: from 4.5.0, the clean shutdown
			# here could hang on this screen until POWER was held. The
			# installer, which runs next, reboots through the clean
			# shutdown.
			reboot
		else
			echo -ne "\n\n" \
				"${RED}Error: Something wrong happened during decompression.${NC}\n" \
				"Try to run OTA update again or do a manual update.\n\n"
			echo -ne "${YELLOW}"
			press_a "Press A to exit"
			exit 6
		fi
	else
		exit 7
	fi
}

# Build date (YYYYMMDD) of a version such as 4.4.0-beta-20261010-f7c522a5.
# A build made by hand can add a word after the commit (…-a6a42951-readline):
# without its date the check below let an older stable look like an update.
build_date() { echo "$1" | sed -n -e 's/^.*-\([0-9]\{8\}\)-[0-9a-f]\{7,\}$/\1/p' -e 's/^.*-\([0-9]\{8\}\)-[0-9a-f]\{7,\}-[A-Za-z0-9._]*$/\1/p' -e 's/^.*-\([0-9]\{8\}\)$/\1/p' | head -n 1; }

get_version() { echo "$@" | tr -d '[:alpha:]' | awk -F'[.-]' '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }'; }


# ---- New UI ---------------------------------------------------------------
# The OTA app runs the updater outside the terminal with OTA_NATIVE=1
# (OTA_TERMINAL=1: the terminal, as before). The same steps and texts:
# - what is printed between two questions is collected in a file and shown
#   by prompt above the answers (Continue / Exit, Reboot...);
# - the menus are drawn by prompt (shellect.sh's native mode), with
#   "Update" before their title;
# - while it works, an info panel says what it does;
# - the download and the extraction are shown by progressUI.
if [ -n "$OTA_NATIVE" ] && [ "$cmd" != "check" ]; then
	SHELLECT_NATIVE=1
	SHELLECT_PARENT=Update
	SHELLECT_NATIVE_PID=$$
	export SHELLECT_NATIVE SHELLECT_PARENT SHELLECT_NATIVE_PID
	ui_screen=/tmp/ota_screen.txt
	: > "$ui_screen"
	exec > "$ui_screen" 2>&1
	clear() { : > "$ui_screen"; }
	quiet_keys() { :; }
	# (on EXIT too: the info panel must not outlive the updater)
	restore_keys() { ui_unbusy; }
	drop_keys() { :; }

	# The text printed since the last question: no colours, no frame lines.
	ui_message() {
		sed -e "s/${ESC}\[[0-9;?]*[A-Za-z]//g" -e 's/^ *//' -e '/^[ =*-]*$/d' \
			-e 's/^=* *\([A-Za-z][A-Za-z ]*[A-Za-z]\) *=*$/\1/' "$ui_screen" | head -n 14
	}

	ui_busy() { # what it is doing
		ui_unbusy
		rm -f /tmp/dismiss_info_panel
		"$sysdir/bin/infoPanel" -t "Update" -m "$1" --persistent > /dev/null 2>&1 &
		ui_busy_pid=$!
	}

	ui_unbusy() {
		[ -n "$ui_busy_pid" ] || return 0
		touch /tmp/dismiss_info_panel
		wait "$ui_busy_pid" 2> /dev/null
		ui_busy_pid=
		rm -f /tmp/dismiss_info_panel
	}

	# "Press A to exit": the text, and one button with that word.
	press_a() { # prompt
		ui_unbusy
		_word=${1#Press A to }
		_word="$(echo "$_word" | cut -c1 | tr 'a-z' 'A-Z')$(echo "$_word" | cut -c2-)"
		_msg=$(ui_message)
		"$sysdir/bin/prompt" -t "Update" -m "${_msg:- }" "$_word" > /dev/null
		: > "$ui_screen"
	}

	# The release found: Continue (0) or Exit (1, MENU and B too).
	wait_key() { # prompt
		ui_unbusy
		_msg=$(ui_message)
		"$sysdir/bin/prompt" -t "Update" -m "${_msg:- }" "Continue" "Exit" > /dev/null
		_wk=$?
		: > "$ui_screen"
		[ $_wk -eq 0 ]
	}

	# Leaving: yes returns 0. B and MENU answer no.
	ask_leave() { # question
		ui_unbusy
		"$sysdir/bin/prompt" -t "Update" -m "$1" "No" "Yes" > /dev/null
		[ $? -eq 1 ]
	}

	ota_menu() { # items, title[, the item to start on]
		ui_unbusy
		while :; do
			menu_choice=$(echo -e "$1" | $sysdir/script/shellect.sh -m -s "${3:-1}" -t "--== $(echo "$2" | tr a-z A-Z) ==--")
			_rc=$?
			[ $_rc -eq 2 ] || return 0
			ask_leave "Exit the updater?" && exit 0
		done
	}

	# The download on progressUI: the percent, the MB, the speed and the
	# time left. B there asks "Stop now?": stopped, 130 as in the terminal.
	download_file() { # url, file, expected size (bytes)
		ui_unbusy
		curl -k -L -f -s --connect-timeout 15 --retry 3 --retry-delay 2 \
			-o "$2" "$1" < /dev/null &
		_dl_pid=$!
		{
			echo "@@title RedOnion $Release_Version · $channel"
			echo "@@total 100"
			echo "@@autoclose"
			echo "@@note Keep the console charging. Your games and saves are not touched."
			echo "@@tip B stops the download: nothing is changed on the card."
			echo "@@tip When it is done, you are asked before anything is installed."
			echo "@@tip The update is $Release_size_MB: a few minutes on a good Wi-Fi."
			_t0=$(date +%s)
			while kill -0 "$_dl_pid" 2> /dev/null; do
				_d=$(stat -c %s "$2" 2> /dev/null || echo 0)
				onion_percent "$_d" "$3"
				_el=$(($(date +%s) - _t0))
				_mb=$(awk -v d="$_d" -v t="$3" 'BEGIN { printf "%.1f of %.1f MB", d / 1048576, t / 1048576 }')
				_sp=$(awk -v d="$_d" -v s="$_el" 'BEGIN { if (s > 0) printf "%.1f MB/s", d / 1048576 / s; else printf "-" }')
				_left=$(awk -v d="$_d" -v t="$3" -v s="$_el" 'BEGIN { if (d > 0 && s > 0) { l = (t - d) * s / d; printf "%02d:%02d", l / 60, l % 60 } else printf "--:--" }')
				echo "@@item $((onion_pct + 1)) $_mb"
				echo "@@panel 1 DOWNLOADED|$(echo "$_mb" | cut -d' ' -f1) MB|of $(echo "$_mb" | cut -d' ' -f3) MB"
				echo "@@panel 2 SPEED|$_sp|Wi-Fi"
				echo "@@panel 3 TIME LEFT|$_left|about"
				sleep 1
			done
		} | "$sysdir/bin/progressUI" -t Update -i down
		if [ $? -eq 2 ]; then
			stop_download
			rm -f "$2"
			return 130
		fi
		wait "$_dl_pid"
		_dl_rc=$?
		_dl_pid=
		return $_dl_rc
	}

	# The extraction on progressUI, which cannot be stopped there (a card
	# half written does not start); 7z's own result as before.
	apply_extract() { # zip
		# 7z writes to a log, not to the screen: in a pipe into progressUI
		# it was killed by SIGPIPE mid-extraction if the screen went away
		# (SDL failing to start, a crash). The screen follows the log.
		rm -f /tmp/ota_7z_rc
		: > /tmp/ota_7z.log
		(
			7z x -aoa -o"/mnt/SDCARD" "$1" -x!.tmp_update/updater -x!.tmp_update/runtime.sh > /tmp/ota_7z.log 2>&1
			echo $? > /tmp/ota_7z_rc
		) &
		_zpid=$!
		{
			echo "@@title Installing RedOnion $Release_Version"
			echo "@@nostop"
			echo "@@autoclose"
			echo "@@note Don't turn the console off: it restarts when this is done."
			echo "@@tip This takes a few minutes."
			echo "@@tip Then the console restarts to finish the installation."
			# the log's new lines every second (no tail -f: not in every
			# BusyBox), and the last ones once 7z is done
			_shown=0
			while :; do
				_done=0
				[ -f /tmp/ota_7z_rc ] && _done=1
				_lines=$(wc -l < /tmp/ota_7z.log)
				if [ "$_lines" -gt "$_shown" ]; then
					sed -n "$((_shown + 1)),${_lines}p" /tmp/ota_7z.log
					_shown=$_lines
				fi
				[ $_done = 1 ] && break
				sleep 1
			done
		} 2> /dev/null | "$sysdir/bin/progressUI" -t Update -i down
		wait "$_zpid" 2> /dev/null
		_rc=$(cat /tmp/ota_7z_rc 2> /dev/null)
		rm -f /tmp/ota_7z_rc /tmp/ota_7z.log
		return "${_rc:-1}"
	}
fi

main
