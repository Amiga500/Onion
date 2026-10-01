# SPDX-License-Identifier: GPL-3.0-only
"""Exercise Favorite editing through models and the real SDL popup/name UI."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="favorite-edit-", dir=BUILD))
THEME = ONION_THEME


def write_lines(path, rows):
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def fixture(name):
    sd = OUT / name
    for directory in ("Emu", "Roms", ".tmp_update/config"):
        (sd / directory).mkdir(parents=True)
    (sd / ".tmp_update/config/main-menu.json").write_text('{"menu":["favorites"]}')
    games = [dict(label=label, rompath=label, type=5, keep="stock") for label in ("Zebra", "apple", "Apple", "Inside")]
    write_lines(sd / "Roms/favourite.json", games)
    write_lines(sd / "Roms/favourite-folders.json", [
        dict(schema=1, generation=4, keep="header"),
        dict(kind="folder", id="container", parent="", name="Container", order=0, keep="folder"),
        dict(kind="folder", id="nested", parent="container", name="Nested", order=0),
        dict(kind="item", key="Inside", type=5, folder="container", order=0, keep="assignment"),
        dict(kind="extension", keep="opaque"),
    ])
    return sd


def capture(sd, name, actions, text=None):
    path = OUT / (name + ".bmp")
    command = [str(BUILD / "MainUI-dev"),
               "--sd-root", str(sd), "--theme", str(THEME), "--input", actions, "--snapshot", str(path)]
    if text is not None:
        command += ["--text", text]
    subprocess.run(command, cwd=ROOT, check=True, timeout=30)
    return Image.open(path).convert("RGB").tobytes()


model = fixture("model")
stock = (model / "Roms/favourite.json").read_bytes()
subprocess.run([str(BUILD / "fixture-favorite_edit"), str(model)], cwd=ROOT, check=True)
assert (model / "Roms/favourite.json").read_bytes() == stock
records = [json.loads(line) for line in (model / "Roms/favourite-folders.json").read_text().splitlines() if line.strip()]
assert records[0]["keep"] == "header"
assert any(row.get("keep") == "assignment" for row in records)
assert any(row.get("keep") == "opaque" for row in records)
assert not any(row.get("id") == "container" for row in records)
assert any(row.get("id") == "nested" and row["parent"] == "" for row in records)

ui = fixture("ui")
sidecar = ui / "Roms/favourite-folders.json"
stock = (ui / "Roms/favourite.json").read_bytes()
baseline = sidecar.read_bytes()
# SELECT on the first folder -> Create -> name keyboard -> START saves.
capture(ui, "create-cancel", "ESDEB", "Cancelled")
assert sidecar.read_bytes() == baseline
capture(ui, "create", "ESDET", "New folder")
rows = [json.loads(line) for line in sidecar.read_text().splitlines()]
created = next(row for row in rows if row.get("name") == "New folder")
assert capture(ui, "new-selected", "ESDET", "Another") != capture(ui, "root", "E")
# First folder Rename is the third menu entry, Delete is fourth.
capture(ui, "rename", "ESDDET", "Renamed container")
rows = [json.loads(line) for line in sidecar.read_text().splitlines()]
assert next(row for row in rows if row.get("id") == "container")["name"] == "Renamed container"
assert next(row for row in rows if row.get("id") == "container")["keep"] == "folder"
# Moving the first folder changes only its marker until Move here executes.
before = sidecar.read_bytes()
assert capture(ui, "cut-marker", "ESE") != capture(ui, "uncut", "E")
assert sidecar.read_bytes() == before
capture(ui, "cut-exit", "ESEBE")
assert sidecar.read_bytes() == before
# Rename failure retains the input file and keeps the name keyboard open.
capture(ui, "duplicate", "ESDDET", "New folder")
assert sidecar.read_bytes() == before
(sidecar.parent / (sidecar.name + ".writing")).write_text("foreign writer")
capture(ui, "locked-create", "ESDET", "Locked")
assert sidecar.read_bytes() == before
(sidecar.parent / (sidecar.name + ".writing")).unlink()
sidecar.write_bytes(b"{broken")
capture(ui, "malformed-create", "ESDET", "Unsafe")
assert sidecar.read_bytes() == b"{broken"
assert (ui / "Roms/favourite.json").read_bytes() == stock
print("Favorite editing persistence, UI name/cancel/cut flow, unknown fields and failed-save preservation passed:", OUT)

removal = fixture("remove-assignment")
assignment = removal / "Roms/favourite-folders.json"
with assignment.open("a", encoding="utf-8") as stream:
    stream.write(json.dumps(dict(kind="item", key="Zebra", type=5, folder="", order=0)) + "\n")
capture(removal, "remove-assignment", "EDSDDDE")
assert not any(json.loads(line).get("label") == "Zebra" for line in (removal / "Roms/favourite.json").read_text().splitlines())
assert not any(json.loads(line).get("key") == "Zebra" for line in assignment.read_text().splitlines())
print("Remove Favorite also removes its sidecar assignment")

keyboard = fixture("keyboard")
capture(keyboard, "keyboard-blank", "ESDE", "")
capture(keyboard, "keyboard-text", "ESDE", "Readable")
blank = Image.open(OUT / "keyboard-blank.bmp").convert("RGB")
filled = Image.open(OUT / "keyboard-text.bmp").convert("RGB")
assert blank.crop((40, 66, 600, 114)).tobytes() != filled.crop((40, 66, 600, 114)).tobytes()
assert filled.getpixel((80, 130)) == (0, 0, 0), "Eight-pixel gap after the first 52px key"
assert filled.getpixel((25, 127)) != filled.getpixel((80, 130)), "First key starts at (24,126)"
print("Recovered keyboard field contrast and key geometry passed")
