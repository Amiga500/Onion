# SPDX-License-Identifier: GPL-3.0-only
"""Saved-library behavior, real SDL navigation, and input byte preservation."""
import os
import json
from pathlib import Path
import subprocess
from PIL import Image
from env import unlink_if_exists, BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = BUILD / "library-checks"
OUT.mkdir(exist_ok=True)
THEME = ONION_THEME

def lines(path, records):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(''.join(json.dumps(row)+'\n' for row in records), encoding='utf-8')

def game(label, rom, **extra):
    return dict(label=label, rompath=rom, launch='/mnt/SDCARD/Emu/FC/launch.sh', type=5, **extra)

sd = OUT/'sd'
roms = sd/'Roms'
(sd/'Emu').mkdir(parents=True,exist_ok=True)
recent = [game('First saved name', '/mnt/SDCARD/Emu/FC/launch.sh:/mnt/SDCARD/Roms/FC/one.nes')]
recent[0]['launch'] = '/mnt/SDCARD/App/Search/launch.sh'
recent += [game('Later duplicate', '/mnt/SDCARD/Roms/FC/one.nes')]
recent += [dict(label='App',rompath='app',launch='app',type=3), dict(label='State',rompath='state',launch='setstate',type=5)]
recent += [game(f'Game {i}', f'/mnt/SDCARD/Roms/FC/{i}.nes') for i in range(1,70)]
lines(roms/'recentlist.json', recent)
records = [game('Root game','root'), game('Inside game','inside'), game('Deep game','deep'), game('Root game','duplicate-label')]
lines(roms/'favourite.json', records)
folders = [dict(schema=1,generation=1), dict(kind='folder',id='a',parent='',name='Collection',order=0),
           dict(kind='folder',id='b',parent='a',name='Nested',order=0),
           dict(kind='item',key='inside',type=5,folder='a',order=0),
           dict(kind='item',key='deep',type=5,folder='b',order=0)]
lines(roms/'favourite-folders.json', folders)
config = sd/'.tmp_update/config'
config.mkdir(parents=True,exist_ok=True)
(config/'main-menu.json').write_text('{"menu":{"recents":true,"favorites":true,"games":true}}')

def check(root, scenario):
    subprocess.run([str(BUILD / "fixture-library"),str(root),scenario],cwd=ROOT,check=True,timeout=20)

before = {p:p.read_bytes() for p in sd.rglob('*') if p.is_file()}
check(sd,'recent')
check(sd,'folders')

def capture(name, actions):
    target = OUT/(name+'.bmp')
    subprocess.run([os.environ.get('MAINUI_TEST_EXE',str(BUILD / "MainUI-dev")),'--sd-root',str(sd),'--theme',str(THEME),
                    '--snapshot',str(target),'--input',actions],cwd=ROOT,check=True,timeout=20)
    return target.read_bytes()

recent_view = capture('recents','E')
assert recent_view != capture('recents-last','EU')
assert recent_view == capture('recents-no-launch','EE')
favorites = capture('favorites','RE')
assert favorites != capture('favorite-folder','REE')
assert favorites == capture('favorite-return','REEB')
assert favorites == capture('nested-return','REEDEBB')
assert capture('home','') == capture('recent-back','EB')
assert before == {p:p.read_bytes() for p in sd.rglob('*') if p.is_file()}

cap = OUT/'cap'
lines(cap/'Roms/recentlist.json', [dict(label='App',rompath='app',launch='app',type=3)]*200 + [game('Too late','late')])
check(cap,'cap')
flat = OUT/'flat'
lines(flat/'Roms/favourite.json',records)
unlink_if_exists((flat/'Roms/favourite-folders.json.bak'))
(flat/'Roms/favourite-folders.json').write_text('{broken')
check(flat,'flat')
lines(flat/'Roms/favourite-folders.json.bak',folders)
check(flat,'folders')
missing = OUT/'missing'
missing.mkdir(exist_ok=True)
check(missing,'missing')

# The 1px transparent icon must leave game text near the normal 20px
# edge. Compare a populated row with an otherwise identical empty-list frame.
empty = OUT/'empty.json'
empty.write_text('')
label = OUT/'label.json'
lines(label,[game('Margin check','game')])
for name,path in [('empty-row',empty),('text-row',label)]:
    subprocess.run([os.environ.get('MAINUI_TEST_EXE',str(BUILD / "MainUI-dev")),'--list',str(path),'--theme',str(THEME),
                    '--snapshot',str(OUT/(name+'.bmp'))],cwd=ROOT,check=True,timeout=20)
# White text pixels within the first row provide an asset-independent bound.
im = Image.open(OUT/'text-row.bmp').convert('RGB')
xs = [x for y in range(70,110) for x in range(640) if min(im.getpixel((x,y))) > 220]
assert xs and 20 <= min(xs) < 30, min(xs)
print('Library checks passed: Recent identities/limits, Favorite folders/fallback, SDL navigation, margin, unchanged saved files')


# Recent uses the same artwork rule on the list, RIGHT-open and detail refresh.
# Every source is a generated empty ROM / solid-color PNG; no real library writes.
art_sd = OUT / "recent-art"
for folder in ("Emu/FC", "Roms/FC/Imgs", "Roms/FC/Sub", ".tmp_update/config"):
    (art_sd / folder).mkdir(parents=True, exist_ok=True)
(art_sd / "Emu/FC/config.json").write_text(json.dumps(dict(
    label="Source", rompath="../../Roms/FC", imgpath="../../Roms/FC/Imgs",
    launch="launch.sh", extlist="nes")), encoding="utf-8")
(art_sd / ".tmp_update/config/main-menu.json").write_text('{"menu":{"recents":true}}')
red, blue = (251, 7, 101), (3, 221, 247)
for filename, color in (("a.png", red), ("a.PNG", red), ("b.png", blue)):
    Image.new("RGB", (200, 200), color).save(art_sd / "Roms/FC/Imgs" / filename)
for filename in ("a.nes", "b.nes", "Sub/a.nes", "missing.nes"):
    (art_sd / "Roms/FC" / filename).write_bytes(b"")
(art_sd / "Roms/FC/Imgs/corrupt.png").write_bytes(b"not an image")
portable = "/mnt/SDCARD/Roms/FC/"
source = "/mnt/SDCARD/Emu/FC/launch.sh"
image_host = str((art_sd / "Roms/FC/Imgs/a.png"))
cases = [
    ("canonical", portable + "a.nes", "", red),
    ("bare-name", portable + "a.nes", "Saved display name", red),
    ("relative-image", portable + "a.nes", "./Imgs/a.png", red),
    ("absolute-image", portable + "a.nes", portable + "Imgs/a.png", red),
    ("host-image", portable + "a.nes", image_host, red),
    ("upper-image", portable + "a.nes", "./Imgs/a.PNG", red),
    ("nested-rom", portable + "Sub/a.nes", "", red),
    ("relative-rom", "Roms/FC/a.nes", "", red),
    ("host-rom", str((art_sd / "Roms/FC/a.nes")), "", red),
    ("missing-art", portable + "missing.nes", "", None),
    ("missing-explicit", portable + "a.nes", "./Imgs/absent.png", None),
    ("corrupt-explicit", portable + "a.nes", "./Imgs/corrupt.png", None),
]


def artwork_capture(name, actions):
    target = OUT / (name + ".bmp")
    subprocess.run([os.environ.get('MAINUI_TEST_EXE', str(BUILD / "MainUI-dev")),
                    '--sd-root', str(art_sd), '--theme', str(THEME), '--input', actions,
                    '--snapshot', str(target)], cwd=ROOT, check=True, timeout=30)
    with Image.open(target) as image:
        return image.convert("RGB")


def check_art(image, expected, detail=False):
    box = (0, 60, 274, 420) if detail else (390, 60, 640, 420)
    colors = list(image.crop(box).getdata())
    if expected:
        assert colors.count(expected) > 1000, (expected, detail)
    else:
        assert red not in colors and blue not in colors, detail


for name, rom, art, expected in cases:
    for search_origin in (False, True):
        label = name + ("-search" if search_origin else "-direct")
        first = game("Artwork", source + ":" + rom if search_origin else rom, imgpath=art)
        first["launch"] = "/mnt/SDCARD/App/Search/launch.sh" if search_origin else source
        second = game("Second", portable + "b.nes", imgpath="")
        lines(art_sd / "Roms/recentlist.json", [first, second])
        before_recent = (art_sd / "Roms/recentlist.json").read_bytes()
        check_art(artwork_capture(label + "-list", "E"), expected)
        initial = artwork_capture(label + "-open", "ER")
        check_art(initial, expected, True)
        check_art(artwork_capture(label + "-next", "ERD"), blue, True)
        restored = artwork_capture(label + "-return", "ERDU")
        assert initial.tobytes() == restored.tobytes(), label
        assert (art_sd / "Roms/recentlist.json").read_bytes() == before_recent
print("Recent artwork matrix passed: canonical/Search records, explicit/fallback paths, "
      "missing/corrupt art, RIGHT-open and Up/Down refresh")

# Startup must recover Onion's hidden Recent file without overwriting an existing
# normal list. Use the native reader to verify the recovered 50-row identities.
hidden_sd = OUT / "hidden-recent"
hidden_roms = hidden_sd / "Roms"
hidden_roms.mkdir(parents=True, exist_ok=True)
(hidden_sd / "Emu").mkdir(exist_ok=True)
# Recovery is independent of menu visibility and must not create a legacy flag.
hidden_config = hidden_sd / ".tmp_update/config"
hidden_config.mkdir(parents=True, exist_ok=True)
(hidden_config / "main-menu.json").write_text('{"menu":{"recents":false,"games":true}}')
unlink_if_exists(hidden_config / ".showRecents")
normal = hidden_roms / "recentlist.json"
hidden = hidden_roms / "recentlist-hidden.json"
unlink_if_exists(normal)
hidden.write_bytes((roms / "recentlist.json").read_bytes())
original = hidden.read_bytes()
subprocess.run([str(BUILD / "MainUI-dev"),
                "--sd-root", str(hidden_sd), "--theme", str(THEME),
                "--snapshot", str(OUT / "hidden-recovery.bmp")], check=True, timeout=20)
assert normal.read_bytes() == original and not hidden.exists()
check(hidden_sd, "recent")
hidden.write_bytes(b"preserve hidden file")
subprocess.run([str(BUILD / "MainUI-dev"),
                "--sd-root", str(hidden_sd), "--theme", str(THEME),
                "--snapshot", str(OUT / "hidden-preserve.bmp")], check=True, timeout=20)
assert normal.read_bytes() == original and hidden.read_bytes() == b"preserve hidden file"

# A normal-only list is untouched, and absent lists stay absent.
unlink_if_exists(hidden)
subprocess.run([str(BUILD / "MainUI-dev"),
                "--sd-root", str(hidden_sd), "--theme", str(THEME),
                "--snapshot", str(OUT / "recent-normal-only.bmp")], check=True, timeout=20)
assert normal.read_bytes() == original and not hidden.exists()
unlink_if_exists(normal)
subprocess.run([str(BUILD / "MainUI-dev"),
                "--sd-root", str(hidden_sd), "--theme", str(THEME),
                "--snapshot", str(OUT / "recent-neither.bmp")], check=True, timeout=20)
assert not normal.exists() and not hidden.exists()
assert not (hidden_config / ".showRecents").exists()
assert json.loads((hidden_config / "main-menu.json").read_text())["menu"]["recents"] is False
