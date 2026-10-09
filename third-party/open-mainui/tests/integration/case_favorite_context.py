# SPDX-License-Identifier: GPL-3.0-only
"""Favorite popup row inventory drives real theme backgrounds and selection."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="favorite-context-", dir=BUILD))
SD = OUT / "sd"
THEME = OUT / "theme"
FALLBACK = ONION_THEME
for directory in (SD / "Emu", SD / "Roms", SD / ".tmp_update/config", THEME / "skin"):
    directory.mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text('{"menu":["favorites"]}')
(THEME / "config.json").write_bytes((FALLBACK / "config.json").read_bytes())
background = Image.new("RGB", (400, 190), (30, 80, 210))
background.paste((210, 30, 70), (0, 180, 400, 190))
background.save(THEME / "skin/bg-pop-menu-3.png")
Image.new("RGB", (400, 60), (40, 210, 60)).save(THEME / "skin/bg-list-popup-s.png")
records = [dict(label="Root game", rompath="root", type=5),
           dict(label="Deep game", rompath="deep", type=5)]
(SD / "Roms/favourite.json").write_text("".join(json.dumps(record) + "\n" for record in records))
folders = [dict(schema=1, generation=1),
           dict(kind="folder", id="a", parent="", name="First", order=0),
           dict(kind="folder", id="b", parent="a", name="Second", order=0),
           dict(kind="folder", id="c", parent="b", name="Third", order=0),
           dict(kind="item", key="deep", type=5, folder="c", order=0)]
(SD / "Roms/favourite-folders.json").write_text("".join(json.dumps(row) + "\n" for row in folders))
before = {file: file.read_bytes() for file in SD.rglob("*") if file.is_file()}


def capture(name, actions):
    target = OUT / (name + ".bmp")
    subprocess.run([
        str(BUILD / "MainUI-dev"),
        "--sd-root", str(SD), "--theme", str(THEME), "--fallback", str(FALLBACK),
        "--input", actions, "--snapshot", str(target),
    ], cwd=ROOT, check=True, timeout=20)
    return Image.open(target).convert("RGB")


folder = capture("folder-popup", "ES")
game = capture("game-popup", "EDS")
# Four rows expand the active three-row skin, retaining its ten-pixel trim.
assert game.getpixel((5, 10)) == (40, 210, 60)
assert game.getpixel((5, 200)) == (30, 80, 210)
assert game.getpixel((5, 245)) == (210, 30, 70)
assert folder.crop((0, 120, 400, 240)).tobytes() != game.crop((0, 120, 400, 240)).tobytes()
assert capture("cancel", "EDSB").tobytes() == capture("game-list", "ED").tobytes()
parent = capture("parent-popup", "EES")
assert parent.getpixel((5, 10)) == (40, 210, 60)
assert parent.getpixel((5, 200)) != (30, 80, 210)
deep = capture("depth-three-game", "EEDEDEDS")
assert deep.getpixel((5, 185)) == (210, 30, 70)
assert capture("depth-three-parent-select", "EEDEDES").tobytes() == capture("depth-three-parent", "EEDEDE").tobytes()
assert before == {file: file.read_bytes() for file in SD.rglob("*") if file.is_file()}
(SD / "Roms/favourite-folders.json").unlink()
(SD / "Roms/favourite.json").write_bytes(b"")
assert capture("empty-root-popup", "ES").getpixel((5, 10)) == (40, 210, 60)
print("Favorite game/folder/parent/empty/depth popups and active skin fallback passed:", OUT)
