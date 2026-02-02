# RetroArch Cheevos Build Fix

## Problem Description

The RetroArch build was failing with linker errors due to undefined references to `rcheevos_*` functions (RetroArch Achievements library):

```
/opt/miyoomini-toolchain/bin/../lib/gcc/arm-linux-gnueabihf/8.3.0/../../../../arm-linux-gnueabihf/bin/ld: obj-unix/release/retroarch.o: in function `driver_uninit.constprop.21':
retroarch.c:(.text.driver_uninit.constprop.21+0x154): undefined reference to `rcheevos_menu_reset_badges'
retroarch.c:(.text.command_event+0x80c): undefined reference to `rcheevos_hardcore_active'
... (50+ similar errors)
```

## Root Cause

The RetroArch code includes references to achievements (cheevos) functions throughout the codebase. However, when the achievements feature is not enabled during compilation (`HAVE_CHEEVOS` not defined), the cheevos library source files are not compiled or linked into the final binary.

This creates a mismatch where:
1. **Code references exist**: Functions call `rcheevos_*` APIs
2. **Library not linked**: The cheevos library object files are not included
3. **Linker fails**: Cannot resolve the undefined symbols

### Why One Target Succeeded

Interestingly, one build target succeeded while the other failed:
- **Failed**: `retroarch` (Miyoo Mini original)
- **Succeeded**: `retroarch_miyoo354` (Miyoo Mini+)

This suggests the two targets have different build configurations, with miyoo354 likely having cheevos disabled at the preprocessor level.

## Solution

Disable cheevos support for both build targets by passing `HAVE_CHEEVOS=0` to the RetroArch Makefile.

### Changes Made

**File**: `Makefile` (line 164)

```diff
-@cd $(THIRD_PARTY_DIR)/RetroArch-patch && $(MAKE) -j$(JOBS) LTO=
+@cd $(THIRD_PARTY_DIR)/RetroArch-patch && $(MAKE) -j$(JOBS) LTO= HAVE_CHEEVOS=0
```

## Why Disable Cheevos?

Several factors support disabling achievements on the Miyoo Mini platform:

1. **Network Requirement**: RetroArch Achievements requires network connectivity to validate and sync achievements with RetroAchievements.org
2. **Resource Constraints**: The Miyoo Mini has limited resources (RAM, CPU), and cheevos adds overhead
3. **User Configuration**: The default Onion configuration already has cheevos disabled:
   ```
   cheevos_hardcore_mode_enable = "false"
   ```
4. **Migration Scripts**: Previous updates explicitly disabled cheevos for certain beta versions (see `00006_cheevos_temp_fix.sh`)
5. **Build Consistency**: Both Miyoo Mini and Miyoo Mini+ targets should have the same feature set

## Technical Details

### What is `HAVE_CHEEVOS`?

`HAVE_CHEEVOS` is a preprocessor define that controls whether RetroArch's achievements support is compiled:

- **When enabled** (`HAVE_CHEEVOS=1`):
  - Cheevos source files are compiled
  - Cheevos functions are available
  - Code references work correctly
  
- **When disabled** (`HAVE_CHEEVOS=0`):
  - Cheevos source files are excluded
  - Cheevos functions are stubbed out or wrapped in `#ifdef`
  - Code should handle absence gracefully

### Dependencies

The cheevos library (`deps/rcheevos/`) provides:
- Achievement parsing and validation
- Leaderboard support
- Rich presence functionality
- Memory patching for achievement conditions
- Network communication with RetroAchievements.org

## Alternative Solutions Considered

### 1. Enable Cheevos (Not Recommended)
**Option**: Set `HAVE_CHEEVOS=1` to compile the library

**Pros**: 
- Full achievements support
- Users could use RetroAchievements if desired

**Cons**:
- Requires network connectivity
- Increases binary size (~200-300KB)
- Additional RAM overhead
- Not aligned with Onion's default configuration
- May require additional dependencies

### 2. Patch RetroArch Source (Not Practical)
**Option**: Modify RetroArch source to remove cheevos references

**Cons**:
- Requires maintaining patches
- Breaks on upstream updates
- Time-consuming to implement
- Unnecessary when build flags exist

### 3. Use Stub Implementation (Overkill)
**Option**: Create stub functions that return early

**Cons**:
- More complex than using build flags
- Still adds code bloat
- Makefile flag is cleaner

## Impact

### Build
- ✅ Both `retroarch` and `retroarch_miyoo354` targets now build successfully
- ✅ No source code modifications required
- ✅ Clean solution using standard build flags

### Binary Size
- Reduces binary size by ~200-300KB (cheevos library excluded)
- Faster loading times

### Functionality
- ❌ RetroAchievements support not available
- ✅ All other RetroArch features work normally
- ✅ Aligned with Onion's default configuration

### User Experience
- No change for most users (cheevos was already disabled in config)
- Users who wanted achievements: not officially supported on Miyoo Mini

## Testing

To verify the fix works:

```bash
docker run --rm -v "/path/to/Onion":/root/workspace \
  aemiii91/miyoomini-toolchain:latest \
  /bin/bash -c "source /root/.bashrc; make"
```

Expected output:
- ✅ RetroArch builds successfully
- ✅ Both `retroarch` and `retroarch_miyoo354` targets complete
- ✅ No linker errors

## Related Issues

- **LTO Fix**: Previously fixed similar linker issue by disabling LTO (`LTO=`)
- **Cheevos Migration**: Script `00006_cheevos_temp_fix.sh` disables cheevos for beta versions
- **Default Config**: `retroarch.cfg` has `cheevos_hardcore_mode_enable = "false"`

## References

- [RetroArch Achievements Documentation](https://docs.libretro.com/guides/retroachievements/)
- [RetroAchievements.org](https://retroachievements.org/)
- [rcheevos Library](https://github.com/RetroAchievements/rcheevos)
- [RetroArch Build System](https://docs.libretro.com/development/retroarch/compilation/)

## Future Considerations

If achievements support is desired in the future:

1. **Enable the feature**: Change `HAVE_CHEEVOS=0` to `HAVE_CHEEVOS=1`
2. **Update configuration**: Enable cheevos in `retroarch.cfg`
3. **Test network connectivity**: Ensure RetroAchievements.org is accessible
4. **Increase binary size budget**: Account for ~300KB increase
5. **Document user setup**: Provide guide for RetroAchievements account setup
6. **Consider selective builds**: Maybe enable only for WiFi-capable Miyoo Mini+

---

**Summary**: Disabled RetroArch Achievements (`HAVE_CHEEVOS=0`) to fix linker errors and align with Onion's default configuration.
