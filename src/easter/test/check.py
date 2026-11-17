"""Reads the levels out of easter.c, solves each one, then plays the solutions
through the real game binary and checks what it reports."""
import re, subprocess, sys
from collections import deque
SRC, BIN = sys.argv[1], sys.argv[2]
text = open(SRC).read()
block = text[text.index('static const Level levels[] = {'):text.index('#define LEVEL_COUNT')]
levels = [re.findall(r'"([^"]*)"', part) for part in block.split('{{')[1:]]
DIRS = {'L': (-1, 0), 'R': (1, 0), 'U': (0, -1), 'D': (0, 1)}

def parse(rows):
    walls, goals, boxes, player = set(), set(), set(), None
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == '#': walls.add((x, y))
            if ch in '.*+': goals.add((x, y))
            if ch in '$*': boxes.add((x, y))
            if ch in '@+': player = (x, y)
    return walls, goals, frozenset(boxes), player

def solve(rows):
    walls, goals, boxes, player = parse(rows)
    assert player and len(boxes) == len(goals) > 0
    start = (player, boxes); seen = {start: None}; queue = deque([start])
    while queue:
        state = queue.popleft(); p, bx = state
        if bx == goals:
            out = []
            while seen[state]:
                state, k = seen[state]; out.append(k)
            return ''.join(reversed(out))
        for k, d in DIRS.items():
            n = (p[0] + d[0], p[1] + d[1])
            if n in walls: continue
            nb = bx
            if n in bx:
                m = (n[0] + d[0], n[1] + d[1])
                if m in walls or m in bx: continue
                nb = frozenset(bx - {n} | {m})
            ns = (n, nb)
            if ns not in seen:
                seen[ns] = (state, k); queue.append(ns)

def run(keys, *more):
    out = subprocess.run([BIN, '--mute', '--input', keys, '--frames', str(len(keys) + 1), *more],
                         capture_output=True, text=True).stdout.strip()
    return dict(kv.split('=') for kv in out.split())

ok = True
def check(name, cond):
    global ok
    ok &= bool(cond)
    print(('  ok   ' if cond else '  FAIL ') + name)

sols = []
for i, rows in enumerate(levels, 1):
    s = solve(rows)
    w, h = max(map(len, rows)), len(rows)
    print(f"level {i}: {w}x{h}, {sum(r.count('$') + r.count('*') for r in rows)} crates, "
          + (f"shortest solution {len(s)} moves" if s else "NO SOLUTION"))
    ok &= s is not None and w <= 16 and h <= 12
    sols.append(s or '')


print("played through the game itself:")
r = run('A' + 'A'.join(sols) + 'A')
check("all ten solved in one run ends on the final screen, then A returns to the title", r['state'] == '0')
r = run('A' + 'A'.join(sols))
check("final screen after level 10", r['state'] == '3' and r['level'] == '10')
check("total moves add up", int(r['total_moves']) == sum(map(len, sols)))
for i, s in enumerate(sols, 1):
    r = run('A' + s, '--level', str(i))
    want = '3' if i == 10 else '2'
    check(f"level {i} alone: solved in {len(s)} moves", r['state'] == want and r['moves'] == str(len(s)) and r['home'].split('/')[0] == r['home'].split('/')[1])
s = sols[2]
r = run('A' + s[:-1], '--level', '3')
check("one move short of the end: still playing", r['state'] == '1')
r = run('A' + s + 'B' * 5, '--level', '3')
check("moves after a level is solved are ignored", r['state'] == '2' and r['moves'] == str(len(s)))
half = s[:len(s) // 2]
r = run('A' + half + 'B' * len(half), '--level', '3')
check("undoing every move gives the starting position", r['moves'] == '0' and r['pushes'] == '0' and r['home'] == '0/2')
r = run('A' + half + 'B' * len(half) + 'B' * 3 + s, '--level', '3')
check("undo past the start does nothing, the level still solves", r['state'] == '2' and r['moves'] == str(len(s)))
r = run('A' + 'UUUU', '--level', '1')
check("two free cells then a wall: four presses make two moves", r['moves'] == '2' and r['pushes'] == '0')
r = run('A' + half + 'X', '--level', '3')
check("X once only asks", r['moves'] == str(len(half)))
r = run('A' + half + 'XX' + s, '--level', '3')
check("X twice restarts, and the level solves from the start", r['state'] == '2' and r['moves'] == str(len(s)))
r = run('A' + half + 'X' + '.' * 160 + 'X', '--level', '3')
check("X, a long pause, X: not restarted", r['moves'] == str(len(half)))
r = run('A' + 'LLLLLLLL', '--level', '4')
check("a crate against a wall cannot be pushed, nor two crates at once", r['pushes'] in ('0', '1'))
print("ALL OK" if ok else "SOMETHING FAILED")
sys.exit(0 if ok else 1)
