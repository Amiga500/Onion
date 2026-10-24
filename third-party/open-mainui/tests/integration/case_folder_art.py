# SPDX-License-Identifier: GPL-3.0-only
"""A folder in a ROM list shows <image folder>/<folder name>.png as its cover,
at any depth, with its whole name (dots included); one without a PNG shows
none, and a ROM's cover is as before. Favorites folders show none. The cache
keeps stock's imgpath for a folder, its own path."""
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="folder-art-", dir=BUILD))
SD = OUT / "sd"
ROMS = SD / "Roms/FC"
for folder in ("Emu/FC", ".tmp_update/config", "Roms/FC/Imgs"):
    (SD / folder).mkdir(parents=True)
(SD / "Emu/FC/config.json").write_text(json.dumps(dict(
    label="NES", rompath="../../Roms/FC", imgpath="../../Roms/FC/Imgs", extlist="nes")))
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": dict(games=True)}))
for rom in ("Folder/a.nes", "Folder/Sub/b.nes", "Mario Bros. Collection/c.nes", "NoArt/d.nes",
            "game.nes"):
    (ROMS / rom).parent.mkdir(parents=True, exist_ok=True)
    (ROMS / rom).write_bytes(b"rom")
COLOURS = {"Folder": (200, 0, 0), "Sub": (0, 200, 0), "Mario Bros. Collection": (0, 0, 200),
           "game": (200, 200, 0), "Mario Bros": (200, 0, 200)}
for name, colour in COLOURS.items():
    Image.new("RGB", (200, 200), colour).save(ROMS / "Imgs" / f"{name}.png")


def cover(actions):
    """The colour at the middle of the cover pane."""
    target = OUT / "shot.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--input", actions, "--snapshot", str(target)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    return Image.open(target).convert("RGB").getpixel((510, 240))


# The console's list: Folder, Mario Bros. Collection, NoArt, then game.
assert cover("EE") == COLOURS["Folder"]
# The whole name: not Mario Bros.png, the name up to its last dot.
assert cover("EED") == COLOURS["Mario Bros. Collection"]
no_cover = cover("EEDD")
assert no_cover not in COLOURS.values()
assert cover("EEDDD") == COLOURS["game"]
# Inside Folder: .. (no cover), then the nested Sub.
assert cover("EEE") == no_cover
assert cover("EEED") == COLOURS["Sub"]

# Favorites folders show none, even one named as a folder with art; a game
# in Favorites still shows its own.
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": ["favorites"]}))
(SD / "Roms/favourite.json").write_text(json.dumps(dict(
    label="game", rompath="/mnt/SDCARD/Roms/FC/game.nes", type=5)) + "\n")
(SD / "Roms/favourite-folders.json").write_text("".join(json.dumps(row) + "\n" for row in [
    dict(schema=1, generation=0), dict(kind="folder", id="f", parent="", name="Folder", order=0)]))
assert cover("E") == no_cover
assert cover("ED") == COLOURS["game"]

with closing(sqlite3.connect(f"file:{ROMS / 'FC_cache6.db'}?mode=ro", uri=True)) as db:
    rows = db.execute("SELECT path, imgpath FROM FC_roms WHERE type=1").fetchall()
assert len(rows) == 4 and all(path == image for path, image in rows), rows
print("Folders show <folder name>.png at any depth; the cache keeps stock's folder imgpath")
