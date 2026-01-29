#!/bin/bash
#
# Profile System Test Suite
# Tests basic profile management functionality
#

SCRIPT_DIR="/home/runner/work/Onion/Onion/static/build/.tmp_update/script/profiles"
PROFILES_DIR="/tmp/test_profiles"
SAVES_BASE="/tmp/test_saves"

# Setup test environment
setup_test() {
    echo "Setting up test environment..."
    
    # Override directories for testing
    export PROFILES_DIR="/tmp/test_profiles"
    export SAVES_BASE="/tmp/test_saves"
    
    # Clean up any previous test data
    rm -rf "$PROFILES_DIR" "$SAVES_BASE"
    
    # Create test directories
    mkdir -p "$PROFILES_DIR"
    mkdir -p "$SAVES_BASE"
    
    # Source the profile manager
    export PROFILES_CONFIG="$PROFILES_DIR/profiles.cfg"
    export ACTIVE_PROFILE_FILE="$PROFILES_DIR/.active_profile"
    export CURRENT_PROFILE_LINK="$SAVES_BASE/CurrentProfile"
    export SYSDIR="/home/runner/work/Onion/Onion/static/build/.tmp_update"
    
    . "$SCRIPT_DIR/profile_manager.sh"
}

# Clean up test environment
cleanup_test() {
    echo "Cleaning up test environment..."
    rm -rf "$PROFILES_DIR" "$SAVES_BASE"
}

# Test functions
test_init() {
    echo -n "Test: Profile initialization... "
    profile_init
    
    if [ -d "$PROFILES_DIR" ] && [ -f "$PROFILES_CONFIG" ] && [ -f "$ACTIVE_PROFILE_FILE" ]; then
        echo "PASS"
        return 0
    else
        echo "FAIL"
        return 1
    fi
}

test_guest_profile() {
    echo -n "Test: Guest profile creation... "
    
    if profile_exists "Guest"; then
        local active=$(profile_get_active)
        if [ "$active" = "Guest" ]; then
            echo "PASS"
            return 0
        fi
    fi
    
    echo "FAIL"
    return 1
}

test_create_normal_profile() {
    echo -n "Test: Create normal profile... "
    
    profile_create "TestUser" "normal" ""
    
    if profile_exists "TestUser"; then
        local type=$(profile_get_type "TestUser")
        if [ "$type" = "normal" ]; then
            echo "PASS"
            return 0
        fi
    fi
    
    echo "FAIL"
    return 1
}

test_create_limited_profile() {
    echo -n "Test: Create limited profile... "
    
    profile_create "LimitedUser" "limited" "test1234"
    
    if profile_exists "LimitedUser"; then
        local type=$(profile_get_type "LimitedUser")
        if [ "$type" = "limited" ]; then
            echo "PASS"
            return 0
        fi
    fi
    
    echo "FAIL"
    return 1
}

test_password_verification() {
    echo -n "Test: Password verification... "
    
    if profile_verify_password "LimitedUser" "test1234"; then
        if ! profile_verify_password "LimitedUser" "wrong"; then
            echo "PASS"
            return 0
        fi
    fi
    
    echo "FAIL"
    return 1
}

test_profile_switch() {
    echo -n "Test: Profile switching... "
    
    profile_switch "TestUser"
    local active=$(profile_get_active)
    
    if [ "$active" = "TestUser" ]; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL"
    return 1
}

test_profile_list() {
    echo -n "Test: Profile listing... "
    
    local count=$(profile_count)
    
    # Should have 3 profiles: Guest, TestUser, LimitedUser
    if [ "$count" -ge 3 ]; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL (expected at least 3, got $count)"
    return 1
}

test_delete_profile() {
    echo -n "Test: Profile deletion... "
    
    # Switch to Guest first
    profile_switch "Guest"
    
    # Delete TestUser
    profile_delete "TestUser"
    
    if ! profile_exists "TestUser"; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL"
    return 1
}

test_invalid_name() {
    echo -n "Test: Invalid profile name rejection... "
    
    # Try to create with invalid characters
    profile_create "Test@User!" "normal" "" 2>&1 | grep -q "ERROR"
    local result1=$?
    
    # Try to create with too long name
    profile_create "ThisNameIsWayTooLongForAProfile" "normal" "" 2>&1 | grep -q "ERROR"
    local result2=$?
    
    if [ $result1 -eq 0 ] && [ $result2 -eq 0 ]; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL"
    return 1
}

test_cannot_delete_guest() {
    echo -n "Test: Cannot delete Guest profile... "
    
    profile_delete "Guest" 2>&1 | grep -q "ERROR"
    
    if [ $? -eq 0 ] && profile_exists "Guest"; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL"
    return 1
}

test_cannot_delete_active() {
    echo -n "Test: Cannot delete active profile... "
    
    local active=$(profile_get_active)
    profile_delete "$active" 2>&1 | grep -q "ERROR"
    
    if [ $? -eq 0 ]; then
        echo "PASS"
        return 0
    fi
    
    echo "FAIL"
    return 1
}

# Run all tests
run_tests() {
    local passed=0
    local failed=0
    
    echo "======================================"
    echo "Profile System Test Suite"
    echo "======================================"
    echo ""
    
    setup_test
    
    # Run tests
    test_init && passed=$((passed+1)) || failed=$((failed+1))
    test_guest_profile && passed=$((passed+1)) || failed=$((failed+1))
    test_create_normal_profile && passed=$((passed+1)) || failed=$((failed+1))
    test_create_limited_profile && passed=$((passed+1)) || failed=$((failed+1))
    test_password_verification && passed=$((passed+1)) || failed=$((failed+1))
    test_profile_switch && passed=$((passed+1)) || failed=$((failed+1))
    test_profile_list && passed=$((passed+1)) || failed=$((failed+1))
    test_delete_profile && passed=$((passed+1)) || failed=$((failed+1))
    test_invalid_name && passed=$((passed+1)) || failed=$((failed+1))
    test_cannot_delete_guest && passed=$((passed+1)) || failed=$((failed+1))
    test_cannot_delete_active && passed=$((passed+1)) || failed=$((failed+1))
    
    echo ""
    echo "======================================"
    echo "Results: $passed passed, $failed failed"
    echo "======================================"
    
    cleanup_test
    
    if [ $failed -eq 0 ]; then
        return 0
    else
        return 1
    fi
}

# Run tests
run_tests
exit $?
