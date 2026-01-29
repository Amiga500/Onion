# Onion OS Profile System Scripts

## Current Status: Manual Use Only

**IMPORTANT:** Profile system boot integration is DISABLED. These scripts do NOT run during system boot.

## Files in This Directory

- `profile_manager.sh` - Core library with profile management functions
- `profile_menu.sh` - Interactive menu UI (called by ProfileManager app)
- `profile_cli.sh` - Command-line interface for scripting
- `profile_boot.sh` - Boot integration (CURRENTLY DISABLED)
- `profile_boot_async.sh` - Alternative async boot integration (EXPERIMENTAL)
- `profile_hotkey.sh` - Hotkey support framework

## Usage

### Via ProfileManager App (Recommended)
1. Launch ProfileManager from Apps menu
2. Use the interactive menu to manage profiles

### Via CLI
```bash
# List all profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh list

# Create a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "MyProfile" normal

# Switch profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh switch "MyProfile"
```

## Permission Issues

If you get "Permission denied" errors:
```bash
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh
```

## Boot Integration Status

The profile system was causing boot hangs when enabled at boot. It has been disabled in runtime.sh to ensure the device boots properly.

**Profile system does NOT affect boot time** - it only runs when manually invoked.

If your device takes a long time on the Miyoo logo, this is unrelated to the profile system. Check:
- SD card health
- Other boot scripts
- Corrupted files

## Documentation

See `/mnt/SDCARD/docs/PROFILES.md` for full documentation.
