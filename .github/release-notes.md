<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus beta 9: Arabic and Bengali, a faster start, settings and GameSwitcher fixes

> 🧪 **Beta.** This build is published as a pre-release: only devices on the **beta** OTA channel receive it. Stable installs stay on the current stable release until this beta is promoted.

This beta makes **Arabic and Bengali** readable everywhere, starts about **1 second faster**, brings @robcodedev's faster **GameSwitcher**, and fixes settings that two programs saved at the same time. It also gets the updater ready for the project's new name, **RedOnion**.

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip. Open MainUI is still **1.0.8**: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in beta 9

### 🌍 Arabic and Bengali

- **Readable in every program:** the launcher, Settings, the GameSwitcher, Tweaks and Onion's apps now join Arabic letters and put them right to left, and draw Bengali vowel signs and conjuncts in place, with the Noto Sans Arabic UI and Bengali UI fonts. Before, both languages showed boxes or loose letters.
- Text without Arabic or Bengali is drawn exactly as before. Game names in Arabic work too, also mixed with Latin titles and brackets, such as `العربية (Mario [USA])`.
- Arabic and Bengali now translate Search, Info and Website too.

### ⚡ Faster start

- **About 1 second less to the menu:** the slow first set-up of the display (about 2 seconds after power-on) now runs in the background while the rest of the start continues, instead of inside the launcher. By @robcodedev.

### 🔀 GameSwitcher

- **Faster:** quicker to open over a game and quicker to resume. By @robcodedev.
- **Switching to another game:** the running game's state is saved first ("SAVING", then "LOADING"), and the game's picture is written safely, so a power cut can't leave a broken one.
- The GameSwitcher now also has the settings fixes below.

### ⚙️ Settings

- **Saved at the same time:** two programs saving the settings at the same moment could lose one of the saves or damage the file. Fixed: each save uses its own temporary file and takes the same lock Open MainUI uses, and a failed save is tried again.
- **Language:** the language chosen in Settings could go back to the previous one after changing the brightness or volume with the buttons. Fixed.
- **Settings › About** always shows the build date and "RedOnion version".

### 🔤 Languages

- **Language list:** Bahasa Indonesia (Indonesian), Lietuvių (Lithuanian) and Lenga d'òc (Occitan) show their English name in brackets like the others, and Japanese and Uchinaguchi use normal brackets.
- **"Refresh roms" and "Refresh all roms"** had the same text in Italian, German, French, Brazilian Portuguese, Russian and Traditional Chinese: in the Select menu of the console grid you couldn't tell them apart. Now they differ in every language; Turkish said "clean all roms" and Romanian was too long for the menu.

### 🛠️ Tweaks

- **Appearance › Main menu › Context menu items:** Search shows off when the Search app isn't installed, and Custom 1–3 when they aren't defined, as the menu itself hides them.
- **Appearance › Game lists:** after changing Rows, the theme's list icons are resized also when you leave Tweaks with MENU. Resized icons keep clean transparent edges (new pngresize tool by @robcodedev), and a wide folder icon or favourite mark is resized too.

### 🔧 Other fixes

- **Game List Options** no longer shows "Refresh list": the Select menu has it.
- **Update from beta 7 or older:** a device still on the old default theme, Silky by DiMo, moves to Silky RedOnion once; any other theme is kept.
- **Updater:** ready for the RedOnion name (it will find packages called RedOnion or OnionPlus) and safer when GitHub answers with an error page.

## 🔧 In beta 8

- **The RedOnion look:** Silky RedOnion is the default theme, and system screens and apps follow the theme's colours.
- **Loading/saving screen** choice in Tweaks › Appearance.
- **Open MainUI 1.0.8.**
- **OTA:** no more error 137 and black screen when leaving; Up/Down, A and MENU on every screen.

## ⚠️ Known issues

- **Mini Flip display colours** (Luminance, Hue, Saturation, Contrast) can be back at their defaults after a reboot: Open MainUI sets them only from Settings › Display, as stock does, and OnionPlus doesn't apply them at boot yet.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Arabic:** lines stay left-aligned like every other language, and text drawn one letter at a time (the on-screen keyboard) shows Arabic letters unjoined.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful things to check:

- Arabic or Bengali: the menus, Settings › About, the GameSwitcher, Tweaks, a game named in Arabic
- Start-up time: with logging on (Tweaks › Advanced), the `warmup` line in `.tmp_update/logs/timing.log` should say about 2 s
- GameSwitcher: open it over a game, go back to it, switch to another game, leave with MENU while it saves
- Change the language, then the brightness or volume with the buttons, and restart: the language should stay
- Tweaks › Appearance › Game lists: change Rows and leave with MENU, then look at the list icons
- The updater on the beta channel, after installing this build: it should find the next build

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, the faster GameSwitcher, pngresize and the boot warmup, to the contributor of the RedOnion artwork and to DiMo + fireball33x for the original Silky theme, and to the testers who sent reports and helped confirm the fixes, especially QuackWalks, @Zazzago, @Ziko577, Veuks and Zombiediplomat. Thanks to @fengfrw for the Chinese translations and to @yuruyang for the CUE generator report and review.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

The score is the sum of integrated bug reports and pull requests on both OnionPlus and Open MainUI: issues closed as fixed and pull requests merged, on [Amiga500/Onion](https://github.com/Amiga500/Onion) and [robcodedev/onionos-mainui-opensource](https://github.com/robcodedev/onionos-mainui-opensource). Reports that are still open or were not accepted don't count yet. Reports opened by the maintainers on someone's behalf count for the person credited in them.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.
