# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.0 rc3** (video previews in game lists, a very
early beta).
`MainUI.md5` is its checksum, checked by the build.

When this folder has a `MainUI`, `make dist` / `make release` install it
instead of compiling `third-party/open-mainui` (src/openMainUI/openMainUI.mk).
Delete the folder to build Open MainUI from source again.

Not for a release: a release is built from the source subtree.
