# SPDX-License-Identifier: GPL-3.0-only
"""Verify cache creation, refresh and failed publication on isolated ROM trees."""
from contextlib import closing
import json
import os
from pathlib import Path
import sqlite3
import stat
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()

ROOT = Path(__file__).resolve().parents[2]


def run(sd, *options, expected=0):
    executable = str(BUILD / "MainUI-dev")
    theme = ONION_THEME
    result = subprocess.run(
        ([str(BUILD / "fixture-cache"), str(sd), "rebuild"]
         if options == ("--rebuild",) else
         [executable, "--sd-root", str(sd), "--theme", str(theme),
          "--snapshot", str(sd / "capture.bmp"), *options]),
        cwd=ROOT, timeout=30, capture_output=True,
    )
    assert result.returncode == expected, result.stderr.decode(errors="replace")


def names(database):
    with closing(sqlite3.connect(database)) as connection:
        assert connection.execute("pragma integrity_check").fetchone() == ("ok",)
        query = """SELECT disp FROM "Odd'System_roms" """
        return {row[0] for row in connection.execute(query)}


def main():
    sd = Path(tempfile.mkdtemp(prefix="cache-build-", dir=BUILD))
    rom = sd / "Roms/Odd'System"
    emu = sd / "Emu/FC"
    emu.mkdir(parents=True)
    rom.mkdir(parents=True)
    (emu / "config.json").write_text(json.dumps({
        "label": "Build test", "rompath": "../../Roms/Odd'System", "extlist": "nes",
        "imgpath": "../../Roms/Odd'System/Imgs",
    }), encoding="utf-8")
    (rom / "one.nes").write_bytes(b"")
    (rom / "Sets").mkdir()
    (rom / "Sets/two.nes").write_bytes(b"")
    run(sd, "--system", "Build test")
    database = rom / "Odd'System_cache6.db"
    assert database.exists() and names(database) == {"Sets", "one", "two"}
    (rom / "new.nes").write_bytes(b"")
    run(sd, "--refresh-caches")
    assert not database.exists()
    run(sd, "--system", "Build test")
    assert names(database) == {"Sets", "one", "two", "new"}
    # A power-cut truncation must repair itself without Refresh Roms.
    database.write_bytes(database.read_bytes()[:100])
    run(sd, "--system", "Build test")
    assert names(database) == {"Sets", "one", "two", "new"}
    before = database.read_bytes()
    run(sd, "--system", "Build test")
    assert database.read_bytes() == before

    # Interrupted unique builds and SQLite sidecars are removed under the lock.
    stale = [Path(str(database) + suffix) for suffix in (
        ".building.999999.0", ".building.999999.0-journal",
        ".building.999999.1-wal", ".building.999999.1-shm",
    )]
    unrelated = rom / "Other_cache6.db.building.999999.0"
    for path in stale:
        path.write_bytes(b"interrupted build")
    unrelated.write_bytes(b"another cache")
    run(sd, "--rebuild")
    assert all(not path.exists() for path in stale)
    assert unrelated.read_bytes() == b"another cache"
    assert names(database) == {"Sets", "one", "two", "new"}
    before = database.read_bytes()
    unrelated.unlink()

    temporary = Path(str(database) + ".building")
    temporary.write_bytes(b"owned by another builder")
    run(sd, "--rebuild", expected=4)
    assert database.read_bytes() == before
    assert temporary.read_bytes() == b"owned by another builder"
    temporary.unlink()
    # A read-only ROM folder cannot take the new cache, so the old one must stay.
    # (A read-only cache file alone does not stop rename() on Linux.) Skipped as
    # root, where the kernel ignores the permission bits and the write would succeed.
    if os.geteuid() != 0:
        os.chmod(rom, stat.S_IREAD | stat.S_IEXEC)
        try:
            run(sd, "--rebuild", expected=4)
        finally:
            os.chmod(rom, stat.S_IRWXU)
        assert database.read_bytes() == before and not temporary.exists()
    # A full arcade-sized directory must exceed the former 10,000-entry cap.
    for index in range(10001):
        (rom / f"arcade{index:05d}.nes").write_bytes(b"")
    run(sd, "--rebuild")
    rebuilt = names(database)
    assert len(rebuilt) == 10005
    assert {"arcade00000", "arcade10000", "Sets", "one", "two", "new"} <= rebuilt
    print("Cache build passed: missing cache, nested scan, quoted name, refresh, "
          "integrity, stale-build cleanup, >10,000 entries and failed-publication preservation")


if __name__ == "__main__":
    main()
