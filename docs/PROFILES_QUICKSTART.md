# Profile System - Quick Reference

## ⚠️ CLI-Only (SSH Required)

Profile management requires SSH or serial console access. The ProfileManager app shows instructions only.

## Quick Start

```bash
# Connect via SSH
ssh root@<device-ip>

# Navigate to profiles directory
cd /mnt/SDCARD/.tmp_update/script/profiles

# List profiles
./profile_cli.sh list

# Create profile
./profile_cli.sh create "MyProfile" normal

# Switch profile
./profile_cli.sh switch "MyProfile"

# Get help
./profile_cli.sh help
```

## All Commands

```bash
./profile_cli.sh init                        # Initialize system
./profile_cli.sh list                        # List all profiles  
./profile_cli.sh create <name> <type>        # Create (type: normal or limited)
./profile_cli.sh switch <name>               # Switch to profile
./profile_cli.sh delete <name>               # Delete profile
./profile_cli.sh info <name>                 # Show profile details
./profile_cli.sh set-password <name> <pw>    # Set password
./profile_cli.sh check-password <name> <pw>  # Verify password
./profile_cli.sh help                        # Show help
```

## Profile Types

- **normal**: Full system access (Consoles, Apps, Settings, etc.)
- **limited**: Consoles only (parental controls)

## What's Isolated

Each profile has separate:
- Game saves (.srm)
- Save states (.state)
- RetroArch history
- Favorites
- Play activity

## Examples

```bash
# Create normal profile
./profile_cli.sh create "Player1" normal
./profile_cli.sh switch "Player1"

# Create kid-friendly profile with password
./profile_cli.sh create "Kids" limited
./profile_cli.sh set-password "Kids" "1234"
./profile_cli.sh switch "Kids"

# Switch back to main profile
./profile_cli.sh switch "Player1"

# Delete old profile
./profile_cli.sh switch "Guest"
./profile_cli.sh delete "OldProfile"
```

## Troubleshooting

**Permission denied:**
```bash
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh
```

**Can't SSH:** Enable in Onion Settings → Network

**Boot slow:** Not related to profiles (disabled from boot)

## Why No GUI?

MainUI apps don't have terminal (TTY) access required for interactive menus. CLI provides full functionality via SSH.

## More Info

- Full docs: `PROFILES.md`
- Testing: `PROFILES_TESTING.md`
- Italian: `PROFILES_TROUBLESHOOTING_IT.md`
