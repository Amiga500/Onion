# SPDX-License-Identifier: GPL-3.0-only
"""Startup with a damaged SD card.

An unusable Emu starts with an empty Games list; a missing font exits and
names the font instead of printing a stale SDL error.
"""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="startup-fallbacks-", dir=BUILD))
EXE = str(BUILD / "MainUI-dev")


def start(sd, theme, fallback, name):
    output = OUT / (name + ".bmp")
    result = subprocess.run(
        [EXE, "--sd-root", str(sd), "--theme", str(theme), "--fallback", str(fallback),
         "--systems", "--snapshot", str(output)],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    return result, output


# Emu is a file: startup continues with an empty Games list and says why.
bad = OUT / "emu-file"
bad.mkdir()
(bad / "Emu").write_text("not a folder")
result, output = start(bad, ONION_THEME, ONION_THEME, "emu-file")
assert result.returncode == 0, result.stderr
assert output.is_file()
assert "Cannot open systems folder Emu: Not a directory" in result.stderr, result.stderr
assert "starting with an empty Games list" in result.stderr, result.stderr

# Emu as a symlink is not followed; startup still continues.
linked = OUT / "emu-link"
linked.mkdir()
(linked / "Emu").symlink_to(bad.resolve(), target_is_directory=True)
result, output = start(linked, ONION_THEME, ONION_THEME, "emu-link")
assert result.returncode == 0, result.stderr
assert "Emu is a symlink" in result.stderr, result.stderr
assert "starting with an empty Games list" in result.stderr, result.stderr

# An unreadable favourite.json shows a message instead of quitting.
favorites = OUT / "favorites-folder"
(favorites / "Emu").mkdir(parents=True)
(favorites / "Roms/favourite.json").mkdir(parents=True)
output = OUT / "favorites-folder.bmp"
result = subprocess.run(
    [EXE, "--sd-root", str(favorites), "--theme", str(ONION_THEME), "--fallback",
     str(ONION_THEME), "--input", "E", "--snapshot", str(output)],
    cwd=ROOT, capture_output=True, text=True, timeout=30)
assert result.returncode == 0, result.stderr
assert output.is_file()
assert "Cannot read saved library (favourite.json)" in result.stderr, result.stderr

# A theme path in system.json that cannot be represented uses the stock theme.
long_theme = OUT / "long-theme"
(long_theme / "Emu").mkdir(parents=True)
(long_theme / "system.json").write_text(json.dumps({"theme": "x" * 8000}))
output = OUT / "long-theme.bmp"
result = subprocess.run(
    [EXE, "--sd-root", str(long_theme), "--fallback", str(ONION_THEME), "--systems",
     "--snapshot", str(output)],
    cwd=ROOT, capture_output=True, text=True, timeout=30)
assert result.returncode == 0, result.stderr
assert output.is_file()
assert "Ignoring unusable theme path" in result.stderr, result.stderr

# Fallback font by language, as in stock: wqy-microhei.ttc for a non-English
# language, Exo 2 for English. With only WenQuanYi available, a missing theme
# font still starts in Chinese but not in English.
cjk = OUT / "cjk-only"
shutil.copytree(ONION_THEME / "skin", cjk / "skin")
config = json.loads((ONION_THEME / "config.json").read_text())
for section in config.values():
    if isinstance(section, dict) and "font" in section:
        section["font"] = "missing.ttf"
(cjk / "config.json").write_text(json.dumps(config))
shutil.copy(ONION_THEME / "wqy-microhei.ttc", cjk / "wqy-microhei.ttc")
language_sd = OUT / "language-sd"
(language_sd / "Emu").mkdir(parents=True)
for language, code in (("ch.lang", 0), ("en.lang", 3)):
    (language_sd / "system.json").write_text(json.dumps({"language": language}))
    result, output = start(language_sd, cjk, cjk, "cjk-" + language)
    assert result.returncode == code, (language, result.returncode, result.stderr)
assert "Exo-2-Bold-Italic.ttf" in result.stderr, result.stderr

# No font anywhere: a clean exit that names the font, not an SDL error.
fontless = OUT / "fontless"
shutil.copytree(ONION_THEME / "skin", fontless / "skin")
shutil.copy(ONION_THEME / "config.json", fontless / "config.json")
empty = OUT / "empty"
(empty / "Emu").mkdir(parents=True)
result, output = start(empty, fontless, fontless, "fontless")
assert result.returncode == 3, (result.returncode, result.stderr)
assert "Cannot open font" in result.stderr, result.stderr
assert "colorkey" not in result.stderr, result.stderr
assert not output.exists()
print("startup_fallbacks: ok")
