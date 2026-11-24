#!/usr/bin/env python3
"""Host UI test for Library category picker and short/long B navigation."""
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
APP = pathlib.Path(os.environ.get('REDAMP_APP_DIR', ROOT.parent / 'App' / 'RedAmp'))
EXE = ROOT / 'build' / 'redamp-host'


def run(root, label, events, max_frames=72, dump_every=5):
    work = root / label
    work.mkdir()
    data = work / 'data'
    inp = work / 'events.txt'
    inp.write_text(''.join(f'{frame} {key} {value}\n' for frame, key, value in events))
    shots = work / 'screens'
    shots.mkdir()
    env = dict(os.environ, MA_HOME=str(APP), MA_ROOT=str(work), MA_DATA=str(data),
               M2D_INPUT=str(inp), M2D_DUMP=str(shots),
               M2D_DUMP_EVERY=str(dump_every), MA_MAX_FRAMES=str(max_frames),
               MA_BATTERY_MARKER=str(work / 'battery-marker'))
    proc = subprocess.run([str(EXE)], env=env, capture_output=True, timeout=25)
    assert proc.returncode == 0, f'{label}: return {proc.returncode}: {proc.stderr[-500:]}'
    return shots


def region(fname, x1=135, y1=80, x2=500, y2=410):
    with fname.open('rb') as f:
        assert f.readline().strip() == b'P6'
        w, h = map(int, f.readline().split())
        assert (w, h) == (640, 480)
        assert f.readline().strip() == b'255'
        b = f.read()
    return b''.join(b[(j*w+x1)*3:(j*w+x2)*3] for j in range(y1, y2))


def main():
    assert EXE.is_file(), 'make host first'
    with tempfile.TemporaryDirectory(prefix='redamp-library-views-') as t:
        root = pathlib.Path(t)
        # SELECT tap -> Library; DOWN x3 then A -> Folder browser.
        # B press at frame 16, hold beyond the 0.65s timer, release at 42.
        sh = run(root, 'hold-from-folders', [
            (2,97,1),(4,97,0),
            (6,108,1),(7,108,0),(8,108,1),(9,108,0),
            (10,108,1),(11,108,0),(12,57,1),(13,57,0),
            (16,29,1),(43,29,0),
            (48,108,1),(49,108,0),(51,57,1),(52,57,0),
        ], 68)
        # Picker overlays the Folder view before release, then selecting an
        # indexed category leaves the picker and shows Music Library.
        a = region(sh / 'f000015.ppm')
        b = region(sh / 'f000035.ppm')
        c = region(sh / 'f000060.ppm')
        assert sum(x != y for x,y in zip(a,b)) > 12000, 'holding B did not open category picker'
        assert sum(x != y for x,y in zip(b,c)) > 12000, 'A did not switch category'
        print('PASS hold B opens views from nested Folders; A chooses another view')
        # Short B should return one folder level, without showing the picker.
        sh = run(root, 'tap-b', [(2,97,1),(4,97,0),(6,108,1),(7,108,0),
                                 (8,108,1),(9,108,0),(10,108,1),(11,108,0),
                                 (12,57,1),(13,57,0),(16,29,1),(18,29,0)], 38)
        assert region(sh / 'f000015.ppm') != region(sh / 'f000030.ppm'), 'tap B did not back out'
        print('PASS tap B still navigates back')

if __name__ == '__main__':
    main()
