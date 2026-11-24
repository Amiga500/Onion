#!/usr/bin/env python3
"""Optional: fetch the four original CC0 Winamp skins from YTMinamp-Skins.
Run on a machine with internet access: python3 src/tools/fetch_cc0_skins.py
Their exact original binaries are NOT part of the source-only dev release.
"""
from pathlib import Path
import io
import urllib.request
import zipfile

BASE = 'https://raw.githubusercontent.com/ergosteur/YTMinamp-Skins/main/bundled-skins'
SKINS = {
    'ember': 'YTMinamp-Ember.wsz',
    'material': 'YTMinamp-Material-Music.wsz',
    'vaporwave': 'YTMinamp-Vaporwave.wsz',
    'vfd': 'YTMinamp-VFD.wsz',
}
ROOT = Path(__file__).resolve().parents[1]
OUT = (ROOT.parent / 'App' / 'RedAmp' / 'skins')
REQUIRED = {'main.bmp', 'cbuttons.bmp', 'titlebar.bmp', 'text.bmp'}

def main():
    if not (OUT.parent / 'launch.sh').is_file():
        raise SystemExit('Run from a complete App/+src checkout, not the source-only VM.')
    OUT.mkdir(parents=True, exist_ok=True)
    for folder, name in SKINS.items():
        url = f'{BASE}/{folder}/{name}'
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={
                    'User-Agent': 'RedAmp-dev-skin-fetcher/1.0'}), timeout=20) as response:
                data = response.read(2 * 1024 * 1024 + 1)
            if len(data) > 2 * 1024 * 1024 or not zipfile.is_zipfile(io.BytesIO(data)):
                raise ValueError('oversized or invalid Winamp skin archive')
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                files = {i.filename.replace('\\', '/').split('/')[-1].lower()
                         for i in z.infolist() if not i.is_dir()}
                if not REQUIRED.issubset(files):
                    raise ValueError('missing required classic skin bitmaps')
                if len(z.infolist()) > 128 or any(i.file_size > 4 * 1024 * 1024
                                                  for i in z.infolist()):
                    raise ValueError('suspicious archive size')
            (OUT / (name.removeprefix("YTMinamp-"))).write_bytes(data)
            print('Downloaded', name)
        except (OSError, ValueError, zipfile.BadZipFile) as exc:
            print('SKIPPED', name, ':', exc)
    print('Source and CC0 attribution: https://github.com/ergosteur/YTMinamp-Skins')

if __name__ == '__main__': main()
