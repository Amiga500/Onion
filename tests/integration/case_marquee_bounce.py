# SPDX-License-Identifier: GPL-3.0-only
"""Bounce title scrolling: a long title travels to its end, pauses and returns.

.romListTitleScroll takes an optional third value. "bounce" bounces; no third
value, "rolling" or anything else rolls exactly as before.
"""
import functools
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
OUT = Path(tempfile.mkdtemp(prefix='marquee-bounce-', dir=BUILD))
SD = OUT / 'sd'
for directory in ['Emu/FC', 'Roms/FC/Imgs', '.tmp_update/config']:
    (SD / directory).mkdir(parents=True)
(SD / 'Emu/FC/config.json').write_text(
    json.dumps(dict(label='NES', rompath='../../Roms/FC', imgpath='../../Roms/FC/Imgs',
                    extlist='nes')))
SCROLL = SD / '.tmp_update/config/.romListTitleScroll'
EDGE = 640 - Image.open(ONION_THEME / 'skin/preview-bg.png').width


def run(elapsed, refresh=False):
    output = OUT / 'row.bmp'
    subprocess.run([os.environ.get('MAINUI_TEST_EXE', str(BUILD / 'MainUI-dev')),
                    '--sd-root', str(SD), '--theme', str(ONION_THEME), '--fallback',
                    str(ONION_THEME), '--system', 'NES', '--elapsed', str(elapsed),
                    '--snapshot', str(output)] + (['--refresh-caches'] if refresh else []),
                   cwd=ROOT, check=True, timeout=20)
    return output


def use(title, cover=False):
    """List only this title, with or without a cover; its list is built once."""
    for old in list((SD / 'Roms/FC').glob('*.nes')) + list((SD / 'Roms/FC/Imgs').iterdir()):
        old.unlink()
    (SD / 'Roms/FC' / (title + '.nes')).write_bytes(b'')
    if cover:
        Image.new('RGB', (10, 10), (0, 0, 0)).save(SD / 'Roms/FC/Imgs' / (title + '.png'))
    run(0, refresh=True)


def row(elapsed):
    """The selected row at a given instant, as bytes: equal rows look equal."""
    return Image.open(run(elapsed)).convert('RGB').crop((0, 60, 640, 120)).tobytes()


@functools.lru_cache(maxsize=None)
def bright(band):
    """The bright pixels of a row."""
    return frozenset((i // 3 % 640, 60 + i // 3 // 640) for i in range(0, len(band), 3)
                     if min(band[i:i + 3]) > 200)


def ends(band, lane):
    pixels = bright(band)
    start = min(x for x, _ in pixels)
    return (sum(1 for x, _ in pixels if x < start + 60),
            sum(1 for x, _ in pixels if lane - 60 <= x < lane))


# Dense blocks at both ends of a title only just too long for its row (with
# a cover, for the shorter lane), sparse dots between: a bounce is a few
# frames each way.
TITLE = 'M' * 8 + '.' * 30 + 'W' * 8
COVERED = 'M' * 8 + '.' * 6 + 'W' * 8
RTL = 'ب' + TITLE
# Starts after 100 ms at 400 px/s: 16 px per 40 ms frame at full speed,
# resting 10 frames (400 ms) at each end.
SCROLL.write_text('100,400,bounce')
REST = 10


def runs(frames):
    """[frame, how many in a row] for each run of equal frames."""
    out = []
    for frame in frames:
        if out and out[-1][0] == frame:
            out[-1][1] += 1
        else:
            out.append([frame, 1])
    return out


def sweep(whole=True):
    """Rows every 40 ms frame through a bounce, until it is back at the
    start, or only until it has rested at the far end."""
    frames, rest = [], row(0)
    for _ in range(60):
        frames.append(row(100 + len(frames) * 40))
        held = runs(frames)[-2:]
        # Back at the start (the rest there is the unit tests' to check).
        if whole and frames[-1] == rest and any(
                count >= REST and frame != rest for frame, count in runs(frames)):
            break
        if not whole and len(held) == 2 and held[0][0] != rest and held[0][1] >= REST:
            break
    return rest, frames


def step(before, after):
    """How far the text moved between two frames, right positive, within one
    marquee step (16 px here): the best overlap."""
    b, a = bright(before), bright(after)
    return max(range(-17, 18), key=lambda d: len({(x + d, y) for x, y in b} & a))


for cover in (False, True):
    use(COVERED if cover else TITLE, cover)
    lane = EDGE if cover else 620
    # The return is the same with a cover: follow that one to the far end.
    rest, frames = sweep(whole=not cover)
    # Rest, a few steps out, the rest at the far end and as many steps back.
    # Easing in and out may hold an end a frame or two longer.
    seen = runs(frames)
    far = [i for i, (frame, count) in enumerate(seen) if count > 1 and frame != rest]
    assert len(far) == 1 and REST <= seen[far[0]][1] <= REST + 2, [c for _, c in seen]
    end = seen[far[0]][0]
    out = far[0] - 1
    if not cover:
        back = len(seen) - far[0] - 2
        assert abs(out - back) <= 1 and out >= 1 and seen[-1][0] == rest, [c for _, c in seen]
    # Each step out moves the title left by at most 16 px, each step back
    # right, and each way eases: its first and last steps are its smallest.
    turned = frames.index(end)
    steps = []
    for i, (before, after) in enumerate(zip(frames, frames[1:])):
        if before != after:
            moved = step(before, after)
            assert (moved < 0) == (i < turned), i
            if i < turned:
                steps.append(-moved)
    assert len(steps) >= 3 and max(steps[0], steps[-1]) < max(steps), steps
    # The far end is the start moved left by up to those steps: away from
    # the edges, it matches the start shifted left and nothing else does.
    start, last = bright(rest), bright(end)
    left = min(x for x, _ in start)

    def matched(d):
        moved = {(x - d, y) for x, y in start if left + 20 <= x - d < lane - 40}
        return len(moved & last) / max(1, len(moved))
    best = max(range(0, 17 * (out + 1) + 1), key=matched)
    assert best > sum(steps) - 4 and matched(best) > 0.85 > matched(0), (best, matched(best))
    # At the far end the title ends at the lane's end: the right margin, or
    # the cover's edge.
    right = max(x for x, _ in bright(end) if x < lane + 2)
    assert lane - 30 <= right < lane + 2, (right, lane)

# A right-to-left title starts at its right end and moves right first.
use(RTL)
rest, frames = row(0), [row(100 + 40 * k) for k in range(4)]
assert ends(rest, 620)[1] > 150, ends(rest, 620)
first, later = bright(rest), bright(frames[3])
moved = max(range(-60, 61), key=lambda d: len({(x + d, y) for x, y in first} & later))
assert 0 < moved <= 56, moved

# Rolling is unchanged: the two-value form, "rolling" and an unknown style
# draw the same pixels at every instant, and a short title never moves.
use(TITLE)
instants = [0, 300, 1300]
SCROLL.write_text('100,400')
plain = [row(t) for t in instants]
SCROLL.write_text('100,400,rolling')
assert [row(t) for t in instants] == plain
SCROLL.write_text('100,400,sideways')
assert row(1300) == plain[2]
SCROLL.write_text('100,400,bounce')
# Here bounce has stopped at the end while rolling went on.
assert row(1300) != plain[2]
use('Short Game')
assert row(0) == row(2000)
shutil.rmtree(OUT)
print('Bounce titles travel to their end, pause and return; rolling titles are unchanged')
