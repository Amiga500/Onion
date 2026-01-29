#!/bin/sh

# ProfileManager needs terminal access for shellect to work
# Ensure we have stdin/stdout/stderr connected properly

# Method 1: Try with /dev/tty if available
if [ -c "/dev/tty" ]; then
    /mnt/SDCARD/.tmp_update/script/profiles/profile_menu.sh menu </dev/tty >/dev/tty 2>&1
    exit $?
fi

# Method 2: Try with /dev/console
if [ -c "/dev/console" ]; then
    /mnt/SDCARD/.tmp_update/script/profiles/profile_menu.sh menu </dev/console >/dev/console 2>&1
    exit $?
fi

# Method 3: Fallback - run directly
/mnt/SDCARD/.tmp_update/script/profiles/profile_menu.sh menu

