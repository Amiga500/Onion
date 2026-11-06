# SPDX-License-Identifier: GPL-3.0-only
"""In Apps, X or Y with "fav" as its shortcut (keymap.json) adds the selected
app to Favorites or removes it, as the context menu does, and an app that is
a Favorite shows the theme's Favorite mark at its row's right, where no
row's text reaches. Game List Options, no shortcut, or keymon's do nothing
there, as before: X stays B."""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme, unlink_if_exists  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="app-favorites-", dir=BUILD))
SD = OUT / "sd"
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / "Roms").mkdir()
KEYMAP = SD / ".tmp_update/config/keymap.json"
FAVORITES = SD / "Roms/favourite.json"
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": dict(apps=True)}))
# Apps lists the folders in the order the SD card gives them.
for folder, label in (("Alpha", "Alpha Tool"), ("Bravo", "Bravo Tool"),
                      ("Long", "Charlie Long " + "Wide Label " * 6)):
    (SD / "App" / folder).mkdir(parents=True)
    (SD / "App" / folder / "config.json").write_text(json.dumps(dict(label=label,
                                                                     launch="launch.sh")))
MARK = Image.open(ONION_THEME / "skin/ic-favorite-mark.png")
STAR = (640 - 20 - MARK.width, 62, 620, 152)  # the first row's mark


def run(actions):
    shot = OUT / "shot.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--input", actions, "--snapshot", str(shot)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    unlink_if_exists(SD / "appconfigs/romwinidx.json")
    return Image.open(shot).convert("RGB")


def favorites():
    if not FAVORITES.exists():
        return []
    return [json.loads(line)["label"] for line in FAVORITES.read_text().splitlines()
            if line.strip()]


def shortcuts(x=None, y=None):
    keymap = {}
    if x is not None:
        keymap["mainui_button_x"] = x
    if y is not None:
        keymap["mainui_button_y"] = y
    KEYMAP.write_text(json.dumps(keymap))


apps = run("E")
plain = apps.crop(STAR).tobytes()
# No row's text, the long label's either, reaches the mark's place: it is
# the same on every unselected row.


def row(image, n):
    return image.crop((STAR[0], 62 + 90 * n, STAR[2], 152 + 90 * n)).tobytes()


assert row(apps, 1) == row(apps, 2)
down = run("ED")
assert row(down, 0) == row(down, 2)

# Game List Options, no shortcut and keymon's: nothing on Y, X is B.
for value in ("glo", None, "app:Search"):
    shortcuts(x=value, y=value)
    assert run("EY").tobytes() == apps.tobytes() and favorites() == [], value
shortcuts()
assert run("EX").tobytes() == run("EB").tobytes()

# "fav" on Y adds the first app, and the mark shows on its row, also when
# Apps opens again; again on Y removes it.
shortcuts(y="fav")
marked = run("EY")
assert len(favorites()) == 1, favorites()
first = favorites()[0]
assert marked.crop(STAR).tobytes() != plain
assert run("E").crop(STAR).tobytes() == marked.crop(STAR).tobytes()
assert run("EY").crop(STAR).tobytes() == plain and favorites() == []
# On X too, here on the second app.
shortcuts(x="fav")
run("EDX")
assert len(favorites()) == 1 and favorites()[0] != first, favorites()
assert run("E").crop(STAR).tobytes() == plain
print("X and Y with fav add and remove apps in Apps, and Favorites are marked there")
