#!/bin/sh
sysdir=/mnt/SDCARD/.tmp_update
mediadir=/mnt/SDCARD/Media/PDF

while :
do
	# The list writes the file chosen with A. START or MENU leaves the list
	# without writing one, so a choice left from the last run must not count:
	# the terminal now ends with 143 (SIGTERM), not 137, and the old test
	# reopened the last file instead of leaving.
	rm -f "$mediadir/.selected.pls"
	cd $sysdir
	./bin/st -q -e "$sysdir/script/file_selector.sh" "PDF Reader" "$mediadir" "pdf" "A: open, START: exit, MENU: close a PDF."
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

	echo "$selected_file"
	LD_LIBRARY_PATH="$sysdir/lib/parasyte:$LD_LIBRARY_PATH" $sysdir/bin/green/green "$selected_file"
done
