---
slug: /dev/github-actions
---

# GitHub Actions Workflows

![](https://user-images.githubusercontent.com/7110113/184558441-dc2783c1-0447-489d-9bde-b99d63b6d4b7.png)


## Overview

The Onion project uses GitHub Actions to automate building and releasing. When you trigger a workflow, it takes time for the build process to complete and generate the downloadable .zip files.


## Available Workflows

### Pre-release Workflow

The pre-release workflow creates development builds with the latest changes from the main branch.

**How to trigger:**
1. Go to the [Actions tab](https://github.com/OnionUI/Onion/actions/workflows/pre-release.yml) in the repository
2. Click "Run workflow"
3. Select the branch (usually `main`)
4. Click "Run workflow" button

**Expected timing:**
- The workflow typically takes **10-15 minutes** to complete
- You will see the source files appear immediately when the workflow starts
- The .zip file will only appear **after the workflow completes successfully**

**Where to find the build:**
- Once complete, the .zip file will be available on the [Releases page](https://github.com/OnionUI/Onion/releases/tag/latest) under the "latest" pre-release
- The file will be named: `Onion-v{VERSION}-{COMMIT}.zip`


### Tagged Release Workflow

The tagged release workflow creates official stable releases when a version tag is pushed.

**How to trigger:**
1. Create and push a version tag (e.g., `v4.4.0`)
2. The workflow automatically starts

**Expected timing:**
- Similar to pre-release: **10-15 minutes** to complete
- Creates a draft release that requires manual publishing

**Where to find the build:**
- The .zip file appears in the draft release on the [Releases page](https://github.com/OnionUI/Onion/releases)
- The file will be named: `Onion-v{VERSION}.zip`


## Build Process Steps

When you trigger a workflow, the following steps occur:

1. **Checkout** (~30 seconds): Downloads the source code and submodules
2. **Setup** (~1 minute): Prepares the build environment
3. **Build** (~8-12 minutes): Compiles all components, apps, and emulators
4. **Package** (~1 minute): Creates the .zip file from the built components
5. **Upload** (~30 seconds): Uploads the .zip file to the release

**Total time: 10-15 minutes typically**


## Troubleshooting

### The workflow has been running for over 30 minutes

If a workflow runs longer than expected:
1. Check the workflow run logs in the Actions tab
2. Look for any error messages or failing steps
3. The build may have encountered compilation errors
4. You may need to cancel and retry the workflow

### I only see source files, no .zip

This is normal behavior during the build process:
- Source files are committed to the repository
- The .zip file is only created and uploaded **after** the entire build completes
- Wait for the green checkmark indicating workflow success
- Then check the Releases page for the .zip file

### The .zip file is missing even after workflow success

If the workflow shows success but no .zip appears:
1. Check the workflow logs for upload errors
2. Verify you're looking at the correct release (latest vs tagged)
3. Sometimes browser caching can hide new releases - try refreshing
4. Check that the `marvinpinto/action-automatic-releases` step completed successfully

### Build failures

If the workflow fails:
1. Click on the failed workflow run
2. Expand the failed step to see error details
3. Common issues include:
   - Compilation errors in modified code
   - Missing dependencies or submodules
   - Insufficient permissions or quota issues


## Monitoring Build Progress

You can monitor your workflow in real-time:

1. Go to the [Actions tab](https://github.com/OnionUI/Onion/actions)
2. Click on your running workflow
3. Watch the step-by-step progress
4. Each step will show as:
   - 🟡 Yellow (running)
   - ✅ Green (success)
   - ❌ Red (failure)

Once all steps show green checkmarks, your .zip file will be available on the Releases page within a few seconds.
