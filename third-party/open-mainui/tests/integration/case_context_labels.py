# SPDX-License-Identifier: GPL-3.0-only
"""Context menu labels in the Onion theme's font and popup: a label that fits
is drawn exactly as v1.0.9 drew it, also within the 5 px kept clear on the
right; one cut off at the popup's edge ends in "..." instead, cut after a
whole character (tests/test_dialogs.c). With .noContextMenuEllipsis it is cut
off at the edge again, as in v1.0.9."""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
result = subprocess.run([str(BUILD / "fixture-dialogs"), str(ONION_THEME)], cwd=ROOT,
                        check=True, timeout=30, capture_output=True, text=True)
print(result.stdout.strip())

# The main menu's context menu in French, with the reported long label.
OUT = Path(tempfile.mkdtemp(prefix="context-labels-", dir=BUILD))
SD = OUT / "sd"
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": dict(games=True)}))
(SD / "miyoo/app/lang").mkdir(parents=True)
(SD / "system.json").write_text(json.dumps({"language": "fr.lang"}))
french = json.loads((ONION_THEME / "lang/fr.lang").read_text(encoding="utf-8-sig"))
french["54"] = "Rafraîchir toutes les roms"
(SD / "miyoo/app/lang/fr.lang").write_text(json.dumps(french, ensure_ascii=False),
                                           encoding="utf-8")


def shot(name):
    target = OUT / f"{name}.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--input", "S", "--snapshot", str(target)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    return Image.open(target).convert("RGB")


def text_at_edge(image):
    """Light text pixels in the first row's last 5 px before the 320 px edge."""
    return sum(1 for x in range(315, 320) for y in range(0, 60)
               if min(image.getpixel((x, y))) > 200)


shortened = shot("ellipsis")
assert text_at_edge(shortened) == 0, "a shortened label reaches into the last 5 px"
(SD / ".tmp_update/config/.noContextMenuEllipsis").write_bytes(b"")
cut_off = shot("no-ellipsis")
assert text_at_edge(cut_off) > 0, "with .noContextMenuEllipsis the label runs to the edge"
assert shortened.tobytes() != cut_off.tobytes()
print("The French label ends in dots, and runs to the edge with .noContextMenuEllipsis")
