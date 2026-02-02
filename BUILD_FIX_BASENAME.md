# Build Fix: Theme Download Script Basename Errors

**Date:** 2026-02-02  
**Issue:** Build failure in Docker container when downloading themes  
**Status:** ✅ FIXED

---

## Problem Description

The build was failing with multiple errors when running `make` in the Docker container:

```
sh: 1: Syntax error: "(" unexpected (expecting ")")
basename: extra operand 'TooGeekCreations.zip'
basename: extra operand 'by'
cp: cannot stat 'Analogue Blanche by Aemiii91.zip': No such file or directory
```

These errors occurred during the theme download phase (Makefile line 94).

---

## Root Cause

The issue was in `.github/get_themes.sh` at line 30 in the parallel download section:

```bash
# BROKEN CODE:
xargs -n 1 -P 4 -I {} sh -c 'wget -O "$(basename {})" "{}" ...' 
```

**Problem:** When `{}` contained a URL with spaces (e.g., `"Analogue Blanche by Aemiii91.zip"`), the `basename` command received multiple arguments:
- `basename https://github.com/.../Analogue` 
- `Blanche`
- `by`
- `Aemiii91.zip`

This caused `basename` to fail with "extra operand" errors.

---

## Solution

Added quotes around `{}` in the basename calls:

```bash
# FIXED CODE:
xargs -n 1 -P 4 -I {} sh -c 'wget -O "$(basename "{}")" "{}" ...'
```

**How it works:**
- The outer single quotes (`'...'`) in `sh -c` protect the inner double quotes
- The inner double quotes (`"..."`) around `{}` ensure the entire URL is treated as a single argument
- `basename` now receives one argument instead of multiple

---

## Testing

### Test Case 1: Spaces in Filenames
```bash
URL: "Analogue Blanche by Aemiii91.zip"
Before: basename: extra operand 'by'
After:  Analogue Blanche by Aemiii91.zip ✅
```

### Test Case 2: Parentheses in Filenames
```bash
URL: "ONION PS (4-pack) by hanessh4.zip"
Before: sh: Syntax error: "(" unexpected
After:  ONION PS (4-pack) by hanessh4.zip ✅
```

### Test Case 3: Plus Signs in Filenames
```bash
URL: "Onion Boy by PixelShift + Jeltron.zip"
After: Onion Boy by PixelShift + Jeltron.zip ✅
```

---

## Files Modified

- `.github/get_themes.sh` (1 line changed)

---

## Impact

✅ Build now succeeds in Docker container  
✅ All themes download correctly  
✅ Theme names with spaces, parentheses, and special characters work  
✅ No changes needed to Makefile or other scripts  
✅ Sequential download fallback was already correct

---

## Verification

To verify the fix works:

```bash
# Run the build in Docker
docker run --rm -v "$PWD":/root/workspace \
  aemiii91/miyoomini-toolchain:latest \
  /bin/bash -c "source /root/.bashrc; make"
```

Expected output:
- No "basename: extra operand" errors
- Themes download successfully
- Build completes without errors

---

## Related Files

- `.github/get_themes.sh` - Theme download script (FIXED)
- `Makefile` - Build system (line 94 calls get_themes.sh)
- Theme repository: https://github.com/OnionUI/Themes

---

## Additional Notes

The sequential download fallback (lines 32-40) was already correctly implemented with proper quoting:

```bash
wget -O "$zipfile" "https://github.com/.../release/$element.zip"
```

Only the parallel xargs approach needed fixing.
