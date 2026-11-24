#!/usr/bin/env python3
"""Host integration smoke tests for SELECT navigation and battery marker ownership."""
import os
import pathlib
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
APP = pathlib.Path(os.getenv('REDAMP_APP_DIR', ROOT.parent / 'App' / 'RedAmp'))
PLAYER = ROOT / 'build' / 'redamp-host'


def run_case(root, name, events, frames=65, marker_exists=False):
    case = root / name
    case.mkdir()
    marker = case / 'hasBatteryDisplay'
    if marker_exists:
        marker.write_text('pre-existing marker\n')
    inputs = case / 'inputs.txt'
    inputs.write_text(''.join(f'{f} {k} {v}\n' for f, k, v in events))
    dump = case / 'dumps'
    dump.mkdir()
    env = dict(os.environ, MA_HOME=str(APP), MA_ROOT=str(case),
               MA_DATA=str(case / 'data'), M2D_INPUT=str(inputs),
               M2D_DUMP=str(dump), M2D_DUMP_EVERY='10',
               MA_MAX_FRAMES=str(frames), MA_BATTERY_MARKER=str(marker))
    proc = subprocess.Popen([str(PLAYER)], env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    created = False
    for _ in range(120):
        if marker.exists() and not marker_exists:
            created = True
            break
        if proc.poll() is not None:
            break
        time.sleep(.02)
    try:
        out, err = proc.communicate(timeout=12)
    except subprocess.TimeoutExpired:
        proc.kill()
        raise AssertionError(f'{name}: player timed out')
    assert proc.returncode == 0, f'{name}: failed ({proc.returncode}): {err[-600:]}'
    if marker_exists:
        assert marker.read_text() == 'pre-existing marker\n', f'{name}: deleted foreign marker'
    else:
        assert created, f'{name}: marker not created while running'
        assert not marker.exists(), f'{name}: marker left behind after exit'
    return dump


def pixels(path):
    # Minimal P6 PPM reader; avoid additional host dependency.
    with path.open('rb') as f:
        assert f.readline().strip() == b'P6'
        line = f.readline().strip()
        while line.startswith(b'#'):
            line = f.readline().strip()
        w, h = map(int, line.split())
        assert f.readline().strip() == b'255'
        data = f.read()
        return w, h, data


def pixel(frame, x, y):
    w, h, buf = pixels(frame)
    assert w == 640 and h == 480
    i = (y*w+x)*3
    return tuple(buf[i:i+3])


def main():
    assert PLAYER.is_file(), 'Build first with make host'
    with tempfile.TemporaryDirectory(prefix='redamp-nav-test-') as temp:
        root = pathlib.Path(temp)
        sw = run_case(root, 'switch', [(3,97,1),(5,97,0),(14,97,1),(16,97,0)], 42)
        p10 = pixel(sw/'f000010.ppm',20,25)
        p20 = pixel(sw/'f000020.ppm',20,25)
        assert p10 != p20, f'SELECT tap failed to switch view: {p10} vs {p20}'
        print('PASS SELECT tap switches Player/Library')

        hd = run_case(root, 'hold', [(3,97,1),(5,97,0),(11,97,1),
                                    (48,97,0),(52,29,1),(53,29,0)], 72)
        a = pixels(hd/'f000010.ppm')[2]
        b = pixels(hd/'f000040.ppm')[2]
        c = pixels(hd/'f000060.ppm')[2]
        assert len(a) == len(b) == len(c)
        diff_open = sum(x != y for x,y in zip(a,b))
        # Exclude transient library-scan toast near the bottom: at lower UI FPS
        # it expires between reference frames although the library view is unchanged.
        diff_back = sum(a[i] != c[i] for i in range(640 * 360 * 3))
        assert diff_open > 2000, f'Options did not open on hold: differences={diff_open}'
        assert diff_back < 2000, f'Options did not return to Library: differences={diff_back}'
        print('PASS SELECT hold opens Options, B returns to Library')

        run_case(root, 'foreign-marker', [(3,1,1),(8,1,0)], 24, marker_exists=True)
        print('PASS pre-existing battery marker preserved')
        print('PASS battery marker created and removed only when owned')


if __name__ == '__main__':
    main()
