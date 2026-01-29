#!/bin/sh
#
# Profile Manager - Core library for profile management
# Provides functions for creating, switching, and managing user profiles
#

# Use environment variables if set, otherwise use defaults
PROFILES_DIR="${PROFILES_DIR:-/mnt/SDCARD/Profiles}"
PROFILES_CONFIG="${PROFILES_CONFIG:-$PROFILES_DIR/profiles.cfg}"
ACTIVE_PROFILE_FILE="${ACTIVE_PROFILE_FILE:-$PROFILES_DIR/.active_profile}"
SAVES_BASE="${SAVES_BASE:-/mnt/SDCARD/Saves}"
CURRENT_PROFILE_LINK="${CURRENT_PROFILE_LINK:-$SAVES_BASE/CurrentProfile}"
SYSDIR="${SYSDIR:-/mnt/SDCARD/.tmp_update}"

# Initialize profile system
profile_init() {
    # Create profiles directory if it doesn't exist
    if [ ! -d "$PROFILES_DIR" ]; then
        mkdir -p "$PROFILES_DIR" || return 1
    fi
    
    # Create profiles config if it doesn't exist
    if [ ! -f "$PROFILES_CONFIG" ]; then
        cat > "$PROFILES_CONFIG" << 'EOF'
# Onion OS Profiles Configuration
# Format: profile_name|profile_type|password_hash
# profile_type: normal or limited
# password_hash: MD5 hash for limited profile exit (empty if not set)
EOF
        sync || return 1
    fi
    
    # If no active profile, initialize with Guest
    if [ ! -f "$ACTIVE_PROFILE_FILE" ]; then
        # Check if Guest profile exists in config
        if ! grep -q "^Guest|" "$PROFILES_CONFIG" 2>/dev/null; then
            echo "Guest|normal|" >> "$PROFILES_CONFIG" || return 1
            sync || return 1
        fi
        
        # Create Guest profile directory if it doesn't exist
        if [ ! -d "$PROFILES_DIR/Guest" ]; then
            profile_create_internal "Guest" "normal" "" || return 1
        fi
        
        # Set Guest as active
        echo "Guest" > "$ACTIVE_PROFILE_FILE" || return 1
        sync || return 1
        
        # Migrate existing CurrentProfile data to Guest if it exists and is not a symlink
        if [ -d "$CURRENT_PROFILE_LINK" ] && [ ! -L "$CURRENT_PROFILE_LINK" ]; then
            # Remove the empty Guest/CurrentProfile we just created
            rm -rf "$PROFILES_DIR/Guest/CurrentProfile"
            # Move the existing CurrentProfile to Guest
            mv "$CURRENT_PROFILE_LINK" "$PROFILES_DIR/Guest/CurrentProfile" || return 1
        fi
    fi
    
    # Ensure CurrentProfile symlink points to active profile
    profile_activate_current || return 1
    
    return 0
}

# Get the currently active profile name
profile_get_active() {
    if [ -f "$ACTIVE_PROFILE_FILE" ]; then
        cat "$ACTIVE_PROFILE_FILE"
    else
        echo "Guest"
    fi
}

# Get profile type (normal or limited)
profile_get_type() {
    local profile_name="$1"
    
    if [ -z "$profile_name" ]; then
        return 1
    fi
    
    local profile_line=$(grep "^${profile_name}|" "$PROFILES_CONFIG")
    if [ -z "$profile_line" ]; then
        return 1
    fi
    
    echo "$profile_line" | cut -d'|' -f2
}

# Check if profile has a password set
profile_has_password() {
    local profile_name="$1"
    
    if [ -z "$profile_name" ]; then
        return 1
    fi
    
    local profile_line=$(grep "^${profile_name}|" "$PROFILES_CONFIG")
    if [ -z "$profile_line" ]; then
        return 1
    fi
    
    local password_hash=$(echo "$profile_line" | cut -d'|' -f3)
    [ -n "$password_hash" ]
}

# Verify password for a profile
profile_verify_password() {
    local profile_name="$1"
    local password="$2"
    
    if [ -z "$profile_name" ] || [ -z "$password" ]; then
        return 1
    fi
    
    local profile_line=$(grep "^${profile_name}|" "$PROFILES_CONFIG")
    if [ -z "$profile_line" ]; then
        return 1
    fi
    
    local stored_hash=$(echo "$profile_line" | cut -d'|' -f3)
    local password_hash=$(echo -n "$password" | md5sum | cut -d' ' -f1)
    
    [ "$stored_hash" = "$password_hash" ]
}

# Set password for a profile
profile_set_password() {
    local profile_name="$1"
    local password="$2"
    
    if [ -z "$profile_name" ]; then
        return 1
    fi
    
    local profile_line=$(grep "^${profile_name}|" "$PROFILES_CONFIG")
    if [ -z "$profile_line" ]; then
        return 1
    fi
    
    local profile_type=$(echo "$profile_line" | cut -d'|' -f2)
    local password_hash=""
    
    if [ -n "$password" ]; then
        password_hash=$(echo -n "$password" | md5sum | cut -d' ' -f1)
    fi
    
    # Update config
    sed -i "/^${profile_name}|/d" "$PROFILES_CONFIG"
    echo "${profile_name}|${profile_type}|${password_hash}" >> "$PROFILES_CONFIG"
    sync
    
    return 0
}

# List all profiles
profile_list() {
    if [ ! -f "$PROFILES_CONFIG" ]; then
        return
    fi
    
    grep -v '^#' "$PROFILES_CONFIG" | grep -v '^$' | while IFS='|' read -r name type hash; do
        echo "$name"
    done
}

# Count profiles
profile_count() {
    profile_list | wc -l
}

# Check if profile exists
profile_exists() {
    local profile_name="$1"
    
    if [ -z "$profile_name" ]; then
        return 1
    fi
    
    grep -q "^${profile_name}|" "$PROFILES_CONFIG"
}

# Validate profile name
profile_validate_name() {
    local name="$1"
    
    # Check length (1-20 characters)
    local len=$(echo -n "$name" | wc -c)
    if [ $len -lt 1 ] || [ $len -gt 20 ]; then
        return 1
    fi
    
    # Check for invalid characters (only alphanumeric, underscore, dash, space)
    if echo "$name" | grep -q '[^a-zA-Z0-9_ -]'; then
        return 1
    fi
    
    # Check if name is reserved
    if [ "$name" = "." ] || [ "$name" = ".." ] || [ "$name" = "CurrentProfile" ]; then
        return 1
    fi
    
    return 0
}

# Internal function to create profile structure
profile_create_internal() {
    local profile_name="$1"
    local profile_type="$2"
    local password="$3"
    
    local profile_dir="$PROFILES_DIR/$profile_name"
    
    # Create profile directory structure
    mkdir -p "$profile_dir/CurrentProfile/lists"
    mkdir -p "$profile_dir/CurrentProfile/config"
    mkdir -p "$profile_dir/CurrentProfile/saves"
    mkdir -p "$profile_dir/CurrentProfile/states"
    
    # Copy default files if they exist
    if [ -d "/mnt/SDCARD/Saves/CurrentProfile" ]; then
        # Copy default config files but not save data
        if [ -d "/mnt/SDCARD/Saves/CurrentProfile/config" ]; then
            cp -r /mnt/SDCARD/Saves/CurrentProfile/config/* "$profile_dir/CurrentProfile/config/" 2>/dev/null || true
        fi
    fi
    
    # Create empty lists
    echo "[]" > "$profile_dir/CurrentProfile/lists/content_history.lpl"
    echo "[]" > "$profile_dir/CurrentProfile/lists/content_favorites.lpl"
    
    # Create profile metadata
    cat > "$profile_dir/.profile_info" << EOF
profile_name=$profile_name
profile_type=$profile_type
created=$(date +%s)
EOF
    
    sync
}

# Create a new profile
profile_create() {
    local profile_name="$1"
    local profile_type="$2"
    local password="$3"
    
    # Validate inputs
    if ! profile_validate_name "$profile_name"; then
        echo "ERROR: Invalid profile name. Use 1-20 alphanumeric characters, spaces, underscores, or dashes."
        return 1
    fi
    
    if [ "$profile_type" != "normal" ] && [ "$profile_type" != "limited" ]; then
        profile_type="normal"
    fi
    
    # Check if profile already exists
    if profile_exists "$profile_name"; then
        echo "ERROR: Profile '$profile_name' already exists."
        return 1
    fi
    
    # Check max profiles limit (10 profiles max)
    local count=$(profile_count)
    if [ $count -ge 10 ]; then
        echo "ERROR: Maximum number of profiles (10) reached."
        return 1
    fi
    
    # Check available disk space (need at least 10MB)
    local available=0
    if df "$PROFILES_DIR" > /dev/null 2>&1; then
        available=$(df "$PROFILES_DIR" 2>/dev/null | tail -1 | awk '{print $4}')
    fi
    
    if [ -n "$available" ] && [ "$available" -lt 10240 ]; then
        echo "ERROR: Insufficient disk space. Need at least 10MB free."
        return 1
    fi
    
    # Create profile structure
    profile_create_internal "$profile_name" "$profile_type" "$password"
    
    # Generate password hash
    local password_hash=""
    if [ -n "$password" ]; then
        password_hash=$(echo -n "$password" | md5sum | cut -d' ' -f1)
    fi
    
    # Add to config
    echo "${profile_name}|${profile_type}|${password_hash}" >> "$PROFILES_CONFIG"
    sync
    
    echo "Profile '$profile_name' created successfully."
    return 0
}

# Delete a profile
profile_delete() {
    local profile_name="$1"
    
    if [ -z "$profile_name" ]; then
        echo "ERROR: Profile name required."
        return 1
    fi
    
    # Cannot delete Guest profile
    if [ "$profile_name" = "Guest" ]; then
        echo "ERROR: Cannot delete the Guest profile."
        return 1
    fi
    
    # Check if profile exists
    if ! profile_exists "$profile_name"; then
        echo "ERROR: Profile '$profile_name' does not exist."
        return 1
    fi
    
    # Cannot delete active profile
    local active_profile=$(profile_get_active)
    if [ "$active_profile" = "$profile_name" ]; then
        echo "ERROR: Cannot delete the active profile. Switch to another profile first."
        return 1
    fi
    
    # Remove from config
    sed -i "/^${profile_name}|/d" "$PROFILES_CONFIG"
    
    # Remove profile directory
    rm -rf "$PROFILES_DIR/$profile_name"
    
    sync
    echo "Profile '$profile_name' deleted successfully."
    return 0
}

# Activate the current profile symlink
profile_activate_current() {
    local active_profile=$(profile_get_active)
    local profile_dir="$PROFILES_DIR/$active_profile/CurrentProfile"
    
    # Remove old symlink or directory
    if [ -L "$CURRENT_PROFILE_LINK" ]; then
        rm -f "$CURRENT_PROFILE_LINK"
    elif [ -d "$CURRENT_PROFILE_LINK" ]; then
        # Backup if it's a real directory (migration case)
        mv "$CURRENT_PROFILE_LINK" "$CURRENT_PROFILE_LINK.backup"
    fi
    
    # Create new symlink
    ln -sf "$profile_dir" "$CURRENT_PROFILE_LINK"
    sync
}

# Switch to a different profile
profile_switch() {
    local target_profile="$1"
    
    if [ -z "$target_profile" ]; then
        echo "ERROR: Profile name required."
        return 1
    fi
    
    # Check if profile exists
    if ! profile_exists "$target_profile"; then
        echo "ERROR: Profile '$target_profile' does not exist."
        return 1
    fi
    
    local current_profile=$(profile_get_active)
    
    # Already on this profile
    if [ "$current_profile" = "$target_profile" ]; then
        echo "Already on profile '$target_profile'."
        return 0
    fi
    
    # Update active profile
    echo "$target_profile" > "$ACTIVE_PROFILE_FILE"
    sync
    
    # Update symlink
    profile_activate_current
    
    # Update system flags based on profile type
    local profile_type=$(profile_get_type "$target_profile")
    if [ "$profile_type" = "limited" ]; then
        touch "$SYSDIR/config/.limited_profile"
    else
        rm -f "$SYSDIR/config/.limited_profile"
    fi
    
    sync
    
    echo "Switched to profile '$target_profile'."
    return 0
}

# Export functions for use in other scripts
export -f profile_init
export -f profile_get_active
export -f profile_get_type
export -f profile_has_password
export -f profile_verify_password
export -f profile_set_password
export -f profile_list
export -f profile_count
export -f profile_exists
export -f profile_validate_name
export -f profile_create
export -f profile_delete
export -f profile_switch
export -f profile_activate_current
