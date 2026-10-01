# SPDX-License-Identifier: GPL-3.0-only
import os
#!/usr/bin/env python3
"""Generate cache fixtures in build/ and verify compiled C plus actual SDL paging.
All ROMs are empty filename fixtures. Valid/unsupported caches remain byte-identical.
"""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
from env import unlink_if_exists, BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
BASE = BUILD / "cache-fixtures"
SYSTEM = "Odd'System"
TABLE = SYSTEM + '_roms'
def quoted(name):
    return '"' + name.replace('"', '""') + '"' 
def make(name, rows=(), corrupt=False, wrong=False, wal=False):
    sd = BASE / name
    emu = sd / 'Emu/FC'
    rom = sd / 'Roms' / SYSTEM
    emu.mkdir(parents=True, exist_ok=True)
    rom.mkdir(parents=True, exist_ok=True)
    (emu/'config.json').write_text(json.dumps(dict(label='Cache test', rompath=f'../../Roms/{SYSTEM}',
                                                 imgpath=f'../../Roms/{SYSTEM}/Imgs', extlist='nes')))
    (rom/'fallback.nes').write_bytes(b'')
    db = rom / (SYSTEM + '_cache6.db')
    if db.exists():
        # Exact generated fixture file, never an SD-card input.
        db.unlink()
    if corrupt:
        db.write_bytes(b'not a database')
        return sd, db
    with sqlite3.connect(db) as conn:
        if wrong:
            conn.execute('CREATE TABLE unrelated (x)')
        else:
            schema = '(id INTEGER PRIMARY KEY, disp TEXT NOT NULL, path TEXT NOT NULL, imgpath TEXT NOT NULL, type INTEGER DEFAULT 0, ppath TEXT NOT NULL, pinyin TEXT NOT NULL, cpinyin TEXT NOT NULL)'
            conn.execute(f'CREATE TABLE {quoted(TABLE)} {schema}')
            conn.executemany(f'INSERT INTO {quoted(TABLE)} VALUES (?,?,?,?,?,?,?,?)', rows)
            conn.execute(f'CREATE INDEX browse ON {quoted(TABLE)} (ppath,type DESC,disp COLLATE NOCASE)')
            quoted_table = quoted('quoted"table')
            conn.execute(f'CREATE TABLE {quoted_table} {schema}')
            conn.execute('INSERT INTO "quoted""table" VALUES (1,\'Quoted identifier\',\'./q.nes\',\'\',0,\'.\',\'\',\'\')')
    conn.close()
    if wal:
        connection = sqlite3.connect(db)
        connection.execute('PRAGMA journal_mode=WAL')
        connection.close()
    return sd, db
def row(i, label, path, kind=0, parent='.', art=''):
    return (i,label,path,art,kind,parent,'','')
def run(scenario, name, rows=(), **kwargs):
    sd, db = make(name, rows, **kwargs)
    before = hashlib.sha256(db.read_bytes()).digest()
    files = sorted(p.relative_to(sd).as_posix() for p in sd.rglob('*') if p.is_file())
    subprocess.run([str(BUILD / "fixture-cache"), str(sd), scenario], check=True, cwd=ROOT, timeout=30)
    if scenario == "repaired":
        assert hashlib.sha256(db.read_bytes()).digest() != before
        with sqlite3.connect(db) as connection:
            assert connection.execute("pragma integrity_check").fetchone() == ("ok",)
        repaired = db.read_bytes()
        subprocess.run([str(BUILD / "fixture-cache"), str(sd), scenario], check=True, cwd=ROOT, timeout=30)
        assert db.read_bytes() == repaired
    else:
        assert hashlib.sha256(db.read_bytes()).digest() == before
    after_files = {p.relative_to(sd).as_posix() for p in sd.rglob('*') if p.is_file()}
    expected_files = set(files)

    assert expected_files == after_files
    return sd
large = [row(1,'Collections',"./Sets/O'Brien",1),row(2,'Empty','./Empty',1)]
large += [row(i+3,f'Game {i:05d}',f'./raw{i}.nes') for i in range(13000)]
large += [row(14000,'A proper cached title',f"/mnt/SDCARD/Roms/{SYSTEM}/Sets/O'Brien/raw.nes",0,"./Sets/O'Brien",f'/mnt/SDCARD/Roms/{SYSTEM}/Imgs/cover.png')]
sd = run('large','large',large)
run('empty','empty')
run('repaired','corrupt',corrupt=True)
run('repaired','wrong-schema',wrong=True)
run('fallback', 'wal', wal=True)
run('fallback','invalid-row',[row(1,'Invalid','./x.nes',9)])
run('fallback','nul-row',[row(1,'bad\0label','./x.nes')])
run('fallback','long-row',[row(1,'x'*4096,'./x.nes')])
run('bad-later','bad-later',[row(i+1,f'Game {i:03d}','./x.nes' if i<69 else '') for i in range(70)])
run('sort','sort',[row(i+1,label,f'./{i}.nes') for i,label in enumerate(['alpha','ALPHA','beta','Zebra'])])
# Real SDL rendering exercises the former fixed-size list boundary and cross-window navigation.
ui = BUILD / "MainUI-dev"
theme = ONION_THEME
def capture(name, actions=''):
    unlink_if_exists((sd / "appconfigs/romwinidx.json"))
    out = BASE/(name+'.bmp')
    command = [str(ui),'--sd-root',str(sd),'--theme',str(theme),'--system','Cache test','--snapshot',str(out)]
    if actions:
        command += ['--input', actions]
    subprocess.run(command, check=True, cwd=ROOT, timeout=30)
    return out.read_bytes()
first = capture('first')
assert capture('folder-return','EB') == first
assert capture('last','U') != first
assert capture('last-return','UD') == first
assert capture('window-boundary','D'*65) != first
print('Cache checks passed: 10 C scenarios, 13002-row paging, 5 SDL captures, unchanged input databases')
