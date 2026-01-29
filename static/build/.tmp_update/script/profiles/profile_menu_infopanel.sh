#!/bin/sh

# ProfileManager - infoPanel-based menu system
# Works without TTY requirement

SYSDIR="/mnt/SDCARD/.tmp_update"

# Source profile manager
if [ ! -f "$SYSDIR/script/profiles/profile_manager.sh" ]; then
    "$SYSDIR/bin/infoPanel" --title "Error" --message "Profile system not found" --auto
    exit 1
fi

. "$SYSDIR/script/profiles/profile_manager.sh" || exit 1

# Initialize profile system
profile_init || {
    "$SYSDIR/bin/infoPanel" --title "Error" --message "Failed to initialize profile system" --auto
    exit 1
}

# Show profile info
show_profile_info() {
    local active=$(profile_get_active)
    local ptype=$(profile_get_type "$active")
    local profiles=$(profile_list | tr '\n' ', ' | sed 's/,$//')
    
    "$SYSDIR/bin/infoPanel" --title "Current Profile" \
        --message "Active: $active ($ptype)\n\nAll Profiles:\n$profiles" \
        --button "OK"
}

# Switch profile menu
switch_profile_menu() {
    local profiles=$(profile_list)
    local active=$(profile_get_active)
    
    if [ -z "$profiles" ]; then
        "$SYSDIR/bin/infoPanel" --title "No Profiles" --message "No profiles available" --auto
        return
    fi
    
    # Show each profile as an option
    for profile in $profiles; do
        if [ "$profile" = "$active" ]; then
            continue  # Skip current profile
        fi
        
        local result=$("$SYSDIR/bin/infoPanel" --title "Switch Profile" \
            --message "Switch to: $profile?" \
            --button "Switch" \
            --button "Next" \
            --button "Cancel")
        
        case "$result" in
            1)  # Switch button
                if profile_switch "$profile"; then
                    "$SYSDIR/bin/infoPanel" --title "Success" \
                        --message "Switched to profile: $profile\n\nRestarting MainUI..." \
                        --auto
                    sleep 1
                    killall -9 MainUI
                    exit 0
                else
                    "$SYSDIR/bin/infoPanel" --title "Error" \
                        --message "Failed to switch profile" --auto
                fi
                return
                ;;
            2)  # Next button - continue to next profile
                continue
                ;;
            *)  # Cancel or close
                return
                ;;
        esac
    done
    
    "$SYSDIR/bin/infoPanel" --title "Info" --message "No more profiles" --auto
}

# Create profile menu
create_profile_menu() {
    # Get profile name (simplified - use default names)
    local count=$(profile_list | wc -l)
    local new_name="Player$((count + 1))"
    
    local result=$("$SYSDIR/bin/infoPanel" --title "Create Profile" \
        --message "Create new profile:\n$new_name\n\nChoose type:" \
        --button "Normal" \
        --button "Limited" \
        --button "Cancel")
    
    case "$result" in
        1)  # Normal
            if profile_create "$new_name" "normal"; then
                "$SYSDIR/bin/infoPanel" --title "Success" \
                    --message "Created profile: $new_name" --auto
            else
                "$SYSDIR/bin/infoPanel" --title "Error" \
                    --message "Failed to create profile" --auto
            fi
            ;;
        2)  # Limited
            if profile_create "$new_name" "limited"; then
                "$SYSDIR/bin/infoPanel" --title "Success" \
                    --message "Created limited profile: $new_name" --auto
            else
                "$SYSDIR/bin/infoPanel" --title "Error" \
                    --message "Failed to create profile" --auto
            fi
            ;;
        *)  # Cancel
            return
            ;;
    esac
}

# Delete profile menu
delete_profile_menu() {
    local profiles=$(profile_list)
    local active=$(profile_get_active)
    
    # Remove active profile from list (can't delete active)
    profiles=$(echo "$profiles" | grep -v "^${active}$")
    
    if [ -z "$profiles" ]; then
        "$SYSDIR/bin/infoPanel" --title "No Profiles" \
            --message "No other profiles to delete\n(Cannot delete active profile)" --auto
        return
    fi
    
    # Show each profile as an option
    for profile in $profiles; do
        if [ "$profile" = "Guest" ]; then
            continue  # Never allow deleting Guest
        fi
        
        local result=$("$SYSDIR/bin/infoPanel" --title "Delete Profile" \
            --message "Delete profile: $profile?" \
            --button "Delete" \
            --button "Next" \
            --button "Cancel")
        
        case "$result" in
            1)  # Delete button
                if profile_delete "$profile"; then
                    "$SYSDIR/bin/infoPanel" --title "Success" \
                        --message "Deleted profile: $profile" --auto
                else
                    "$SYSDIR/bin/infoPanel" --title "Error" \
                        --message "Failed to delete profile" --auto
                fi
                return
                ;;
            2)  # Next button
                continue
                ;;
            *)  # Cancel
                return
                ;;
        esac
    done
    
    "$SYSDIR/bin/infoPanel" --title "Info" --message "No more profiles" --auto
}

# Set password menu
set_password_menu() {
    local active=$(profile_get_active)
    
    "$SYSDIR/bin/infoPanel" --title "Set Password" \
        --message "Password protection for profile:\n$active\n\nNote: Use CLI to set custom password\nvia SSH/terminal access" \
        --auto
}

# Main menu - show one option at a time
main_menu() {
    local active=$(profile_get_active)
    local ptype=$(profile_get_type "$active")
    
    # Show welcome message
    "$SYSDIR/bin/infoPanel" --title "Profile Manager" \
        --message "Current Profile:\n$active ($ptype)\n\nPress A to continue" \
        --auto
    
    # Option 1: Switch Profile
    "$SYSDIR/bin/infoPanel" --title "Option 1: Switch Profile" \
        --message "Switch to a different profile\n\nPress A to select\nor B to skip" \
        --persistent &
    local pid=$!
    
    # Wait for button press
    local choice=$(waitForButton)
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if [ "$choice" = "A" ]; then
        switch_profile_menu
        return
    fi
    
    # Option 2: Create Profile
    "$SYSDIR/bin/infoPanel" --title "Option 2: Create Profile" \
        --message "Create a new profile\n\nPress A to select\nor B to skip" \
        --persistent &
    pid=$!
    
    choice=$(waitForButton)
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if [ "$choice" = "A" ]; then
        create_profile_menu
        return
    fi
    
    # Option 3: Delete Profile
    "$SYSDIR/bin/infoPanel" --title "Option 3: Delete Profile" \
        --message "Delete an existing profile\n\nPress A to select\nor B to skip" \
        --persistent &
    pid=$!
    
    choice=$(waitForButton)
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if [ "$choice" = "A" ]; then
        delete_profile_menu
        return
    fi
    
    # Option 4: Show Info
    "$SYSDIR/bin/infoPanel" --title "Option 4: Profile Info" \
        --message "Show profile information\n\nPress A to select\nor B to exit" \
        --persistent &
    pid=$!
    
    choice=$(waitForButton)
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
    
    if [ "$choice" = "A" ]; then
        show_profile_info
        return
    fi
    
    # Exit
    return 0
}

# Wait for button press - returns A or B
waitForButton() {
    # Create a named pipe for input
    local input_file="/tmp/profile_input_$$"
    rm -f "$input_file"
    
    # Monitor /dev/input/event0 for key presses
    # This is a simplified approach - in reality we'd need proper event handling
    # For now, use a timeout-based approach
    local timeout=30
    local count=0
    
    while [ $count -lt $timeout ]; do
        # Check if A button file marker exists (created by system)
        if [ -f "/tmp/btn_a_$$" ]; then
            rm -f "/tmp/btn_a_$$"
            echo "A"
            return
        fi
        
        # Check for B button
        if [ -f "/tmp/btn_b_$$" ]; then
            rm -f "/tmp/btn_b_$$"
            echo "B"
            return
        fi
        
        sleep 0.1
        count=$((count + 1))
    done
    
    # Timeout - treat as B (cancel)
    echo "B"
}

# Start menu
main_menu
