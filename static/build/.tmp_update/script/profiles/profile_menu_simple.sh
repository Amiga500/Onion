#!/bin/sh

# ProfileManager - Simple sequential menu
# Works with infoPanel (no TTY required)

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

# Show current profile and menu
show_menu() {
    local active=$(profile_get_active)
    local ptype=$(profile_get_type "$active")
    local profiles=$(profile_list | wc -l)
    
    "$SYSDIR/bin/infoPanel" --title "Profile Manager" \
        --message "Current Profile: $active ($ptype)\n\nTotal Profiles: $profiles\n\n=== Menu ===\n1. Switch Profile\n2. Create Profile\n3. Delete Profile\n4. Profile Info\n\nUse CLI for full control:\n/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh" \
        --auto
}

# Switch to next available profile
switch_to_next() {
    local active=$(profile_get_active)
    local profiles=$(profile_list)
    local found_current=0
    local next_profile=""
    
    # Find next profile after current
    for profile in $profiles; do
        if [ $found_current -eq 1 ]; then
            next_profile="$profile"
            break
        fi
        if [ "$profile" = "$active" ]; then
            found_current=1
        fi
    done
    
    # If no next profile, wrap to first
    if [ -z "$next_profile" ]; then
        next_profile=$(echo "$profiles" | head -1)
    fi
    
    # Don't switch if it's the same
    if [ "$next_profile" = "$active" ]; then
        "$SYSDIR/bin/infoPanel" --title "Info" \
            --message "Only one profile available\n\nCreate more profiles via CLI" --auto
        return 1
    fi
    
    # Switch profile
    if profile_switch "$next_profile"; then
        "$SYSDIR/bin/infoPanel" --title "Success" \
            --message "Switched to: $next_profile\n\nRestarting MainUI..." \
            --auto
        sleep 1
        killall -9 MainUI
        exit 0
    else
        "$SYSDIR/bin/infoPanel" --title "Error" \
            --message "Failed to switch profile" --auto
        return 1
    fi
}

# Create new profile with auto-name
create_new_profile() {
    local count=$(profile_list | wc -l)
    local new_name="Player$((count + 1))"
    
    if profile_create "$new_name" "normal"; then
        "$SYSDIR/bin/infoPanel" --title "Success" \
            --message "Created profile: $new_name\n\nSwitch to it via menu option 1" --auto
        return 0
    else
        "$SYSDIR/bin/infoPanel" --title "Error" \
            --message "Failed to create profile\n\nMay have reached limit (10 profiles)" --auto
        return 1
    fi
}

# Show profile list
show_info() {
    local active=$(profile_get_active)
    local ptype=$(profile_get_type "$active")
    local profiles=$(profile_list | tr '\n' ', ' | sed 's/,$//')
    
    "$SYSDIR/bin/infoPanel" --title "Profile Info" \
        --message "Active: $active ($ptype)\n\nAll Profiles:\n$profiles\n\nUse CLI to:\n- Set passwords\n- Delete profiles\n- Create limited profiles" \
        --auto
}

# Main entry point
main() {
    # Show menu
    show_menu
    
    # For now, just show info since we can't do interactive selection
    # User would need to use CLI for actual management
    show_info
}

main
