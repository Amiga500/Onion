# SPDX-License-Identifier: GPL-3.0-only
"""Real launch handoff writes Recent independently of menu visibility."""
import json
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


for name, recents, legacy in (
    ("absent", None, False),
    ("disabled", False, False),
    ("disabled-legacy", False, True),
    ("enabled", True, True),
):
    with tempfile.TemporaryDirectory(prefix="recent-gating-" + name + "-", dir=BUILD) as temp:
        sd = Path(temp)
        config = sd / ".tmp_update/config"
        menu = {"games": True}
        if recents is not None:
            menu["recents"] = recents
        write(config / "main-menu.json", json.dumps({"menu": menu}))
        marker = config / ".showRecents"
        if legacy:
            write(marker, "1")
        write(sd / "Emu/FC/config.json", json.dumps({
            "label": "Host", "launch": "launch.sh",
            "rompath": "../../Roms/FC", "extlist": "nes",
        }))
        write(sd / "Emu/FC/launch.sh", "#!/bin/sh\nexit 0\n")
        write(sd / "Roms/FC/Collection/It's first.nes", "")
        write(sd / "Roms/FC/Collection/Two.nes", "")
        handoff = sd / "handoff"
        handoff.mkdir()
        subprocess.run([
            str(BUILD / "MainUI-dev"), "--sd-root", str(sd),
            "--theme", str(ONION_THEME), "--system", "Host",
            "--input", "EDDE", "--snapshot", str(sd / "launch.bmp"),
            "--handoff-dir", str(handoff),
        ], cwd=ROOT, check=True, timeout=30)
        assert (handoff / "cmd_to_run.sh").is_file(), name
        envelope = json.loads((handoff / "mainui-return.json").read_text(encoding="utf-8"))
        assert envelope["committed"], name
        recent = sd / "Roms/recentlist.json"
        rows = [json.loads(line) for line in recent.read_text(encoding="utf-8").splitlines()]
        assert rows == [{
            "label": "Two", "rompath": "/mnt/SDCARD/Emu/FC/../../Roms/FC/Collection/Two.nes",
            "launch": "/mnt/SDCARD/Emu/FC/launch.sh", "type": 5,
            "imgpath": "/mnt/SDCARD/Emu/FC/Imgs/Two.png",
        }], (name, rows)
        assert marker.exists() == legacy, name
        assert json.loads((config / "main-menu.json").read_text(encoding="utf-8")) == {"menu": menu}
print("Recent launch visibility scenarios passed")
