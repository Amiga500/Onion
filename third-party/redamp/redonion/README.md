# RedOnion native launch integration (opt-in, no shared-library swaps)

The **player** uses `App/RedAmp/launch.sh` unchanged. To mix background music
into **other processes**, RedOnion must explicitly source the launch fragment
`/mnt/SDCARD/App/RedAmp/redonion-launch-audio.sh` after setting its normal
`LD_PRELOAD` and just before launching **MainUI, RetroArch or a game**:

```sh
# Existing RedOnion runtime/launch script sets its own LD_PRELOAD first.
[ ! -f /mnt/SDCARD/App/RedAmp/redonion-launch-audio.sh ] || \
    . /mnt/SDCARD/App/RedAmp/redonion-launch-audio.sh
# Existing RedOnion exec / game command follows unchanged.
```

This must be inserted in the appropriate **RedOnion launch path(s)**, not
just in RedAmp's own `launch.sh`: sourcing it in the music player's launcher
cannot affect processes started later by MainUI. The repository provided
here did not include RedOnion `runtime.sh`, so this package does **not**
automatically patch it. Review placement in that actual file before applying.

The fragment only enables the preload when the user has explicitly enabled
Music in games, settings.ini says `game_music=1`, and both the original
`miyoo/lib/libpadsp.so` and the separate RedAmp `lib/libpadsp_wrap.so` exist.
It will *not* install, rename or modify system libraries. It sets
`REDAMP_NATIVE_PRELOAD=1`, which disables the legacy re-exec constructor;
preloading both libraries in the correct order allows RTLD_NEXT chaining.

**Compatibility:** Certain launchers overwrite LD_PRELOAD after this fragment;
those launchers need the fragment at the last assignment. Static binaries,
processes which clear LD_PRELOAD, and programs using unrelated audio APIs may
not support background mixing. Validate every launch type on Miyoo hardware.
The library must be built for the target ARM ABI before this works.

## Legacy installation recovery

Older MiniAmp/RedAmp builds may have already replaced the global
`miyoo/lib/libpadsp.so`. Do not activate native mode while that legacy wrapper
remains. Use `App/RedAmp/redamp-recover.sh` first to restore the verified
original; the new player refuses background service with a legacy wrapper.

## Background game hotkeys

Conflicting SELECT shortcuts are disabled by default. Advanced users may set
`game_hotkeys=1` in `data/settings.ini` to opt into the previous shortcuts;
these may conflict with RedOnion hotkeys and particular emulators.

## Proposed upstream RedOnion integration change (not yet submitted)

The standalone helper is already at `App/RedAmp/redonion-launch-audio.sh`; the safe ioctl
forwarding changes and host preload regressions are part of the RedAmp source. A RedOnion pull
request should insert the source command shown above immediately before RedOnion launches each
menu/game process, following its final `LD_PRELOAD` assignment. Do not inject into unrelated
applications or replace `/mnt/SDCARD/miyoo/lib/libpadsp.so` globally.

Before proposing or merging upstream, test with Music in games Off and On, SDL framebuffer
ioctls, `FIONREAD`, RetroArch, standalone emulators and shutdown recovery on real ARM hardware.
The RedOnion source tree and authorized GitHub access were not part of this release; no PR has
been opened. The integration must be reviewed against the actual RedOnion launch scripts.
