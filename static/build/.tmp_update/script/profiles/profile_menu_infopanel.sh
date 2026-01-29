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

# Main menu loop
main_menu() {
    while true; do
        local active=$(profile_get_active)
        local ptype=$(profile_get_type "$active")
        
        local result=$("$SYSDIR/bin/infoPanel" --title "Profile Manager" \
            --message "Current: $active ($ptype)\n\nSelect option:" \
            --button "Switch" \
            --button "Create" \
            --button "Delete" \
            --button "Info" \
            --button "Exit")
        
        case "$result" in
            1)  # Switch Profile
                switch_profile_menu
                ;;
            2)  # Create Profile
                create_profile_menu
                ;;
            3)  # Delete Profile
                delete_profile_menu
                ;;
            4)  # Show Info
                show_profile_info
                ;;
            *)  # Exit or close
                return 0
                ;;
        esac
    done
}

# Start menu
main_menu
