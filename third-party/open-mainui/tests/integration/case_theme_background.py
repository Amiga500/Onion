# SPDX-License-Identifier: GPL-3.0-only
"""Theme backgrounds are stored upside down; like Onion, rotate them on load."""
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="theme-background-", dir=BUILD))
EXE = str(BUILD / "MainUI-dev")

# Background: four solid quadrants. A 180-degree rotation moves each quadrant
# to the opposite corner; a mirror or flip would not match all four.
sd = OUT / "sd"
(sd / "Emu").mkdir(parents=True)
theme = OUT / "theme"
(theme / "skin").mkdir(parents=True)
(theme / "config.json").write_text((ONION_THEME / "config.json").read_text())
quadrants = {"tl": (255, 0, 0), "tr": (0, 255, 0), "bl": (0, 0, 255), "br": (255, 255, 0)}
background = Image.new("RGB", (640, 480))
background.paste(quadrants["tl"], (0, 0, 320, 240))
background.paste(quadrants["tr"], (320, 0, 640, 240))
background.paste(quadrants["bl"], (0, 240, 320, 480))
background.paste(quadrants["br"], (320, 240, 640, 480))
background.save(theme / "skin/background.png")
output = OUT / "background.bmp"
result = subprocess.run(
    [EXE, "--sd-root", str(sd), "--theme", str(theme), "--fallback", str(ONION_THEME),
     "--systems", "--snapshot", str(output)],
    cwd=ROOT, capture_output=True, text=True, timeout=30)
assert result.returncode == 0, result.stderr
frame = Image.open(output).convert("RGB")
# Points clear of the header, footer and the centered empty-list label.
expected = {(20, 120): "br", (620, 120): "bl", (20, 360): "tr", (620, 360): "tl"}
for point, source in expected.items():
    assert frame.getpixel(point) == quadrants[source], (point, frame.getpixel(point), source)
print("theme_background: ok")
