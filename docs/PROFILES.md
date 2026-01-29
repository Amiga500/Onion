# User Profiles Feature

## ⚠️ Current Status: Boot Integration Temporarily Disabled

The profile boot integration has been temporarily disabled due to a boot hang issue. The device was getting stuck on the Onion OS logo during startup. 

**Current State:**
- ✅ Device boots successfully
- ✅ ProfileManager app works after boot
- ✅ Manual profile management fully functional
- ❌ Automatic profile loading at boot disabled
- ❌ Profile persistence across reboots disabled

**Workaround:** Use ProfileManager app after boot to manage profiles manually.

---

## Overview

The User Profiles feature allows multiple users to have separate configurations, save data, and settings on the same Onion OS device. This is ideal for families or when you want to keep different gaming sessions separate.

## Features

- **Multiple Profiles**: Create up to 10 user profiles
- **Profile Types**:
  - **Normal**: Full access to all sections (Consoles, Apps, etc.)
  - **Limited**: Restricted access to Consoles section only (ideal for parental controls)
- **Password Protection**: Secure profiles with MD5-hashed passwords
- **Data Isolation**: Each profile has separate:
  - Game saves and save states
  - RetroArch history
  - Favorites and recent games lists
  - Play activity tracking
  - Profile-specific configurations
- **Easy Switching**: Switch between profiles through the ProfileManager app
- **Boot Integration**: Automatically loads the last active profile on startup

## Installation

The profile system is automatically initialized on first boot. The default "Guest" profile is created automatically.

## Using Profiles

### Via ProfileManager App

1. Launch the **Profile Manager** app from the Apps section
2. Select from the menu:
   - **Switch Profile**: Change to a different profile
   - **Create New Profile**: Add a new profile
   - **Delete Profile**: Remove an existing profile
   - **Set Profile Password**: Add or change password for current profile

### Via Command Line

For advanced users, the `profile_cli.sh` script provides command-line access:

```bash
# List all profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh list

# Show active profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh active

# Create a normal profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "MyProfile" normal

# Create a limited profile with password
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "KidsProfile" limited "1234"

# Switch profiles
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh switch "MyProfile"

# Delete a profile
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh delete "OldProfile"

# Show profile info
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh info "MyProfile"
```

## Profile Types

### Normal Profiles

- Full access to all menu sections
- Can access Apps, Games, Settings, etc.
- No restrictions

### Limited Profiles

- Access restricted to Consoles section only
- Ideal for children or restricted use
- Requires password to exit or switch profiles
- Menu automatically filters non-console sections

## Creating Profiles

### Profile Name Requirements

- 1-20 characters long
- Alphanumeric characters, spaces, underscores, and dashes only
- Cannot use reserved names (`.`, `..`, `CurrentProfile`)
- Must be unique

### Profile Limits

- Maximum 10 profiles
- Requires at least 10MB free disk space per profile

## Password Protection

### Setting a Password

Passwords are optional for Normal profiles but recommended for Limited profiles.

To set a password:
1. Open Profile Manager
2. Select "Set Profile Password"
3. Enter your desired password
4. Confirm

Passwords are stored as MD5 hashes for security.

### Resetting a Password

If you forget a password, you can reset it via command line:

```bash
# Remove password
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh set-password "ProfileName" ""

# Set new password
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh set-password "ProfileName" "newpassword"
```

## Profile Data

Each profile stores data in `/mnt/SDCARD/Profiles/<ProfileName>/CurrentProfile/`:

```
CurrentProfile/
├── config/         # Profile-specific configs
├── lists/          # History and favorites lists
├── saves/          # Game save files (.srm)
└── states/         # Save states (.state)
```

The system uses symlinks to point `/mnt/SDCARD/Saves/CurrentProfile` to the active profile's directory.

## Switching Profiles

When you switch profiles:
1. The system updates the active profile marker
2. The CurrentProfile symlink is updated
3. MainUI is restarted to apply changes
4. All profile-specific data is loaded

**Note**: Always exit your current game before switching profiles to avoid data loss.

## Deleting Profiles

To delete a profile:
1. Switch to a different profile first (cannot delete active profile)
2. Open Profile Manager
3. Select "Delete Profile"
4. Choose the profile to delete
5. Confirm deletion

**Warning**: Deleting a profile permanently removes all its data (saves, states, history). This cannot be undone!

The "Guest" profile cannot be deleted.

## Limited Profile Exit

To exit a Limited profile:
1. Open Profile Manager
2. Select "Exit to Profile Selector"
3. Enter the profile password
4. Choose a different profile to switch to

## Troubleshooting

### Profile not switching

- Make sure you've exited any running games
- Try restarting the device
- Check that the profile exists: `profile_cli.sh list`

### Cannot create profile

- Check available disk space: at least 10MB required
- Verify profile name is valid (alphanumeric, 1-20 chars)
- Maximum 10 profiles allowed

### Lost password

Use the CLI to reset:
```bash
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh set-password "ProfileName" ""
```

### Profile data corruption

Each profile's data is isolated. If one profile has issues:
1. Switch to a different profile
2. The problematic profile's data is in `/mnt/SDCARD/Profiles/<ProfileName>/`
3. You can manually backup or restore data from this directory

## Technical Details

### Architecture

- **Profile Manager** (`profile_manager.sh`): Core library with all profile functions
- **Profile Boot** (`profile_boot.sh`): Loads profile system at boot
- **Profile Menu** (`profile_menu.sh`): Interactive menu UI
- **Profile CLI** (`profile_cli.sh`): Command-line interface
- **Integration**: Boot script modified to call profile initialization

### Storage

- Profiles config: `/mnt/SDCARD/Profiles/profiles.cfg`
- Active profile marker: `/mnt/SDCARD/Profiles/.active_profile`
- Profile data: `/mnt/SDCARD/Profiles/<ProfileName>/CurrentProfile/`
- Symlink: `/mnt/SDCARD/Saves/CurrentProfile` → active profile

### Password Security

Passwords are hashed using MD5 before storage. While MD5 is not the most secure hash, it provides basic protection suitable for a handheld gaming device.

## FAQ

**Q: Will my existing saves be preserved?**
A: Yes! On first initialization, any existing data in `/mnt/SDCARD/Saves/CurrentProfile` is migrated to the Guest profile.

**Q: Can I share saves between profiles?**
A: No, saves are isolated per profile. You can manually copy save files between profile directories if needed.

**Q: How much space does each profile use?**
A: Minimal overhead (~10-50KB) plus your actual save data. The system uses symlinks to avoid duplication.

**Q: Can I rename a profile?**
A: Currently no. You would need to create a new profile and manually copy data.

**Q: Does this affect RetroArch configurations?**
A: Profile-specific RetroArch history and favorites are isolated. Core RetroArch configs are shared unless separately configured per profile.

**Q: What happens if I switch profiles while a game is running?**
A: Always exit games before switching profiles to prevent save data loss or corruption.

## Development

### Running Tests

A comprehensive test suite is included:

```bash
cd /home/runner/work/Onion/Onion
./test_profiles.sh
```

All 11 tests should pass.

### Adding Features

The modular design makes it easy to extend:

- Add new profile types in `profile_manager.sh`
- Extend UI in `profile_menu.sh`
- Add CLI commands in `profile_cli.sh`

## Credits

Implemented as part of the Onion OS enhancement project for Miyoo Mini handheld devices.

## License

Same as Onion OS - see main LICENSE file.
