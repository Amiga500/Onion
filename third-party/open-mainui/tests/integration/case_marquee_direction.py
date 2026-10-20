# SPDX-License-Identifier: GPL-3.0-only
"""A title that starts with Arabic or Hebrew is shown and scrolled from its right end."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix='marquee-direction-', dir=BUILD))
SD = OUT / 'sd'
for directory in ['Emu/FC', 'Roms/FC/Imgs', '.tmp_update/config']:
    (SD / directory).mkdir(parents=True)
(SD / 'Emu/FC/config.json').write_text(
    json.dumps(dict(label='NES', rompath='../../Roms/FC', imgpath='../../Roms/FC/Imgs',
                    extlist='nes')))
# Scrolling starts after 100 ms at 100 px/s.
(SD / '.tmp_update/config/.romListTitleScroll').write_text('100,100')


def row(title, elapsed, cover=False):
    """Bright pixels of the selected row, with only this title listed."""
    for rom in list((SD / 'Roms/FC').glob('*.nes')) + list((SD / 'Roms/FC/Imgs').iterdir()):
        rom.unlink()
    (SD / 'Roms/FC' / (title + '.nes')).write_bytes(b'')
    if cover:
        Image.new('RGB', (10, 10), (0, 0, 0)).save(SD / 'Roms/FC/Imgs' / (title + '.png'))
    output = OUT / 'row.bmp'
    subprocess.run([os.environ.get('MAINUI_TEST_EXE', str(BUILD / 'MainUI-dev')),
                    '--sd-root', str(SD), '--theme', str(ONION_THEME), '--fallback',
                    str(ONION_THEME), '--system', 'NES', '--refresh-caches', '--elapsed',
                    str(elapsed), '--snapshot', str(output)], cwd=ROOT, check=True, timeout=20)
    image = Image.open(output).convert('RGB')
    return {(x, y) for x in range(640) for y in range(60, 120) if min(image.getpixel((x, y))) > 200}


def shift(still, moved):
    """How far the text moved, right positive: the best overlap within 60 px."""
    return max(range(-60, 61), key=lambda d: len({(x + d, y) for x, y in still} & moved))


# Dense letters at the start, sparse dots at the end: the end shows as few pixels.
latin = 'M' * 40 + '.' * 80
arabic = 'ب' + latin
ltr_still, rtl_still = row(latin, 0), row(arabic, 0)
# A left-to-right title shows its start, a right-to-left one its right end.
assert len(rtl_still) * 3 < len(ltr_still), (len(rtl_still), len(ltr_still))
# After 300 ms at 100 px/s the Latin title has moved about 30 px left, the
# Arabic one as far right, bringing in more of itself from the left.
# (Words that do not repeat, so the measure cannot slip by a letter.)
words = 'Super Mario Bros Zelda Link Metroid Kirby Donkey Kong Pikmin Wario Yoshi Fox'
moved = shift(row(words, 0), row(words, 400))
assert -40 <= moved <= -20, moved
moved = shift(row('\u0628 ' + words, 0), row('\u0628 ' + words, 400))
assert 20 <= moved <= 40, moved
# A title that fits is drawn as before, from the left.
short = row('\u0628 Mario', 0)
assert abs(min(x for x, _ in short) - min(x for x, _ in ltr_still)) <= 12
# With a cover shown, a right-to-left title stops at the pane, so its right
# end (dense letters here) shows left of it rather than under the cover.
edge = 640 - Image.open(ONION_THEME / 'skin/preview-bg.png').width
end = '.' * 80 + 'M' * 6
for title, wanted in ((end, False), ('\u0628' + end, True)):
    near = [x for x, _ in row(title, 0, cover=True) if edge - 80 <= x < edge]
    assert (len(near) > 300) == wanted, (title[:1], len(near))
shutil.rmtree(OUT)
print('Right-to-left titles show and scroll from their right end; others are unchanged')
