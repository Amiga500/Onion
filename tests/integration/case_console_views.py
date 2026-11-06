# SPDX-License-Identifier: GPL-3.0-only
"""Games and Expert can show their consoles as a carousel or a list instead of
the grid, from .tmp_update/config/console-view.json. The carousel: the
selected console centred on a plain selection with its name beneath, its
neighbours either side, in the grid's order, the theme's icons as cells of
the grid or the thumbnails of a "thumbs" folder on a plain selection. The list: as a game
list, each console a folder with its name trimmed, its thumbnail as its
cover, its number of games as the counter. Left and Right move,
wrapping round; A opens the console and B goes back, as in the grid. Without
the file, with a file that is not a JSON object, or with any value but
"carousel", the grid is drawn exactly as before."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="console-carousel-", dir=BUILD))
SD = OUT / "sd"
CONFIG = OUT / "config"
CONFIG.mkdir()
ICONS = OUT / "icons"
ICONS.mkdir()
# Five consoles, each with its own colours: normal and selected icons.
CONSOLES = [("00A", "Alpha"), ("01B", "Bravo"), ("02C", "Charlie"), ("03D", "Delta"),
            ("04E", "Echo")]
NORMAL = [(200, 40, 40), (40, 200, 40), (40, 40, 200), (200, 200, 40), (40, 200, 200)]
SELECTED = [(250, 90, 90), (90, 250, 90), (90, 90, 250), (250, 250, 90), (90, 250, 250)]
for i, (folder, label) in enumerate(CONSOLES):
    (SD / "Emu" / folder).mkdir(parents=True)
    (SD / "Roms" / folder).mkdir(parents=True)
    Image.new("RGB", (80, 60), NORMAL[i]).save(ICONS / f"{folder}.png")
    Image.new("RGB", (80, 60), SELECTED[i]).save(ICONS / f"{folder}-sel.png")
    (SD / "Emu" / folder / "config.json").write_text(json.dumps(dict(
        label=label, rompath=f"../../Roms/{folder}", extlist="nes",
        icon=str(ICONS / f"{folder}.png"), iconsel=str(ICONS / f"{folder}-sel.png"))))
SETTING = CONFIG / "console-view.json"


def capture(actions="", start="--systems", setting=None):
    SETTING.unlink(missing_ok=True)
    if setting is not None:
        SETTING.write_text(setting)
    target = OUT / "shot.bmp"
    command = [os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
               str(SD), "--theme", str(ONION_THEME), "--config-dir", str(CONFIG), "--snapshot",
               str(target)] + ([start] if start else [])
    if actions:
        command += ["--input", actions]
    subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(target).convert("RGB")


def bytes_of(image):
    return image.tobytes()


# The grid, unchanged without the file and with anything but "carousel".
grid = capture()
for setting in ('{"games": "grid"}', '{"expert": "carousel"}', '{"games": "Carousel"}',
                '{"games": 1}', "not json", '["carousel"]'):
    assert bytes_of(capture(setting=setting)) == bytes_of(grid), setting
assert bytes_of(capture("R", setting='{"expert": "carousel"}')) == bytes_of(capture("R"))

# Most checks read pictures by their exact colours, so neighbours at full
# opacity and no wrapping; the defaults are checked on their own below.
BASE = '"opacity_unselected": 1, "wrap": false'
CAROUSEL = '{"games": "carousel", "carousel": {%s}}' % BASE
CENTRE = (320, 212)


def colour_at(image, x):
    return image.getpixel((x, CENTRE[1]))


def neighbours(image):
    """The colours of the icons left and right of the centre, as far as shown."""
    found = []
    for x in range(0, 640, 2):
        colour = colour_at(image, x)
        if (colour in NORMAL or colour in SELECTED) and (not found or found[-1][1] != colour):
            found.append((x, colour))
    return found


first = capture(setting=CAROUSEL)
assert bytes_of(first) != bytes_of(grid)
# The first console, selected, centred; the second on its right; nothing left.
assert colour_at(first, 320) == SELECTED[0], colour_at(first, 320)
row = neighbours(first)
assert [colour for _, colour in row] == [SELECTED[0], NORMAL[1], NORMAL[2]], row
# Right: the second centred, the first on its left, the third and fourth right.
second = capture("R", setting=CAROUSEL)
assert colour_at(second, 320) == SELECTED[1]
assert [colour for _, colour in neighbours(second)] == [NORMAL[0], SELECTED[1], NORMAL[2],
                                                         NORMAL[3]], neighbours(second)
# The neighbours' cells, tile and icon, at scale_unselected, 0.7 by default:
# side by side, the selected 155 px cell in the middle, each neighbour 108
# px, its 80x60 icon 56x42. With 1, spaced evenly at the grid's size.


def middles(image, colours):
    found = []
    for colour in colours:
        xs = [x for x in range(640) if image.getpixel((x, CENTRE[1])) == colour]
        found.append(((xs[0] + xs[-1]) / 2, xs[-1] - xs[0] + 1))
    return found


row = middles(second, [NORMAL[0], SELECTED[1], NORMAL[2], NORMAL[3]])
assert [width for _, width in row] == [56, 80, 56, 56], row
assert abs(row[1][0] - 320) <= 1 and abs(row[1][0] - row[0][0] - (155 + 108) / 2) <= 2, row
assert abs(row[2][0] - row[1][0] - (155 + 108) / 2) <= 2 and abs(row[3][0] - row[2][0] - 108) <= 2
icon_rows = [y for y in range(60, 420) if second.getpixel((int(row[0][0]), y)) == NORMAL[0]]
assert len(icon_rows) == 42, len(icon_rows)
FULL = '{"games": "carousel", "carousel": {"scale_unselected": 1, %s}}' % BASE
row = middles(capture("R", setting=FULL), [NORMAL[0], SELECTED[1], NORMAL[2], NORMAL[3]])
assert [width for _, width in row[:3]] == [80, 80, 80], row  # the fourth cut by the edge
assert all(abs(b[0] - a[0] - 155) <= 2 for a, b in zip(row[:3], row[1:3])), row
# The neighbours' tiles shrink with them: with a theme whose unselected tile
# (154x170) is solid, 107 px wide beside the selected one, 154 at full size.
TILE = (70, 70, 160)
tiled_theme = OUT / "tiled-theme"
(tiled_theme / "skin").mkdir(parents=True)
for item in ONION_THEME.iterdir():
    if item.name != "skin":
        (tiled_theme / item.name).symlink_to(item)
for item in (ONION_THEME / "skin").iterdir():
    if item.name != "bg-game-item-n.png":
        (tiled_theme / "skin" / item.name).symlink_to(item)
Image.new("RGB", (154, 170), TILE).save(tiled_theme / "skin/bg-game-item-n.png")


def tile_width(setting):
    command = [os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
               str(SD), "--theme", str(tiled_theme), "--config-dir", str(CONFIG), "--systems",
               "--input", "R", "--snapshot", str(OUT / "tiled.bmp")]
    SETTING.write_text(setting)
    subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    image = Image.open(OUT / "tiled.bmp").convert("RGB")
    return len([x for x in range(0, 245) if image.getpixel((x, 175)) == TILE])


assert tile_width(CAROUSEL) == 107 and tile_width(FULL) == 154, (tile_width(CAROUSEL),
                                                                 tile_width(FULL))
# Left from the first wraps to the last, as in the grid, by default; with
# "wrap": false Left on the first and Right on the last do nothing. Up and
# Down do nothing.
WRAPS = '{"games": "carousel", "carousel": {"opacity_unselected": 1}}'
assert colour_at(capture("L", setting=WRAPS), 320) == SELECTED[4]
assert colour_at(capture("L", setting=CAROUSEL), 320) == SELECTED[0]
assert colour_at(capture("RRRRRR", setting=CAROUSEL), 320) == SELECTED[4]
assert bytes_of(capture("UD", setting=CAROUSEL)) == bytes_of(first)
# The selected console's name beneath it: different for each console.
band = (230, 250, 410, 416)
assert second.crop(band).tobytes() != capture("RR", setting=CAROUSEL).crop(band).tobytes()
assert len(set(second.crop(band).getdata())) > 2
# A opens the console as in the grid, and B goes back from the carousel to
# the main menu, as from the grid.
assert bytes_of(capture("RA", setting=CAROUSEL)) == bytes_of(capture("RA"))
assert bytes_of(capture("B", setting=CAROUSEL)) == bytes_of(capture("B"))
# Going in from the main menu shows the carousel too.
assert bytes_of(capture("RE", start=None, setting=CAROUSEL)) == bytes_of(first)
# R1/L1 retain a grid page; R2/L2 move three positions, wrapping round, or
# stopping at the ends with "wrap": false.
assert colour_at(capture("2", setting=CAROUSEL), 320) == SELECTED[4]
assert colour_at(capture("4", setting=CAROUSEL), 320) == SELECTED[3]
assert colour_at(capture("1", setting=CAROUSEL), 320) == SELECTED[0]
assert colour_at(capture("3", setting=WRAPS), 320) == SELECTED[2]
assert colour_at(capture("3", setting=CAROUSEL), 320) == SELECTED[0]
assert colour_at(capture("44", setting=CAROUSEL), 320) == SELECTED[4]
# The number of games beneath the name, "(n)", only once the console has a
# ROM list cache: none drawn before Alpha's is built by opening it.
NAME = (230, 250, 410, 416)
before = capture(setting=CAROUSEL).crop(NAME)
for game in ("One", "Two", "Three"):
    (SD / "Roms/00A" / f"{game}.nes").write_bytes(b"rom")
(SD / "Roms/00A/Sub").mkdir()
(SD / "Roms/00A/Sub/Four.nes").write_bytes(b"rom")
subprocess.run([os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
                str(SD), "--theme", str(ONION_THEME), "--config-dir", str(CONFIG), "--system",
                "Alpha", "--snapshot", str(OUT / "alpha.bmp")], cwd=ROOT, check=True, timeout=30,
               capture_output=True)
assert any((SD / "Roms/00A").glob("*_cache6.db"))
after = capture(setting=CAROUSEL).crop(NAME)
assert after.tobytes() != before.tobytes()
# Two lines now, the name and the count beneath it.


def lines(image):
    rows = [y for y in range(image.height)
            if any(sum(image.getpixel((x, y))) > 300 for x in range(image.width))]
    return sum(1 for a, b in zip([-2] + rows, rows) if b != a + 1)
assert lines(before) == 1 and lines(after) == 2, (lines(before), lines(after))

# Thumbnails: a folder of <emu folder>.png in lowercase, given in the file.
# In the carousel they replace the icons, larger ones scaled down to fit
# 560x260 and smaller ones as they are, with no theme image behind: a plain
# selection.
THUMBS = SD / "Thumbs"
THUMBS.mkdir()
THUMB = [(240, 120, 20), (20, 120, 240), (120, 20, 240), (240, 20, 120)]
Image.new("RGB", (120, 90), THUMB[0]).save(THUMBS / "00a.png")
Image.new("RGB", (60, 400), THUMB[1]).save(THUMBS / "01b.png")  # tall
Image.new("RGB", (420, 70), THUMB[2]).save(THUMBS / "02c.png")  # wide
# 03D has none: its icon shows.
WITH = '{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs", "carousel": {%s}}' % BASE
thumbed = capture(setting=WITH)
assert colour_at(thumbed, 320) == THUMB[0], colour_at(thumbed, 320)
tall = capture("R", setting=WITH)
column = [y for y in range(60, 420) if tall.getpixel((320, y)) == THUMB[1]]
# The stock theme's 31 px name and count lines fit beneath it with the row
# 9 px above its 212 px default middle.
assert len(column) == 260 and abs((column[0] + column[-1]) / 2 - 203) <= 1, (len(column))
row = [x for x in range(640) if tall.getpixel((x, 212)) == THUMB[1]]
assert abs(len(row) - 39) <= 1, len(row)  # 60x400 scaled by 0.65
wide = capture("RR", setting=WITH)
row = [x for x in range(640) if wide.getpixel((x, 212)) == THUMB[2]]
assert len(row) == 420, len(row)  # 420x70 fits: never enlarged
assert colour_at(capture("RRR", setting=WITH), 320) == SELECTED[3]
# The selection: lighter than the background around it, no theme image.
background = thumbed.getpixel((320 - 174 - 20, 212))
pad = thumbed.getpixel((320 - 60 - 6, 212))  # 120x90 as it is
assert sum(pad) > sum(background) + 15, (pad, background)  # about 12%
assert colour_at(capture(setting=WITH.replace("/mnt/SDCARD/Thumbs", "Thumbs")), 320) == THUMB[0]
# "selection": a colour, "#RRGGBB" opaque; false or "" none.
GREEN = ('{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs", "carousel": {"selection": %s, '
         + BASE + '}}')
assert capture(setting=GREEN % '"#00ff00"').getpixel((320 - 60 - 6, 212)) == (0, 255, 0)
for none in ("false", '""'):
    assert capture(setting=GREEN % none).getpixel((320 - 60 - 6, 212)) == background, none
# A console without a thumbnail is a cell of the grid: the theme's tile
# behind its icon and name, and with "selection": false none.
CELL = (320 - 64, 228 - 76)  # in the tile, beside the icon
tiled = capture("RRR", setting=WITH).getpixel(CELL)
bare_cell = capture("RRR", setting=GREEN % "false").getpixel(CELL)
assert tiled != bare_cell and bare_cell == background, (tiled, bare_cell)
# Neighbours' thumbnails at scale_unselected, 0.7 by default: Bravo's 39x260
# is 27x182 beside Alpha, gap pixels (40) from Alpha's 120 wide thumbnail.


def spans(image, colour, y=212):
    xs = [x for x in range(640) if image.getpixel((x, y)) == colour]
    return xs[0], xs[-1]


def column_of(image, colour, x):
    return len([y for y in range(60, 420) if image.getpixel((x, y)) == colour])


left_edge, right_edge = spans(thumbed, THUMB[1])
assert abs(left_edge - (380 + 40)) <= 1 and abs(column_of(thumbed, THUMB[1], left_edge + 10) - 182) <= 1
# The keys given come first: the first of two equal keys counts.
CAROUSEL_WITH = ('{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs", "carousel": {%s, '
                 + BASE + '}}')
full = capture(setting=CAROUSEL_WITH % '"scale_unselected": 1, "gap": 0')
left_edge, _ = spans(full, THUMB[1])
assert abs(left_edge - 380) <= 1 and column_of(full, THUMB[1], left_edge + 10) == 260
# "wrap": Echo, the last console, left of Alpha, the first, as a cell.
assert thumbed.getpixel((143, 212)) != NORMAL[4]
assert capture(setting=CAROUSEL_WITH % '"wrap": true').getpixel((143, 212)) == NORMAL[4]
# "selection_padding" and "offset_y" move the plain selection and the row.
padded = capture(setting=CAROUSEL_WITH % '"selection_padding": 4')
assert padded.getpixel((320 - 60 - 6, 212)) == background
assert padded.getpixel((320 - 60 - 3, 212)) == pad
lowered = capture(setting=CAROUSEL_WITH % '"offset_y": 20')
column = [y for y in range(60, 420) if lowered.getpixel((320, y)) == THUMB[0]]
assert abs((column[0] + column[-1]) / 2 - 223) <= 1, column[0]
# "animation": "slide": part way (--elapsed 20 ms of 180), Alpha still the
# large one and left of the middle; at its end, as without the slide.


def slid(elapsed):
    SETTING.write_text(CAROUSEL_WITH % '"animation": "slide"')
    target = OUT / "slid.bmp"
    subprocess.run([os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
                    str(SD), "--theme", str(ONION_THEME), "--config-dir", str(CONFIG), "--systems",
                    "--input", "R", "--elapsed", str(elapsed), "--snapshot", str(target)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(target).convert("RGB")


moving = slid(20)
alpha_left, alpha_right = spans(moving, THUMB[0])
assert alpha_right - alpha_left + 1 == 120 and alpha_left < 260, (alpha_left, alpha_right)
assert bytes_of(slid(0)) == bytes_of(capture("R", setting=WITH))
# The name stays centred beneath the middle while the pictures slide, and
# changes to the new selection's halfway, with its size: before that as at
# the start, after it as at the end (no tiles, so only the name is drawn
# there).
NAMED = '{"games": "carousel", "carousel": {"selection": false, "animation": "slide", %s}}' % BASE


def named(elapsed):
    SETTING.write_text(NAMED)
    subprocess.run([os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
                    str(SD), "--theme", str(ONION_THEME), "--config-dir", str(CONFIG), "--systems",
                    "--input", "R", "--elapsed", str(elapsed), "--snapshot", str(OUT / "named.bmp")],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(OUT / "named.bmp").convert("RGB")


NAME_BAND = (200, 255, 440, 300)
part_way, past_half, ended = named(20), named(90), named(0)
assert bytes_of(part_way) != bytes_of(ended) and bytes_of(past_half) != bytes_of(ended)
started = capture(setting=NAMED).crop(NAME_BAND).tobytes()
assert started != ended.crop(NAME_BAND).tobytes()
assert part_way.crop(NAME_BAND).tobytes() == started
assert past_half.crop(NAME_BAND).tobytes() == ended.crop(NAME_BAND).tobytes()
# "labels": false and "count": false: no lines beneath, the picture up to
# 300 tall and centred in the band.
BARE = ('{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs", "count": false, '
        '"carousel": {"labels": false, ' + BASE + '}}')
bare = capture("R", setting=BARE)
column = [y for y in range(60, 420) if bare.getpixel((320, y)) == THUMB[1]]
assert len(column) == 300 and abs((column[0] + column[-1]) / 2 - 240) <= 1, len(column)
BELOW = (230, 260, 410, 416)  # below the 80x60 icon, in the cell centred at 240
assert lines(capture(setting=BARE.replace("/mnt/SDCARD/Thumbs", "none")).crop(BELOW)) == 0
# Only the count: one line, Alpha's games, beneath the cell.
alone = capture(setting='{"games": "carousel", "carousel": {"labels": false, %s}}' % BASE)
assert lines(alone.crop((230, 260, 410, 416))) == 1

# "list": the consoles as a game list, with the folder icon, the theme's
# rows and row count, and the console's thumbnail as its cover.
LIST = '{"games": "list", "thumbs": "/mnt/SDCARD/Thumbs"}'
listed = capture(setting=LIST)
assert bytes_of(listed) != bytes_of(grid)
EDGE = 640 - Image.open(ONION_THEME / "skin/preview-bg.png").width
assert listed.getpixel((EDGE + 125, 240)) == THUMB[0]
assert capture("D", setting=LIST).getpixel((EDGE + 125, 240)) != THUMB[0]
assert capture("DDD", setting=LIST).getpixel((EDGE + 125, 240)) not in THUMB  # no thumbnail
# Up and Down move and wrap; A opens the console and B goes back, as in the grid.
assert bytes_of(capture("DDDDD", setting=LIST)) == bytes_of(listed)
assert bytes_of(capture("U", setting=LIST)) == bytes_of(capture("DDDD", setting=LIST))
assert bytes_of(capture("DA", setting=LIST)) == bytes_of(capture("RA"))
assert bytes_of(capture("B", setting=LIST)) == bytes_of(capture("B"))
# The counter: the selected console's games, "(3)" for Alpha's cache, as
# a folder's, and nothing for Charlie and Delta, which have none (Bravo,
# opened above, has one).
COUNTER = (500, 430, 640, 470)
counters = [capture("D" * n, setting=LIST).crop(COUNTER).tobytes() for n in (0, 1, 2, 3)]
assert counters[0] != counters[2] and counters[1] != counters[2] and counters[2] == counters[3]
# "count": false: none for Alpha either.
hidden = capture(setting=LIST[:-1] + ', "count": false}').crop(COUNTER).tobytes()
assert hidden == counters[2]
# "list": {"folder_icon": false}: no folder icon, the name at the margin.
ICON = (0, 60, 70, 120)
plain_row = capture(setting=LIST[:-1] + ', "list": {"folder_icon": false}}')
assert plain_row.crop(ICON).tobytes() != listed.crop(ICON).tobytes()
assert len(set(plain_row.crop((0, 60, 15, 120)).getdata())) <= 2
# Names without leading and trailing spaces.
ROW0 = (0, 60, 640, 120)
plain = capture(setting=LIST).crop(ROW0).tobytes()
config = json.loads((SD / "Emu/00A/config.json").read_text())
(SD / "Emu/00A/config.json").write_text(json.dumps(dict(config, label="   Alpha  ")))
assert capture(setting=LIST).crop(ROW0).tobytes() == plain
(SD / "Emu/00A/config.json").write_text(json.dumps(config))
# Without thumbnails no cover is drawn; the grid of Expert is not affected.
assert capture(setting='{"games": "list"}').getpixel((EDGE + 125, 240)) not in THUMB
assert bytes_of(capture(setting='{"expert": "list"}')) == bytes_of(grid)
# A long name scrolls when titles scroll (.romListTitleScroll), as in lists.
(SD / "Emu/04E/config.json").write_text(json.dumps(dict(
    label="Echo " + "Long Console Name " * 4, rompath="../../Roms/04E", extlist="nes",
    icon=str(ICONS / "04E.png"))))
(CONFIG / ".romListTitleScroll").write_text("10,200")
ROW = (0, 60 + 4 * 60, 640, 60 + 5 * 60)
still = capture("DDDD", setting=LIST).crop(ROW).tobytes()
subprocess.run([os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
                str(SD), "--theme", str(ONION_THEME), "--config-dir", str(CONFIG), "--systems",
                "--elapsed", "2000", "--input", "DDDD", "--snapshot", str(OUT / "moved.bmp")],
               cwd=ROOT, check=True, timeout=30, capture_output=True)
assert Image.open(OUT / "moved.bmp").convert("RGB").crop(ROW).tobytes() != still
# Live, scrolling there: every partial frame equals a full frame.
(SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
SETTING.write_text(LIST)
env = dict(os.environ, MAINUI_VERIFY_PARTIAL="1")
process = subprocess.Popen([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme",
                            str(ONION_THEME), "--config-dir", str(CONFIG), "--systems"], cwd=ROOT,
                           env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
time.sleep(2)
alive = process.poll() is None
process.kill()
_, errors = process.communicate(timeout=10)
assert alive and "differs" not in errors, errors
# "grid": with ten consoles, Right on the first page's top right goes to the
# next page's top left with "navigation": "stock", and on along the row
# without; "wrap": false stops Left on the first console.
for i, (folder, label) in enumerate([("05F", "Foxtrot"), ("06G", "Golf"), ("07H", "Hotel"),
                                      ("08I", "India"), ("09J", "Juliett")]):
    (SD / "Emu" / folder).mkdir(parents=True)
    (SD / "Roms" / folder).mkdir(parents=True)
    (SD / "Emu" / folder / "config.json").write_text(json.dumps(dict(
        label=label, rompath=f"../../Roms/{folder}", extlist="nes",
        icon=str(ICONS / "00A.png"))))
STOCK = '{"grid": {"navigation": "stock"}}'
assert bytes_of(capture("RRRR", setting=STOCK)) == bytes_of(capture("RRRRRRRR"))
assert bytes_of(capture("RRRR")) != bytes_of(capture("RRRRRRRR"))
assert bytes_of(capture("RRRRL", setting=STOCK)) == bytes_of(capture("RRR"))
assert bytes_of(capture("L", setting='{"grid": {"wrap": false}}')) == bytes_of(capture())
assert bytes_of(capture("L")) != bytes_of(capture())
print("the number of games shows beneath the name once the console has a cache; consoles "
      "list as a game list, with thumbnails there and in the carousel")
print("consoles show as a carousel with console-view.json, and as the grid otherwise")

# If the specific thumbnail is missing, both carousel and list show the
# shared default. Without it, carousel shows the per-console theme icon.
DEFAULT = (17, 209, 73)
Image.new("RGB", (110, 80), DEFAULT).save(THUMBS / "_default.png")
assert colour_at(capture("RRR", setting=WITH), 320) == DEFAULT
assert capture("DDD", setting=LIST).getpixel((EDGE + 125, 240)) == DEFAULT
# "list": {"thumbs": false}: no cover for any console in the list, neither
# its thumbnail, _default.png nor its theme icon; the carousel keeps them.
NO_COVERS = '{"games": "list", "thumbs": "/mnt/SDCARD/Thumbs", "list": {"thumbs": false}}'
for actions in ("", "DDD"):
    covered = capture(actions, setting=LIST).getpixel((EDGE + 125, 240))
    bare_cover = capture(actions, setting=NO_COVERS).getpixel((EDGE + 125, 240))
    assert covered in (THUMB[0], DEFAULT) and bare_cover not in THUMB + [DEFAULT], actions
assert (bytes_of(capture(setting=NO_COVERS))
        == bytes_of(capture(setting=NO_COVERS.replace("/mnt/SDCARD/Thumbs", "none"))))
NO_LIST_COVERS = WITH[:-1] + ', "list": {"thumbs": false}}'
assert colour_at(capture(setting=NO_LIST_COVERS), 320) == THUMB[0]
(THUMBS / "_default.png").unlink()
assert colour_at(capture("RRR", setting=WITH), 320) == SELECTED[3]
assert capture("DDD", setting=LIST).getpixel((EDGE + 125, 240)) == NORMAL[3]
assert capture("DDD", setting=NO_COVERS).getpixel((EDGE + 125, 240)) != NORMAL[3]

# The defaults: neighbours at 50% opacity, and the row round the ends. Bravo's
# thumbnail right of Alpha is blended with the background; the last console,
# Juliett, shows left of Alpha.
DEFAULTS = '{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs"}'
default_row = capture(setting=DEFAULTS)
faded = default_row.getpixel((430, 212))  # Bravo's 27 px wide copy from x = 420
want = tuple(round(0.5 * c + 0.5 * b) for c, b in zip(THUMB[1], background))
assert all(abs(a - b) <= 3 for a, b in zip(faded, want)), (faded, want)
assert default_row.crop((0, 100, 200, 380)).tobytes() != thumbed.crop((0, 100, 200, 380)).tobytes()
# "opacity_unselected": 0 draws no neighbour pictures.
hidden = capture(setting=DEFAULTS[:-1] + ', "carousel": {"opacity_unselected": 0}}')
assert hidden.getpixel((430, 212)) == background


def run(sd, setting, actions="", elapsed=None, theme=ONION_THEME):
    (sd / ".tmp_update/config").mkdir(parents=True, exist_ok=True)
    (sd / ".tmp_update/config/console-view.json").write_text(setting)
    target = OUT / "run.bmp"
    command = [os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev")), "--sd-root",
               str(sd), "--theme", str(theme), "--config-dir", str(sd / ".tmp_update/config"),
               "--systems", "--snapshot", str(target)]
    command += ["--input", actions] if actions else []
    command += ["--elapsed", str(elapsed)] if elapsed is not None else []
    subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    (sd / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(target).convert("RGB")


# One, two and three consoles round the ends: each picture at most once.
SHORT = [(230, 60, 60), (60, 200, 60), (60, 60, 230)]
ONCE = '{"games": "carousel", "thumbs": "/mnt/SDCARD/Thumbs", "carousel": {%s}}'
for count in (1, 2, 3):
    short = OUT / f"short{count}"
    for i in range(count):
        (short / "Emu" / f"S{i}").mkdir(parents=True)
        (short / "Roms" / f"S{i}").mkdir(parents=True)
        (short / "Emu" / f"S{i}" / "config.json").write_text(json.dumps(dict(
            label=f"Short {i}", rompath=f"../../Roms/S{i}", extlist="nes")))
        (short / "Thumbs").mkdir(exist_ok=True)
        Image.new("RGB", (100, 140), SHORT[i]).save(short / "Thumbs" / f"s{i}.png")
    for actions in ("", "R", "L"):
        image = run(short, ONCE % '"opacity_unselected": 1', actions)
        for colour in SHORT[:count]:
            xs = [x for x in range(640) if image.getpixel((x, 212)) == colour]
            assert xs and xs[-1] - xs[0] + 1 == len(xs), (count, actions, colour)
# Two consoles sliding: Right brings the other in from the right while the
# first leaves to the left, Left the other way round, never across.
two = OUT / "short2"
SLIDE = ONCE % '"opacity_unselected": 1, "animation": "slide"'
for actions, incoming in (("R", 1), ("L", -1)):
    image = run(two, SLIDE, actions, elapsed=40)
    new = [x for x in range(640) if image.getpixel((x, 212)) == SHORT[1]]
    old = [x for x in range(640) if image.getpixel((x, 212)) == SHORT[0]]
    new_middle, old_middle = sum(new) / len(new), sum(old) / len(old)
    assert (new_middle - 320) * incoming > 0 and (old_middle - 320) * incoming < 0, actions
assert run(two, ONCE % '"wrap": false').getpixel((100, 212)) != SHORT[1]
# A theme with a large grid font: the name and count stay above the footer,
# the picture made smaller to fit.
big = OUT / "big-theme"
big.mkdir()
for item in ONION_THEME.iterdir():
    if item.name != "config.json":
        (big / item.name).symlink_to(item)
theme_config = json.loads((ONION_THEME / "config.json").read_text())
theme_config["grid"]["grid1x4"] = 44
(big / "config.json").write_text(json.dumps(theme_config))
tall_text = run(SD, CAROUSEL_WITH % '"scale_unselected": 1', "R", theme=big)
column = [y for y in range(60, 420) if tall_text.getpixel((320, y)) == THUMB[1]]
assert len(column) < 260, len(column)
bare_text = run(SD, BARE, "R", theme=big)
assert tall_text.crop((200, 417, 440, 425)).tobytes() == bare_text.crop((200, 417, 440, 425)).tobytes()
print("neighbours fade and the row goes round its ends by default, each console once, "
      "sliding the way it moves; the lines fit the theme's font")
