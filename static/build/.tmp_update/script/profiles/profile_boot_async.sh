#!/bin/sh
#
# Profile Boot Integration (Non-blocking version)
# Loads profile system at boot without blocking the boot process
#

SYSDIR="/mnt/SDCARD/.tmp_update"

# Run profile initialization in background to avoid blocking boot
(
    # Give the system a moment to fully initialize
    sleep 2
    
    # Safely source profile manager with error handling
    if [ -f "$SYSDIR/script/profiles/profile_manager.sh" ]; then
        . "$SYSDIR/script/profiles/profile_manager.sh" 2>/dev/null || {
            logger "Profile system: Failed to load profile_manager.sh, skipping profile initialization"
            exit 0
        }
    else
        logger "Profile system: profile_manager.sh not found, skipping initialization"
        exit 0
    fi
    
    # Initialize profile system with error handling and timeout
    logger "Profile system: Initializing..."
    
    timeout 5 sh -c '
        . "'$SYSDIR'/script/profiles/profile_manager.sh" 2>/dev/null
        profile_init 2>/dev/null
    ' || {
        logger "Profile system: Initialization failed or timed out, continuing with default setup"
        exit 0
    }
    
    # Get active profile
    active_profile=$(profile_get_active 2>/dev/null) || {
        logger "Profile system: Could not get active profile, skipping"
        exit 0
    }
    
    profile_type=$(profile_get_type "$active_profile" 2>/dev/null) || profile_type="normal"
    
    logger "Profile system: Active profile is '$active_profile' (type: $profile_type)"
    
    # Ensure symlink is correct
    profile_activate_current 2>/dev/null || {
        logger "Profile system: Failed to activate profile symlink, continuing anyway"
    }
    
    # Set limited profile flag if needed
    if [ "$profile_type" = "limited" ]; then
        touch "$SYSDIR/config/.limited_profile" 2>/dev/null || true
        logger "Profile system: Limited profile mode enabled"
    else
        rm -f "$SYSDIR/config/.limited_profile" 2>/dev/null || true
    fi
    
    # Create profile info file for MainUI
    cat > /tmp/active_profile.info 2>/dev/null << EOF
profile_name=$active_profile
profile_type=$profile_type
EOF
    
    sync
    
    logger "Profile system: Initialization complete"
) &

# Return immediately to not block boot
exit 0
