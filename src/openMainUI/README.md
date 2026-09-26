# Open MainUI test integration

Branch `test/open-mainui` only. Not for release.

The build installs [Open MainUI](https://github.com/Amiga500/onionos-mainui-opensource)
(submodule `third-party/open-mainui`, branch `onionplus-test`) as the launcher:

- every `.tmp_update/bin/MainUI-*-clean|expert` becomes a small wrapper;
- the original binary is kept in `.tmp_update/mainui-test/stock/<variant>`;
- Open MainUI itself is `.tmp_update/mainui-test/MainUI`.

## Build

```
git submodule update --init third-party/open-mainui
make with-toolchain CMD=release          # OPEN_MAINUI=0 builds stock
sh src/openMainUI/test_wrapper.sh        # host test of installer + wrapper
```

The zip is named `OnionPlusOMTest-v…` so the OTA filter (`OnionPlus-v`) never
offers it to users. Do not publish a GitHub release or prerelease from this
branch. Note that OTA on a test device can replace this build with a normal
OnionPlus release, which restores the stock launcher.

## Switching back to stock

The wrapper runs the stock binary of its own variant when:

| Condition | How long |
| --- | --- |
| `.tmp_update/mainui-test/DISABLED` exists | until removed |
| `.tmp_update/mainui-test/MainUI` missing or not executable | until fixed |
| Open MainUI failed twice in a row before its first frame | until reboot |

The first-frame check needs the `MAINUI_START_MARKER` support from the
`onionplus-test` branch of Open MainUI; wrapper and binary are built together.
It does not catch crashes after the first frame or hangs.

## Logs

Create `.tmp_update/config/.logging`, reproduce, then read
`.tmp_update/logs/MainUI.log`. Logging is best effort: a full or read-only
card starts the launcher without a log instead of not starting it.
