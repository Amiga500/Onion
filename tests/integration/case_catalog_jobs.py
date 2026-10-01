# SPDX-License-Identifier: GPL-3.0-only
"""Qualify cancellable catalog jobs and coherent external edits on isolated data."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="catalog-jobs-", dir=BUILD))
THEME = ONION_THEME
EXE = str(BUILD / "MainUI-dev")
(SD / "Emu/Host").mkdir(parents=True)
(SD / "Roms/Host").mkdir(parents=True)
(SD / "Emu/Host/config.json").write_text(json.dumps(dict(label="Host", rompath="../../Roms/Host", launch="launch.sh", extlist="nes")))
for i in range(200):
    (SD / f"Roms/Host/game{i:03}.nes").write_bytes(b"")
subprocess.run([str(BUILD / "fixture-catalog_job"), str(SD)], check=True, timeout=30)
# A cancelled XML import must retain the previous complete cache too.
(SD / "Roms/Host/miyoogamelist.xml").write_text("<gameList>" + "".join(
    f"<game><path>game{i:03}.nes</path><name>game{i:03}</name></game>" for i in range(200)) + "</gameList>")
# Use a fresh scan cache for the same native selection/order fixture.
(SD / "Roms/Host/Host_cache6.db").unlink()
subprocess.run([str(BUILD / "fixture-catalog_job"), str(SD)], check=True, timeout=30)
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text('{"menu":{"games":true}}')

def capture(name, actions):
    path = SD / (name + ".bmp")
    subprocess.run([EXE, "--sd-root", str(SD), "--theme", str(THEME), "--input", actions,
                    "--snapshot", str(path)], check=True, timeout=30)
    return path.read_bytes()

assert capture("home", "") == capture("home-back", "B")
assert capture("home", "") == capture("home-start", "T")
assert capture("home", "") == capture("home-menu", "M")
assert capture("systems", "E") == capture("cancelled-enter", "EEC")
assert capture("games", "EE") == capture("reload", "EET")
print("Catalog job fixtures passed:", SD)
