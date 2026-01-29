#!/bin/sh
# Wrapper for shellect to ensure it has proper terminal access
# This is needed because shellect uses /dev/tty which may not be available
# when launched from MainUI app system

SYSDIR="/mnt/SDCARD/.tmp_update"

# Try to run shellect with different terminal approaches
# Method 1: Direct execution (works if /dev/tty is available)
if [ -c "/dev/tty" ] && [ -w "/dev/tty" ]; then
    exec "$SYSDIR/script/shellect.sh" "$@"
fi

# Method 2: Try with setsid to create new session
if command -v setsid >/dev/null 2>&1; then
    exec setsid "$SYSDIR/script/shellect.sh" "$@" </dev/tty >/dev/tty 2>&1
fi

# Method 3: Try with openvt if available (creates virtual terminal)
if command -v openvt >/dev/null 2>&1; then
    exec openvt -c 1 -w "$SYSDIR/script/shellect.sh" "$@"
fi

# Method 4: Fallback - run directly and hope for the best
exec "$SYSDIR/script/shellect.sh" "$@"
