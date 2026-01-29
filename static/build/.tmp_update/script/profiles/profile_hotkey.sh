#!/bin/sh
#
# Profile Hotkey Monitor
# Monitors for SELECT + START combo (5 seconds) to trigger profile switching
#

SYSDIR="/mnt/SDCARD/.tmp_update"
. "$SYSDIR/script/profiles/profile_manager.sh"

HOTKEY_FILE="/tmp/profile_hotkey_pressed"
HOTKEY_TIME=5  # seconds

# Check if both SELECT and START are pressed
check_hotkey() {
    # This would need to integrate with the actual key monitoring system
    # For now, this is a placeholder that would be called by keymon
    
    local current_profile=$(profile_get_active)
    local profile_type=$(profile_get_type "$current_profile")
    
    if [ "$profile_type" = "limited" ]; then
        # For limited profiles, require password
        password=$(prompt_password "Enter password to switch profiles:")
        
        if [ -z "$password" ]; then
            return 1
        fi
        
        if ! profile_verify_password "$current_profile" "$password"; then
            infoPanel --title "Error" --message "Incorrect password." --auto
            return 1
        fi
    fi
    
    # Show profile selector
    $SYSDIR/script/profiles/profile_menu.sh switch
}

# Export function for use by keymon
export -f check_hotkey
