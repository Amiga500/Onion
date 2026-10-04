# SPDX-License-Identifier: GPL-3.0-only
"""Exercise XML cache publication on isolated, empty ROM fixtures."""
from contextlib import closing
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="xml-import-", dir=BUILD))
ROM = SD / "Roms/Jeux"
EMU = SD / "Emu/Test"
THEME = ONION_THEME
EXE = str(BUILD / "MainUI-dev")
XML = ROM / "miyoogamelist.xml"
DB = ROM / "Jeux_cache6.db"


def run(*options, code=0):
    result = subprocess.run(
        ([str(BUILD / "fixture-cache"), str(SD), "rebuild"]
         if options == ("--refresh-caches",) else
         [EXE, "--sd-root", str(SD), "--theme", str(THEME),
          "--snapshot", str(SD / "capture.bmp"), *options]),
        cwd=ROOT, timeout=30, capture_output=True,
    )
    assert result.returncode == code, result.stderr.decode(errors="replace")


def rows():
    with closing(sqlite3.connect(DB)) as connection:
        assert connection.execute("pragma integrity_check").fetchone() == ("ok",)
        return connection.execute(
            "select disp,path,imgpath,type,ppath from Jeux_roms order by id"
        ).fetchall()


def game(path, name, image=""):
    return f"<game><path>{path}</path><name>{name}</name>{image}</game>"


EMU.mkdir(parents=True)
(ROM / "Sets/Deep").mkdir(parents=True)
(EMU / "config.json").write_text(json.dumps({
    "label": "XML test", "rompath": "../../Roms/Jeux", "extlist": "nes",
}), encoding="utf-8")
for name in ("first.nes", "second.nes", "third.nes", "unlisted.nes", "café.nes",
             "Sets/Deep/a.nes", "Sets/Deep/b.nes"):
    (ROM / name).write_bytes(b"")

document = (
    '\ufeff<?xml version="1.0" encoding="UTF-8"?>\n<gameList>\n'
    '<!-- ignored <game> markup -->'
    '<game id="one"><path>./first.nes</path><name>A &amp; B &#x1F3AE;</name>'
    '<image>./Imgs/one.png</image><desc><extra>Ignored</extra></desc></game>'
    + game("./second.nes", "Missing image")
    + game("./third.nes", "Empty image", "<image/>")
    + game("./café.nes", "<![CDATA[Café <demo>]]>", "<image></image>")
    + game("./Sets/Deep/a.nes", "Nested A")
    + game("/mnt/SDCARD/Roms/Jeux/Sets/Deep/b.nes", "Nested B")
    + game("./missing.nes", "Skip missing")
    + game("./Sets", "Skip directory")
    + game("./first.nes", "")
    + '<game><name>Missing path</name></game></gameList>'
)
XML.write_text(document, encoding="utf-8")
run("--system", "XML test")
actual = rows()
stock_prefix = "/mnt/SDCARD/Emu/Test/../../Roms/Jeux/"
assert actual == [
    ("A & B " + chr(0x1f3ae), stock_prefix + "./first.nes", stock_prefix + "./Imgs/one.png", 0, "."),
    ("Missing image", stock_prefix + "./second.nes", "", 0, "."),
    ("Empty image", stock_prefix + "./third.nes", "", 0, "."),
    ("Café <demo>", stock_prefix + "./café.nes", "", 0, "."),
    ("Sets", stock_prefix + "Sets", stock_prefix + "Sets", 1, "."),
    ("Deep", stock_prefix + "Sets/Deep", stock_prefix + "Sets/Deep", 1, "Sets"),
    ("Nested A", stock_prefix + "./Sets/Deep/a.nes", "", 0, "Sets/Deep"),
    ("Nested B", "/mnt/SDCARD/Roms/Jeux/Sets/Deep/b.nes", "", 0, "Sets/Deep"),
], actual
run("--system", "XML test", "--input", "EDE")
assert XML.read_text(encoding="utf-8") == document

# Malformed XML must not replace the old database, even after valid rows.
prefix = '<gameList>' + game('./first.nes', 'Uncommitted replacement')
invalid = [
    prefix, prefix + '</wrong>', '<gameList/><gameList/>',
    prefix + '<game><path>bad</name></game></gameList>',
    prefix + game('./first.nes', '&unknown;') + '</gameList>',
    prefix + game('./first.nes', '&#0;') + '</gameList>',
    prefix + game('./first.nes', '&#xD800;') + '</gameList>',
    prefix + game('./first.nes', 'x' * 4096) + '</gameList>',
    prefix + '<game><name>x</name><name>y</name></game></gameList>',
    '<!DOCTYPE gameList [<!ENTITY x SYSTEM "file:///ignored">]><gameList/>',
    '<gameList>' + '<unknown>' * 33 + '</unknown>' * 33 + '</gameList>',
    '<gameList><game attr="unfinished', '<gameList>\x00</gameList>',
    '<gameList><!-- invalid -- comment --></gameList>',
]
before = DB.read_bytes()
for text in invalid:
    XML.write_text(text, encoding="utf-8")
    run("--refresh-caches", code=4)
    assert DB.read_bytes() == before, text[:80]
    assert not Path(str(DB) + ".building").exists()

# Invalid UTF-8 and oversized inputs also preserve the published cache.
for data in (b'<gameList>\xff</gameList>', b'<gameList>\xc0\xaf</gameList>',
             b'<gameList>\xed\xa0\x80</gameList>', b'x' * (16 * 1024 * 1024 + 1)):
    XML.write_bytes(data)
    run("--refresh-caches", code=4)
    assert DB.read_bytes() == before
    assert not Path(str(DB) + ".building").exists()

# A valid empty list is authoritative; removing XML restores filesystem scanning.
XML.write_text('<gameList/>', encoding="utf-8")
run("--refresh-caches")
assert rows() == []
XML.unlink()
run("--refresh-caches")
assert len(rows()) == 9

# Index a large directory once, then reuse it for distinct XML ROM records.
for index in range(10050):
    (ROM / f"bulk{index:05d}.nes").write_bytes(b"")
XML.write_text('<gameList>' + ''.join(
    game(f'./bulk{i:05d}.nes', f'Title {i:05d}') for i in range(10050)
) + '</gameList>', encoding="utf-8")
run("--refresh-caches")
assert len(rows()) == 10050
run("--system", "XML test", "--input", "U")
# Interleaved folders, symlink targets, and nonregular entries retain stat semantics.
(ROM / "Alias.nes").symlink_to("first.nes")
(ROM / "Dangling.nes").symlink_to("absent.nes")
(ROM / "Directory.nes").symlink_to("Sets", target_is_directory=True)
os.mkfifo(ROM / "Pipe.nes")
paths = ["./first.nes", "./Sets/Deep/a.nes", "./second.nes",
         "./Sets/Deep/b.nes", "./Alias.nes", "./Dangling.nes",
         "./Directory.nes", "./Pipe.nes", "./missing.nes", "./FIRST.NES",
         "./MissingFolder/no.nes", "./café.nes"]
expected = {path for path in paths if (ROM / path).is_file()}
XML.write_text("<gameList>" + "".join(game(path, path) for path in paths)
               + "</gameList>", encoding="utf-8")
run("--refresh-caches")
assert {row[1] for row in rows() if row[3] == 0} == {stock_prefix + path for path in expected}
# Directory snapshots are import-local: additions/removals appear next time.
(ROM / "second.nes").unlink()
(ROM / "missing.nes").write_bytes(b"")
expected = {path for path in paths if (ROM / path).is_file()}
run("--refresh-caches")
assert {row[1] for row in rows() if row[3] == 0} == {stock_prefix + path for path in expected}

print("XML import checks passed: names/art, empty images, Unicode/entities/CDATA, nested "
      "folders, invalid records, rollback, empty list, scan fallback, 10,050 rows")
