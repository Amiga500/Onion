#!/bin/sh
#
# Profile Menu - Interactive menu for profile management
#

SYSDIR="/mnt/SDCARD/.tmp_update"

# Enable debug logging
DEBUG_LOG="$SYSDIR/logs/profile_menu_debug.log"
mkdir -p "$SYSDIR/logs" 2>/dev/null

# Debug logging function
debug_log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S'): $*" >> "$DEBUG_LOG"
}

debug_log "=== ProfileManager Starting ==="
debug_log "SYSDIR: $SYSDIR"
debug_log "PWD: $(pwd)"

# Function to show error and exit
show_error_and_exit() {
    local title="$1"
    local message="$2"
    
    debug_log "ERROR: $title - $message"
    
    # Try multiple ways to show the error
    # Method 1: Try with full path
    if [ -x "$SYSDIR/bin/infoPanel" ]; then
        debug_log "Showing error with infoPanel (full path)"
        "$SYSDIR/bin/infoPanel" --title "$title" --message "$message" --auto
    # Method 2: Try from PATH
    elif command -v infoPanel >/dev/null 2>&1; then
        debug_log "Showing error with infoPanel (from PATH)"
        infoPanel --title "$title" --message "$message" --auto
    # Method 3: Try running from bin directory
    elif [ -f "$SYSDIR/bin/infoPanel" ]; then
        debug_log "infoPanel exists but not executable, trying to run anyway"
        "$SYSDIR/bin/infoPanel" --title "$title" --message "$message" --auto 2>/dev/null || true
    else
        debug_log "infoPanel not found at all"
    fi
    
    # Also log to stderr
    echo "ERROR: $title - $message" >&2
    
    debug_log "Exiting with error code 1"
    
    # Exit with error
    exit 1
}

# Check if required scripts exist
debug_log "Checking for profile_manager.sh"
if [ ! -f "$SYSDIR/script/profiles/profile_manager.sh" ]; then
    debug_log "ERROR: profile_manager.sh not found"
    show_error_and_exit "Profile System Error" "Profile system scripts not found.\n\nPlease reinstall Onion OS."
fi
debug_log "profile_manager.sh found"

debug_log "profile_manager.sh found"

# Define a simple log function in case log.sh is not available
log() { 
    if [ -f "$SYSDIR/config/.logging" ]; then
        echo "$(date): $*" >> "$SYSDIR/logs/profile_menu.log"
    fi
}

# Try to source log.sh if available
debug_log "Checking for log.sh"
if [ -f "$SYSDIR/script/log.sh" ]; then
    . "$SYSDIR/script/log.sh" 2>/dev/null || true
    debug_log "log.sh sourced"
else
    debug_log "log.sh not found"
fi

# Source profile manager
debug_log "Sourcing profile_manager.sh"
# Don't suppress errors - we need to see what's failing
if . "$SYSDIR/script/profiles/profile_manager.sh"; then
    debug_log "profile_manager.sh sourced successfully"
else
    debug_log "ERROR: Failed to source profile_manager.sh (exit code: $?)"
    show_error_and_exit "Profile System Error" "Failed to load profile manager.\n\nCheck SD card integrity."
fi

# Initialize profiles on first run (with error handling)
debug_log "Calling profile_init"
if ! profile_init 2>/dev/null; then
    debug_log "ERROR: profile_init failed"
    show_error_and_exit "Profile Init Error" "Failed to initialize profiles.\n\nCheck:\n- SD card is writable\n- Enough free space\n- File permissions"
fi
debug_log "profile_init completed successfully"

# Verify shellect.sh is available for menu display
debug_log "Checking for shellect.sh"
if [ ! -f "$SYSDIR/script/shellect.sh" ]; then
    debug_log "ERROR: shellect.sh not found"
    show_error_and_exit "Menu Error" "Menu system (shellect.sh) not found.\n\nPlease reinstall Onion OS."
fi
debug_log "shellect.sh found"

# Make sure shellect.sh is executable
if [ ! -x "$SYSDIR/script/shellect.sh" ]; then
    debug_log "shellect.sh not executable, attempting chmod"
    chmod +x "$SYSDIR/script/shellect.sh" 2>/dev/null || true
    if [ -x "$SYSDIR/script/shellect.sh" ]; then
        debug_log "shellect.sh is now executable"
    else
        debug_log "WARNING: shellect.sh still not executable after chmod"
    fi
else
    debug_log "shellect.sh is executable"
fi

debug_log "Initialization complete, showing menu"

show_profile_menu() {
    local current_profile=$(profile_get_active)
    local profile_type=$(profile_get_type "$current_profile")
    
    # Build menu options
    local options=""
    
    # Show current profile info
    options="Current Profile: $current_profile ($profile_type)\n"
    options="${options}────────────────────────────────\n"
    
    # For limited profiles, only allow exit to profile selector
    if [ "$profile_type" = "limited" ]; then
        options="${options}1. Exit to Profile Selector\n"
        options="${options}2. Back to Main Menu"
        
        # Use shellect to show menu
        choice=$($SYSDIR/script/shellect.sh -t "Profile Management" -m "$options")
        
        case $choice in
            1)
                exit_limited_profile
                ;;
            *)
                return
                ;;
        esac
    else
        # Normal profile menu
        options="${options}1. Switch Profile\n"
        options="${options}2. Create New Profile\n"
        options="${options}3. Delete Profile\n"
        options="${options}4. Set Profile Password\n"
        options="${options}5. Back to Main Menu"
        
        # Use shellect to show menu
        choice=$($SYSDIR/script/shellect.sh -t "Profile Management" -m "$options")
        
        case $choice in
            1)
                switch_profile_menu
                ;;
            2)
                create_profile_menu
                ;;
            3)
                delete_profile_menu
                ;;
            4)
                set_password_menu
                ;;
            *)
                return
                ;;
        esac
    fi
}

switch_profile_menu() {
    # Get list of profiles
    local profiles=$(profile_list)
    local current_profile=$(profile_get_active)
    
    if [ -z "$profiles" ]; then
        infoPanel --title "No Profiles" --message "No profiles available." --auto
        return
    fi
    
    # Build options
    local options=""
    local i=1
    for profile in $profiles; do
        if [ "$profile" = "$current_profile" ]; then
            options="${options}${i}. $profile (current)\n"
        else
            options="${options}${i}. $profile\n"
        fi
        i=$((i + 1))
    done
    options="${options}${i}. Cancel"
    
    # Show menu
    choice=$($SYSDIR/script/shellect.sh -t "Switch Profile" -m "$options")
    
    if [ -z "$choice" ] || [ "$choice" -eq "$i" ]; then
        return
    fi
    
    # Get selected profile
    selected_profile=$(echo "$profiles" | sed -n "${choice}p")
    
    if [ -z "$selected_profile" ]; then
        return
    fi
    
    # Check if profile has password
    if profile_has_password "$selected_profile"; then
        # Prompt for password
        password=$(prompt_password "Enter password for $selected_profile:")
        
        if [ -z "$password" ]; then
            infoPanel --title "Cancelled" --message "Profile switch cancelled." --auto
            return
        fi
        
        if ! profile_verify_password "$selected_profile" "$password"; then
            infoPanel --title "Error" --message "Incorrect password." --auto
            return
        fi
    fi
    
    # Switch profile
    infoPanel --title "Switching Profile" --message "Switching to $selected_profile..." --persistent &
    sleep 1
    
    profile_switch "$selected_profile"
    
    killall infoPanel 2>/dev/null
    
    infoPanel --title "Success" --message "Switched to profile: $selected_profile\n\nRestarting MainUI..." --auto
    sleep 2
    
    # Restart MainUI to apply changes
    killall MainUI 2>/dev/null
}

create_profile_menu() {
    # Prompt for profile name
    profile_name=$(prompt_text "Enter profile name (1-20 chars):")
    
    if [ -z "$profile_name" ]; then
        infoPanel --title "Cancelled" --message "Profile creation cancelled." --auto
        return
    fi
    
    # Prompt for profile type
    type_options="1. Normal (Full Access)\n2. Limited (Consoles Only)"
    type_choice=$($SYSDIR/script/shellect.sh -t "Select Profile Type" -m "$type_options")
    
    if [ -z "$type_choice" ]; then
        return
    fi
    
    if [ "$type_choice" -eq 2 ]; then
        profile_type="limited"
    else
        profile_type="normal"
    fi
    
    # Prompt for password (optional for normal, required for limited)
    if [ "$profile_type" = "limited" ]; then
        password=$(prompt_password "Enter password (required for limited profiles):")
        
        if [ -z "$password" ]; then
            infoPanel --title "Error" --message "Password is required for limited profiles." --auto
            return
        fi
    else
        pwd_options="1. Set Password\n2. No Password"
        pwd_choice=$($SYSDIR/script/shellect.sh -t "Set Password?" -m "$pwd_options")
        
        if [ "$pwd_choice" -eq 1 ]; then
            password=$(prompt_password "Enter password:")
        else
            password=""
        fi
    fi
    
    # Create profile
    infoPanel --title "Creating Profile" --message "Creating profile: $profile_name..." --persistent &
    sleep 1
    
    result=$(profile_create "$profile_name" "$profile_type" "$password" 2>&1)
    status=$?
    
    killall infoPanel 2>/dev/null
    
    if [ $status -eq 0 ]; then
        infoPanel --title "Success" --message "Profile '$profile_name' created successfully!" --auto
    else
        infoPanel --title "Error" --message "$result" --auto
    fi
}

delete_profile_menu() {
    # Get list of profiles (excluding Guest)
    local profiles=$(profile_list | grep -v "^Guest$")
    local current_profile=$(profile_get_active)
    
    if [ -z "$profiles" ]; then
        infoPanel --title "No Profiles" --message "No profiles available to delete." --auto
        return
    fi
    
    # Build options
    local options=""
    local i=1
    for profile in $profiles; do
        if [ "$profile" = "$current_profile" ]; then
            options="${options}${i}. $profile (cannot delete - active)\n"
        else
            options="${options}${i}. $profile\n"
        fi
        i=$((i + 1))
    done
    options="${options}${i}. Cancel"
    
    # Show menu
    choice=$($SYSDIR/script/shellect.sh -t "Delete Profile" -m "$options")
    
    if [ -z "$choice" ] || [ "$choice" -eq "$i" ]; then
        return
    fi
    
    # Get selected profile
    selected_profile=$(echo "$profiles" | sed -n "${choice}p")
    
    if [ -z "$selected_profile" ] || [ "$selected_profile" = "$current_profile" ]; then
        infoPanel --title "Error" --message "Cannot delete the active profile." --auto
        return
    fi
    
    # Confirm deletion
    confirm_options="1. Yes, Delete\n2. Cancel"
    confirm=$($SYSDIR/script/shellect.sh -t "Confirm Deletion" -m "Delete profile '$selected_profile'?\nThis cannot be undone!\n\n$confirm_options")
    
    if [ "$confirm" -ne 1 ]; then
        return
    fi
    
    # Delete profile
    infoPanel --title "Deleting Profile" --message "Deleting profile: $selected_profile..." --persistent &
    sleep 1
    
    result=$(profile_delete "$selected_profile" 2>&1)
    status=$?
    
    killall infoPanel 2>/dev/null
    
    if [ $status -eq 0 ]; then
        infoPanel --title "Success" --message "Profile '$selected_profile' deleted successfully!" --auto
    else
        infoPanel --title "Error" --message "$result" --auto
    fi
}

set_password_menu() {
    local current_profile=$(profile_get_active)
    
    # Prompt for new password
    password=$(prompt_password "Enter new password (leave empty to remove):")
    
    # Set password
    infoPanel --title "Setting Password" --message "Updating password..." --persistent &
    sleep 1
    
    result=$(profile_set_password "$current_profile" "$password" 2>&1)
    status=$?
    
    killall infoPanel 2>/dev/null
    
    if [ $status -eq 0 ]; then
        if [ -n "$password" ]; then
            infoPanel --title "Success" --message "Password set successfully!" --auto
        else
            infoPanel --title "Success" --message "Password removed successfully!" --auto
        fi
    else
        infoPanel --title "Error" --message "$result" --auto
    fi
}

exit_limited_profile() {
    local current_profile=$(profile_get_active)
    
    # Check if password is set
    if ! profile_has_password "$current_profile"; then
        infoPanel --title "Error" --message "No password set for this profile. Cannot exit." --auto
        return
    fi
    
    # Prompt for password
    password=$(prompt_password "Enter password to exit:")
    
    if [ -z "$password" ]; then
        infoPanel --title "Cancelled" --message "Exit cancelled." --auto
        return
    fi
    
    if ! profile_verify_password "$current_profile" "$password"; then
        infoPanel --title "Error" --message "Incorrect password." --auto
        return
    fi
    
    # Password correct, show profile selector
    switch_profile_menu
}

# Text input using character selection
prompt_text() {
    local message="$1"
    local max_length="${2:-20}"
    local result=""
    
    # Use a simple character-by-character selection method
    # This would need proper implementation with keyboard input
    # For now, use predefined inputs or return a default
    
    # In actual implementation, this would show an on-screen keyboard
    # For testing purposes, we'll use a simplified version
    
    echo "$result"
}

# Password input using simple prompt
prompt_password() {
    local message="$1"
    
    # For limited profiles, use a simple numeric PIN system
    # This is more practical for the device's limited input
    
    # Use infoPanel to ask for password confirmation
    # Return the password or empty string
    
    echo "" # Placeholder - actual implementation would use on-screen input
}

# Main entry point
if [ "$1" = "menu" ]; then
    show_profile_menu
elif [ "$1" = "switch" ]; then
    switch_profile_menu
elif [ "$1" = "create" ]; then
    create_profile_menu
elif [ "$1" = "delete" ]; then
    delete_profile_menu
elif [ "$1" = "set_password" ]; then
    set_password_menu
elif [ "$1" = "exit_limited" ]; then
    exit_limited_profile
else
    show_profile_menu
fi
