# SPDX-License-Identifier: GPL-3.0-only
"""Refresh all roms goes on past a console whose ROM folder does not exist.

It stopped at the first such console with "ROM folder is missing" and
refreshed nothing; stock skips those consoles.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="refresh-missing-", dir=BUILD))
SD = OUT / "sd"
for name in ("Emu/FC", "Emu/GBA", "Roms/FC", ".tmp_update/config"):
    (SD / name).mkdir(parents=True)
(SD / "Emu/FC/config.json").write_text(
    json.dumps(dict(label="Famicom", rompath="../../Roms/FC", extlist="nes")))
(SD / "Emu/GBA/config.json").write_text(
    json.dumps(dict(label="Game Boy Advance", rompath="../../Roms/GBA", extlist="gba")))
(SD / "Roms/FC/one.nes").write_bytes(b"fixture")
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": {"games": True}}))
EXE = os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev"))


def run(name, actions):
    result = subprocess.run(
        [EXE, "--sd-root", str(SD), "--theme", str(ONION_THEME), "--fallback", str(ONION_THEME),
         "--snapshot", str(OUT / f"{name}.bmp"), "--input", actions],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    return result.stderr


cache = SD / "Roms/FC/FC_cache6.db"
run("enter", "EE")
assert cache.exists(), "entering NES builds its cache"
log = run("refresh-all", "SE")
assert "ROM folder is missing" not in log, log
assert "Refresh all roms failed" not in log, log
assert not cache.exists(), "Refresh all roms removed the NES cache"
assert not (SD / "Roms/GBA").exists(), "the missing folder is not created"
print("Refresh all roms skips a console without a ROM folder")
