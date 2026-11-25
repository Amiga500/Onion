# Open MainUI, prebuilt (test builds only)

`MainUI` here is a binary given by @robcodedev before its source is on
GitHub: **Open MainUI 1.1.1 rc11**, on top of 1.1.0 (the source in
`third-party/open-mainui`).

Since rc10:

- Settings › Sleep has its own icon, `skin/icon-sleep-48.png` (it used
  About's, `icon-device-info-48.png`). Silky RedOnion and the stock skin
  (`miyoo/app/skin`, the fallback for themes without it) have one.

From rc10:

- Stopped with SIGTERM (as keymon would), MainUI saves the screen it is on
  (`mainui-screen.json` in its handoff directory, `/tmp` in RedOnion) and
  opens it again at its next start. keymon still stops MainUI with SIGKILL
  to open the GameSwitcher: that needs a keymon change, and the exact
  screen is not kept until then (the screen after a GameSwitcher stays as
  before). The only other signal RedOnion sends it is `app_sorter.sh`'s
  SIGINT, when MainUI is running (not from Tweaks, where it is not); in
  the 1.1.0 source SIGINT and SIGTERM take the same path.

From rc9:

- Lumination missing or invalid in system.json now falls back to 10 on
  the Mini Flip and Mini+ and 7 on the Mini, as in RedOnion's
  `res/miyoo<ID>_system.json` defaults (it was 7 on every model).

From rc8:

- `gallery.direct_from_list` in game-menu.json (off by default): RIGHT on
  a game in a list opens its gallery straight away instead of Game Info.
  Needs `gallery.right_shortcut` (on by default). Closes
  https://github.com/Amiga500/Onion/issues/349.

From rc7 (edge cases found in a code review):

- The Wi-Fi networks are saved again once they are enabled.
- The screen stays restored when a Search return finds no Search.
- Reading the About page leaves system.json and keymon's memory alone.
- A console's cache is rebuilt when a folder row is outside its ROM
  folder.
- X's or Y's release action is cancelled when another button is pressed.
- Two small races between threads are closed.
- The frame timer stops while a PDF manual is open.
- The consoles' game counts are read again after a refresh.

From rc6:

- A theme can give the console carousel and the console list their own
  pictures, and `carousel.scale` makes the selected picture smaller.
- The carousel slide starts once its consoles are ready, so the animation
  always shows.
- A refused Delete ROM no longer blocks the list's other games.
- MainUI says when the Wi-Fi switch cannot be saved.
- List rows are drawn without reading the cache again, a ROM list's
  position is written only when it changes, and its videos folder is read
  off the input thread.
- Save states: an empty one is not resumed, a game's save states are
  offered only with its core installed, and a launch known to fail is
  refused before a resume changes the saves.

From rc4:

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
