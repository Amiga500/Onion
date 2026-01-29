#!/bin/sh
#
# Profile Boot Integration
# Loads profile system at boot and ensures profile is active
#

SYSDIR="/mnt/SDCARD/.tmp_update"

# Safely source profile manager with error handling
if [ -f "$SYSDIR/script/profiles/profile_manager.sh" ]; then
    . "$SYSDIR/script/profiles/profile_manager.sh" 2>/dev/null || {
        log "Profile system: Failed to load profile_manager.sh, skipping profile initialization"
        return 0
    }
else
    log "Profile system: profile_manager.sh not found, skipping initialization"
    return 0
fi

# Initialize profile system with error handling
log "Profile system: Initializing..."

profile_init 2>/dev/null || {
    log "Profile system: Initialization failed, continuing with default setup"
    return 0
}

# Get active profile
active_profile=$(profile_get_active 2>/dev/null) || {
    log "Profile system: Could not get active profile, skipping"
    return 0
}

profile_type=$(profile_get_type "$active_profile" 2>/dev/null) || profile_type="normal"

log "Profile system: Active profile is '$active_profile' (type: $profile_type)"

# Ensure symlink is correct
profile_activate_current 2>/dev/null || {
    log "Profile system: Failed to activate profile symlink, continuing anyway"
}

# Set limited profile flag if needed
if [ "$profile_type" = "limited" ]; then
    touch "$SYSDIR/config/.limited_profile" 2>/dev/null || true
    log "Profile system: Limited profile mode enabled"
else
    rm -f "$SYSDIR/config/.limited_profile" 2>/dev/null || true
fi

# Create profile info file for MainUI
cat > /tmp/active_profile.info 2>/dev/null << EOF
profile_name=$active_profile
profile_type=$profile_type
EOF

sync

log "Profile system: Initialization complete"
