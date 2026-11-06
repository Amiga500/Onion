# SPDX-License-Identifier: GPL-3.0-only
"""A MainUI started again (after the Game Switcher, say) opens Games and Expert
on the console last selected in each, which it keeps in the runtime folder by
config file, not by row; a console that is gone gives the first one."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
TOP = Path(tempfile.mkdtemp(prefix="console-position-", dir=BUILD))
SD, RUNTIME = TOP / "sd", TOP / "runtime"
FILE = RUNTIME / "console-position.json"
UI = os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev"))
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text(
    json.dumps({"menu": dict(games=True, expert=True, apps=True, settings=True)}))
for root in ("Emu", "RApp"):
    for i in range(12):
        (SD / root / f"C{i:02}").mkdir(parents=True)
        (SD / root / f"C{i:02}/config.json").write_text(json.dumps(dict(
            label=f"Console {i:02}", rompath=f"../../Roms/C{i:02}", launch="launch.sh",
            extlist="nes")))
    # Two emulators for one ROM folder, listed right after Console 00.
    for name in ("A", "B"):
        (SD / root / f"Shared{name}").mkdir(parents=True)
        (SD / root / f"Shared{name}/config.json").write_text(json.dumps(dict(
            label=f"Console 00 {name}", rompath="../../Roms/C00", launch="launch.sh",
            extlist="nes")))
for i in range(12):
    (SD / f"Roms/C{i:02}").mkdir(parents=True, exist_ok=True)
    (SD / f"Roms/C{i:02}/Game.nes").write_bytes(b"rom")
RUNTIME.mkdir()


def shot(actions, remember=True):
    out = TOP / f"{actions}{remember}.bmp"
    options = ["--handoff-dir", str(RUNTIME)] if remember else []
    subprocess.run([UI, "--sd-root", str(SD), "--theme", str(ONION_THEME), "--input", actions,
                    "--snapshot", str(out), *options], cwd=ROOT, check=True, timeout=30,
                   capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return out.read_bytes()


# Home is Games, Expert, Apps, Settings; Games and Expert are 4x2 / 3x3 grids.
first_games, first_expert = shot("E", False), shot("RE", False)
games, expert = shot("EDDDR", False), shot("REDDR", False)
assert games != first_games and expert != first_expert
assert shot("EDDDR") == games
assert json.loads(FILE.read_text()) == {"games": str(SD / "Emu/C11/config.json")}
assert shot("E") == games                 # started again: the same console
assert shot("REDDR") == expert            # Expert has its own
assert shot("RE") == expert and shot("E") == games
assert set(json.loads(FILE.read_text())) == {"games", "expert"}
assert shot("EBREBLE") == games           # by way of Expert, as before
# Entering a console's ROM list leaves it remembered, and a new start opens
# the console again, not its ROM list.
assert shot("EEBBE") == games and shot("E") == games
# With nothing remembered, the first console.
FILE.unlink()
assert shot("E") == first_games
# Of two emulators sharing a ROM folder, the selected one: by its config file.
shared = shot("ERR", False)
assert shot("ERR") == shared and shot("E") == shared
# A console no longer there, or a damaged file: the first console.
FILE.write_text(json.dumps({"games": str(SD / "Emu/Gone/config.json")}))
assert shot("E") == first_games
FILE.write_text("{")
assert shot("E") == first_games and shot("RE") == first_expert
print("Games and Expert open on the console last selected in each after a restart")
