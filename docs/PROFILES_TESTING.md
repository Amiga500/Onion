# Profile System Testing Guide

This guide helps you verify that the profile system is working correctly.

## Prerequisites

- Onion OS with profile system installed
- SSH/Telnet access for command-line testing (optional)
- At least 50MB free space for testing

## Quick Test (Basic Functionality)

### 1. Profile Manager App Test

1. **Boot the Device**
   - Device should boot normally
   - Check `/tmp/active_profile.info` exists (via SSH)
   - Verify Guest profile is active

2. **Launch Profile Manager**
   - Go to Apps → Profile Manager
   - Should show current profile (Guest)
   - Menu should appear with options

3. **Create a Test Profile**
   - Select "Create New Profile"
   - Enter name: "Test1"
   - Select type: Normal
   - No password needed
   - Should show "Profile created successfully"

4. **Switch Profiles**
   - Select "Switch Profile"
   - Choose "Test1"
   - Should switch and restart MainUI
   - Launch Profile Manager again
   - Verify current profile shows "Test1"

5. **Delete Profile**
   - Switch back to Guest
   - Select "Delete Profile"
   - Choose "Test1"
   - Confirm deletion
   - Profile should be deleted

### 2. Limited Profile Test

1. **Create Limited Profile**
   - Create profile "Limited1"
   - Type: Limited
   - Set password: "1234"

2. **Switch to Limited Profile**
   - Switch to "Limited1"
   - Enter password when prompted

3. **Verify Restrictions**
   - Limited profile flag exists: `/mnt/SDCARD/.tmp_update/config/.limited_profile`
   - Profile Manager shows "Exit to Profile Selector" option

4. **Exit Limited Profile**
   - Select "Exit to Profile Selector"
   - Enter password: "1234"
   - Should allow switching to another profile

## Command-Line Tests

Run these commands via SSH/Telnet:

```bash
# Navigate to scripts directory
cd /mnt/SDCARD/.tmp_update/script/profiles

# 1. Test initialization
./profile_cli.sh init
echo "Expected: Profile system initialized"

# 2. List profiles
./profile_cli.sh list
echo "Expected: Guest profile listed"

# 3. Show active profile
./profile_cli.sh active
echo "Expected: Guest (normal)"

# 4. Create normal profile
./profile_cli.sh create "TestUser" normal
echo "Expected: Profile created successfully"

# 5. Create limited profile with password
./profile_cli.sh create "TestKid" limited "pass123"
echo "Expected: Profile created successfully"

# 6. List all profiles
./profile_cli.sh list
echo "Expected: Guest, TestUser, TestKid"

# 7. Show profile info
./profile_cli.sh info "TestUser"
echo "Expected: Profile details shown"

# 8. Switch profiles
./profile_cli.sh switch "TestUser"
echo "Expected: Switched to TestUser"

# 9. Verify active profile
./profile_cli.sh active
echo "Expected: TestUser (normal)"

# 10. Try to delete active profile (should fail)
./profile_cli.sh delete "TestUser"
echo "Expected: ERROR - cannot delete active profile"

# 11. Switch back to Guest
./profile_cli.sh switch "Guest"

# 12. Delete test profiles
./profile_cli.sh delete "TestUser"
./profile_cli.sh delete "TestKid"

# 13. Verify cleanup
./profile_cli.sh list
echo "Expected: Only Guest remaining"
```

## File System Verification

Check these paths exist after initialization:

```bash
# Profile system files
ls -la /mnt/SDCARD/Profiles/
# Expected: .active_profile, profiles.cfg, Guest/

# Guest profile structure
ls -la /mnt/SDCARD/Profiles/Guest/
# Expected: CurrentProfile/, .profile_info

ls -la /mnt/SDCARD/Profiles/Guest/CurrentProfile/
# Expected: config/, lists/, saves/, states/

# Symlink verification
ls -la /mnt/SDCARD/Saves/
# Expected: CurrentProfile -> /mnt/SDCARD/Profiles/Guest/CurrentProfile

# Active profile marker
cat /mnt/SDCARD/Profiles/.active_profile
# Expected: Guest
```

## Data Isolation Test

### Test Save Game Isolation

1. **Setup**
   ```bash
   # Create two profiles
   ./profile_cli.sh create "User1" normal
   ./profile_cli.sh create "User2" normal
   ```

2. **Test with User1**
   ```bash
   # Switch to User1
   ./profile_cli.sh switch "User1"
   
   # Create a test save file
   echo "User1 save data" > /mnt/SDCARD/Saves/CurrentProfile/saves/test_game.srm
   ```

3. **Verify User1 Data**
   ```bash
   # Check file exists
   cat /mnt/SDCARD/Saves/CurrentProfile/saves/test_game.srm
   # Expected: User1 save data
   ```

4. **Switch to User2**
   ```bash
   ./profile_cli.sh switch "User2"
   
   # Verify User2 doesn't see User1's save
   ls /mnt/SDCARD/Saves/CurrentProfile/saves/
   # Expected: Empty or no test_game.srm
   
   # Create User2's save
   echo "User2 different data" > /mnt/SDCARD/Saves/CurrentProfile/saves/test_game.srm
   ```

5. **Switch Back to User1**
   ```bash
   ./profile_cli.sh switch "User1"
   
   # Verify User1's data is intact
   cat /mnt/SDCARD/Saves/CurrentProfile/saves/test_game.srm
   # Expected: User1 save data
   ```

6. **Cleanup**
   ```bash
   ./profile_cli.sh switch "Guest"
   ./profile_cli.sh delete "User1"
   ./profile_cli.sh delete "User2"
   ```

## Password Verification Test

```bash
# Create profile with password
./profile_cli.sh create "Protected" normal "secret123"

# Try to switch without password (should fail)
./profile_cli.sh switch "Protected"
# Expected: ERROR - Password required

# Switch with correct password
./profile_cli.sh switch "Protected" "secret123"
# Expected: Success

# Switch with wrong password (should fail)
./profile_cli.sh switch "Guest"
./profile_cli.sh switch "Protected" "wrongpass"
# Expected: ERROR - Incorrect password

# Change password
./profile_cli.sh set-password "Protected" "newsecret"

# Test new password
./profile_cli.sh switch "Protected" "newsecret"
# Expected: Success

# Cleanup
./profile_cli.sh switch "Guest"
./profile_cli.sh delete "Protected"
```

## Error Handling Tests

### Test Invalid Profile Names

```bash
# Too long (>20 chars)
./profile_cli.sh create "ThisNameIsWayTooLongForAProfile" normal
# Expected: ERROR - Invalid profile name

# Invalid characters
./profile_cli.sh create "Test@User!" normal
# Expected: ERROR - Invalid profile name

# Reserved name
./profile_cli.sh create "CurrentProfile" normal
# Expected: ERROR - Invalid profile name
```

### Test Profile Limits

```bash
# Create max profiles (10 total including Guest)
for i in {1..9}; do
    ./profile_cli.sh create "User$i" normal
done

# Try to create 11th profile
./profile_cli.sh create "User10" normal
# Expected: ERROR - Maximum profiles reached

# Cleanup
for i in {1..9}; do
    ./profile_cli.sh delete "User$i"
done
```

### Test Disk Space

```bash
# Simulate low disk space (requires root)
# This test is optional and may not work in all environments

# Check current space
df -h /mnt/SDCARD

# Profile creation should check for at least 10MB free
```

## Boot Integration Test

1. **Create a profile and switch to it**
   ```bash
   ./profile_cli.sh create "BootTest" normal
   ./profile_cli.sh switch "BootTest"
   ```

2. **Reboot the device**
   ```bash
   reboot
   ```

3. **After boot, verify active profile**
   ```bash
   ./profile_cli.sh active
   # Expected: BootTest (normal)
   
   # Check symlink
   ls -la /mnt/SDCARD/Saves/CurrentProfile
   # Expected: Points to /mnt/SDCARD/Profiles/BootTest/CurrentProfile
   ```

4. **Cleanup**
   ```bash
   ./profile_cli.sh switch "Guest"
   ./profile_cli.sh delete "BootTest"
   ```

## Automated Test Suite

Run the comprehensive test suite:

```bash
cd /home/runner/work/Onion/Onion
./test_profiles.sh
```

**Expected Output:**
```
======================================
Profile System Test Suite
======================================

Test: Profile initialization... PASS
Test: Guest profile creation... PASS
Test: Create normal profile... PASS
Test: Create limited profile... PASS
Test: Password verification... PASS
Test: Profile switching... PASS
Test: Profile listing... PASS
Test: Profile deletion... PASS
Test: Invalid profile name rejection... PASS
Test: Cannot delete Guest profile... PASS
Test: Cannot delete active profile... PASS

======================================
Results: 11 passed, 0 failed
======================================
```

## Common Issues and Solutions

### Profile Manager doesn't launch

**Check:**
- App exists: `/mnt/SDCARD/App/ProfileManager/`
- Launch script is executable: `chmod +x /mnt/SDCARD/App/ProfileManager/launch.sh`
- Scripts are executable: `chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh`

### Profiles not persisting after reboot

**Check:**
- Boot integration: `/mnt/SDCARD/.tmp_update/runtime.sh` includes profile_boot.sh
- Active profile file: `/mnt/SDCARD/Profiles/.active_profile` exists
- SD card is properly mounted and writeable

### Cannot create symlink

**Check:**
- Parent directory exists: `/mnt/SDCARD/Saves/`
- No file/directory conflicts
- SD card filesystem supports symlinks (ext4, etc.)

### Password not working

**Check:**
- Password is stored as MD5 hash in profiles.cfg
- md5sum command is available: `which md5sum`
- Profile config file is not corrupted

## Performance Testing

### Profile Switch Time

Time how long it takes to switch profiles:

```bash
time ./profile_cli.sh switch "TestUser"
```

**Expected:** < 2 seconds

### Storage Usage

Check disk usage per profile:

```bash
du -sh /mnt/SDCARD/Profiles/*
```

**Expected:** 10-50KB per profile (without save data)

## Stress Testing

### Rapid Profile Switching

```bash
# Switch profiles rapidly
for i in {1..10}; do
    ./profile_cli.sh switch "User1"
    ./profile_cli.sh switch "Guest"
done

# Verify system stability
./profile_cli.sh list
./profile_cli.sh active
```

### Large Number of Profiles

```bash
# Create many profiles
for i in {1..9}; do
    ./profile_cli.sh create "StressTest$i" normal
done

# Switch between them
for i in {1..9}; do
    ./profile_cli.sh switch "StressTest$i"
    sleep 1
done

# Verify all exist
./profile_cli.sh list

# Cleanup
./profile_cli.sh switch "Guest"
for i in {1..9}; do
    ./profile_cli.sh delete "StressTest$i"
done
```

## Test Results Checklist

Mark each test as completed:

- [ ] Profile Manager App launches successfully
- [ ] Can create normal profile
- [ ] Can create limited profile with password
- [ ] Can switch between profiles
- [ ] Can delete profiles (except active and Guest)
- [ ] Limited profile restrictions work
- [ ] Password protection works
- [ ] Data is isolated between profiles
- [ ] Profiles persist after reboot
- [ ] All CLI commands work
- [ ] Automated test suite passes (11/11)
- [ ] No errors in system logs

## Reporting Issues

If you find bugs, report with:

1. **Steps to reproduce**
2. **Expected behavior**
3. **Actual behavior**
4. **System logs** (`/mnt/SDCARD/.tmp_update/logs/runtime.log`)
5. **Profile config** (`/mnt/SDCARD/Profiles/profiles.cfg`)
6. **Active profile** (`cat /mnt/SDCARD/Profiles/.active_profile`)

## Success Criteria

The profile system is working correctly if:

✓ All 11 automated tests pass
✓ Can create, switch, and delete profiles
✓ Data is properly isolated between profiles
✓ Password protection works for limited profiles
✓ Profiles persist across reboots
✓ No data loss or corruption
✓ Performance is acceptable (< 2s profile switch)
