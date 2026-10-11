# SPDX-License-Identifier: GPL-3.0-only
"""Details of a saved game whose system is not in the open catalog (an Expert
emulator while Games is loaded) read the gamelist.xml in that system's ROM
folder, found from its launcher's rompath, as for a system in the catalog."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="details-saved-root-", dir=BUILD))
SD = OUT / "sd"
UI = os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev"))
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text(
    json.dumps({"menu": dict(favorites=True, games=True)}))
(SD / "Emu/NES").mkdir(parents=True)
(SD / "Emu/NES/config.json").write_text(json.dumps(dict(
    label="NES", rompath="../../Roms/NES", launch="launch.sh", extlist="nes")))
(SD / "Roms/NES").mkdir(parents=True)
(SD / "Roms/NES/Other.nes").write_bytes(b"rom")
# Only in Expert, with the game one folder below its ROM root.
(SD / "RApp/Only").mkdir(parents=True)
(SD / "RApp/Only/config.json").write_text(json.dumps(dict(
    label="Expert only", rompath="../../Roms/EXPERT_ONLY", launch="launch.sh", extlist="rom")))
(SD / "Roms/EXPERT_ONLY/Sub").mkdir(parents=True)
(SD / "Roms/EXPERT_ONLY/Sub/Game.rom").write_bytes(b"rom")
(SD / "Roms/favourite.json").write_text(json.dumps(dict(
    label="Game", rompath="/mnt/SDCARD/Roms/EXPERT_ONLY/Sub/Game.rom",
    launch="/mnt/SDCARD/RApp/Only/launch.sh", type=5)) + "\n")


def details(name):
    shot = OUT / f"{name}.bmp"
    subprocess.run([UI, "--sd-root", str(SD), "--theme", str(ONION_THEME), "--input", "ER",
                    "--snapshot", str(shot)], cwd=ROOT, check=True, timeout=30,
                   capture_output=True)  # Favorites, Details
    return shot.read_bytes()


without = details("without")
(SD / "Roms/EXPERT_ONLY/gamelist.xml").write_text(
    "<gameList><game><path>./Sub/Game.rom</path><name>Game</name><genre>Puzzle</genre>"
    "<desc>A description from the system's gamelist.</desc></game></gameList>")
assert details("with") != without, "the saved game's gamelist.xml was not read"
# Whitespace in the genre collapses, as stock's parser does, so a line break
# or a run of spaces does not change the summary line.
(SD / "Roms/EXPERT_ONLY/gamelist.xml").write_text(
    "<gameList><game><path>./Sub/Game.rom</path><name>Game</name>"
    "<genre>Puzzle   and\n   Action</genre><desc>Text.</desc></game></gameList>")
spaced = details("spaced")
(SD / "Roms/EXPERT_ONLY/gamelist.xml").write_text(
    "<gameList><game><path>./Sub/Game.rom</path><name>Game</name>"
    "<genre>Puzzle and Action</genre><desc>Text.</desc></game></gameList>")
assert details("single") == spaced, "genre whitespace was not collapsed"
print("Details of a saved game outside the open catalog read its system's gamelist.xml; "
      "genre whitespace collapses")
