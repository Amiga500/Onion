# Navigation

How the cursor and the visible window move in Open MainUI. Where this differs from stock MainUI, it is on purpose: the aim is a UI that behaves predictably, not a copy of every stock quirk.

## Button mapping

At start, as stock does, MainUI hands the `keymap` in `system.json` (default `L2,L,R2,R,X,A,B,Y`) to the device's button driver, which remaps the physical buttons for MainUI and every other program. A remapped button, such as X set to act as B, then does on every screen what the button it is mapped to does.
A missing `keymap`, or one that is not eight comma-separated names from the default (`L2`, `L`, `R2`, `R`, `X`, `A`, `B`, `Y`, repeats allowed), hands over the default. Stock hands over any string, which could leave the buttons unusable everywhere until MainUI starts again.
The tables below name the buttons after mapping.

## When buttons act

A, B and X act when pressed; stock acts on their release. Acting at once makes moving through the menus faster, and a held A cannot answer a question twice (see [Confirmations and messages](#confirmations-and-messages)). Y, Select, Start and Menu act when released, as in stock: Onion's keymon uses them for shortcuts and combos, which must not also act in MainUI.
B, and X where it acts as B, act once per press: held down, they do nothing more until released, so a held B goes back one level, not several.

## X

X acts as B (back, close, cancel) on every screen, as stock does; in the on-screen keyboard it types a space. Search in a ROM list is in the context menu (Select or Menu). A shortcut set for X in Tweaks (Button shortcuts > MainUI: X button) is run by Onion's keymon instead.

A main-menu tab Onion leaves in `/tmp/state.json` opens when MainUI starts, as with stock: at boot the startup tab set in Tweaks (Recents, Favorites, Games, Expert or Apps), and Expert for Onion's Expert app (Expert in Apps, or Expert set as the X or Y shortcut in Tweaks), also after a return from Apps, where MainUI would otherwise reopen Apps.
A tab opens also when it is hidden on the main menu, and B goes back to the main menu with that tab selected. As stock does, MainUI removes `state.json` once it has read it, so a later start without a new one, such as after keymon's GameSwitcher, is on the main menu; stock opens the main menu there too.

Games and Expert each open on the console last selected in them, also after MainUI was started again, such as after keymon's GameSwitcher.
MainUI keeps both in `/tmp/console-position.json`, by the console's config file (its folder without one), written as soon as the selection changes, so it is lost only when the device is turned off.
A return after a launch or from Search opens what it names instead, and a console that is gone gives the first one. The console's ROM list is not opened again.

On a launch, MainUI writes `state.json` with the main menu and the open tab only, which is what Onion's scripts read from it (Game List Options and Random Game take the tab). Stock writes every open screen, down to the ROM folder. MainUI comes back to that folder, with each parent folder's selection, from its own return file (`mainui-return.json`), written at the same moment.

A game is launched with `LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "<launch script>" "<ROM>"`, the command Onion's own launchers write too (GameSwitcher, Random Game, the start after boot and runtime.sh's own rewrite).
Stock MainUI leaves the preload out for a console with `noaudiofix` (Onion's NDS has it) or with `audiofix` off in `system.json`, adds `PADSP_EN_FLIP=1` for an app with `autoflip`, and offers to create a swap file for a console with `useswap`.
Onion's launchers do none of that and Onion turns swap on at every start, so MainUI launches as they do and the same game starts the same way from either.
Neither file is written when keymon kills MainUI (for GameSwitcher, say), and stock's isn't either.
Onion's `reset_list.sh` (Game List Options' list reset, also run by the ScummVM and Ports importers) sets the last screen's position in `state.json` to the top: in stock that is the ROM list, which reopens at its first game.
Here that screen is the tab, and the return file is followed instead, so the rebuilt list reopens on the game launched, or at the same row, kept within the list, when that game is gone.

## Console grids

The Games grid shows 4 columns by 2 rows per page, the Expert grid 3 by 3.

| Button | Moves to |
| --- | --- |
| Right / Left | The next / previous console in reading order: along the row, then on to the start of the next row, then on to the next page. At the end of the list it wraps to the first console, and Left on the first console wraps to the last. |
| Down / Up | The console below / above in the same column, also onto the next or previous page. Where there is none, in the first row, the last, or a column the short last row does not reach, it stays: it never moves sideways or wraps. With the stock navigation, below, it goes round the column within the page, or with `"wrap": false` stops at its ends. |
| R / L, R2 / L2 | The same position one page forward / back, stopping at the first and last console. Stock pages with R2 and L2 only. |

**Differs from stock:** in stock MainUI, Right on the last console of the top row opens the next page at its top-left console. Here Right always takes the next console in order, which is the first console of the row below on the same page. Every console is then reached by pressing Right repeatedly, and the cursor never skips the rest of a page.

With `"grid": {"navigation": "stock"}` in [`console-view.json`](#carousel-and-list), Right at the end of a row goes to the first console of the same row on the next page, and Left at its start to the same row's last console on the page before, as stock does from the top row; on a short page it lands on the last console.
Up and Down then stay on the page, going round its column: Up from the first row goes to the column's last console on that page, and Down from there back to the first; a column with one console on a short page stays.
Past the last page Right goes round to the first, unless `"wrap": false`, which also stops Right and Left at the first and last console in the default navigation. Only the top row's move is known from stock; the lower rows follow the same rule.

### Carousel and list

`.tmp_update/config/console-view.json` can show the consoles of Games, Expert or both as a carousel or a list instead of the grid. The grid stays the default. As in `main-menu.json` and `game-menu.json`, comments (`//`, `/* */` and `#`) and trailing commas are allowed. Every key is optional:

```json
{
  "games": "carousel",
  "expert": "list",
  "thumbs": "/mnt/SDCARD/Thumbs",
  "count": true,
  "menu": ["refresh_all", "refresh"],
  "carousel": {
    "labels": true,
    "selection": "#FFFFFF38",
    "selection_padding": 12,
    "scale_unselected": 0.7,
    "opacity_unselected": 0.5,
    "gap": 40,
    "offset_y": 0,
    "animation": "slide",
    "duration_ms": 180,
    "wrap": true
  },
  "grid": { "navigation": "linear", "wrap": true },
  "list": { "folder_icon": true, "thumbs": true }
}
```

| Key | For | Values | Without it |
| --- | --- | --- | --- |
| `games`, `expert` | that section's console page | `grid`, `carousel` or `list`, in lowercase | `grid` |
| `thumbs` | carousel and list | a folder of console pictures, below, with `_default.png` for consoles without their own | the theme's icons |
| `count` | carousel and list | `false` hides the number of games | shown |
| `menu` | every view, also the grid | the context menu's rows, below | the default menu |
| `carousel.labels` | carousel | `false` hides the console names | shown |
| `carousel.selection` | carousel | `false` or `""` for no background, or `"#RRGGBB"` (opaque) or `"#RRGGBBAA"` for a thumbnail's | the theme's, or the plain default, about 12% of the selected names' colour |
| `carousel.selection_padding` | carousel | pixels of plain background round the selected thumbnail, 0 to 40 | 12 |
| `carousel.scale_unselected` | carousel | the neighbours' size, thumbnails and theme icons' cells alike, 0.6 to 1 of the selected one's | 0.7 |
| `carousel.opacity_unselected` | carousel | the neighbours' pictures' opacity, 0 (not drawn) to 1 (as the selected one) | 0.5 |
| `carousel.gap` | carousel | pixels between a thumbnail and the next console, 0 to 100 | 40 |
| `carousel.offset_y` | carousel | pixels to move the pictures, names and counts down, or up when negative, -60 to 60 | 0 |
| `carousel.animation` | carousel | `"none"`: a new selection shows at once | `"slide"`: it slides into place |
| `carousel.easing` | carousel | `"smooth"`: ease in and out during a slide, instead of the faster-starting default | default ease-out |
| `carousel.duration_ms` | carousel | how long a slide takes, 80 to 500 ms | 180 |
| `carousel.repeat_ms` | carousel | least ms between moves with Right and Left, 0 to 400; while slides are on, a move also waits for the slide | 0: one move per slide, or with slides off no limit |
| `carousel.vsync` | carousel | `false`: draw each frame without waiting for the screen's refresh. On the device only; the wait stops by itself where the screen refuses it or it takes over 50 ms | `true`: wait for the refresh before each frame is copied |
| `carousel.wrap` | carousel | `false`: nothing left of the first console or right of the last, and Right, Left, R2 and L2 stop there | `true`: the row goes round, the last consoles left of the first, and the reverse |
| `grid.navigation` | grid | `"stock"`: Right and Left page at a row's ends, and Up and Down go round the page's column, below | `"linear"` |
| `grid.wrap` | grid | `false`: Right and Left stop at the first and last console, and with stock navigation Up and Down at the column's ends | `true` |
| `list.folder_icon` | list | `false` hides the folder icon | shown |
| `list.thumbs` | list | `false`: no console covers in the list, though the carousel still shows its pictures | shown with `thumbs` |

Any other value is taken as missing; numbers outside their range are kept to it.
`thumbs` is written as paths in `config.json` are, from `/mnt/SDCARD/` or from the SD card's root, and holds one PNG per console named by its folder in `Emu` in lowercase: `sfc.png` for `Emu/SFC`, `fc.png` for `Emu/FC`. A console without one shows `_default.png` from the same folder, or, without that either, its theme icon, which the list also shows as its cover then.
`menu` orders and hides the console page's context menu as [`game-menu.json`](#game-list-context-menus) does game lists': `refresh_all` is Refresh all roms, `refresh` Refresh roms (never for the Search console), and `[]` means no menu.

The carousel shows the selected console in the middle and two neighbours either side, the next ones cut by the screen's edges, in the grid's order, and by default round the ends, each console once however few there are.
A console with a thumbnail shows it on a plain, faint background in the colour of the selected names, or in the `selection` colour, with its name beneath and, once the console has a ROM list cache, its number of games beneath that, such as `(4562)`, not in bold.
Thumbnails are at most 560 by 260 pixels, or 560 by 300 with the name or the count hidden: a larger one is scaled down to fit, keeping its proportions, and a smaller one is never enlarged. With nothing beneath, the picture is centred.
The room for the name and the count comes from the theme's grid font (Expert's in Expert): where the tallest picture and its lines would not fit between the header and the footer, the row moves up or the picture gets smaller, so the lines never overlap the picture or the footer.
A console without one is drawn as a cell of the grid: the theme's tile (`skin/bg-game-item-f.png` for the selected console and `-n.png` for the others in Games, `skin/bg-ra-list-item.png` for the selected one in Expert), its icon and name placed as the grid places them, and the number of games beneath. Its name and the count use the grid's colours.
`"selection": false` drops the tiles and the plain background alike.
The neighbours show at `scale_unselected` of their size, thumbnails and theme icons' cells alike, a cell's tile and icon together, and their pictures at `opacity_unselected`, on the same middle line as the selected one.
Names and numbers of games are never scaled: only the selected console shows them, at full size.
The pictures are read and scaled in the background, nearest first, and up to 16 consoles' are kept while MainUI runs, also through the main menu and the grid: moving never waits for a picture.
On the device the carousel first shows once the selected console and its two nearest neighbours are ready, and it is drawn at most 25 times a second. A menu or message opened before then shows at once, over the carousel without its pictures.
With `"animation": "slide"`, moving slides the row from where it was into its new place over `duration_ms`, slowing at the end (or easing both in and out with `"easing": "smooth"`); the console moved to takes the selected size and opacity halfway. A press during a slide carries on from where the row is, and round the ends a console comes in from the side the row moves from.
The name and number of games stay centred beneath the selected console while the pictures slide, and change to the new selection halfway through the slide, as it takes the selected size, or at once without a slide.
The count is read from the cache only when a console is selected. The counter at the bottom right gives the console's position instead of the page.
Right and Left move to the next and previous console, wrapping round at the ends as in the grid, or stopping there with `"wrap": false`.
Right and Left move once per slide while slides are on, or every `repeat_ms` when that is longer, and every `repeat_ms` with slides off. The first press moves at once; a press made during a slide waits for it to end instead of being lost, and a held button's repeats leave nothing to catch up on once it is released.
R2 and L2 move three consoles, wrapping round too or stopping at the ends, and R and L as far as a grid page, both jumping there without a slide. Up and Down do nothing.

The list shows the consoles as a game list does its games, with the theme's rows, row count (`.romListRows`), selection and font, and the theme's folder icon for each console. Names are shown without leading and trailing spaces, keeping the place those spaces sort them to.
A selected name too long for its row scrolls as titles do in game lists (`.romListTitleScroll`). With `thumbs`, the selected console's thumbnail shows where a game's cover does, read and kept as covers are, unless `list.thumbs` is `false`.
Up and Down move to the next and previous console, wrapping round at the ends; Left and Right, R and L and R2 and L2 move a page.
The counter at the bottom right gives the selected console's number of games, such as `(762)`, as for a folder, once the console has a ROM list cache, and nothing without one.

In both, A opens the console, B goes back, and every other button acts as in the grid.
A file that is not a JSON object shows the grid, which the log notes once. The file is read when MainUI starts and never written.

#### Console thumbnails

A transparent PNG of 500 by 720 pixels suits both views:

| View | Shown at |
| --- | --- |
| List (250 by 360 cover area) | 250 by 360 |
| Carousel, name and count shown | 181 by 260 |
| Carousel, name or count hidden | 208 by 300 |
| Carousel neighbour, at the default 0.7 | 127 by 182 |

Pictures are only ever scaled down, keeping their proportions, never cropped. Both views scale by picking pixels, so fine lines and small text can look rough.
Transparent padding counts as part of the picture, so keep it to a few percent: unused space makes the console look smaller.
A layout that works: the console's name or logo in the top fifth, the console large below it, a little room at the bottom. Wide consoles come out shorter than handhelds; a steeper angle, or a controller beneath, evens out the row.

## Lists

This applies to ROM lists, Favorites, Recents, Apps and Search results.

| Button | Moves to |
| --- | --- |
| Down / Up | The next / previous row. From the last row it wraps to the first, and from the first to the last. |
| R / L | One page (a screen of rows) down / up, stopping at the first and last row. |
| Right | Opens the details of the selected game in game lists. |
| Left | Opens the selected game's [Saved Games](#saved-games) in game lists, when it has any and `left_shortcut` in `game-menu.json` is `true`; else, as before, starts its title scrolling again. A title already scrolling is started again by the first Left, and the next Left on that game opens them. |
| Y | Launches the selected game as A does, in ROM lists, Favorites, Recents and Search results, as stock does. With Onion's default keymap (`mainui_button_y` set to `glo` in `.tmp_update/config/keymap.json`), Onion then opens Game List Options for that game instead of starting it. Keymon sets the flag for that (`/tmp/launch_alt`) on Y, but not while its cached state does not say MainUI is running (after boot, or just after a game), so MainUI sets it too. With no Y shortcut (Tweaks shows "A button"), Y launches as A does. With `fav` as the Y shortcut, Y adds the game to Favorites, or removes it when it is one, instead of launching it. With an app or tool as the Y shortcut, keymon runs it; should keymon miss the press, Y does nothing. If `keymap.json` exists but cannot be read, Y does nothing either, without a message; the log says why (`[launch] Y ignored`). Y on a folder or `..` does nothing. Game List Options removes the first line of `recentlist.json`, taken to be this launch, so Y puts the game's line first and leaves the other lines as they are: backing out leaves Recents unchanged. That line is written before the launch: if it cannot be, nothing is launched (A still is). When a Y launch fails, keymon's Y flag is cleared, and every other launch clears it too: keymon sets it on any Y press, also one MainUI ignores (on a folder, say), and only B or X clear it, so A starts the game instead of opening Game List Options. Y acts when it is released, as stock does: with an app or tool set as the Y shortcut in Tweaks, keymon runs it as Y goes down, and MainUI, killed by then, neither launches the game nor puts it in Recents. In Apps, with `fav` as the Y shortcut, Y adds the selected app to Favorites, or removes it when it is one. Elsewhere (main menu, console grids, Apps with another shortcut, Settings) Y does nothing; stock opens the selection there as A does. That is left out for now: with Onion's default Y shortcut, keymon arms Game List Options on any Y press, so what Y opened there could start with it. |
| X | Acts as B, as stock does. With a MainUI shortcut as the X shortcut (`mainui_button_x` in `keymap.json`), X on a game in ROM lists, Favorites, Recents and Search results does that instead, when X is released: `fav` adds the game to Favorites or removes it, as Y can, and `glo` opens Game List Options for it, as Y does by default. Keymon sets its Game List Options flag only for Y, and clears it as X goes down, so MainUI sets it after the release. In Game Info, `fav` on X or Y adds or removes the game shown, and Game Info closes when that removes it from the Favorites list it was opened from. With `fav` as the X shortcut, X in Apps adds the selected app to Favorites or removes it, as Y can. With `fav` or `glo` as the X shortcut, X does nothing anywhere else (folders, `..`, console grids, menus, dialogs, Apps for `glo`, and Game Info for `glo`), as Y with a shortcut does only that. With no X shortcut, or an app or tool (keymon's), X is B. On the on-screen keyboard X types a space and Y erases, whatever their shortcuts. |
| Select | Opens the context menu when Select is released, as stock does with Onion's keymon, and closes it. Not when another button went down while Select was held: Select with L2 or R2 is keymon's brightness, and Select, Start and L1 or R1 its CPU clock. While Select or Start is held, MainUI ignores the other buttons, which are keymon's, so a combo neither pages nor jumps in a list. Keymon still sends a Menu press after the CPU clock combo, and on the Mini Plus and Flip after Select with L2 or R2; unlike stock, that does not open the menu here. |
| Start | Reloads the console grid or ROM list when released, as stock acts on release. Not after a combo: Start with L2 or R2 is keymon's volume boost. |
| Menu | Opens the context menu, as Select does, when Menu is released. With Onion's default "Context menu" long press (Tweaks > Button shortcuts), keymon sends MainUI only that release, as for stock. A second release, such as the physical one after keymon's, does not close the menu; B or Select does. Keymon also sends an L1 press after its release, after each repeat of Menu while it is held and after the physical release. Every L1 within 3 s of a Menu event, with no other key between, is ignored with its repeats, so holding Menu does not move the selection in a list. |

Either button can be turned off with `false` or `"disable"` (exactly) as its shortcut in `keymap.json`, such as `"mainui_button_x": false`: its presses, holds and releases then do nothing anywhere in MainUI, including Game Info, the gallery, Apps, Settings and dialogs, and a press held as the button is turned off does nothing on release.
On the on-screen keyboard it still works, X typing a space and Y erasing. The other button keeps its own shortcut. MainUI cannot stop keymon acting on the press: keymon must take these values as no shortcut too.

The main menu's context menu shows Search, Tweaks and Themes only while their app (`App/Search`, `App/Tweaks`, `App/ThemeSwitcher`) is installed, also when `.tmp_update/config/main-menu.json` lists them, as Settings shows Tweaks and Themes. Installed again, they come back without changing that file.

Apps are listed in the order the SD card lists their folders, as in stock, not by name: on FAT that is usually the order the folders were created in, so Quick Guide may come first.
An empty file named `.appsort` in `.tmp_update/config` sorts them by name instead, with the same case rule as the other lists. Consoles and ROMs are sorted by name (see [CATALOG_CACHE.md](CATALOG_CACHE.md#differences-from-stock)).

The window scrolls only as far as needed to keep the selected row on screen. A row that is already visible never moves the window.

A selected title too long for its row scrolls when `.tmp_update/config/.romListTitleScroll` holds a delay and a speed, such as `700,120` (700 ms, then 120 px/s). It rolls round, its start following its end after a gap.
A third value, `700,120,bounce`, makes it bounce instead: after the delay it travels until its end shows, rests there, travels back, rests again, and repeats.
Each way speeds up smoothly from rest and slows into the end, over about a quarter of a second, and runs at the set speed in between. The rest lasts as long as scrolling 84 px takes at that speed, at least 400 ms and at most 2 s: 720 ms at 120 px/s.
With a cover shown, its end stops at the cover's edge. A title starting with Hebrew or Arabic starts at its right end and moves the other way. `rolling`, nothing or any other third value rolls, as before.

### Video previews

With a file named `.romListVideoPreview` in `.tmp_update/config`, a game that stays selected in a ROM list, Favorites, Recents or Search results plays its video in place of its cover: the gallery's video in `media/videos` ([below](#game-infos-media-gallery)), silently and repeating, fitted to the cover's 250x360 area.
The file holds the delay in milliseconds and the mode, separated by a comma or spaces: `1500`, `800,cover` or `800,fullscreen`. Without a delay it is 1000 ms, so an empty file is `1000,cover`; delays are kept within 0 to 30000, and to at least 100 for `fullscreen`. Without the file nothing plays, as before.
Scrolling past games plays nothing: a video starts only once its game has been selected for the whole delay, and it stops, showing the cover again, as soon as the selection moves or anything opens over the list (Game Info, a menu, a message or Search). It stops while the device sleeps and starts again on waking.
A game without a video, or a video that cannot be played, keeps its cover, without a message. A game with a video but no cover shows the video where the cover would be, and titles stop at its edge as at a cover's.

`fullscreen` (in any case; any other mode is `cover`) plays the video over the whole screen instead, header and footer too, fitted to it on black, also silently and repeating. The list stays until the video's first frame is ready, and comes back if the video cannot play.
B, and X where it acts as B, closes the video and stays in the list; held down, it does nothing more until released. Any other button closes it and does what it does in the list: Up and Down select another game, whose video plays after the delay, A launches the game, Right opens Game Info and Select or Menu the context menu, after which the video stays closed.
A video closed this way does not play again until another game has been selected, also after sleep.

## Game-list context menus

`.tmp_update/config/game-menu.json` can reorder and hide the rows of the context menus in ROM lists, Favorites, Recents and Search results. It cannot add actions. This file, with every ID each section knows, gives the default menus:

```json
{
  "roms": ["launch", "favorite", "delete", "search", "refresh"],
  "favorites": ["move", "create_folder", "sort", "remove_favorite", "rename_folder", "delete_folder"],
  "recents": ["launch", "remove_recent", "clear_recents"],
  "search_results": ["launch", "favorite"],
  "details": ["game_info", "gallery", "saved_games", "maps", "walkthrough", "manual"],
  "gallery": { "right_shortcut": true, "upscale_images": true },
  "saved_games": { "resume_mode": "backup", "left_shortcut": false, "show_auto": false, "upscale_preview": true, "correct_aspect_ratio": true, "layout": "framed", "disabled_rom_dirs": ["ARCADE"] }
}
```

A menu shows the rows listed for its section, in that order, that the selection offers, as without the file: a folder in a ROM list offers only Search and Refresh roms, the Search console offers no Refresh roms, and a Favorites folder offers no Sort or Remove Favorite.
`favorite` is Add to favorites or Remove Favorite, and `move` is Move here while a Favorite is being moved. A row left out is hidden; an empty list means that section has no menu, and Select or Menu does nothing there.

A missing section keeps its default menu. Unknown and repeated IDs are skipped. A missing file gives the default menus, and so does a file that is not a JSON object, which the log notes once (`[menu] ... game-menu.json is not a JSON object`).
`details` orders and hides the rows of Game Info's [media menu](#media-menu-maps-and-manuals) the same way. Game Info always stays in it, first unless the list places it, and leaving out `saved_games` turns [Saved Games](#saved-games) off, Left in game lists too.
`gallery`'s `right_shortcut`, `false`, turns off Right's gallery in Game Info; Select still opens it from the media menu. Anything else keeps it on.
`upscale_images`, `false`, shows an image smaller than the screen at its own size, centred; larger ones are still made to fit. Anything else enlarges them to the screen, keeping their shape. Videos, maps and Saved Games are not affected.
`saved_games`'s `resume_mode` is `backup`, `overwrite` or `swap`; anything else is `backup`. Its `left_shortcut`, `true`, turns on Left's Saved Games in game lists, which are otherwise only in the media menu; `upscale_preview`, `false`, keeps a save's picture at its own size when it fits the screen, instead of enlarging it; `show_auto`, `true`, shows the game's Auto Save among them.
A save's picture shows in its console's screen shape, enlarged or not, unless `correct_aspect_ratio` is `false`: 4:3 for FC, NES, FDS, SFC, SNES, MD, GEN, GENESIS, SMS, MS, PCE, PCECD, TG16, PS, PS1 and PSX, 10:9 for GB, GBC and GG, and 3:2 for GBA, by the name of the ROM folder in any case.
RedOnion's NEOGEO, NEOCD, SEGACD, THIRTYTWOX, SEGASGONE, SATURN, ATARI, FIFTYTWOHUNDRED, SEVENTYEIGHTHUNDRED, EIGHTHUNDRED, ATARIST, COLECO, INTELLIVISION, MSX, AMIGA, AMIGACD, DOS, ZXS, COMMODORE, CPC, PANASONIC, SGFX, SATELLAVIEW, FAIRCHILD and ODYSSEY are 4:3 too; arcade folders and handhelds with other screens keep their pictures' shape.
With `false`, and for other folders, a picture keeps its own shape; anything else keeps the default. Only the picture shown changes, not its file nor RetroArch's settings.
An arcade game's picture shows as RetroArch saved it, never turned: a vertical game's is saved in the same landscape size as a horizontal game's, so the two cannot be told apart, and a vertical game's picture can show on its side or squeezed.
`disabled_rom_dirs` lists ROM folders, by name in any case, whose games have no Saved Games: Left is as before for them, the media menu does not offer them, and their save states are not even looked for; the files themselves stay as they are.
Without it the list is `["ARCADE"]` (see [Cores and save states](#cores-and-save-states)); a list given replaces it, such as `["ARCADE", "SCUMMVM"]`, and `[]` keeps Saved Games everywhere. Entries that are not folder names are skipped.
`layout`, `"overlay"`, shows Saved Games' pictures over the whole screen with their labels on boxes the picture shows through; anything else is `"framed"`, the default, which fits them between the theme's title bar and a footer of their own.
The file is read when MainUI starts and never written. Confirmations still ask as before. The main menu's context menu (`main-menu.json`) and Apps are not affected; the console page's menu is set with `menu` in [`console-view.json`](#carousel-and-list).

## Favorites in Apps

An app that is a Favorite shows the theme's Favorite mark, `skin/ic-favorite-mark.png` at its own size, at the right of its row in Apps, centred in the row. With that image in the theme, every row's name and description stop short of it, so adding or removing a Favorite moves nothing; a theme without it shows no mark, and Apps is as before.
Apps are added and removed with the context menu, as before, or with `fav` as the X or Y shortcut ([Lists](#lists)). Onion's FavFix must keep app Favorites: an older FavFix removed them again after the change to `favourite.json`.

## Game Info's description

L2 and R2 move Game Info's description a page at a time, as before. With a file named `.gameInfoAutoScroll` in `.tmp_update/config`, the description also scrolls down on its own, pixel by pixel, once a game's Game Info has shown for a delay, and stops at its last full page.
The file holds the delay in milliseconds and the speed in pixels per second, separated by a comma or spaces: `1500,30`. Empty, it is `2000,20`. Delays are kept within 0 to 30000 and speeds within 5 to 150. A delay that is not a number keeps both defaults, and a speed that is not one keeps its own.
Before the scrolling starts, L2 or R2 moves a page at once and the scrolling no longer starts. Once it moves, the first L2 or R2 press only stops it where it is, also when held; the next presses move a page from there as before.
A message, the context menu and sleep pause it, and it goes on after them. Opening the gallery or the media menu stops it and puts the description back at the top, where it starts again after the delay on return. Up and Down show another game, whose description starts again after the delay.
Only the description scrolls. Without the file Game Info is as before.

## Game Info's media gallery

Right in Game Info opens a fullscreen gallery of the game's media, when it has any, and only the gallery: without gallery items Right does nothing there, and Select opens the [media menu](#media-menu-maps-and-manuals) for its other media.
With `right_shortcut` `false` under `gallery` in [`game-menu.json`](#game-list-context-menus), Right does nothing there, and the media menu still offers the gallery.
In the gallery, Right shows the next item, and on the last one does nothing, and Left the previous one; Left on the first item goes back to Game Info, as B does from any item, when Right opened the gallery. Opened from the media menu, it stops at both ends, and B goes back.
A video starts once it has been shown for 100 ms, so moving past it with Right or Left starts nothing. On a video, A pauses it (a pause sign shows at the bottom left) and plays it on, and L and R jump about 5 seconds back and forward; paused, it stays paused there.
A jump lands on a keyframe, a picture the video can start from, and shows it at once: forward the first one at least 5 seconds on, back the last one at least 5 seconds back, so it can go further than 5 seconds; past the last one the video starts again.
Videos with a keyframe every second or two, such as `ffmpeg -g 30` makes at 30 frames per second, jump closest to 5 seconds. While the device sleeps a video stands still, and on waking it goes on from there. Select opens the media menu over the gallery. Other buttons do nothing in the gallery, and the X and Y shortcuts do not act there.

The media is in a `media` folder in the console's ROM folder, beside `Imgs`, under the ROM file's name without its extension (not the game's title). Every folder is optional. In this order:

```text
Roms/GB/
├── Game.gb
├── Imgs/
│   └── Game.png              the list's and Game Info's cover, as before; not in the gallery
└── media/
    ├── box2dfront/Game.png   front cover
    ├── box2dback/Game.png    back cover
    ├── box3d/Game.png        3D box
    ├── screenshot/
    │   ├── Game.png
    │   ├── Game-1.png
    │   └── Game-2.png        and on, up to 20 screenshots
    ├── videos/Game.mp4       video
    ├── maps/
    │   ├── Game.png          maps, below; not in the gallery
    │   └── Game-1.png
    └── manuals/
        ├── Game.txt          walkthrough
        └── Game.pdf          manual
```

The folders are named as [Skraper](https://www.skraper.net/) names them by default, so the media it downloads can be copied to the SD card as it comes, its `media` folder beside the ROMs.
Numbered screenshots stop at the first missing number from `-2` on: a missing `-1` does not end them, so screenshots numbered from `-2` show too.
On the SD card's FAT the folder names match in any case; elsewhere they must be lowercase, as above. A game in a subfolder uses the same `media` folder, at the console's ROM folder. The console is found as for Game Info, so the gallery works from ROM lists, Favorites, Recents and Search results, also for consoles that share a ROM folder.
Only these files are looked for, when the gallery opens. The `videos` folder is read only when `Game.mp4` is not there: a video can also be `.mkv`, `.avi`, `.mov`, `.webm` or `.m4v`, its extension in any case, and with more than one the first in that order plays. The `maps` folder is read for the game's maps, their names in any case.
A `media` folder, in any case and at any depth, is never listed as a ROM folder, nor anything in it, as for `Imgs` (see [CATALOG_CACHE.md](CATALOG_CACHE.md#differences-from-stock)).

Images are scaled to fit the screen, keeping their shape, on black. A counter such as `2/5` shows at the bottom right when there is more than one item.
Videos play as soon as they are shown, and repeat. They use the FFmpeg libraries Onion already has for its video player, in `.tmp_update/lib/parasyte`, loaded from there only while a video plays, so MainUI starts without them.
Without them, or for a file they cannot play, the gallery says so instead.
A video's sound plays in place of the theme's music, which goes on when the video is left, at the device's volume rather than the menu music's. It plays without gaps, and the picture follows it, within about 50 ms.
Sound and pictures are read ahead of each other, so files that store their sound in blocks some way from its pictures (as MP4Box does) play smoothly too.
A video without sound, or whose sound cannot be decoded, plays silently; so do videos when the theme's music cannot play (no SDL_mixer). H.264 MP4 files of about 320x240 at 15 to 30 frames per second suit the Miyoo Mini; videos over 1280x720 are not played.

### Media menu, maps and manuals

Select in Game Info, for a game with any media, opens a menu in the theme's context-menu style over Game Info: Gallery, Saved Games, Game Maps, Walkthrough (TXT) and Manual (PDF), each only when the game has it, in the order `details` in [`game-menu.json`](#game-list-context-menus) gives.
Up and Down move round it, A opens the row, and B or Select closes it. For a game without media, or when `details` leaves the menu no row for the game, Select does nothing there.
Select in the gallery, Saved Games, a map or the walkthrough opens the same menu over it, with Game Info as its first row and without the one shown; B or Select goes back to it as it was, a video paused by the menu playing on from where it stopped.
B in the gallery, Saved Games, a map or the walkthrough goes back to Game Info, and B there back to the list, as before. No titles or button hints show in the gallery, the maps or the walkthrough. The gallery's counter and pause sign and a map's counter sit on black boxes the picture shows through; the walkthrough's line number, over text, on a solid one.

Maps are PNG pictures in `media/maps`, named after the ROM file: `Game.png`, then `Game-1.png`, `Game-2.png` and on, in number order, up to 32. JPEG and BMP maps and the older `Game_map1.png` names are not shown.
A map shows fitted to the screen, with which one it is, such as `2/3`, at the bottom right. L and R show the previous and next map, round the ends.
R2 shows the map at its own size, up to 2000 pixels a side, where the D-pad moves round it, and L2 fits it again; A switches between the two.

The walkthrough is `media/manuals/Game.txt`, UTF-8 (other bytes show as `?`), of which the first 2 MiB are read. It shows in the theme's Game Info font, wrapped at words, with the number of its top line at the bottom left: Up and Down move a line, L and R 15 lines, L2 and R2 75.
A searches it: the keyboard opens with the last search, and its OK looks for that text from the page shown, A to Z in any case, going round from the start. The line found goes to the top, every match on the page marked; Start finds the next one. When there is none, `Not found` shows for 2 seconds in place of the line number.
The manual is `media/manuals/Game.pdf`, opened in Onion's PDF reader, Green (`.tmp_update/bin/green/green`, with its libraries in `.tmp_update/lib/parasyte`), which MainUI does not install. While Green shows it MainUI hands over the screen and sound, and takes them back after; B in Green closes it. Without Green the menu says so.
Back from Green, the menu is over the map, save picture or gallery picture it was opened from, as before. A gallery video is not kept while Green runs: the menu is then over black, and the video starts again from its beginning when the menu closes.
As for the gallery, the folder and file names match in any case on the SD card's FAT, and must be as above elsewhere.

### Saved Games

A RetroArch game's save states show as Saved Games: Saved Games in Game Info's media menu opens them over Game Info. With `left_shortcut` on in [`game-menu.json`](#game-list-context-menus), Left on the game in a ROM list, Favorites, Recents or Search results opens them too, and B there goes back to the list.
They are the states RetroArch keeps in `Saves/CurrentProfile/states/<core>/`, `Game.state` (slot 0) and `Game.state1` and on, named after the ROM file, with their pictures, `Game.state1.png` and so on.
The core is the one the game launches with: its own, chosen in Game List Options, else, on RedOnion, the one chosen there for all the system's games (`.tmp_update/config/system_cores`), each only when its `.so` is in `RetroArch/.retroarch/cores`, else its console's in `launch.sh`; the folder is named as the core's `.info` names it.
A game whose launcher names no RetroArch core, or without any state shown, Auto included with `show_auto`, has no Saved Games, and Left is as before for it; so has a game in a folder `disabled_rom_dirs` lists.

They show from left to right by slot number: the Previous Session, below, first, then the ten highest slots, the lowest of them first, then, with `show_auto`, Auto Save, the game's `Game.state.auto`, when there is one. They open on the rightmost.
Each shows as its picture in its console's shape, or its own with `correct_aspect_ratio` `false` (only made to fit when `upscale_preview` is `false`), or a grey box saying "No thumbnail" when it has none, under the theme's title bar with Saved Games in it.
By default, `"framed"`, the picture is fitted to the 640x360 between the title bar and a footer, on the theme's background; the footer has the theme's footer bar, with its slot, such as `#5`, or its name at the left and which one it is, such as `5/5`, at the right, in the footer's font and colour, without button hints.
With `"layout": "overlay"` the picture is scaled to the full screen, and the labels are at the bottom left and right on boxes the picture shows through. The highest slot is not always the latest save, as slots can be saved over; save times are not used.
Left goes to lower slots and Right to higher ones; Left stops at the first, and the held Left that opened them does nothing more. Opened by Left in a list, Right on the last goes back to the list as B does, a held Right stopping at the last; opened from the media menu, Right stops at the last, and B goes back to Game Info.
A on Auto Save starts the game from it, changing no file. A on a slot resumes it: it goes into the game's Auto state, and the game starts, loading Auto, as GameSwitcher resumes a state (Onion's `/tmp/force_auto_load_state`).
`resume_mode` in [`game-menu.json`](#game-list-context-menus) says what becomes of Auto:

| Mode | Auto before | The state chosen | If the game cannot launch |
| --- | --- | --- | --- |
| `backup` (default) | kept in one backup per game, `Game.state.auto.mainui-backup`, replaced by the next load | unchanged | Auto is put back from the backup |
| `overwrite` | replaced | unchanged | Auto keeps the state, as there is nothing to put back |
| `swap` | moved into the chosen slot (only copied there without an Auto) | holds Auto's | they are exchanged back |

With `backup`, the backup shows first as Previous Session: A on it exchanges it with Auto, so neither is lost, and again exchanges them back; without an Auto, it is copied into Auto and stays. Only `swap` ever writes a numbered state; RetroArch's slot numbering is left as it is. Pictures go with their states; a state without one leaves Auto without one.
Each file is written in full beside the one it replaces, as `.mainui-new`, then put in its place, so it is always whole, old or new.
If the card refuses that, what was not put in place stays as `.mainui-new`, perhaps the only copy of the Auto before, and a message says so. Such a file is never overwritten: a resume that would write over it is refused, naming it, until it is copied back by hand or deleted. A on Auto Save changes no file, and a resume writing other files leaves it alone.

#### Cores and save states

Saved Games lists the states RetroArch has saved for the game's core, and A resumes one as GameSwitcher does: the state becomes Auto, and the core loads it as the game starts. Whether that works is up to the core, not MainUI.
Some cores save states but refuse to load one before their first frame: MAME 2003-Plus, RedOnion's default arcade core, is one, so its Auto Save can exist and still not load at startup, the game then starting from the beginning, and what it supports differs from game to game; this is why `disabled_rom_dirs` lists ARCADE by default.
MAME 2003 is game-dependent too, while MAME 2000, FinalBurn Neo (with some limits) and DOSBox Pure support states.
Some cores have none at all, so Saved Games show none for them: blueMSX, the original DOSBox core (DOSBox-SVN), ScummVM (its games' own saves still work) and fake-08 for PICO-8.
With the default `backup` mode the Auto before a resume stays as Previous Session, so a state a core would not load loses nothing: choose Previous Session to go back.
This follows the libretro documentation; the cores RedOnion installs, and the one chosen for a game, decide what actually happens.

## When a list changes

Edits change the list under the window: Create, Rename, Sort, Delete, Move and Paste in Favorites, removing a Favorite or Recent, and deleting a ROM. After an edit:

- **The window stays where it is** if the selected row still fits on it. Moving a Favorite a few rows within the screen does not scroll the list.
- **Otherwise it scrolls just far enough** to show the selected row, as moving the cursor there would.
- **No gap is left at the bottom.** When a removal shortens a long list, the rows above move into view rather than leaving an empty row under the last one.
- **A short list keeps its top row.** In a folder, the `..` row stays in view, also after pasting or creating the first entry there.

Paste puts the moved Favorite where the cursor is: in place of the selected row, or at the end of the open folder on "..". A game pasted on a folder goes into that folder instead, after its games, and the cursor stays on the folder.
A game pasted on the folder it was moved out of, where it would change nothing, moves up into the open folder instead, after its games: Back from a folder selects it, so Move, Back, Paste takes a game out of its folder, also when that folder is all the open folder holds. A folder pasted on a folder takes that folder's place; to put it inside, open the folder and paste there.

Folders remember where you were. Back returns to the row and window you entered the folder from, also after editing inside it, at any depth. The remembered place of a folder you have visited survives edits elsewhere, and is dropped only when that folder is removed.

This also holds when the folders were reordered outside MainUI while a game ran: Back selects the folder you came from at its new row.

## Confirmations and messages

Delete ROM, Clear Recents, Shutdown and Delete folder in Favorites ask before they act. Deleting a Favorites folder removes all it holds: the folders inside it and their Favorites, which leave the Favorites list (`favourite.json`); the games themselves stay on the SD card.
Holding A after it opened the question never answers it: release A and press it again. Likewise, A that confirms or dismisses a message acts once; holding it does nothing more until it is released. Leaving MainUI's focus or going to sleep cancels an open question.

A message shown over game details takes the keys first: A or Back dismisses it, and the details stay open behind it.
