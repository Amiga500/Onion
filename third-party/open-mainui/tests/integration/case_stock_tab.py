# SPDX-License-Identifier: GPL-3.0-only
"""A main-menu tab Onion's setState leaves in the stock state.json opens, as
stock MainUI opens it.

Onion runs setState at boot for the startup tab set in Tweaks, and its Expert
app (App/Expert_Mode) runs setState 4. Set as a Tweaks X or Y shortcut, keymon
starts that app by killing MainUI, so no return envelope exists. Started from
the Apps list, MainUI returns with an envelope for Apps, but the app has
replaced the Apps state with Expert's. As stock does, MainUI removes the
state once read, so a later start without a new one (after keymon's
GameSwitcher, say) is on Home; a pending launch keeps its state.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="stock-tab-", dir=BUILD))
UI = os.environ.get("MAINUI_TEST_EXE", str(BUILD / "MainUI-dev"))


def tab_state(title, page_type):
    """What setState writes to /tmp/state.json (Onion's write_mainui_state)."""
    return {"list": [
        {"title": 157, "type": 0, "currpos": 0, "pagestart": 0, "pageend": 3},
        {"title": title, "type": page_type, "currpos": 0, "pagestart": 0, "pageend": 5}]}


RECENTS, FAVORITES, GAMES = tab_state(18, 10), tab_state(1, 2), tab_state(2, 1)
EXPERT, APPS, SETTINGS = tab_state(0, 16), tab_state(107, 3), tab_state(15, 7)


def sd_card(name, show_tabs):
    """Home: Recents, Favorites, Games, Expert, Apps, Settings, or without
    Recents and Expert."""
    sd = OUT / name / "sd"
    (sd / ".tmp_update/config").mkdir(parents=True)
    if show_tabs:
        (sd / ".tmp_update/config/.showRecents").write_bytes(b"")
        (sd / ".tmp_update/config/.showExpert").write_bytes(b"")
    for label in ("fMSX", "Other"):
        (sd / f"RApp/{label}").mkdir(parents=True)
        (sd / f"RApp/{label}/config.json").write_text(json.dumps(dict(label=label,
                                                                      launch="launch.sh")))
    (sd / "Emu/NES").mkdir(parents=True)
    (sd / "Emu/NES/config.json").write_text(json.dumps(dict(
        label="NES", rompath="../../Roms/NES", launch="launch.sh", extlist="nes")))
    (sd / "Roms/NES").mkdir(parents=True)
    (sd / "Roms/NES/Game.nes").write_bytes(b"rom")
    (sd / "App/Expert_Mode").mkdir(parents=True)
    (sd / "App/Expert_Mode/config.json").write_text(json.dumps(dict(label="Expert",
                                                                    launch="launch.sh")))
    (sd / "App/Expert_Mode/launch.sh").write_text("#!/bin/sh\n./bin/setState 4\n")
    (sd.parent / "handoff").mkdir()
    return sd


def run(sd, name, *options):
    shot = sd.parent / f"{name}.bmp"
    result = subprocess.run([UI, "--sd-root", str(sd), "--theme", str(ONION_THEME),
                             "--snapshot", str(shot)] + list(options),
                            cwd=ROOT, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (name, result.stderr[-400:])
    return shot.read_bytes()


def restart(sd, name, *options):
    return run(sd, name, "--handoff-dir", str(sd.parent / "handoff"), *options)


def state(sd, value):
    (sd.parent / "handoff/state.json").write_text(json.dumps(value))


sd = sd_card("shown", True)
home = run(sd, "home")
screens = {"recents": ("E", RECENTS), "favorites": ("RE", FAVORITES), "games": ("RRE", GAMES),
           "expert": ("RRRE", EXPERT), "apps": ("RRRRE", APPS)}
shots = {name: run(sd, name, "--input", keys) for name, (keys, _) in screens.items()}
assert len(set(shots.values()) | {home}) == 6

# 1. Each tab setState leaves opens (at boot, the startup tab), with no
# envelope; B goes back to Home with that tab selected. Settings is no tab.
for name, (keys, value) in screens.items():
    state(sd, value)
    assert restart(sd, f"tab-{name}") == shots[name], name
    assert not (sd.parent / "handoff/state.json").exists(), name
    assert restart(sd, f"tab-{name}-again") == home, name
    state(sd, value)
    assert restart(sd, f"tab-{name}-back", "--input", "B") == \
        run(sd, f"home-on-{name}", "--input", keys + "B"), name
state(sd, SETTINGS)
assert restart(sd, "tab-settings") == home

# 2. Recents and Expert hidden on Home: they open all the same.
hidden = sd_card("hidden", False)
for name, value in (("recents", RECENTS), ("expert", EXPERT)):
    state(hidden, value)
    assert restart(hidden, f"hidden-{name}") == shots[name], name

# 3. Started from the Apps list: Expert, though the envelope names Apps. An
# app that leaves the Apps state returns to Apps.
for name, replaced, expected in (("from-apps", True, "expert"), ("other-app", False, "apps")):
    sd = sd_card(name, True)
    handoff = sd.parent / "handoff"
    restart(sd, "launch", "--input", "RRRREE")  # Home, Apps, Expert_Mode
    assert (handoff / "mainui-return.json").exists(), name
    (handoff / "cmd_to_run.sh").unlink()  # the runtime ran it
    if replaced:
        state(sd, EXPERT)
    assert restart(sd, "after") == shots[expected], name

# 4. After a return the launch's state is gone too: a start without an
# envelope, as after GameSwitcher, is on Home. A pending launch keeps it.
sd = sd_card("launched", True)
handoff = sd.parent / "handoff"
restart(sd, "launch", "--input", "RREEE")  # Home, Games, NES, Game.nes
assert (handoff / "state.json").exists() and (handoff / "mainui-return.json").exists()
(handoff / "cmd_to_run.sh").unlink()
restart(sd, "returned")
assert not (handoff / "state.json").exists()
assert restart(sd, "killed") == home
state(sd, GAMES)
(handoff / "cmd_to_run.sh").write_text("pending\n")
restart(sd, "pending")
assert (handoff / "state.json").exists()
print("A tab left by setState opens at boot, after a keymon shortcut and after a launch from "
      "Apps; MainUI's own launch state goes on return")
