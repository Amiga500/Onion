# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.1 rc4**, on top of 1.1.0 (the source in
`third-party/open-mainui`).

Since rc3:

- Changing the Display page quickly no longer sometimes ends in "Could not
  save system.json": a save waits briefly while keymon reads the file.
- The Wi-Fi page says "Connecting..." while it joins a network, then
  "Connected" or why it couldn't, and choosing the network again meanwhile
  asks for its password.

From rc3:

- In Apps, titles sit a little higher above their descriptions, and text
  runs closer to the right edge on rows that aren't Favorites.

From rc2:

- A button pressed together with MENU, such as MENU+B for the blue light
  filter, no longer also acts in MainUI.
- The Display page (Lumination, Hue, Saturation, Contrast) shows only B's
  hint.
- When changing colours finds the display driver broken, MainUI asks, then
  closes and runs RedOnion's `disp_csc_reset.sh --repair` before it starts
  again. An extra layer: runtime.sh still repairs after a game.

From rc1:

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
