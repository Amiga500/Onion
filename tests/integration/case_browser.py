# SPDX-License-Identifier: GPL-3.0-only
import os
#!/usr/bin/env python3
"""Exercise actual SDL input/navigation and snapshot output with synthetic ROMs."""
from pathlib import Path
import subprocess
import shutil
import tempfile
from env import unlink_if_exists, BUILD, ONION_THEME, require_fixture_sd, require_onion_theme  # noqa: E402

require_onion_theme()

FIXTURE_SD = require_fixture_sd()
ROOT = Path(__file__).resolve().parents[2]
EXE = Path(os.environ.get('MAINUI_TEST_EXE', BUILD / "MainUI-dev"))
THEME = ONION_THEME
OUT = BUILD / "browser-checks"
OUT.mkdir(exist_ok=True)
SD = Path(tempfile.mkdtemp(prefix='browser-sd-', dir=BUILD)) / 'sdcard'
shutil.copytree(FIXTURE_SD, SD)
def capture(name, *options, active=False):
    unlink_if_exists((SD / 'appconfigs/romwinidx.json'))
    target = OUT / (name + '.bmp')
    args = [str(EXE), '--sd-root', str(SD), '--systems',
            '--fallback' if active else '--theme', str(THEME),
            '--snapshot', str(target), *options]
    subprocess.run(args, cwd=ROOT, check=True, timeout=20)
    data = target.read_bytes()
    assert data[:2] == b'BM' and len(data) > 640 * 480 * 3
    return data
systems = capture('systems')
nes = capture('nes', '--system', 'NES')
assert nes == capture('open-nes', '--input', 'RE')
assert nes == capture('folder-return', '--system', 'NES', '--input', 'EDEBB')
assert nes == capture('game-no-launch', '--system', 'NES', '--input', 'DDEUU')
assert systems == capture('system-return', '--input', 'EB')
assert capture('gb', '--system', 'Game Boy') != nes
assert capture('sfc', '--system', 'Super Nintendo') != nes
assert capture('empty', '--system', 'NES', '--input', 'DE') != nes
assert capture('active', '--system', 'NES', active=True) != nes
result = subprocess.run([str(EXE), '--sd-root', str(SD), '--systems', '--theme', str(THEME),
                         '--system', 'Missing', '--snapshot', str(OUT/'missing.bmp')],
                        cwd=ROOT, capture_output=True, timeout=20)
assert result.returncode == 2
print('SDL browser checks passed: 10 rendered scenarios, navigation round trips, selected theme, invalid-system error')
