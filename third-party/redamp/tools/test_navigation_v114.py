#!/usr/bin/env python3
"""Host key/navigation smoke checks for v1.1.4 and shared library row geometry."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
APP = Path(os.environ.get('REDAMP_APP_DIR', ROOT.parent/'App'/'RedAmp'))
EXE = ROOT/'build'/'redamp-host'


def run(folder, name, events, frames=48):
    dst = folder/name
    dst.mkdir()
    inp = dst/'keys.txt'
    inp.write_text(''.join(f'{f} {key} {val}\n' for f,key,val in events))
    shots = dst/'screens'
    shots.mkdir()
    env = dict(os.environ, MA_HOME=str(APP), MA_ROOT=str(folder),
               MA_DATA=str(dst/'data'), M2D_INPUT=str(inp),
               M2D_DUMP=str(shots), M2D_DUMP_EVERY='5',
               MA_MAX_FRAMES=str(frames), MA_BATTERY_MARKER=str(dst/'marker'))
    proc = subprocess.run([str(EXE)], env=env, capture_output=True, timeout=20)
    assert proc.returncode == 0, proc.stderr[-800:]
    return shots


def px(path, x, y):
    with path.open('rb') as f:
        assert f.readline().strip() == b'P6'
        w,h = map(int, f.readline().split())
        assert (w,h) == (640,480)
        assert f.readline().strip() == b'255'
        f.seek((y*w+x)*3, 1)
        return f.read(3)


def main():
    assert EXE.is_file()
    with tempfile.TemporaryDirectory(prefix='redamp-v114-') as temp:
        t = Path(temp)
        (t/'Media'/'Music').mkdir(parents=True)
        sh = run(t, 'player-b', [(7,29,1),(9,29,0)], 28)
        for xy in [(20,25),(15,85),(30,320)]:
            assert px(sh/'f000005.ppm',*xy) == px(sh/'f000020.ppm',*xy), \
                'B unexpectedly left the player'
        print('PASS player B remains in Player')

        sh = run(t, 'library-home-b', [(2,97,1),(4,97,0),(10,29,1),(12,29,0)], 29)
        assert px(sh/'f000010.ppm',20,25) == px(sh/'f000025.ppm',20,25), \
            'B at Library home switched to Player'
        print('PASS Library home B stays in Library')

        sh = run(t, 'restore-album', [(2,97,1),(4,97,0),
                   (9,108,1),(10,108,0), (12,57,1),(13,57,0),
                   (18,29,1),(20,29,0)], 32)
        # Selected category is Albums, second row in the home list.
        assert px(sh/'f000030.ppm',15,84) != px(sh/'f000030.ppm',15,54), \
            'return to Library home did not restore Albums cursor'
        print('PASS B from Albums restores Albums selection at Library home')

        source=(ROOT/'main.c').read_text()
        assert '#define B_TOP LIB_TOP' in source, 'Folder and indexed Library top do not match'
        assert '#define B_ROWS ((LIB_BOTTOM - B_TOP) / B_ROWH)' in source
        assert 'm->lib_h=libh[i];m->br_h=libh[i]' in source
        assert 'playing_file ? acc' in source and 'draw_play_marker(27, cy' in source
        assert 'draw_play_marker(20, y + LIB_ROW_H / 2' in source
        print('PASS Folder and indexed rows have identical page geometry and playback markers')


if __name__ == '__main__':
    main()
