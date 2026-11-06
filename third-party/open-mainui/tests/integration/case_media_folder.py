# SPDX-License-Identifier: GPL-3.0-only
"""A media folder (Game Info's box art, screenshots and videos) is never a ROM
folder: like Imgs and Manuals, in any case and at any depth, it and all below
it are left out of ROM lists, their caches and so Search, also when the list
comes from miyoogamelist.xml. A ROM folder whose name only contains the word
is listed as before."""
from xml.sax.saxutils import escape
import json
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="media-folder-", dir=BUILD))
GAMES = ["Super Game$ Collection (USA).nes", "Other Game (Europe).nes",
         "Media Games/Inside.nes", "Sub/Nested.nes", "Mediaplayer.nes"]
MEDIA = ["media/box2dfront/Other Game (Europe).nes", "media/box2dback/Other Game (Europe).png",
         "media/box3d/Other Game (Europe).nes", "media/screenshot/Other Game (Europe)-1.nes",
         "media/videos/Other Game (Europe).mp4", "media/videos/Old/Clip.nes", "MEDIA/Upper.nes",
         "Sub/Media/Deep.nes", "media/Top.zip"]


def build(name, files, xml=None):
    """A console with these files, its cache rebuilt (from a gamelist listing
    the files in xml, when given), and its list drawn."""
    sd = OUT / name
    (sd / "Emu/FC").mkdir(parents=True)
    (sd / "Emu/FC/config.json").write_text(json.dumps(dict(
        label="NES", rompath="../../Roms/FC", imgpath="../../Roms/FC/Imgs", extlist="nes|zip|mp4")))
    for file in files:
        path = sd / "Roms/FC" / file
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"rom")
    if xml is not None:
        games = "".join(f"<game><path>./{escape(file)}</path><name>{escape(Path(file).stem)}</name>"
                        "</game>" for file in xml)
        (sd / "Roms/FC/miyoogamelist.xml").write_text(f"<gameList>{games}</gameList>")
    shot = sd / "shot.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(sd), "--theme", str(ONION_THEME),
                    "--refresh-caches", "--system", "NES", "--snapshot", str(shot)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    with sqlite3.connect(sd / "Roms/FC/FC_cache6.db") as db:
        rows = sorted(db.execute("select disp, path, type, ppath from FC_roms"))
    return rows, shot.read_bytes()


plain, plain_shot = build("plain", GAMES)
with_media, media_shot = build("media", GAMES + MEDIA)
assert with_media == plain and media_shot == plain_shot, with_media
assert not any("edia/" in row[1] for row in with_media), with_media
assert ("Media Games", 1) in {(row[0], row[2]) for row in plain}
assert ("Mediaplayer", 0) in {(row[0], row[2]) for row in plain}
# From a gamelist that lists the media folder's files too: left out the same.
xml_plain, xml_plain_shot = build("xml-plain", GAMES + MEDIA, GAMES)
xml_media, xml_media_shot = build("xml-media", GAMES + MEDIA, GAMES + MEDIA)
assert xml_media == xml_plain and xml_media_shot == xml_plain_shot, xml_media
assert ("Media Games", 1) in {(row[0], row[2]) for row in xml_plain}, xml_plain
assert len(xml_plain) == len(plain), (xml_plain, plain)

# Browsing the folder itself, as with a cache MainUI does not read (WAL
# mode), leaves it out too.
for name in ("plain", "media"):
    cache = OUT / name / "Roms/FC/FC_cache6.db"
    cache.unlink()
    with sqlite3.connect(cache) as db:
        db.execute("pragma journal_mode=wal")
        db.execute("create table t (x)")
shots = []
for name in ("plain", "media"):
    shot = OUT / name / "browse.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(OUT / name), "--theme",
                    str(ONION_THEME), "--system", "NES", "--input", "D", "--snapshot", str(shot)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    shots.append(shot.read_bytes())
assert shots[0] == shots[1]
print("media folders are never listed as ROM folders; similar names still are")
