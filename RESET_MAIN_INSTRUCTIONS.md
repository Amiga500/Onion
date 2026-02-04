# Instructions to Reset Main Branch to Match OnionUI/Onion

## Summary
I have successfully prepared your local main branch to match the upstream OnionUI/Onion repository. However, due to system limitations, I cannot push directly to the main branch. You will need to complete the final step manually.

## Current Status
✅ Local main branch has been reset to commit `07505ea5` (matches OnionUI/Onion)  
✅ Your PRs to OnionUI/Onion are not affected (they remain on their separate branches)  
⚠️ The remote main branch still needs to be updated

## Remote Branch Status
- **Current remote main (Amiga500/Onion)**: `4c283bd` - "Correct log file name to 'retroarch.log'"
- **Upstream main (OnionUI/Onion)**: `07505ea5` - "Update RA_SUBVERSION in Makefile"
- **Local main (prepared)**: `07505ea5` - "Update RA_SUBVERSION in Makefile" ✓

## To Complete the Reset

You need to force push the local main branch to your fork. Run these commands:

```bash
git checkout main
git push origin main --force
```

## Why Force Push is Needed

The current main branch in your fork (`4c283bd`) has diverged from the upstream. To align it with OnionUI/Onion, we need to replace the history, which requires a force push.

## Your PRs Are Safe

This reset will NOT affect any Pull Requests you have open to OnionUI/Onion because:
1. PRs are based on their source branches (not main)
2. Those branches remain unchanged
3. GitHub PRs track specific commits, not the main branch

## Alternative Approach (If You Cannot Force Push)

If force pushing is not an option, you can:

1. Delete the main branch from your fork on GitHub
2. Recreate it from OnionUI/Onion:
   ```bash
   git push origin :main  # Delete remote main
   git push origin main   # Push the new main
   ```

Or use the GitHub web interface:
1. Go to your fork settings → Branches
2. Change the default branch temporarily (if main is default)
3. Delete the main branch
4. Create a new main branch from OnionUI/Onion

## Verification

After pushing, verify with:
```bash
git ls-remote origin refs/heads/main
```

The output should show: `07505ea58c7bba698d6b9220ff43946a43cac76b`
