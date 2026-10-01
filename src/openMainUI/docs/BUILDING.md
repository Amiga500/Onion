# Building, testing and installing

**Building for your handheld? Start with [Device build](#device-build), then [Installing on a device](#installing-on-a-device).** The host build is only needed for desktop development and testing.

There are two separate environments:

| Task | Where to run it | Command |
| --- | --- | --- |
| Build the handheld launcher | Inside the toolchain container | `make device` |
| Build the desktop preview | On the Linux host, outside the toolchain container/VM | `make` |
| Run host tests | On the Linux host, outside the toolchain container/VM | `make check ONION_ROOT=...` |

The examples below use `/root/development/dev-miyoomini-toolchain` on the host and `onionos-mainui-opensource` as this repository's directory name. Adjust the host paths if your checkout is elsewhere.

## Device build

This builds the ARM launcher for the handheld. You do not need to build or test the desktop preview first. The toolchain environment must contain an Onion checkout with its libraries built, including `libsqlite3.so`, `libshmvar.so` and the SQLite headers.

Expected directories **on the host**:

```text
/root/development/dev-miyoomini-toolchain/
  workspace/
    Onion/
    onionos-mainui-opensource/
  SDCARD/
```

### 1. Open the toolchain shell from the host

If you use the `dev-miyoomini-toolchain` checkout with its `shell` target:

```sh
# On the host, outside the container
cd /root/development/dev-miyoomini-toolchain
make shell
```

Alternatively, start the container directly with these mounts:

```sh
# On the host, outside the container
docker run -it --rm \
  -v "/root/development/dev-miyoomini-toolchain/workspace":/root/workspace \
  -v "/root/development/dev-miyoomini-toolchain/SDCARD":/mnt/SDCARD \
  aemiii91/miyoomini-toolchain:latest /bin/bash
```

### 2. Build inside the container

After entering the shell using either method:

```sh
# Inside the toolchain container
cd /root/workspace/onionos-mainui-opensource
make device
```

Run this from **this repository's directory**, containing its Makefile. `No rule to make target 'device'` means make did not find that target in the Makefile it loaded: check `pwd` and that you entered `onionos-mainui-opensource`, rather than the toolchain directory, `Onion`, or the workspace parent.

Inside this container, the default `ONION_ROOT=/root/workspace/Onion` is correct. Plain `make` builds the desktop preview and needs host SQLite and SDL development libraries; use `make device` here.

The output is `build/onion/MainUI`, with an unstripped copy beside it for debugging. The mounted workspace makes the output available on the host at:

```text
/root/development/dev-miyoomini-toolchain/workspace/onionos-mainui-opensource/build/onion/MainUI
```

The build verifies that the output is a 32-bit ARM hard-float ELF and that dependencies do not contain build-machine paths. Continue with [Installing on a device](#installing-on-a-device).

`make check-device`, also run inside the container, cross-compiles the unit suites. It does not run them: copy `build/onion/unit-tests` to the handheld and execute it there.

## Host build

**Run this on your Linux host, outside the toolchain container/VM.** It produces a desktop executable, not the handheld launcher. If you are currently in the toolchain shell, run `exit` first.

The host needs a C11 compiler, GNU make, Python 3.7 or newer, Pillow, SQLite, and the SDL 1.2 development packages. On Debian/Ubuntu:

```sh
# On the host
sudo apt update
sudo apt install build-essential python3 python3-pil libsqlite3-dev \
    libsdl1.2-dev libsdl-image1.2-dev libsdl-ttf2.0-dev dosfstools

cd /root/development/dev-miyoomini-toolchain/workspace/onionos-mainui-opensource
make            # build/MainUI-dev
```

## Tests

**Run these on the host, from the same repository directory as the host build.** Set `ONION_ROOT` to the host path, not the container path:

```sh
# On the host
cd /root/development/dev-miyoomini-toolchain/workspace/onionos-mainui-opensource
make check ONION_ROOT=/root/development/dev-miyoomini-toolchain/workspace/Onion
```

This derives the theme path as `/root/development/dev-miyoomini-toolchain/workspace/Onion/static/build/miyoo/app`. The default `/root/workspace/Onion/static/build/miyoo/app` is a container path and is wrong for this host layout. Set `ONION_ROOT` to the checkout root, not its `static/build/miyoo/app` subdirectory. If you previously exported `ONION_THEME`, unset it to let the tests derive the path from `ONION_ROOT`.

To run the suites separately:

```sh
make check-unit
make check-integration ONION_ROOT=/root/development/dev-miyoomini-toolchain/workspace/Onion
```

`build/unit-tests` takes suite names, for example `build/unit-tests core state`. The integration runner `tests/integration/run.py` takes case names and supports `--list`. When invoking that Python runner directly, pass `ONION_ROOT` through its environment as well.

Some integration cases also need a fixture SD card tree at `tests/fixtures/sdcard`, or a path set through `MAINUI_FIXTURE_SD`. For example, if your host `SDCARD` directory contains the required fixtures:

```sh
MAINUI_FIXTURE_SD=/root/development/dev-miyoomini-toolchain/SDCARD \
  make check ONION_ROOT=/root/development/dev-miyoomini-toolchain/workspace/Onion
```

Cases missing an Onion theme tree or fixture SD tree report a skip. Set `MAINUI_STRICT=1` to make missing prerequisites fail instead. The fixture SD tree is not in the repository, so under `MAINUI_STRICT=1` its absence fails only when `MAINUI_FIXTURE_SD` is set; otherwise those three cases (`browser`, `list_parity`, `ui_adjustments`) still skip. CI runs strict against a pinned Onion `v4.5-dev` commit (see `.github/workflows/ci.yml`). A successful run with skipped cases does not mean those cases were tested.

## Running the preview on a host

Run the desktop preview **outside the toolchain container/VM**. With the example host layout:

```sh
cd /root/development/dev-miyoomini-toolchain/workspace/onionos-mainui-opensource
build/MainUI-dev \
  --sd-root /root/development/dev-miyoomini-toolchain/SDCARD \
  --theme /root/development/dev-miyoomini-toolchain/workspace/Onion/static/build/miyoo/app
```

The SD root must contain an SD-card-style directory tree. The preview also supports deterministic input, BMP snapshots and simulated device status; run `build/MainUI-dev --help` for the options.

## Paths and overrides

The same mounted files have different paths in the two environments:

| Location | Linux host | Toolchain container |
| --- | --- | --- |
| Workspace | `/root/development/dev-miyoomini-toolchain/workspace` | `/root/workspace` |
| Onion checkout (`ONION_ROOT`) | `/root/development/dev-miyoomini-toolchain/workspace/Onion` | `/root/workspace/Onion` |
| SD card tree | `/root/development/dev-miyoomini-toolchain/SDCARD` | `/mnt/SDCARD` |

`ONION_ROOT` is used by both device builds and integration tests. It defaults to `/root/workspace/Onion`. Passing the host path on the `make check` command line, as above, keeps the container default intact.

You can put local overrides in an untracked `config.mk` next to the Makefile. That file is shared through the workspace mount: a host-only `ONION_ROOT` written there will also affect container builds. If you set it there, override it inside the container with `make device ONION_ROOT=/root/workspace/Onion`.

Other supported overrides include `CROSS_COMPILE` (the device compiler prefix), `ONION_SDL_CFLAGS`, `ONION_SDL_LIBS`, and the host variables `CC`, `OPT`, `SDL_CFLAGS`, `SDL_LIBS`, `O` and `VERSION`. `make help` lists the targets. `make clean` removes the build output, including device output under the default build directory.

## Installing on a device

Use a spare card copied from a working Onion install, and keep the original available so you can go back. The simplest installation is to shut down the device and put its SD card in your computer. All paths below are relative to the SD card root (`/mnt/SDCARD` on the device). Enable showing hidden files to see `.tmp_update`.

If something goes wrong after installing, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md). Install with the optional wrapper if you want to be able to save logs.

### Choose the launcher file

Onion starts `miyoo/app/MainUI`, but first bind-mounts a selected file from `.tmp_update/bin/` over that path. Replace those **source files** with the device binary (simple method below), or replace one selected source file with the optional wrapper, retaining the source filename. Merely replacing `miyoo/app/MainUI` will be hidden by the mount.

| Device | Expert mode off | Expert mode on |
| --- | --- | --- |
| Miyoo Mini | `.tmp_update/bin/MainUI-283-clean` | `.tmp_update/bin/MainUI-283-expert` |
| Miyoo Mini Plus | `.tmp_update/bin/MainUI-354-clean` | `.tmp_update/bin/MainUI-354-expert` |
| Miyoo Mini Flip | `.tmp_update/bin/MainUI-285-clean` | `.tmp_update/bin/MainUI-285-expert` |

The Mini and Mini Plus names follow Onion's [runtime launcher selection](https://github.com/OnionUI/Onion/blob/main/static/build/.tmp_update/runtime.sh#L641-L657). Check that the selected file exists on your card. If your Onion version uses another layout, inspect `mount_main_ui` and `launch_main_ui` in the card's `.tmp_update/runtime.sh` before replacing anything.

### Simple method: replace the binaries directly

The device build runs under Onion without extra command-line arguments: it already defaults to `/mnt/SDCARD`, real-device mode and `/tmp` for launch handoff. No wrapper is required.

1. Shut down the device and put the SD card in your computer.
2. Back up the original `MainUI-*` launcher binaries listed in the table above to your computer, keeping their filenames.
3. Copy `build/onion/MainUI` over each of those existing launcher files in `.tmp_update/bin/`, retaining each destination filename. Every replacement is a copy of the same new binary. Replace both `clean` and `expert` variants so changing Expert mode continues to use Open MainUI; replacing all six listed variants also covers all three device models.
4. Safely eject the card and boot.

For example, if all six variants are present, copy the same `build/onion/MainUI` to:

```text
.tmp_update/bin/MainUI-283-clean
.tmp_update/bin/MainUI-283-expert
.tmp_update/bin/MainUI-354-clean
.tmp_update/bin/MainUI-354-expert
.tmp_update/bin/MainUI-285-clean
.tmp_update/bin/MainUI-285-expert
```

Here, `MainUI-*` means the launcher binaries in the table, not unrelated files or backups that happen to start with `MainUI`. Use `build/onion/MainUI`, not the host preview `build/MainUI-dev`. You do not need to replace `miyoo/app/MainUI` separately, edit `runtime.sh`, or create a `mainui-test` directory.

To update, repeat the copies with the new device build. To return to stock, restore each original file from your backup while the device is off. This method has no `DISABLED` switch or wrapper session log. An Onion update may restore its own launcher binaries; repeat the installation afterward if needed, saving the updated originals first.

### Optional wrapper: install with a stock switch and logging

Use this alternative if you want to switch back with a `DISABLED` file and capture a session log. Start with an original Onion launcher as the stock backup; if you already used direct replacement, recover that original from your computer backup first.

1. Create `.tmp_update/mainui-test/stock/` on the card.
2. **Before replacing anything**, copy the selected original launcher from the table into that directory and rename the copy to `MainUI`. This is the fallback and uninstall backup. Record its original filename. If a backup already exists from an earlier installation, keep it; do not overwrite it with the wrapper.
3. Copy this project's device build, `build/onion/MainUI`, to `.tmp_update/mainui-test/MainUI`. The host build, `build/MainUI-dev`, cannot run on the device.
4. Copy `device/MainUI-test-wrapper.sh` over the selected file in `.tmp_update/bin/`, using the selected filename from the table, **without a `.sh` extension**. Keep Unix LF line endings in the script.
5. Safely eject the card, insert it into the device, and boot.

For a Miyoo Mini Plus with Expert mode off, the resulting layout is:

```text
SD card/
  .tmp_update/
    bin/
      MainUI-354-clean       <- contents of MainUI-test-wrapper.sh
    mainui-test/
      MainUI                <- build/onion/MainUI
      stock/
        MainUI              <- original MainUI-354-clean binary
```

Install into only the selected variant with this procedure. Switching Expert mode makes Onion select the other variant, which remains stock. The wrapper has one fixed stock-backup path, so this procedure does not provide separate stock fallbacks for multiple variants.

#### What the wrapper does

On each launch, the wrapper checks for `.tmp_update/mainui-test/DISABLED` and whether `.tmp_update/mainui-test/MainUI` is executable. If the marker exists, or the test binary is missing or not executable, it runs `stock/MainUI` instead, provided that is executable; otherwise it still starts Open MainUI rather than no launcher.

Otherwise it sets the library search path, changes to `/mnt/SDCARD/miyoo/app`, and runs the test binary with:

```sh
--sd-root /mnt/SDCARD --device real --handoff-dir /tmp
```

It uses `exec` and keeps the binary named `MainUI` so Onion's key monitor sees the expected process name. Output is silent unless `.tmp_update/config/.logging` exists. When enabled, stdout and stderr append to `.tmp_update/logs/MainUI.log`, including the exit-only [timing report](TIMING.md). Before a launch, a log of at least 1 MiB is moved to `MainUI.log.1`, replacing the previous rotated log. A single running session can exceed that size. Logging is best effort: if the log cannot be written (full or read-only card), MainUI starts without one.

**This is a manual fallback, not crash recovery.** The wrapper does not catch crashes, hangs, invalid executables or missing shared libraries. The stock backup must also exist and be executable for the fallback to be used.

#### Disable, update or uninstall the wrapper

- **Return to stock:** shut down, put the card in your computer, and create an empty file named `DISABLED` inside `.tmp_update/mainui-test/`. Ensure it is not named `DISABLED.txt`. Boot again. Remove the marker and reboot to resume testing. From a device shell, `touch /mnt/SDCARD/.tmp_update/mainui-test/DISABLED` creates the marker; the choice takes effect the next time the wrapper starts.
- **Update Open MainUI:** with the device shut down, replace only `.tmp_update/mainui-test/MainUI` with a new `build/onion/MainUI`. Keep the wrapper and stock backup.
- **Uninstall:** with the card in your computer, copy `.tmp_update/mainui-test/stock/MainUI` back over the exact `.tmp_update/bin/` filename you replaced. After restoring it, you can remove the `mainui-test` directory.

An Onion update may replace the wrapper in `.tmp_update/bin/`. After an update, check the selected launcher again and preserve the updated stock binary before reinstalling the wrapper.
