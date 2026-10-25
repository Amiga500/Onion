# SPDX-License-Identifier: GPL-3.0-only
"""Footers in the Onion theme (tests/test_frame.c): stock's layout exactly as
v1.0.9 drew it, and the alternative with the page counter at x 20 and the
hints at the right, B's before A's. An empty .mainuiFooterAltLayout in the
configuration folder selects the alternative; it changes the footer only."""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
result = subprocess.run([str(BUILD / "fixture-frame"), str(ONION_THEME)], cwd=ROOT,
                        check=True, timeout=30, capture_output=True, text=True)
print(result.stdout.strip())

OUT = Path(tempfile.mkdtemp(prefix="footer-layout-", dir=BUILD))
SD = OUT / "sd"
CONFIG = SD / ".tmp_update/config"
for folder in ("Emu/FC", "Roms/FC", ".tmp_update/config"):
    (SD / folder).mkdir(parents=True)
(SD / "Emu/FC/config.json").write_text(json.dumps(dict(
    label="NES", rompath="../../Roms/FC", extlist="nes")))
(CONFIG / "main-menu.json").write_text(json.dumps({"menu": dict(games=True)}))
for i in range(24):
    (SD / f"Roms/FC/game{i:02}.nes").write_bytes(b"rom")


def shot(actions):
    target = OUT / "shot.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--input", actions, "--snapshot", str(target)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    image = Image.open(target).convert("RGB")
    return image.crop((0, 0, 640, 420)).tobytes(), image.crop((0, 420, 640, 480)).tobytes()


# A ROM list with its counter, and the context menu's OK/CANCEL footer.
for actions in ("EED", "EES"):
    screen, footer = shot(actions)
    (CONFIG / ".mainuiFooterAltLayout").write_bytes(b"")
    alt_screen, alt_footer = shot(actions)
    (CONFIG / ".mainuiFooterAltLayout").unlink()
    assert alt_footer != footer, actions + ": the marker did not change the footer"
    assert alt_screen == screen, actions + ": the marker changed more than the footer"
print("The marker selects the alternative footer and changes nothing else")
