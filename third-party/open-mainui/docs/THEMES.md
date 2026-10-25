# Notes for theme authors

Open MainUI works with themes that provide only the normal stock assets. This page lists the optional settings and assets it adds, and a few places where the behaviour is worth knowing when designing a theme. Most of it matches the [patched MainUI](https://github.com/robcodedev/onionos-mainui-patcher) theme-author guide, so a theme made for one works on the other.

The active theme's `config.json` is read. If the active theme has no `config.json`, the stock theme's configuration is used as a whole; fields are not merged.
Onion's theme overrides from Tweaks (Appearance > Theme overrides), saved in `Saves/CurrentProfile/theme/config.json`, are then applied over it field by field, as Onion does: each field in an object of the overrides (for example `batteryPercentage.size` or `hideLabels.hints`) replaces the theme's, and the object's other fields stay as the theme set them.
A missing override file changes nothing; one that cannot be read or is not a JSON object is logged and ignored. Files in Onion's profile override folder (`Saves/CurrentProfile/theme/skin/`) take precedence over the theme's own `skin/` files.

## Optional settings

A theme can set both game-list options in its top-level `config.json`:

```json
{
  "gamelist": {
    "bold": false,
    "iconLeftMargin": 12,
    "searchColor": "#9E2C45"
  }
}
```

- **Game-list icon left margin:** `gamelist.iconLeftMargin` places the folder and game icons, and with them the start of the title, at an absolute distance from the left edge. It applies at every row count, including the stock six rows.
  `0` is valid and lets artwork start at the list edge; values above 300 are treated as 300. A missing, negative or invalid value keeps the normal layout, which depends on the row count. One value controls both folder and game rows.
- **Search match colour:** `gamelist.searchColor` (`#RRGGBB`) colours the part of a title that matches an in-list search. Missing or invalid, it is purple on a light `list.color` and yellow on a dark one, as before.
- **Game-list font weight:** `gamelist.bold` set to `false` draws ROM, Favorites, Recents and Search result rows in the normal (not bold) style. Missing, invalid or `true` keeps the stock bold style. Other text (Settings, Apps, titles, game details, the keyboard) is not affected.
- **Hidden labels:** `hideLabels.icons` and `hideLabels.hints` hide the main-menu icon labels and the footer hint text, as in Onion. The older `hideIconTitle` still hides both.
- **Font size 0:** as in stock, a size of 0 hides that text: `hint.size` the footer hints and page counter, `grid.grid1x4` the main-menu labels and the Games console labels, `grid.grid3x4` the Expert console labels, and `title.size` the screen title.
  Dialogs, messages and details that share these fonts stay readable at the default size. Only an explicit 0 counts: a missing `hint.size` follows `title.size` but stays visible.

## Optional assets

- **Selection background per row count:** with a row count `N` from 6 to 20, ROM, Favorites and Recents lists first try `skin/bg-list-s_N.png` from the active theme, then the ordinary `skin/bg-list-s.png`. Keep the stock asset width, horizontal alignment and transparency. The row height is `floor(360 / N)`.
- **Popup selection background:** context-menu rows use `skin/bg-list-popup-s.png` from the active theme if present, otherwise `skin/bg-list-s.png`. Make it as wide as `bg-pop-menu-1.png` to `bg-pop-menu-6.png`; a height of 60 px, matching a popup row, is the safest choice. This is the highlighted-row strip; the numbered `bg-pop-menu-N.png` files remain the popup backgrounds.
- **Context menu labels:** each starts 20 px from the popup's left edge. A label too wide to end at the popup's right edge is shortened to end in `...`, 5 px clear of that edge. An empty file `.tmp_update/config/.noContextMenuEllipsis` turns this off, and such a label is cut off at the edge, as before 1.0.10.
- **Numbered popup backgrounds:** `bg-pop-menu-1.png` to `bg-pop-menu-6.png` should share width, border placement, transparency and horizontal padding. Heights may differ. A missing larger one is built from the largest smaller one that exists, by repeating its 60 px rows, unless that image is already tall enough for every row (a full-screen background, for example), which is then drawn as it is.
- **Wide game-icon spacers:** a `skin/icon-game.png` at least 120 px wide and at least three times as wide as it is tall is treated as a deliberate spacer.
  Its full width is kept and only extra height is cropped; titles start after it, and Favorite markers move to the far right so they do not overlap it. Normal icons are unaffected. Theme resizing tools should use the same rule (`width >= 120 && width >= 3 * height`) and leave such assets unchanged.
- **Settings icons:** supply `skin/icon-theme.png` for the **Themes** row and `skin/fixit.png` for the **Tweaks** row in Settings.

## List colours

All list rows, including the selected one, use `list.color`. `list.selectedcolor` has no effect on list rows in stock MainUI, with one exception: stock draws the first row of its Wi-Fi screen (the on/off toggle) in that colour.
Open MainUI does not read `list.selectedcolor` at all, so its Wi-Fi toggle row uses `list.color` like every other row. This differs from stock on purpose: themes set `list.selectedcolor` without that one row in mind, and it can clash with the rest of the list. Make sure `list.color` is readable on the selection background.

## Background orientation

`skin/background.png` is stored upside down, as for stock MainUI and Onion's own apps: it is rotated 180 degrees when loaded. Check the result on the device or in a screenshot, not in an image viewer. The other images are used as stored.

## Fonts and languages

The theme's `font` entries are used in every language. Stock MainUI also reads `fontascii` keys; Open MainUI reads only `grid.fontascii`, for the Expert console labels (see below). No theme in the Onion theme repository sets any of them.

If a theme font cannot be opened, Open MainUI falls back to a built-in font chosen by language, as stock does:

- a language other than English (any language file whose name does not start with `en.lang`): `wqy-microhei.ttc`, then `Exo-2-Bold-Italic.ttf`;
- English: `Exo-2-Bold-Italic.ttf`.

These are looked up in `miyoo/app` and then, on the device, in `/customer/app` on internal flash.

Stock reloads its fonts immediately when the language is changed. Open MainUI chooses the fallback when the theme is loaded, so when a theme relies on a fallback font and the change switches between English and another language, it restarts itself: Onion starts MainUI again and it reopens Settings on the language row.
A change to or from Arabic (`ar.lang`) or Bengali (`bn.lang`) restarts MainUI in the same way with any theme, so Onion's `runtime.sh` can decide again, as it starts MainUI, whether to preload a library that shapes these scripts.
Without a runtime handoff folder (host development builds), the new fallback applies from the next start instead. Other changes with themes whose fonts all load are not affected.

A theme font must contain the characters of the languages its users choose; neither stock nor Open MainUI substitutes another font for missing characters. A Latin-only font shows boxes for Chinese, Japanese, Korean or Cyrillic labels.

Arabic and Bengali need two things that neither Onion nor Open MainUI includes:

- A font with their letters, as the theme's font or a fallback font. No font shipped with Onion has them.
- A library that shapes the text before SDL_ttf draws it, preloaded into MainUI (`LD_PRELOAD`), which Onion's `runtime.sh` would have to add.
  Shaping, as HarfBuzz does it, picks each Arabic letter's joined form and places Bengali's vowel signs and conjuncts; the SDL_ttf Onion uses draws each character on its own.
  Right-to-left text must also be put in display order, by the Unicode bidirectional algorithm (as in FriBidi).

Without both, these languages show boxes, or letters that are unjoined and in the wrong order.
Open MainUI's part is to restart on a change to or from them, so `runtime.sh` can choose the library, and to show right-to-left titles from their start.
A title whose first letter is Hebrew or Arabic and that does not fit its row shows its right end, where it begins, and scrolls right from there, to be read in its own direction. While a cover is shown it stops at the cover's pane, so its beginning is not hidden. Other titles are unchanged.

## Safe margins and borders

The 640x480 screen reserves a 360 px band for the game list, between the title and the footer:

```text
y =   0..59    title
y =  60..419   game list
y = 420..479   footer (hints and counter)
```

The areas above `y = 60` and from `y = 420` down are outside list rows, though the title and footer use them. Inside the list band there is no fixed top or bottom margin: the row height changes with the row count, and selection and icon artwork may fill the whole row.
Borders that must never touch a row should stay outside the 360 px band. Borders inside `bg-list-s_N.png` should fit within that row's own `floor(360 / N)` height.

Horizontally, do not rely on the stock 20 px inset. At high row counts the outer padding shrinks to 15 px, and `gamelist.iconLeftMargin` can move icons and titles further left, down to zero. For a permanent left border of `B` pixels, set `iconLeftMargin` to at least `B` plus the gap you want.
On the right, keep important border artwork conservative: dense rows can approach the 15 px inset, Favorite markers can use the far-right lane, and wide-spacer layouts use more width than stock. Test border-heavy themes at the smallest and largest row counts they support.

Main-menu labels may wrap to two centred lines within a 136 px width. Dialog action labels use the theme's hint style.
Hint text (footer hints, counter, dialog actions) uses `hint.font`; without it, the default font (Exo 2 Bold Italic, or the language font), as in stock, not `title.font`. It is drawn slanted with SDL_ttf's italic style, as in stock: an upright `hint.font` (a pixel font, say) leans, and a font that is italic already stays as it is.
An empty file `.tmp_update/config/.mainuiFooterAltLayout` selects an alternative footer: the page counter, or a Favorites folder's game count, starts at x 20, and the hints end at x 620, B's before A's, each button beside its own action, OK and CANCEL too. Without it the footer is stock's. Hidden hints and hint text apply to both.
Expert console labels use `grid.fontascii`, or without it the list font (`list.font`), at the `grid.grid3x4` size, in the font file's own style, as in stock; a blank `grid.font` that hides the Games labels does not hide them.
Main-menu and Games console labels are drawn bold in the default font; a `grid.font` the theme names keeps its own weight. This differs from stock on purpose: stock makes them bold in any font, which visibly thickens pixel fonts.

The Games grid uses `bg-game-item-n.png` and `bg-game-item-f.png` for every console; Expert draws only `bg-ra-list-item.png` (214x120), behind the selected console and 2 px right of and below its cell, as stock does; Expert icons and labels sit 1 px lower than the Games rule would put them, as in stock.

On the home screen `skin/miyoo-topbar.png` replaces the title, drawn at x 20 and centered on the 60 px header, without clipping, as in stock: a taller image also draws below the header, under the home icons.
The battery icon is `power-0%-icon.png` up to 4 %, `power-20%-icon.png` up to 14 %, `power-50%-icon.png` up to 44 %, `power-80%-icon.png` up to 69 % and `power-full-icon.png` above, as in stock MainUI (Onion's own screens change at 5, 30, 60 and 90 %), and `ic-power-charge-100%.png` while charging.
While charging, the Mini Plus and Mini Flip show the battery percentage beside it, as the theme places it; stock hides it. On the Mini, Onion does not know the level while charging, so only the icon shows, as in stock.
The battery and charging state are checked every second, as Onion's own screens do; on the Mini Plus and Mini Flip, where charging is read from the power chip every 2 seconds, the charging icon can take up to about 3 seconds to appear or go after the charger is plugged in or out.

The Wi-Fi icon (`icon-wifi-signal-02.png` to `-04.png`) is centered in stock's 48x48 status slot left of the battery, at x 504 and y 6; a wider or taller image spreads evenly around it.
When the battery or its percentage would overlap an icon up to 48 px wide, the icon moves left to 8 px from them; stock leaves the two overlapping, with the icon behind. Wider artwork, drawn to spread from the slot, stays where stock puts it.
While Onion's hotspot runs and Wi-Fi is not connected, `icon-hotspot-wifi.png` takes that slot, as in stock. The Wi-Fi icon shows also when the battery icon cannot be drawn (its level unknown, or no image).
`skin/headphone-icon.png` is not drawn: stock shows it only while `/dev/dsp1` exists, which appears to be an audio device rather than the headphone jack.
Onion's optional clock overlay (`libmainuihooks.so`, with the theme's `time` settings) should draw over the header as it does with stock MainUI: MainUI prints the start-up line it waits for. This is not yet confirmed on a device.

Confirmations, the Refresh roms panel and messages are drawn on `skin/pop-bg.png`, centred, as in stock. A confirmation has its title in the top 50 px, its question in the next 100 px and its buttons below.
The title is drawn in `total.color` and the text in `grid.color`, as stock's dialog does; messages use the same two colours, the hint as the text.
Both keys are mainly used elsewhere (the footer's page count, the console grid), so check them against `pop-bg.png`: a colour close to the art's leaves that text hard to read or invisible, in stock as here. TechDweeb's dialog title, in its `total.color`, is an example.
A message's title, text and hint are centred together in the image, at least 30 px from its top and 45 px from its bottom, with the text 50 px from its sides; a message whose text does not fit there, or a theme without the image, gets a plain dark panel instead.
Behind the context menu, messages and confirmations the screen above the footer is darkened with black at 0xAA alpha, as in stock.

Test unusually large fonts, borders, icons and preview art on the device: assets are cropped, not rearranged, when they do not fit.
