# SPDX-License-Identifier: GPL-3.0-only
"""Check complete menu/browser transitions against directly selected screens."""
import os
from pathlib import Path
import json
import subprocess
from env import unlink_if_exists, BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]
OUT = BUILD / "menu-checks"
OUT.mkdir(exist_ok=True)
THEME = ONION_THEME
# Tests own their SD root; the user's editable demo config must not change expectations.
SD = OUT / 'small-sd'
for directory, label in [('00GB', 'Game Boy'), ('01FC', 'NES'), ('02SFC', 'SNES')]:
    emulator = SD / 'Emu' / directory
    emulator.mkdir(parents=True, exist_ok=True)
    (SD / 'Roms' / directory).mkdir(parents=True, exist_ok=True)
    (emulator / 'config.json').write_text(json.dumps(dict(
        label=label, rompath=f'../../Roms/{directory}', extlist='nes',
        icon=str(THEME / '../../Icons/Default/fc.png'))))

def capture(name, *options, sd=SD):
    target = OUT / (name + '.bmp')
    subprocess.run([os.environ.get('MAINUI_TEST_EXE',str(BUILD / "MainUI-dev")), '--sd-root', str(sd),
                    '--theme', str(THEME), '--snapshot', str(target), *options],
                   cwd=ROOT, check=True, timeout=20)
    return target.read_bytes()

home = capture('home')
systems = capture('systems', '--systems')
assert systems == capture('home-games', '--input', 'RE')
assert home == capture('home-return', '--input', 'REBL')
assert capture('nes', '--system', 'NES') == capture('home-nes', '--input', 'RERE')
assert home == capture('unimplemented-back', '--input', 'EB')

settings = OUT/'config'
settings.mkdir(exist_ok=True)
unlink_if_exists((settings/'main-menu.json'))
(settings/'.romListRows').write_text('10')
(settings/'.romListFontSize').write_text('40')
assert systems == capture('independent-grid', '--systems', '--config-dir', str(settings))
assert home == capture('independent-home', '--config-dir', str(settings))
(settings/'main-menu.json').write_text('{"menu":{"games":true,"settings":true}}')
assert home != capture('custom-menu', '--config-dir', str(settings))
assert systems == capture('custom-games', '--config-dir', str(settings), '--input', 'E')

# More than eight systems exposes the second row, last page and wrap behavior.
large = OUT/'sd'
for i in range(10):
    emu = large/'Emu'/f'{i:02}'
    rom = large/'Roms'/f'{i:02}'
    emu.mkdir(parents=True, exist_ok=True)
    rom.mkdir(parents=True, exist_ok=True)
    (emu/'config.json').write_text(json.dumps(dict(label=f'System {i:02}',
        rompath=f'../../Roms/{i:02}', extlist='nes', icon=str(THEME/'../../Icons/Default/fc.png'))))
first = capture('ten-systems', '--systems', sd=large)
assert first != capture('second-row', '--systems', '--input', 'D', sd=large)
assert first != capture('last-page', '--systems', '--input', 'DD', sd=large)
assert first == capture('wrap', '--systems', '--input', 'LR', sd=large)
print('Menu SDL checks passed: home/browser round trips, config order, grid isolation, two rows, paging, wrap')
