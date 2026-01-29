#!/bin/sh
#
# Profile CLI - Command-line interface for profile management
# Usage: profile_cli.sh <command> [args...]
#

SYSDIR="/mnt/SDCARD/.tmp_update"
. "$SYSDIR/script/profiles/profile_manager.sh"

show_usage() {
    cat << EOF
Usage: profile_cli.sh <command> [args...]

Commands:
  init                    Initialize profile system
  list                    List all profiles
  active                  Show active profile
  create <name> <type> [password]
                          Create new profile
                          type: normal or limited
  delete <name>           Delete a profile
  switch <name> [password]
                          Switch to a profile
  set-password <name> <password>
                          Set password for a profile
  info <name>             Show profile information
  help                    Show this help message

Examples:
  profile_cli.sh create "MyProfile" normal
  profile_cli.sh create "KidsProfile" limited "1234"
  profile_cli.sh switch "MyProfile"
  profile_cli.sh delete "OldProfile"
EOF
}

cmd_init() {
    profile_init
    echo "Profile system initialized."
}

cmd_list() {
    local active=$(profile_get_active)
    echo "Profiles:"
    profile_list | while read -r profile; do
        local type=$(profile_get_type "$profile")
        local marker=""
        if [ "$profile" = "$active" ]; then
            marker=" (active)"
        fi
        echo "  - $profile [$type]$marker"
    done
}

cmd_active() {
    local active=$(profile_get_active)
    local type=$(profile_get_type "$active")
    echo "Active profile: $active [$type]"
}

cmd_create() {
    local name="$1"
    local type="$2"
    local password="$3"
    
    if [ -z "$name" ]; then
        echo "ERROR: Profile name required"
        exit 1
    fi
    
    if [ -z "$type" ]; then
        type="normal"
    fi
    
    profile_create "$name" "$type" "$password"
}

cmd_delete() {
    local name="$1"
    
    if [ -z "$name" ]; then
        echo "ERROR: Profile name required"
        exit 1
    fi
    
    profile_delete "$name"
}

cmd_switch() {
    local name="$1"
    local password="$2"
    
    if [ -z "$name" ]; then
        echo "ERROR: Profile name required"
        exit 1
    fi
    
    # Check if password is required
    if profile_has_password "$name" && [ -z "$password" ]; then
        echo "ERROR: Password required for this profile"
        exit 1
    fi
    
    # Verify password if provided
    if [ -n "$password" ]; then
        if ! profile_verify_password "$name" "$password"; then
            echo "ERROR: Incorrect password"
            exit 1
        fi
    fi
    
    profile_switch "$name"
}

cmd_set_password() {
    local name="$1"
    local password="$2"
    
    if [ -z "$name" ]; then
        echo "ERROR: Profile name required"
        exit 1
    fi
    
    profile_set_password "$name" "$password"
    echo "Password updated for profile '$name'"
}

cmd_info() {
    local name="$1"
    
    if [ -z "$name" ]; then
        name=$(profile_get_active)
    fi
    
    if ! profile_exists "$name"; then
        echo "ERROR: Profile '$name' does not exist"
        exit 1
    fi
    
    local type=$(profile_get_type "$name")
    local has_password="No"
    if profile_has_password "$name"; then
        has_password="Yes"
    fi
    
    echo "Profile: $name"
    echo "Type: $type"
    echo "Password protected: $has_password"
    
    if [ -f "$PROFILES_DIR/$name/.profile_info" ]; then
        . "$PROFILES_DIR/$name/.profile_info"
        echo "Created: $(date -d @$created 2>/dev/null || echo 'Unknown')"
    fi
}

# Main command dispatcher
command="$1"
shift

case "$command" in
    init)
        cmd_init
        ;;
    list)
        cmd_list
        ;;
    active)
        cmd_active
        ;;
    create)
        cmd_create "$@"
        ;;
    delete)
        cmd_delete "$@"
        ;;
    switch)
        cmd_switch "$@"
        ;;
    set-password)
        cmd_set_password "$@"
        ;;
    info)
        cmd_info "$@"
        ;;
    help|--help|-h)
        show_usage
        ;;
    *)
        echo "ERROR: Unknown command: $command"
        echo ""
        show_usage
        exit 1
        ;;
esac
