#!/bin/sh

SYSDIR="/mnt/SDCARD/.tmp_update"

# ProfileManager cannot use shellect from MainUI app context (no TTY available)
# Show informational message and provide CLI instructions instead

"$SYSDIR/bin/infoPanel" \
    --title "Profile Manager" \
    --message "Profile management requires terminal access.\n\nTo manage profiles, use the command line:\n\n1. Connect via SSH/serial\n2. Run: /mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh\n\nAvailable commands:\n- list: Show all profiles\n- switch <name>: Switch profile\n- create <name> <type>: Create profile\n- delete <name>: Delete profile\n- info <name>: Show profile info\n\nOr use: profile_cli.sh help\n\nPress A to close." \
    --ok-btn "Close"

exit 0

