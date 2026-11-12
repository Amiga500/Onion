# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.1 rc1**, on top of 1.1.0 (the source in
`third-party/open-mainui`):

- A console's number of games no longer counts Onion's `~` entries, such as
  ~Filter and ~Refresh roms.
- After GameSwitcher and back, MainUI opens the screen you were on: the main
  menu, a section, or a console's ROM list at its top.
- The first Game Info in a console with a large gamelist.xml opens faster.
- MainUI starts faster when it comes back from a game (main menu icons are
  loaded only when needed).

`MainUI.md5` is its checksum, checked by the build.

When this folder has a `MainUI`, `make dist` / `make release` install it
instead of compiling `third-party/open-mainui` (src/openMainUI/openMainUI.mk).
Delete the folder to build Open MainUI from source again.

Not for a release: a release is built from the source subtree.
