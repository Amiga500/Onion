# SPDX-License-Identifier: GPL-3.0-only
"""A message too long for one line wraps instead of being cut at the panel's
edge. Here a console whose cache cannot be built (another build's
reservation is in the way) and whose gamelist must not be scanned around
shows "Catalog unavailable" with a two-line explanation: on the theme's
skin/pop-bg.png, as stock's notices, and on the plain panel without it."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="message-wrap-", dir=BUILD))
SD = OUT / "sd"
(SD / "Emu/GG").mkdir(parents=True)
(SD / "Roms/GG").mkdir(parents=True)
(SD / "Emu/GG/config.json").write_text(json.dumps(dict(
    label="Game Gear", rompath="../../Roms/GG", launch="launch.sh", extlist="gg")))
(SD / "Roms/GG/game.gg").write_bytes(b"rom")
(SD / "Roms/GG/miyoogamelist.xml").write_text("<gameList/>")
(SD / "Roms/GG/GG_cache6.db.building").write_text("another build")



def message(theme, name):
    target = OUT / name
    result = subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(theme),
                             "--fallback", str(theme), "--systems", "--input", "E", "--snapshot",
                             str(target)], cwd=ROOT, timeout=30, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return Image.open(target).convert("RGB")


# On the theme's popup art (640x330, so at y 75): title, body wrapped to two
# lines and hint, as separate rows of text between 30 px from the art's top
# and 45 px from its bottom, inside the art's panel.
with Image.open(ONION_THEME / "skin/pop-bg.png") as art:
    assert art.size == (640, 330), art.size
popup = message(ONION_THEME, "popup.bmp")
panel = sum(popup.getpixel((320, 135)))


def lit(x, y):
    return sum(popup.getpixel((x, y))) > panel + 120


rows = [y for y in range(75, 405) if any(lit(x, y) for x in range(50, 590))]
assert rows and rows[0] >= 105 and rows[-1] < 360, (rows[:1], rows[-1:])
groups = 1 + sum(1 for a, b in zip(rows, rows[1:]) if b - a > 2)
assert groups >= 4, f"title, two body lines and hint expected, found {groups} rows of text"
# The art's panel is opaque from x 35 to 604; text keeps 50 px off its sides.
for y in range(75, 405):
    for x in (*range(36, 50), *range(591, 605)):
        assert not lit(x, y), ("text at the art's edge", x, y)

# Without the art, the plain panel, as before.
plain_theme = OUT / "theme"
shutil.copytree(ONION_THEME, plain_theme)
(plain_theme / "skin/pop-bg.png").unlink()
image = message(plain_theme, "message.bmp")


def text_rows(top, bottom, left=35, right=605):
    """Rows in the band holding light text pixels on the dark panel."""
    return [y for y in range(top, bottom)
            if any(sum(image.getpixel((x, y))) > 450 for x in range(left, right))]


# The first body line starts at y 215; the second follows it, above the hint.
first = text_rows(215, 245)
assert first, "no first line"
second = text_rows(max(first) + 2, max(first) + 40)
assert second, "the explanation was not wrapped onto a second line"
# Nothing reaches the clipped right edge of the panel: no word is cut off.
assert not text_rows(215, max(second) + 1, left=595), "text runs into the panel's edge"
print("A long message wraps onto more lines instead of being cut off")
