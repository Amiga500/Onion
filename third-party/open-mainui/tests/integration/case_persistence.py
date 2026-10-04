# SPDX-License-Identifier: GPL-3.0-only
"""Test abrupt writer death, fresh-process recovery and concurrent updates."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sqlite3
import tempfile
import time
from env import BUILD, unlink_if_exists  # noqa: E402


ROOT = Path(__file__).resolve().parents[2]
SD = Path(tempfile.mkdtemp(prefix="persistence-", dir=BUILD))
PROBE = BUILD / "persistence-probe"
(SD / "Roms").mkdir()
def run(mode, key="value", *, fault=None, sync_failure=None):
    env = dict(os.environ)
    env.pop("MAINUI_TEST_FAULT", None)
    if fault: env["MAINUI_TEST_FAULT"] = fault
    env.pop("MAINUI_TEST_SYNC_FAILURE", None)
    if sync_failure: env["MAINUI_TEST_SYNC_FAILURE"] = sync_failure
    return subprocess.run([str(PROBE), mode, str(SD), key], env=env,
                          capture_output=True, text=True, timeout=30)
# Launch handoff intentionally requires hard links (its runtime directory is /tmp).
if "no-hardlinks" not in os.environ.get("LD_PRELOAD", "") and not os.environ.get("MAINUI_FAT_ROOT"):
    handoff = ("cmd_to_run.sh", "mainui-return.json", "mainui-favourite-folder-return",
               "mainui-context-search-postgame")
    for failed_file in (*handoff, "state.json"):
        result = run("launch", sync_failure=failed_file)
        assert result.returncode == 3, (failed_file, result.stderr)
        assert not any((SD / name).exists() for name in handoff), failed_file
        # No orphaned publication can block the next attempt.
        assert run("launch").returncode == 0
        for name in handoff:
            (SD / name).unlink()
    # A return file that is removed but whose folder flush fails is still
    # consumed: the parsed state is the only copy left.
    assert run("launch").returncode == 0
    (SD / "cmd_to_run.sh").unlink()
    assert run("take", sync_failure="mainui-return.json").stdout.strip() == "returned"
    assert not (SD / "mainui-return.json").exists()
    for name in handoff:
        unlink_if_exists(SD / name)
    (SD / "cmd_to_run.sh").write_text("foreign command")
    assert run("launch").returncode == 3
    assert (SD / "cmd_to_run.sh").read_text() == "foreign command"
    (SD / "cmd_to_run.sh").unlink()

atomic = SD / "atomic.json"
for phase in ("created", "flushed", "published"):
    atomic.write_text('{"old":true}')
    result = run("atomic", '{"new":true}', fault=phase)
    assert result.returncode == 77, result.stderr
    assert json.loads(atomic.read_text()) == ({"new": True} if phase == "published" else {"old": True})
    assert run("atomic", '{"recovered":true}').returncode == 0
    assert json.loads(atomic.read_text()) == {"recovered": True}
reservation = SD / "atomic.json.writing"
reservation.write_text("foreign")
before = atomic.read_bytes()
assert run("atomic", "{}").returncode == 3
assert atomic.read_bytes() == before and reservation.read_text() == "foreign"
reservation.unlink()
assert list(SD.glob("atomic.json.writing.*"))
holder = subprocess.Popen([str(PROBE), "hold", str(SD)])
try:
    deadline = time.monotonic() + 10
    while not (SD / "holder-ready").exists():
        assert time.monotonic() < deadline and holder.poll() is None
        time.sleep(.01)
    holder.kill()
    holder.wait(timeout=10)
finally:
    if holder.poll() is None:
        holder.kill()
        holder.wait(timeout=10)
assert run("system", "after_crash").returncode == 0
for mode in ("system", "favorite", "folder"):
    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(lambda i: run(mode, f"{mode}{i}"), range(16)))
    assert all(r.returncode == 0 for r in results), [(r.returncode, r.stderr) for r in results]
system = json.loads((SD / "system.json").read_text())
assert all(system[f"system{i}"] == 1 for i in range(16)) and system["after_crash"] == 1
favorites = [json.loads(line) for line in (SD / "Roms/favourite.json").read_text().splitlines()]
assert len(favorites) == 16 and len({r["label"] for r in favorites}) == 16
sidecar = [json.loads(line) for line in (SD / "Roms/favourite-folders.json").read_text().splitlines()]
assert len([r for r in sidecar if r.get("kind") == "folder"]) == 16 and sidecar[0]["generation"] == 16
(SD / "mainui-return.json").write_text(json.dumps(dict(schema=1, committed=False, record={}, resume={})))
assert run("take").stdout.strip() == "empty"
assert not (SD / "mainui-return.json").exists()
(SD / "Emu/Test").mkdir(parents=True)
(SD / "Roms/Test").mkdir()
(SD / "Emu/Test/config.json").write_text(json.dumps(dict(label="Test", launch="launch.sh", rompath="../../Roms/Test", extlist="nes")))
rom = SD / "Roms/Test/one.nes"
rom.write_bytes(b"")
assert run("cache").returncode == 0
cache = SD / "Roms/Test/Test_cache6.db"
for phase in ("cache-populated", "cache-prepared"):
    before = cache.read_bytes()
    (SD / f"Roms/Test/{phase}.nes").write_bytes(b"")
    assert run("cache", fault=phase).returncode == 77
    assert cache.read_bytes() == before
    assert list(cache.parent.glob(cache.name + ".building.*"))
    assert not list(cache.parent.glob(cache.name + ".building.*-journal"))
    assert run("cache").returncode == 0 and cache.read_bytes() != before
    assert not list(cache.parent.glob(cache.name + ".building.*"))
    with sqlite3.connect(cache) as database:
        assert database.execute("PRAGMA integrity_check").fetchone() == ("ok",)
        assert database.execute("PRAGMA journal_mode").fetchone() == ("delete",)
        schema = database.execute(
            "SELECT sql FROM sqlite_master WHERE name='Test_roms'").fetchone()[0]
        assert "AUTOINCREMENT" in schema
    (SD / f"Roms/Test/{phase}.nes").unlink()
    assert run("cache").returncode == 0
(SD / "Roms/Test/two.nes").write_bytes(b"")
assert run("cache").returncode == 0
# SQLite commit determines rollback/finalization; duplicate names require manual resolution.
for dual_name in (False, True):
    for phase, retained in (("delete-moved", True), ("delete-committed", False)):
        rom.write_bytes(b"ROM payload")
        assert run("cache").returncode == 0
        result = run("delete", fault=phase)
        assert result.returncode == 77, (result.returncode, result.stderr)
        staged = Path(json.loads(Path(str(cache) + ".delete.json").read_text())["staged"])
        if dual_name:
            rom.write_bytes(b"new payload")  # Same size, different ROM: never delete either file.
            assert run("recover").returncode == 0
            assert rom.read_bytes() == b"new payload" and staged.read_bytes() == b"ROM payload"
            assert Path(str(cache) + ".delete.json").exists()
            rom.unlink()  # Resolve the conflict explicitly, then retry recovery.
        result = run("recover")
        assert result.returncode == 0, result.stderr
        assert rom.exists() == retained
        if retained: assert rom.read_bytes() == b"ROM payload"
        assert not staged.exists() and not Path(str(cache) + ".delete.json").exists()
# UTF-8 catalog paths are passed through unchanged.
unicode_rom = SD / "Roms/Test/aaa-\u4e2d\u6587.nes"
unicode_rom.write_bytes(b"Unicode ROM")
assert run("cache").returncode == 0
assert run("delete").returncode == 0
assert not unicode_rom.exists()
# A cache row changed after the list was read: nothing is staged or deleted (#6).
stale_rom = SD / "Roms/Test/aaa-stale.nes"
for change in ("UPDATE Test_roms SET path=path||'.moved' WHERE path LIKE '%/aaa-stale.nes'",
               "DELETE FROM Test_roms WHERE path LIKE '%/aaa-stale.nes'"):
    stale_rom.write_bytes(b"stale ROM")
    assert run("cache").returncode == 0
    result = run("delete-stale", change)
    assert result.returncode == 3, (result.returncode, result.stderr)
    assert stale_rom.exists(), "ROM deleted although its cache row was gone"
    assert stale_rom.read_bytes() == b"stale ROM"
    assert "out of date" in result.stdout, result.stdout
    assert not Path(str(cache) + ".delete.json").exists()
    assert not list(stale_rom.parent.glob(stale_rom.name + ".mainui-delete*"))
stale_rom.unlink()
assert run("cache").returncode == 0

# Over-limit lists can be reduced without discarding the remaining records.
over_limit = [dict(label="value", rompath="/mnt/SDCARD/Roms/Test/value.nes")]
over_limit += [dict(label=f"overflow{i}", rompath=f"/mnt/SDCARD/Roms/Test/{i}.nes")
               for i in range(10000)]
favorite_file = SD / "Roms/favourite.json"
favorite_file.write_text("".join(json.dumps(record) + "\n" for record in over_limit))
assert run("favorite-remove-once").returncode == 0
assert len(favorite_file.read_text().splitlines()) == 10000

# Publish-then-flush: once the new file is visible, a failed folder flush is not
# reported as "unchanged".
atomic.write_text('{"old":true}')
assert run("atomic", '{"flush":false}', sync_failure="atomic.json").returncode == 0
assert json.loads(atomic.read_text()) == {"flush": False}
if "no-hardlinks" not in os.environ.get("LD_PRELOAD", "") and not os.environ.get("MAINUI_FAT_ROOT"):
    # Exclusive publication uses link(); its request files live in /tmp.
    unlink_if_exists(SD / "new.txt")
    assert run("new", "request", sync_failure="new.txt").returncode == 0
    assert (SD / "new.txt").read_text() == "request"
before = cache.read_bytes()
(SD / "Roms/Test/flush.nes").write_bytes(b"")
result = run("cache", sync_failure="Test_cache6.db")
assert result.returncode == 0, result.stderr
assert "flushing its folder failed" in result.stderr, result.stderr
with sqlite3.connect(cache) as database:
    names = [row[0] for row in database.execute("SELECT path FROM Test_roms")]
assert any(name.endswith("flush.nes") for name in names), names
# Removing a cache whose folder flush fails still reports the removal.
result = run("remove-cache", sync_failure="Test_cache6.db")
assert result.returncode == 0, result.stderr
assert not cache.exists()
assert run("cache").returncode == 0
print("Persistence interruption/concurrent-writer fixtures passed:", SD)
