# SPDX-License-Identifier: GPL-3.0-only
"""A ROM whose name holds $, such as "Super Game$ Collection (USA)",
launches: its path is written raw in cmd_to_run.sh, as
stock MainUI and GameSwitcher write it, and Onion's runtime.sh escapes every
$ of a game's command before running it. Run here through runtime.sh's own
lines, the launcher gets the literal path, and a name such as
"$(touch pwned) $HOME" runs nothing. A $ outside Roms, which runtime.sh does
not escape, is still refused."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="launch-dollar-", dir=BUILD))
HANDOFF = SD / "handoff"
SHELL = shutil.which("bash") or "/bin/sh"
GAME = "Super Game$ Collection (USA).zip"
HOSTILE = "$(touch pwned) $HOME.zip"


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


for directory, rompath in (("Emu/GBA", "../../Roms/GBA"), ("Emu/MD", "../../Media/MD")):
    write(SD / directory / "config.json", json.dumps(dict(
        label=Path(directory).name, rompath=rompath, launch="launch.sh", extlist="zip")))
    script = SD / directory / "launch.sh"
    write(script, '#!/bin/sh\nprintf "%s\\0" "$@" > "$MAINUI_FAKE_LOG"\n')
    script.chmod(0o755)
write(SD / ".tmp_update/config/main-menu.json", json.dumps({"menu": dict(games=True)}))
for name in (GAME, HOSTILE):
    write(SD / "Roms/GBA" / name, "")
write(SD / "Media/MD/a$b.zip", "")
HANDOFF.mkdir()

# runtime.sh's launch_game(): check_is_game, then "Handle dollar sign".
RUNTIME = r'''
cmd=$(cat "$sysdir/cmd_to_run.sh")
if echo "$cmd" | grep -q "retroarch/cores" || echo "$cmd" | grep -q "/../../Roms/" || echo "$cmd" | grep -q "/mnt/SDCARD/Roms/"; then
    rompath=$(echo "$cmd" | awk '{ st = index($0,"\" \""); print substr($0,st+3,length($0)-st-3)}')
    if echo "$rompath" | grep -q "\$"; then
        temp=$(cat $sysdir/cmd_to_run.sh)
        echo "$temp" | sed 's/\$/\\\$/g' > $sysdir/cmd_to_run.sh
    fi
fi
'''


def launch(system, actions):
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--system", system, "--input", actions, "--snapshot", str(SD / "shot.bmp"),
                    "--handoff-dir", str(HANDOFF)], cwd=ROOT, check=True, timeout=30,
                   capture_output=True)
    command = HANDOFF / "cmd_to_run.sh"
    return command.read_text(encoding="utf-8") if command.exists() else None


# GBA's list: the hostile name ($ sorts before S), then Super Game$.
for actions, name in (("E", HOSTILE), ("DE", GAME)):
    command = launch("GBA", actions)
    rom = f"/mnt/SDCARD/Emu/GBA/../../Roms/GBA/{name}"
    assert command == ("LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "
                       f'"/mnt/SDCARD/Emu/GBA/launch.sh" "{rom}"\n'), command
    # Run it as Onion does, with the fake launcher in place of the device's.
    run = HANDOFF / "run"
    run.mkdir()
    write(run / "cmd_to_run.sh", command.replace(
        "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so ", "").replace(
        "/mnt/SDCARD", SD.as_posix()))
    environment = dict(os.environ, sysdir=run.as_posix(), MAINUI_FAKE_LOG=(run / "argv").as_posix())
    subprocess.run([SHELL, "-c", RUNTIME], env=environment, check=True, cwd=run)
    subprocess.run([SHELL, run / "cmd_to_run.sh"], env=environment, check=True, cwd=run)
    host_rom = rom.replace("/mnt/SDCARD", SD.as_posix())
    assert (run / "argv").read_bytes() == host_rom.encode() + b"\0", (run / "argv").read_bytes()
    assert not (run / "pwned").exists() and not (ROOT / "pwned").exists()
    shutil.rmtree(run)
    (HANDOFF / "cmd_to_run.sh").unlink()
    for leftover in ("state.json", "mainui-return.json"):
        (HANDOFF / leftover).unlink(missing_ok=True)

# Outside Roms runtime.sh would not escape it: no command is written.
assert launch("MD", "E") is None
print("ROM names with $ below Roms launch with their literal path; elsewhere $ is refused")
