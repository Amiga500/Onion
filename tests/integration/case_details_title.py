# SPDX-License-Identifier: GPL-3.0-only
"""Game Info draws a kept title layout exactly as a freshly wrapped one."""
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
OUT = Path(tempfile.mkdtemp(prefix='details-title-', dir=BUILD))
SD = OUT / 'sd'
for directory in ['Emu/FC', 'Roms/FC']:
    (SD / directory).mkdir(parents=True)
(SD / 'Emu/FC/config.json').write_text(
    json.dumps(dict(label='NES', rompath='../../Roms/FC', extlist='nes')))
# Seven games, two more than the five title layouts kept.
titles = ['Super Mario Bros. 3 (USA) (Rev 1) [!] The Very Long Edition With Extra Words',
          'العربية ' * 12, 'A' * 120, 'Short',
          'Long_filename_without_any_spaces_at_all_for_wrapping_purposes_x',
          "The Legend of Zelda - Link's Awakening DX (USA, Europe) (Rev 2) (SGB Enhanced)",
          'Castlevania - Symphony of the Night (USA) (Disc 1) (Director\'s Cut Edition)']
for i, title in enumerate(titles):
    (SD / 'Roms/FC' / ('%02d %s.nes' % (i, title))).write_bytes(b'')


def details(inputs):
    output = OUT / 'details.bmp'
    subprocess.run([os.environ.get('MAINUI_TEST_EXE', str(BUILD / 'MainUI-dev')),
                    '--sd-root', str(SD), '--theme', str(ONION_THEME), '--fallback',
                    str(ONION_THEME), '--system', 'NES', '--refresh-caches', '--input', inputs,
                    '--snapshot', str(output)], cwd=ROOT, check=True, timeout=20)
    return Image.open(output).convert('RGB')


first = details('R')
# The long title takes several lines below the system name (y 130 on).
lines = {y // 20 for y in range(130, 300) for x in range(274, 634)
         if min(first.getpixel((x, y))) > 200}
assert len(lines) >= 3, sorted(lines)
# Back to it from the next game: its layout is reused.
assert details('RDU').tobytes() == first.tobytes()
# Back after all seven: its layout was replaced and is worked out again.
assert details('R' + 'D' * 7 + 'U' * 7).tobytes() == first.tobytes()
for i in range(1, len(titles)):
    assert details('R' + 'D' * i + 'U' * i + 'D' * i).tobytes() == details('R' + 'D' * i).tobytes()
# A queued page press may arrive before the first details redraw. Its layout
# must be prepared from the current font/title before L2/R2 is consumed.
first_rom = '00 ' + titles[0] + '.nes'
(SD / 'Roms/FC/gamelist.xml').write_text(
    '<gameList><game><path>./' + first_rom + '</path>'
    '<desc>' + ('This is a description with enough words to need another page. ' * 25) +
    '</desc></game></gameList>')
paged = details('R4').tobytes()
assert paged != details('R').tobytes(), (
    'R2 immediately after opening Game Info must page the description')
# With .gameInfoAutoScroll, R2 before the scrolling starts pages at once
# as well, rather than only cancelling the start.
(SD / '.tmp_update/config').mkdir(parents=True, exist_ok=True)
(SD / '.tmp_update/config/.gameInfoAutoScroll').write_text('30000,20')
assert details('R4').tobytes() == paged, (
    'R2 before auto-scroll starts must page the description at once')
shutil.rmtree(OUT)
print('Game Info title layouts and early page presses work with skipped draws')
