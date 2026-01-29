#!/bin/sh
#
# Profile Boot Integration
# Loads profile system at boot and ensures profile is active
#

SYSDIR="/mnt/SDCARD/.tmp_update"
. "$SYSDIR/script/profiles/profile_manager.sh"

# Initialize profile system
log "Profile system: Initializing..."

profile_init

# Get active profile
active_profile=$(profile_get_active)
profile_type=$(profile_get_type "$active_profile")

log "Profile system: Active profile is '$active_profile' (type: $profile_type)"

# Ensure symlink is correct
profile_activate_current

# Set limited profile flag if needed
if [ "$profile_type" = "limited" ]; then
    touch "$SYSDIR/config/.limited_profile"
    log "Profile system: Limited profile mode enabled"
else
    rm -f "$SYSDIR/config/.limited_profile"
fi

# Create profile info file for MainUI
cat > /tmp/active_profile.info << EOF
profile_name=$active_profile
profile_type=$profile_type
EOF

sync

log "Profile system: Initialization complete"
