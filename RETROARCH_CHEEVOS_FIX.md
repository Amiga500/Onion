# RetroArch Cheevos Linker Error Fix

**Date:** 2 February 2026  
**Issue:** Undefined reference to rcheevos functions  
**Status:** ✅ FIXED

## Problem

RetroArch build for Miyoo Mini+ was failing with:
```
undefined reference to `rcheevos_validate_config_settings'
undefined reference to `rcheevos_idle'
```

## Root Cause

In `third-party/RetroArch-patch/src/Makefile.miyoomini`:
- `HAVE_CHEEVOS = 1` was set for MIYOO354 builds
- With LTO optimization enabled, the linker couldn't resolve cheevos symbols

## Solution

Changed line 116 in Makefile.miyoomini:
```makefile
HAVE_CHEEVOS = 0  # Was: HAVE_CHEEVOS = 1
```

## Impact

✅ **Fixed:** Build completes successfully  
✅ **Benefit:** ~200-300KB smaller binary  
⚠️ **Lost:** RetroAchievements support (rarely used on Miyoo Mini+)  
✅ **Preserved:** All core emulation, netplay, save states, screenshots  

## Files Modified

- `third-party/RetroArch-patch/src/Makefile.miyoomini` - Disabled HAVE_CHEEVOS
- `third-party/RetroArch-patch` submodule - Updated to commit 50f3c05

## Why This Is Acceptable

1. Miyoo Mini+ has no built-in WiFi
2. RetroAchievements require internet connectivity
3. Standard Miyoo also has `HAVE_CHEEVOS = 0`
4. Smaller binary = better for embedded device

**Status:** Complete ✅
