# SPDX-License-Identifier: GPL-3.0-only
"""Cover art keeps its compositing: opaque, half-transparent and transparent
RGBA covers, small enough to be drawn as they are and large enough to be
scaled, against the list drawn without a cover. Opaque art is drawn as a
plain copy (no blending), which must give the same pixels."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix='cover-alpha-', dir=BUILD))
SD = OUT/'sd'
THEME = OUT/'theme'
FALLBACK = ONION_THEME
for directory in ['Emu/FC', 'Roms/FC/Imgs']:
    (SD/directory).mkdir(parents=True)
(THEME/'skin').mkdir(parents=True)
(THEME/'config.json').write_bytes((FALLBACK/'config.json').read_bytes())
(SD/'Emu/FC/config.json').write_text(json.dumps(dict(
    label='NES', rompath='../../Roms/FC', imgpath='../../Roms/FC/Imgs', extlist='nes')))
(SD/'Roms/FC/Game.nes').write_bytes(b'')
cover = SD/'Roms/FC/Imgs/Game.png'


def capture(name):
    path = OUT/(name+'.bmp')
    subprocess.run([os.environ.get('MAINUI_TEST_EXE', str(BUILD/'MainUI-dev')),
                    '--sd-root', str(SD), '--theme', str(THEME), '--fallback', str(FALLBACK),
                    '--system', 'NES', '--snapshot', str(path)], cwd=ROOT, check=True, timeout=20)
    return Image.open(path).convert('RGB')


def near(actual, expected, tolerance):
    return all(abs(a-b) <= tolerance for a, b in zip(actual, expected))


plain = capture('plain')
# Points inside both the 200x300 cover (drawn at x 409 to 608, y 90 to 389)
# and the 400x500 one scaled to 250x312.
points = [(450, 150), (500, 240), (590, 370)]
colour = (200, 40, 40)
for width, height in [(200, 300), (400, 500)]:
    # A transparent cover leaves the pane as without art, but for the theme's
    # preview background, which a cover brings: that is what the others cover.
    Image.new('RGBA', (width, height), colour+(0,)).save(cover)
    base = capture(f'cover-{width}x{height}-0')
    for point in points:
        assert near(base.getpixel(point), plain.getpixel(point), 3), (width, point)
    for alpha in [255, 128]:
        Image.new('RGBA', (width, height), colour+(alpha,)).save(cover)
        shot = capture(f'cover-{width}x{height}-{alpha}')
        for point in points:
            under = base.getpixel(point)
            if alpha == 255:
                assert shot.getpixel(point) == colour, (width, alpha, point, shot.getpixel(point))
            else:
                expected = tuple(u+(c-u)*alpha//255 for c, u in zip(colour, under))
                assert near(shot.getpixel(point), expected, 3), \
                    (width, alpha, point, shot.getpixel(point), expected)
print('Opaque, half-transparent and transparent covers, drawn as they are and scaled, passed:', OUT)
