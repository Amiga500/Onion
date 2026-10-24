# SPDX-License-Identifier: GPL-3.0-only
"""Game Info names, and a saved game's cover is looked for in, the console a
game belongs to by its launcher (tests/test_sources.c): consoles can share a
ROM folder, and a Favorite's console may be an Expert one while Games is
open. They used the first console whose ROM folder held the game."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="saved-source-", dir=BUILD))
for folder, label, images in (("Emu/GB", "Game Boy", "Imgs"), ("Emu/GB2", "Game Boy 2", "Imgs2"),
                              ("RApp/GBX", "Expert GB", "ImgsX")):
    (SD / folder).mkdir(parents=True)
    (SD / folder / "config.json").write_text(json.dumps(dict(
        label=label, rompath="../../Roms/GB", imgpath=f"../../Roms/GB/{images}", extlist="gb",
        launch="launch.sh")))
    (SD / folder / "launch.sh").write_text("#!/bin/sh\n")
    (SD / "Roms/GB" / images).mkdir(parents=True)
    Image.new("RGB", (20, 20), (200, 0, 0)).save(SD / "Roms/GB" / images / "game.png")
(SD / "Roms/GB/game.gb").write_bytes(b"rom")
ROM = "/mnt/SDCARD/Emu/GB/../../Roms/GB/game.gb"
rows = [dict(label="One", launch="/mnt/SDCARD/Emu/GB/launch.sh", imgpath=""),
        dict(label="Two", launch="/mnt/SDCARD/Emu/GB2/launch.sh", imgpath=""),
        dict(label="Three", launch="/mnt/SDCARD/RApp/GBX/launch.sh", imgpath=""),
        dict(label="Four", launch="/mnt/SDCARD/Emu/Gone/launch.sh", imgpath=""),
        dict(label="Five", launch="/mnt/SDCARD/Emu/GB2/launch.sh",
             imgpath="/mnt/SDCARD/Roms/GB/ImgsX/game.png")]
(SD / "Roms/favourite.json").write_text(
    "".join(json.dumps(dict(row, rompath=ROM, type=5)) + "\n" for row in rows))
result = subprocess.run([str(BUILD / "fixture-sources"), str(SD)], cwd=ROOT, check=True,
                        timeout=30, capture_output=True, text=True,
                        env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"))
print(result.stdout.strip())
