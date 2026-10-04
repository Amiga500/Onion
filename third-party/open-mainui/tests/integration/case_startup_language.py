# SPDX-License-Identifier: GPL-3.0-only
"""The language saved in system.json is used from startup, not only after it is
chosen again in Settings. Only a plain file name is accepted."""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="startup-language-", dir=BUILD))
SD = OUT / "sd"
(SD / "Emu").mkdir(parents=True)
(SD / "miyoo/app/lang").mkdir(parents=True)
(SD / "miyoo/app/lang/xx.lang").write_text(
    "\ufeff" + json.dumps({"lang": "Test", "15": "XXXXXXXXXXXX"}), encoding="utf-8")
(OUT / "evil.lang").write_text(json.dumps({"lang": "Evil", "15": "EEEEEEEEEEEE"}))


def settings(language):
    (SD / "system.json").write_text(json.dumps({"language": language}))
    output = OUT / f"{len(list(OUT.glob('*.bmp')))}.bmp"
    result = subprocess.run(
        [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
         "--fallback", str(ONION_THEME), "--input", "RRRE", "--snapshot", str(output)],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    # The Settings title row is the only place label 15 appears.
    return Image.open(output).convert("RGB").crop((0, 0, 640, 60)).tobytes(), result.stderr


english, _ = settings("en.lang")
translated, _ = settings("xx.lang")
assert translated != english
for bad in ("../../evil.lang", "missing.lang", "notalang.txt"):
    title, stderr = settings(bad)
    assert title == english, bad
    assert "using built-in English" in stderr, (bad, stderr)

# The same file name in miyoo/app/lang and lang_backup: startup and a selection
# in Settings both use miyoo/app/lang, the folder stock reads.
(SD / "miyoo/app/lang_backup").mkdir()
(SD / "miyoo/app/lang/dup.lang").write_text(json.dumps({"lang": "Dup", "15": "PPPPPPPPPPPP"}))
(SD / "miyoo/app/lang_backup/dup.lang").write_text(json.dumps({"lang": "Dup", "15": "BBBBBBBBBBBB"}))


def title(language, inputs, name):
    (SD / "system.json").write_text(json.dumps({"language": language}))
    output = OUT / (name + ".bmp")
    result = subprocess.run(
        [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
         "--fallback", str(ONION_THEME), "--input", inputs, "--snapshot", str(output)],
        cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    return Image.open(output).convert("RGB").crop((0, 0, 640, 60)).tobytes()


at_startup = title("dup.lang", "RRRE", "dup-startup")
# Settings, Change language row, open the list (Dup is selected), choose it again.
after_select = title("dup.lang", "RRREDDDDEE", "dup-selected")
assert at_startup == after_select
assert at_startup != english

# A language change that switches the built-in fallback font (English <-> other)
# restarts MainUI when the theme relies on that fallback: the handoff resumes
# Settings, as stock reloads its fonts immediately.
import shutil  # noqa: E402
theme = OUT / "missing-font-theme"
shutil.copytree(ONION_THEME / "skin", theme / "skin")
config = json.loads((ONION_THEME / "config.json").read_text())
for section in config.values():
    if isinstance(section, dict) and "font" in section:
        section["font"] = "missing.ttf"
(theme / "config.json").write_text(json.dumps(config))
restart_sd = OUT / "restart-sd"
(restart_sd / "Emu").mkdir(parents=True)
(restart_sd / "system.json").write_text(json.dumps({"language": "en.lang"}))
handoff = OUT / "handoff"
handoff.mkdir()


def run(inputs, name, theme_dir=theme):
    args = [str(BUILD / "MainUI-dev"), "--sd-root", str(restart_sd), "--theme", str(theme_dir),
            "--fallback", str(ONION_THEME), "--handoff-dir", str(handoff),
            "--snapshot", str(OUT / (name + ".bmp"))]
    if inputs:
        args += ["--input", inputs]
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=30)


# Settings (RRRE), Change language row (DDDD), open (E), next language (D), choose (E).
result = run("RRREDDDDEDE", "restart")
assert result.returncode == 0, result.stderr
chosen = json.loads((restart_sd / "system.json").read_text())["language"]
assert not chosen.startswith("en.lang"), chosen
returned = json.loads((handoff / "mainui-return.json").read_text())
assert returned["committed"] and returned["resume"]["section"] == 5, returned
assert not (handoff / "cmd_to_run.sh").exists()
# The next start takes the return and opens Settings on the language row.
resumed = run("", "resumed")
assert resumed.returncode == 0, resumed.stderr
assert not (handoff / "mainui-return.json").exists()
settings = run("RRRE", "settings-direct")
assert Image.open(OUT / "resumed.bmp").tobytes() == Image.open(OUT / "settings-direct.bmp").tobytes() or \
    Image.open(OUT / "resumed.bmp").crop((0, 0, 640, 60)).tobytes() == \
    Image.open(OUT / "settings-direct.bmp").crop((0, 0, 640, 60)).tobytes()

# No restart when the theme's own fonts load: the change applies in place.
(restart_sd / "system.json").write_text(json.dumps({"language": "en.lang"}))
result = run("RRREDDDDEDE", "no-restart", ONION_THEME)
assert result.returncode == 0, result.stderr
assert not (handoff / "mainui-return.json").exists()
print("startup_language: ok")
