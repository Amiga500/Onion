# Language strings

`en.lang` is Onion's `static/build/miyoo/app/lang/en.lang` with additional strings added for screens this launcher provides that the stock one does not. It is kept here as the reference list of those additions; the launcher does not read this copy.

## Where strings actually come from

At runtime the launcher reads `.lang` files from the card, looking in `miyoo/app/lang`, then `miyoo/app/lang_backup`, then the fallback theme folder's `lang`; for a file name found in several places, the first wins. Startup and the Settings language list use the same order.
`miyoo/app/lang` comes first because it is the folder stock MainUI reads: when labels are hidden by the theme or a Tweaks override, Onion blanks those strings there and keeps the originals in `lang_backup` for its own apps. Those are Onion's files, not this repository's.
`mainui_translate(id, fallback)` returns the English literal compiled into the binary when an ID is missing, so a missing translation shows English rather than failing.

## Current gap

IDs 400–410 are used by `src/menus/context.c`, `src/menus/stock_settings.c` and `src/app/settings_page.c`.
IDs 411–416 are the rows of Game Info's media menu (`src/ui/media_viewer.c`): 411 goes back to Game Info, 412 opens the gallery, 413 the maps, 414 the walkthrough, 415 the PDF manual and 416 Saved Games, which also titles their messages.
417 names the rolling backup in Saved Games and 418 their Auto entry. Until these IDs land upstream, their built-in English fallbacks appear on devices whose language files omit them:

| ID | English |
| --- | --- |
| 400 | Create folder |
| 401 | Move selected |
| 402 | Move here |
| 403 | Rename folder |
| 404 | Delete folder |
| 405 | Remove Favorite |
| 406 | Sort A-Z |
| 407 | Tweaks |
| 408 | Model name |
| 409 | Max resolution |
| 410 | OS version |
| 411 | Game Info |
| 412 | Gallery |
| 413 | Game Maps |
| 414 | Walkthrough (TXT) |
| 415 | Manual (PDF) |
| 416 | Saved Games |
| 417 | Previous Session |
| 418 | Auto Save |

The maps and the walkthrough show no titles, only numbers; their few error messages, such as "Cannot show this map" and the walkthrough search's "Not found", stay in English. The fix for missing translations is upstream: the new keys need to be added to Onion's language files, not installed from here. Copying this file onto a card would only cover English and would overwrite a file Onion owns.

The 400 range was chosen because the highest ID in any shipped Onion `.lang` file is 301, so nothing collides today. Confirm that again before submitting, in case upstream has grown since.

## Adding a string

1. Pick the next free ID in the 400 range and add it here with its English text.
2. Call `mainui_translate(id, "English text")`. The literal is the fallback, so the screen reads correctly before any translation exists.
3. Note the new ID in the table above, and add it to the upstream pull request.

Never call `mainui_translate` with an ID that Onion already uses for something else, the string would silently change meaning in every translated language.

Which font draws translated labels, and the fallback font for other languages, is described in [docs/THEMES.md](../docs/THEMES.md#fonts-and-languages).
