# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.0 rc10** (video previews in game lists, media
gallery in Game Info, Games/Expert as grid, carousel or list, Game Info
auto scroll; rc10 adds VSync for the carousel and better pacing; rc9 added easing, repeat_ms,
and X/Y "disable"; a release candidate).
`MainUI.md5` is its checksum, checked by the build.

When this folder has a `MainUI`, `make dist` / `make release` install it
instead of compiling `third-party/open-mainui` (src/openMainUI/openMainUI.mk).
Delete the folder to build Open MainUI from source again.

Not for a release: a release is built from the source subtree.
