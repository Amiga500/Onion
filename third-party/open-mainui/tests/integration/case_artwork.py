# SPDX-License-Identifier: GPL-3.0-only
"""Theme artwork and fonts (tests/test_artwork.c): artwork reuse and cached
misses, invalidation with the theme, popup background fallback, the text
cache, theme font paths and fonts, and Tweaks' theme overrides."""
from pathlib import Path
import subprocess
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
result = subprocess.run([str(BUILD / "fixture-artwork"), str(ONION_THEME)], cwd=ROOT,
                        check=True, timeout=60, capture_output=True, text=True)
print(result.stdout.strip())
