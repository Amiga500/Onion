<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus beta 10: longer battery with Wi-Fi, theme icons, arcade power button, RedOnion boot screen

> 🧪 **Beta.** This build is published as a pre-release: only devices on the **beta** OTA channel receive it. Stable installs stay on the current stable release until this beta is promoted.

This beta fixes the **battery drain and warm device with Wi-Fi connected** that users reported, resizes list icons when you **install a theme**, stops the **power button** from opening the diagnostic menu in arcade games, and adds the **RedOnion boot screen** to Silky RedOnion.

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip. Open MainUI is still **1.0.8**: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in beta 10

### 🔋 Battery and heat with Wi-Fi

- **Wi-Fi power save is on:** OnionPlus, like Onion, turned the Wi-Fi chip's power saving off whenever Wi-Fi started, so the radio never rested. On a Mini Plus idling in the menu with Wi-Fi connected, that used about 20% more battery (0.75 against 0.62 %/min in our tests) and made the device warmer. Wi-Fi now starts with power saving on, the driver's own default. Onion's stock launcher restarted Wi-Fi and ran this way already, which is why the drain showed up only with Open MainUI.
- **Netplay keeps a quick connection:** power saving delays packets that arrive while the radio rests (a ping to an idle Mini Plus averaged 40 ms with peaks of 220 ms, against 3.5 ms with it off). Netplay turns it off when the game starts and back on when the session ends.
- With Wi-Fi off nothing changes. Turning Wi-Fi off when you don't need it still gives the longest battery life.

### 🎨 Themes

- **List icons fit after installing a theme:** with Tweaks › Appearance › Game lists › "Rescale theme list icons to fit" on, the Themes app now resizes the new theme's list icons for your row count right away. Before, you had to turn the option off and on again. Based on @robcodedev's themeSwitcher patch.
- **Boot screen:** Silky RedOnion shows the RedOnion logo at start-up.

### 🕹️ Arcade

- **Power button and the diagnostic menu:** RetroArch receives the power button as R3, which the FB Alpha and FinalBurn Neo cores use for their diagnostic menu. Turning the console off during a CPS-1 game opened that menu, still on screen at the next power on. R3 is now unbound in CPS-1, CPS-3, Neo Geo (FB Alpha 2012), FB Alpha 2012 and FinalBurn Neo, as it already was in CPS-2. A remap you saved yourself for these cores is kept. A game whose auto save state was made with the menu open shows it once more: leave the menu and it's gone.

## 🔧 In beta 9

- **Arabic and Bengali** readable in every program.
- **About 1 second faster** start (@robcodedev's display warmup).
- **Faster GameSwitcher** by @robcodedev, and settings that two programs saved at the same time no longer get lost.
- Language list, "Refresh roms" labels and Tweaks fixes.

## ⚠️ Known issues

- **Mini Flip display colours** (Luminance, Hue, Saturation, Contrast) can be back at their defaults after a reboot: Open MainUI sets them only from Settings › Display, as stock does, and OnionPlus doesn't apply them at boot yet.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Arabic:** lines stay left-aligned like every other language, and text drawn one letter at a time (the on-screen keyboard) shows Arabic letters unjoined.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful things to check:

- Battery with Wi-Fi connected, idle in the menu: it should last longer than in beta 9
- Anything that uses Wi-Fi with power save on: FTP or the web file manager with a large file, SSH
- Netplay (Standard and Easy, host and join): smooth as in beta 9, and after the session Wi-Fi still works
- Tweaks › Appearance › Game lists: set Rows to 10, then install another theme in Themes: its list icons should fit
- A CPS-1, CPS-3 or Neo Geo game: turn the console off with the power button, then on again: no diagnostic menu
- The boot screen with Silky RedOnion

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, the faster GameSwitcher, pngresize, the boot warmup and the Wi-Fi power save lead, to the contributor of the RedOnion artwork and to DiMo + fireball33x for the original Silky theme, and to the testers who sent reports and helped confirm the fixes, especially QuackWalks, @Zazzago, @Ziko577, Veuks and Zombiediplomat. Thanks to @fengfrw for the Chinese translations and to @yuruyang for the CUE generator report and review. Thanks also to the Reddit users who reported the battery drain and the warmer Mini Plus.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

The score is the sum of integrated bug reports and pull requests on both OnionPlus and Open MainUI: issues closed as fixed and pull requests merged, on [Amiga500/Onion](https://github.com/Amiga500/Onion) and [robcodedev/onionos-mainui-opensource](https://github.com/robcodedev/onionos-mainui-opensource). Reports that are still open or were not accepted don't count yet. Reports opened by the maintainers on someone's behalf count for the person credited in them.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.
