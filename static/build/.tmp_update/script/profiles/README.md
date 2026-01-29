# Onion OS Profile System Scripts

## Current Status: Manual Use Only

**IMPORTANT:** Profile system boot integration is DISABLED. These scripts do NOT run during system boot.

## Debugging ProfileManager App Issues

If the ProfileManager app crashes (shows loading then returns to apps menu), check the debug log:

```bash
cat /mnt/SDCARD/.tmp_update/logs/profile_menu_debug.log
```

This log shows exactly where the app fails and why.

### Common Issues and Solutions

**1. Permission Denied Errors**
```bash
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh
chmod +x /mnt/SDCARD/.tmp_update/script/shellect.sh
```

**2. Script Not Found**
- Verify files exist in `/mnt/SDCARD/.tmp_update/script/profiles/`
- Reinstall Onion OS if files are missing

**3. Initialization Failure**
- Check SD card is writable: `touch /mnt/SDCARD/test.txt && rm /mnt/SDCARD/test.txt`
- Check free space: `df -h /mnt/SDCARD`
- Check permissions: `ls -la /mnt/SDCARD/Profiles/`

**4. No Error Dialog Shown**
- Check debug log (see above)
- Verify infoPanel binary exists: `ls -la /mnt/SDCARD/.tmp_update/bin/infoPanel`
- Error dialogs require infoPanel binary to work

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
2. Check debug log if it crashes: `/mnt/SDCARD/.tmp_update/logs/profile_menu_debug.log`
3. Use the interactive menu to manage profiles

### Via CLI
```bash
# List all profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh list

# Create a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "MyProfile" normal

# Switch profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh switch "MyProfile"
```

## Debug Log Location

The ProfileManager app creates a detailed debug log at:
```
/mnt/SDCARD/.tmp_update/logs/profile_menu_debug.log
```

This log includes:
- Startup checks
- File existence verification
- Script sourcing results
- Initialization status
- Error details

**Always check this log first** if the ProfileManager app isn't working.

## Boot Integration Status

The profile system was causing boot hangs when enabled at boot. It has been disabled in runtime.sh to ensure the device boots properly.

**Profile system does NOT affect boot time** - it only runs when manually invoked.

If your device takes a long time on the Miyoo logo, this is unrelated to the profile system. Check:
- SD card health
- Other boot scripts
- Corrupted files

## Documentation

See `/mnt/SDCARD/docs/PROFILES.md` for full documentation.
See `/mnt/SDCARD/docs/PROFILES_USER_GUIDE.md` for user guide.
