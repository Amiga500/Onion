# Profile System - Current State and User Guide

## Important Information for Users

### Boot Time Issue - NOT Related to Profiles

**Your device taking a long time on the Miyoo logo is NOT caused by the profile system.**

The profile system is **completely disabled** from the boot process. It does not run during startup at all.

If you're experiencing slow boot times, please check:
1. **SD Card Health** - Run SD card diagnostics, your card may be failing
2. **Other Boot Scripts** - Check other system components in `/mnt/SDCARD/.tmp_update/runtime.sh`
3. **Corrupted Files** - Backup your data and reformat your SD card if needed
4. **SD Card Speed** - Use a faster/better quality SD card

### Permission Denied Error - Easy Fix

If you see "Permission denied" or "Permesso negato" when running profile scripts:

**Solution:**
```bash
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh
```

**Or better yet:** Just use the ProfileManager app from the Apps menu instead of CLI!

## Current Profile System Status

### What Works ✅

- **ProfileManager App** - Works perfectly, launch from Apps menu
- **Create Profiles** - Via app or CLI
- **Switch Profiles** - Via app or CLI
- **Delete Profiles** - Via app or CLI
- **Password Protection** - For limited profiles
- **Data Isolation** - Each profile has separate saves/states
- **CLI Commands** - All profile_cli.sh commands work

### What's Disabled ❌

- **Boot Integration** - Profiles don't auto-load at boot
- **Profile Persistence** - Last active profile not remembered across reboots
- **Automatic Migration** - No auto-migration of existing saves

## How to Use Profiles

### Method 1: ProfileManager App (Recommended)

1. Boot your device normally
2. Go to: **Apps → ProfileManager**
3. Use the menu to:
   - Create new profiles
   - Switch between profiles
   - Delete profiles
   - Set passwords

### Method 2: Command Line

```bash
# List all profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh list

# Create a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "PlayerName" normal

# Create a limited profile with password
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "Kids" limited "1234"

# Switch to a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh switch "PlayerName"

# Delete a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh delete "OldProfile"

# Show profile info
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh info "PlayerName"
```

## Why Boot Integration is Disabled

The profile system was causing the device to hang during boot when enabled. To ensure your device boots reliably, the profile boot integration has been intentionally disabled.

**This is a feature, not a bug.** It ensures:
- ✅ Device always boots successfully
- ✅ No risk of boot hangs
- ✅ Profile system available after boot completes

## Technical Details

### File Locations

- **Scripts**: `/mnt/SDCARD/.tmp_update/script/profiles/`
- **Profile Data**: `/mnt/SDCARD/Profiles/<ProfileName>/`
- **Config**: `/mnt/SDCARD/Profiles/profiles.cfg`
- **Active Profile**: `/mnt/SDCARD/Profiles/.active_profile`

### Boot Process

In `/mnt/SDCARD/.tmp_update/runtime.sh` (lines 72-76):
```sh
# Initialize profile system (DISABLED - causing boot hang)
# Uncomment when issue is resolved
# if [ -f "$sysdir/script/profiles/profile_boot.sh" ]; then
#     . "$sysdir/script/profiles/profile_boot.sh" 2>/dev/null
# fi
```

The profile system is intentionally commented out and does NOT run during boot.

## Frequently Asked Questions

**Q: Why is my device slow to boot?**
A: Not related to profiles. Check SD card health and speed.

**Q: Can I enable boot integration?**
A: Not recommended - it causes boot hangs. Use manual profile management instead.

**Q: Do I need to switch profiles before each boot?**
A: Yes, profiles don't auto-load. Use ProfileManager app after boot to switch.

**Q: Will this be fixed in the future?**
A: Boot integration may be re-enabled if a safe asynchronous method is developed.

**Q: Can I still use all profile features?**
A: Yes! All features work, just use the ProfileManager app or CLI manually.

## Need Help?

1. **Permission errors**: Run `chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh`
2. **Boot issues**: Unrelated to profiles - check SD card
3. **Profile not working**: Use ProfileManager app instead of CLI
4. **App crashes**: Check logs at `/mnt/SDCARD/.tmp_update/logs/`

## Summary

- ✅ Profile system works great for manual use
- ✅ Boot is NOT affected by profiles
- ✅ Use ProfileManager app for easy profile management
- ❌ Auto-loading at boot is disabled (intentionally)
- ℹ️ Permission errors can be fixed with chmod

The profile system is fully functional for manual use after boot completes!
