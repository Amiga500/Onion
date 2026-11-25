#!/bin/sh
sysdir=/mnt/SDCARD/.tmp_update
mediadir=/mnt/SDCARD/Media/Videos
appdir=/mnt/SDCARD/App/FFplay

while :
do
	# The list writes the file chosen with A. START or MENU leaves the list
	# without writing one, so a choice left from the last run must not count:
	# the terminal now ends with 143 (SIGTERM), not 137, and the old test
	# reopened the last file instead of leaving.
	rm -f "$mediadir/.selected.pls"
	cd $sysdir
	# New UI: the list drawn natively (prompt); B goes up a folder, MENU
	# leaves. LIST_TERMINAL=1: the terminal list as before.
	if [ -z "$LIST_TERMINAL" ]; then
		SHELLECT_NATIVE=1 sh "$sysdir/script/file_selector.sh" "Video Player" "$mediadir" "3g2 3gp asf avi flv h264 m2t m2ts m4v mkv mod mov mp4 mpg tod vob webm wmv"
	else
		./bin/st -q -e "$sysdir/script/file_selector.sh" "Video Player" "$mediadir" "3g2 3gp asf avi flv h264 m2t m2ts m4v mkv mod mov mp4 mpg tod vob webm wmv"
	fi
	retVal=$?
	
	# No file chosen (START, MENU, or the terminal was killed): leave.
	echo "=================" "$retVal"
	if [ ! -f "$mediadir/.selected.pls" ]; then
		exit
	fi

	selected_file=$(cat "$mediadir/.selected.pls")

	if [ ! -f "$selected_file" ]; then
		continue
	fi

	# attempt to restore resume of other video -> doesn't work
	#(sleep 2 ; mv /mnt/SDCARD/App/FFplay/pos.bck /mnt/SDCARD/App/FFplay/pos.cfg ;) &

	# attempt to avoid resume of other video -> doesn't work, the parameter is not considered
	if [ -f "$appdir/.noResume" ]; then 
		startTimer=" -ss 0"
		rm "$appdir/.noResume"
	fi

	echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

	cd $sysdir
	# No sleep timer while the video plays, but POWER or closing the lid
	# still pause it: stay_awake kept it playing with the screen off (#350).
	touch /tmp/video_playing
	./bin/ffplay -autoexit -vf "hflip,vflip" -i "$selected_file" $startTimer
	rm -f /tmp/video_playing
done
