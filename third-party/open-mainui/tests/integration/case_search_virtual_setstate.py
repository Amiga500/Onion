# SPDX-License-Identifier: GPL-3.0-only
"""Virtual Search folders share empty paths; Search setstate opens Emu/RApp."""
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile

from env import BUILD, ONION_THEME, require_onion_theme

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="search-virtual-", dir=BUILD))
SD = OUT / "sd"
HANDOFF = OUT / "handoff"
HANDOFF.mkdir()
UI = os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev"))


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(data)


write(SD / ".tmp_update/config/main-menu.json", json.dumps({"menu": {"games": True, "expert": True}}))
write(SD / "Emu/SEARCH/config.json", json.dumps({
    "label": " Search ", "launch": "../../App/Search/launch.sh",
    "rompath": "../../App/Search/data", "imgpath": "../../App/Search/data/Imgs",
    "extlist": "miyoocmd"}))
write(SD / "App/Search/launch.sh", "#!/bin/sh\n")
write(SD / "Emu/GB/config.json", json.dumps({
    "label": "GB", "launch": "launch.sh", "rompath": "../../Roms/GB", "extlist": "gb"}))
write(SD / "Roms/GB/Game.gb", "rom")
write(SD / "RApp/fbneo/config.json", json.dumps({
    "label": "fbneo", "launch": "launch.sh", "rompath": "../../Roms/FBNEO", "extlist": "zip"}))
write(SD / "Roms/FBNEO/Example.zip", "rom")

DB = SD / "App/Search/data/data_cache6.db"
DB.parent.mkdir(parents=True, exist_ok=True)
with sqlite3.connect(DB) as db:
    db.execute("CREATE TABLE data_roms (id INTEGER PRIMARY KEY AUTOINCREMENT,disp TEXT NOT "
               "NULL,path TEXT NOT NULL,imgpath TEXT NOT NULL,type INTEGER DEFAULT 0,"
               "ppath TEXT NOT NULL,pinyin TEXT NOT NULL,cpinyin TEXT NOT NULL)")
    rows = [
        ("All systems (1)", "", 1, "."),
        ("~Missing caches (2)", "", 1, "."),
        ("One game", "clear", 0, "All systems (1)"),
        ("GB", "setstate:/mnt/SDCARD/Emu/GB:GB", 0, "~Missing caches (2)"),
        ("fbneo", "setstate:/mnt/SDCARD/RApp/fbneo:fbneo", 0, "~Missing caches (2)"),
    ]
    db.executemany("INSERT INTO data_roms (disp,path,imgpath,type,ppath,pinyin,cpinyin) "
                   "VALUES (?,?,'',?,?,?,'')", [row + (row[0],) for row in rows])


def run(name, *options):
    shot = OUT / (name + ".bmp")
    result = subprocess.run([UI, "--sd-root", str(SD), "--theme", str(ONION_THEME),
                             "--snapshot", str(shot), *options], cwd=ROOT,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (name, result.stderr[-1500:])
    return shot.read_bytes()


# Selection must survive JOB_ENTER's serialize -> restore -> enter cycle.
all_systems = run("all", "--system", " Search ", "--input", "E")
missing = run("missing", "--system", " Search ", "--input", "DE")
assert missing != all_systems, "Two empty-path virtual folders reopened the same results"
# B held in a folder goes back one level, to Search's list, not out of
# Search with its repeats; two presses go back two.
back = run("back", "--system", " Search ", "--input", "DEB")
assert run("held", "--system", " Search ", "--input", "DE+B+B+B") == back
assert run("twice", "--system", " Search ", "--input", "DEBB") != back


def state(page_type, label):
    key = "emuname" if page_type == 5 else "raname"
    return {"list": [
        {"title": 157, "type": 0, "currpos": 1, "pagestart": 0, "pageend": 3},
        {"title": 2 if page_type == 5 else 0, "type": 1 if page_type == 5 else 16,
         "currpos": 0, "pagestart": 0, "pageend": 5},
        {"title": -1, "type": page_type, "currpos": 0, "pagestart": 0,
         "pageend": 5, key: label},
    ]}


for name, keys, kind, label, section in (
    # The folder lists "..", then fbneo and GB, in order regardless of case.
    ("emu", "DEDDE", 5, "GB", "GB"),
    ("expert", "DEDE", 17, "fbneo", "fbneo"),
):
    for path in (HANDOFF / "cmd_to_run.sh", HANDOFF / "mainui-return.json"):
        path.unlink(missing_ok=True)
    run("launch-" + name, "--system", " Search ", "--input", keys,
        "--handoff-dir", str(HANDOFF))
    assert (HANDOFF / "mainui-return.json").exists(), name
    assert (HANDOFF / "cmd_to_run.sh").exists(), name
    (HANDOFF / "cmd_to_run.sh").unlink()  # Onion ran the Search launch script
    write(HANDOFF / "state.json", json.dumps(state(kind, label)))
    actual = run("returned-" + name, "--handoff-dir", str(HANDOFF))
    if kind == 17:
        # --system addresses Games only; enter Expert through the normal menu.
        write(SD / ".tmp_update/config/main-menu.json",
              json.dumps({"menu": {"expert": True}}))
        expected = run("expected-" + name, "--input", "EE")
    else:
        expected = run("expected-" + name, "--system", section)
    assert actual == expected, f"Search setstate did not open {section}'s ROM list"
    assert not (HANDOFF / "state.json").exists(), "state.json must be consumed once"

print("Search virtual folders and Emu/Expert setstate actions restore correctly")
