# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.0 rc17** (video previews in game lists, media
gallery in Game Info, Games/Expert as grid, carousel or list, Game Info
auto scroll; rc17 adds the Saved Games layout,
game-menu.json `saved_games.layout` "framed" or "overlay"; rc16 darkens the Saved Games labels again and makes Wrap
around Off stop the grid's Up/Down too; rc15 slides in 180 ms and turns Saved Games' console ratio
on by default, with a title bar over the save preview; rc14 made Saved Games' Left shortcut and Auto Save optional
and refines grid Up/Down; rc12 added the Game Info menu with Saved Games, maps,
walkthroughs and PDF manuals, and Left in game lists for Saved Games;
rc10 added VSync for the carousel; rc9 easing, repeat_ms and X/Y
"disable"; a release candidate).
`MainUI.md5` is its checksum, checked by the build.

When this folder has a `MainUI`, `make dist` / `make release` install it
instead of compiling `third-party/open-mainui` (src/openMainUI/openMainUI.mk).
Delete the folder to build Open MainUI from source again.

Not for a release: a release is built from the source subtree.
