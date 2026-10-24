# SPDX-License-Identifier: GPL-3.0-only
"""A console with "hidebios": 1 leaves files whose names start with "[BIOS]"
out of its scanned ROM list, as stock does; with 0, or without it, they are
listed. As in stock the match is case-sensitive and only files count: a
folder named "[BIOS] ..." is listed, and a folder holding only BIOS files
is not, as no ROM lies below it."""
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="hidebios-", dir=BUILD))
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": dict(games=True)}))
FILES = ["Game.zip", "[BIOS] Console (World).zip", "[bios] lower.zip",
         "Sub/[BIOS] Nested.zip", "Sub/Nested game.zip", "Only/[BIOS] Alone.zip",
         "[BIOS] Folder/Inside.zip"]
for name, hide in (("HIDE", 1), ("SHOW", 0)):
    (SD / f"Emu/{name}").mkdir(parents=True)
    (SD / f"Emu/{name}/config.json").write_text(json.dumps(dict(
        label=name, rompath=f"../../Roms/{name}", extlist="zip", hidebios=hide)))
    for file in FILES:
        (SD / f"Roms/{name}" / file).parent.mkdir(parents=True, exist_ok=True)
        (SD / f"Roms/{name}" / file).write_bytes(b"rom")


def listed(name):
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--system", name, "--refresh-caches", "--snapshot", str(SD / "shot.bmp")],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    with closing(sqlite3.connect(f"file:{SD}/Roms/{name}/{name}_cache6.db?mode=ro", uri=True)) as db:
        return sorted(row[0].rsplit("/", 1)[-1] for row in
                      db.execute(f"SELECT path FROM {name}_roms").fetchall())


everything = ["Game.zip", "Inside.zip", "Nested game.zip", "Only", "Sub", "[BIOS] Alone.zip",
              "[BIOS] Console (World).zip", "[BIOS] Folder", "[BIOS] Nested.zip",
              "[bios] lower.zip"]
assert listed("SHOW") == everything, listed("SHOW")
assert listed("HIDE") == ["Game.zip", "Inside.zip", "Nested game.zip", "Sub", "[BIOS] Folder",
                          "[bios] lower.zip"], listed("HIDE")
print("hidebios leaves [BIOS] files out of the scanned list, as stock does")
