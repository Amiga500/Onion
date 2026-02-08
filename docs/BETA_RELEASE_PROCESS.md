# Beta Release Process

## Overview
Beta releases are created using GitHub Actions workflow_dispatch (manual trigger) instead of git tags. This prevents duplicate releases and follows the approach used by OnionUI/Onion.

## Version Format

The version number in the `Makefile` follows this format:
```
VERSION=X.Y.Z-beta-DD_MM_YYYY
```

Where:
- `X.Y.Z` is the semantic version (e.g., 4.4.0)
- `beta` indicates this is a beta release
- `DD_MM_YYYY` is the build date in Italian format (e.g., 08_02_2026 for February 8, 2026)

**Important Notes:**
- The date format uses underscores (`_`) as separators
- The date is in DD_MM_YYYY format (Italian style)
- The date portion is for display purposes only; version comparison uses only the X.Y.Z portion
- Update the date in `Makefile` when creating a new beta release

**Example:**
```makefile
VERSION=4.4.0-beta-08_02_2026
```

This will display as "Onion V4.4.0-beta-08_02_2026" in OTA updates and the system.

## Creating a Beta Release

1. Go to the Actions tab in your repository
2. Select the "Pre-release" workflow from the left sidebar
3. Click the "Run workflow" button (on the right)
4. Select the branch (typically `OniOpus46` or your main branch)
5. Click "Run workflow" to start the build

The workflow will:
- Build the release with the current version and commit SHA
- Create or update a release with tag `latest`
- Mark it as a pre-release (beta)
- Upload the release assets

## Why This Approach?

### Previous Issue
The old approach used a `beta` git tag to trigger releases. This caused problems:
- Each push to the `beta` tag created a **duplicate** release instead of updating the existing one
- Multiple beta releases with the same version but different commit SHAs accumulated
- Users were confused about which release to download

### Current Solution
Using `automatic_release_tag: latest`:
- The `latest` tag is a **moving tag** that always points to the most recent beta release
- Each new beta release **updates** the `latest` release instead of creating a duplicate
- Only one beta release exists at any time
- OTA updates can reliably fetch the latest beta via the `latest` tag

## Cleanup: Delete the Old Beta Tag

If you have an existing `beta` tag, it should be deleted to prevent confusion:

```bash
# Delete local tag
git tag -d beta

# Delete remote tag
git push origin :refs/tags/beta
```

Or delete it via the GitHub web interface:
1. Go to the Tags page in your repository
2. Find the `beta` tag
3. Click the delete button (trash icon)

## Comparison with OnionUI/Onion

This approach exactly matches the official OnionUI/Onion repository:
- No git tag triggers for beta releases
- Only workflow_dispatch (manual trigger)
- Uses `latest` as the automatic_release_tag
- One beta release at a time, always updated

## Stable Releases

Stable releases continue to work as before:
- Triggered by pushing tags like `v*` or `stable`
- Managed by the `tagged-release.yml` workflow
- Creates permanent, non-prerelease releases

## Known Issues (Resolved)

### Extra Dash in Release Filename
**Issue**: Previous versions of the workflow generated filenames with an extra dash before `.zip` (e.g., `Onion-v4.4.0-beta-20260120-.zip`)

**Cause**: The workflow tried to extract the commit SHA from `origin/main`, but this repository uses `OniOpus46` as the default branch. This resulted in an empty SHA_SHORT variable.

**Resolution**: Fixed in the workflow to use `HEAD` instead of `origin/main` for SHA extraction. New releases will have proper filenames like `Onion-v4.4.0-beta-08_02_2026-abc1234.zip`.

### Date Format Change
**Change**: The build date format has been changed from YYYYMMDD (e.g., 20260120) to Italian format DD_MM_YYYY (e.g., 08_02_2026).

**Reason**: To make the date more readable for Italian users and follow Italian date conventions.

**Impact**: 
- OTA updates continue to work correctly as version comparison only uses the semantic version portion (X.Y.Z)
- The date is only displayed to users and not used for version comparison
- Old and new format releases can coexist without issues
