# SPDX-License-Identifier: GPL-3.0-only
"""X and Y shortcuts from keymap.json (Tweaks): "fav" on either button adds
the game to Favorites or removes it, and "glo" opens Game List Options for it
from X as from Y, both when the button is released. On a folder, and with no
shortcut or an app or tool (keymon's), X stays B and Y stays as before."""
import json
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme, unlink_if_exists  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="button-shortcuts-", dir=BUILD))
SD = OUT / "sd"
HANDOFF = OUT / "handoff"
HANDOFF.mkdir()
(SD / ".tmp_update/config").mkdir(parents=True)
KEYMAP = SD / ".tmp_update/config/keymap.json"
MENU = SD / ".tmp_update/config/main-menu.json"
FLAG = HANDOFF / "launch_alt"
COMMAND = HANDOFF / "cmd_to_run.sh"
FAVORITES = SD / "Roms/favourite.json"
RECENT = SD / "Roms/recentlist.json"
(SD / "Emu/C").mkdir(parents=True)
(SD / "Emu/C/config.json").write_text(json.dumps(dict(
    label="Console", rompath="../../Roms/C", launch="launch.sh", extlist="nes")))
(SD / "Roms/C/Sub").mkdir(parents=True)
(SD / "Roms/C/Sub/Inner.nes").write_bytes(b"rom")
for i in range(3):
    (SD / f"Roms/C/G{i:02}.nes").write_bytes(b"rom")
SAVED = json.dumps(dict(label="G01", rompath="/mnt/SDCARD/Roms/C/G01.nes",
                        launch="/mnt/SDCARD/Emu/C/launch.sh", type=5)) + "\n"


def run(actions):
    """The screen after the actions, and the command they left, if any."""
    unlink_if_exists(COMMAND)
    unlink_if_exists(HANDOFF / "mainui-return.json")
    unlink_if_exists(HANDOFF / "state.json")
    shot = OUT / "shot.bmp"
    subprocess.run([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
                    "--input", actions, "--snapshot", str(shot), "--handoff-dir", str(HANDOFF)],
                   cwd=ROOT, check=True, timeout=30, capture_output=True)
    unlink_if_exists(SD / "appconfigs/romwinidx.json")
    return shot.read_bytes(), COMMAND.read_bytes() if COMMAND.exists() else None


def favorites():
    if not FAVORITES.exists():
        return []
    return sorted(json.loads(line)["label"] for line in FAVORITES.read_text().splitlines()
                  if line.strip())


def shortcuts(x=None, y=None):
    keymap = {}
    if x is not None:
        keymap["mainui_button_x"] = x
    if y is not None:
        keymap["mainui_button_y"] = y
    KEYMAP.write_text(json.dumps(keymap))


# Home is Favorites, Games. In the console's list the Sub folder comes first,
# then G00. A and B as references.
MENU.write_text(json.dumps({"menu": dict(favorites=True, games=True)}))
FAVORITES.write_text(SAVED)
_, by_a = run("REEDA")
assert by_a and b"G00.nes" in by_a, by_a
back_from_game, _ = run("REEDB")
# Screens where a game cannot be acted on: the main menu, the console grid,
# the list on its folder, the open context menu, Game Info.
ELSEWHERE = ["", "RE", "REE", "REEDS", "REEDR"]
screens = {place: run(place)[0] for place in ELSEWHERE}

# No shortcut, an empty one, or keymon's: X is B, on the press.
for x in (None, "", "app:Search", "tool:clean_logs"):
    shortcuts(x=x)
    assert run("REEDX") == (back_from_game, None), x
    assert run("REED+X")[0] == back_from_game, x
    assert favorites() == ["G01"], x

# Favorite on X: added, then removed; on the release, not the press.
for button in "XY":
    shortcuts(**{button.lower(): "fav"})
    assert run("REED+" + button)[1] is None and favorites() == ["G01"], button
    assert run("REED" + button)[1] is None and favorites() == ["G00", "G01"], button
    assert run("REED" + button)[1] is None and favorites() == ["G01"], button
    # Twice in one run: added and removed again.
    assert run("REED" + button * 2)[1] is None and favorites() == ["G01"], button
# With a shortcut a button does only that: elsewhere nothing, not B or A.
# Game Info takes the Favorite shortcut (below), and nothing else.
for value in ("fav", "glo"):
    shortcuts(x=value, y=value)
    for place in ELSEWHERE if value == "glo" else ELSEWHERE[:-1]:
        for button in "XY":
            assert run(place + button) == (screens[place], None), (value, place, button)
    assert favorites() == ["G01"] and not FLAG.exists(), value

# Game List Options on X: launched with keymon's flag set, as Y does.
shortcuts(x="glo")
unlink_if_exists(FLAG)
assert run("REEDX")[1] == by_a and FLAG.exists()
FLAG.unlink()
assert run("REED+X")[1] is None and not FLAG.exists()
# Both buttons, either action, the same one on both included.
shortcuts(x="glo", y="glo")
assert run("REEDX")[1] == by_a and FLAG.exists()
assert run("REEDY")[1] == by_a and FLAG.exists()
FLAG.unlink()
shortcuts(x="glo", y="fav")
assert run("REEDY")[1] is None and favorites() == ["G00", "G01"] and not FLAG.exists()
assert run("REEDX")[1] == by_a and FLAG.exists()
FLAG.unlink()
FAVORITES.write_text(SAVED)

# Game Info: the game shown is added and removed; Game Info stays, its
# star following.
for button in "XY":
    shortcuts(**{button.lower(): "fav"})
    info, command = run("REEDR" + button)
    assert command is None and favorites() == ["G00", "G01"], button
    assert info != screens["REEDR"], button  # the star
    FAVORITES.write_text(SAVED)
    assert run("REEDR" + button * 2) == (screens["REEDR"], None) and favorites() == ["G01"]
# From the Favorites list: removed, its row is gone, so Game Info closes on
# the list, as it is without the game.
shortcuts(x="fav")
assert run("ERX")[1] is None and favorites() == []
removed, _ = run("ERX")
FAVORITES.write_text("")
assert removed == run("E")[0]
FAVORITES.write_text(SAVED)

# Favorites: X removes the selected Favorite, and the list closes up.
shortcuts(x="fav")
assert run("EX")[1] is None and favorites() == []
FAVORITES.write_text(SAVED)
# Recents: Y adds the selected game to Favorites.
MENU.write_text(json.dumps({"menu": dict(recents=True)}))
RECENT.write_text("".join(json.dumps(dict(
    label=f"G{i:02}", rompath=f"/mnt/SDCARD/Roms/C/G{i:02}.nes",
    launch="/mnt/SDCARD/Emu/C/launch.sh", type=5)) + "\n" for i in (2, 0)))
recents_before = RECENT.read_text()
shortcuts(y="fav")
assert run("EY")[1] is None and favorites() == ["G01", "G02"]
assert RECENT.read_text() == recents_before
RECENT.unlink()

# The launch X starts from its release is the screen's in front, as Y's:
# also as the first action after a start that restored a list, right after
# a list reload, after entering Favorites or Recents, and on Search
# results. Before, it described the screen of the last key, or a freed list.
FAVORITES.write_text(SAVED)


def handoff(actions, text=None, keep=False):
    """cmd_to_run.sh, state.json and mainui-return.json after the actions."""
    names = ("cmd_to_run.sh", "state.json", "mainui-return.json")
    if not keep:
        for name in names:
            unlink_if_exists(HANDOFF / name)
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--input", actions, "--snapshot", str(OUT / "shot.bmp"),
               "--handoff-dir", str(HANDOFF)]
    if text is not None:
        command += ["--text", text]
    subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    unlink_if_exists(SD / "appconfigs/romwinidx.json")
    files = {name: (HANDOFF / name).read_bytes() for name in names if (HANDOFF / name).exists()}
    unlink_if_exists(RECENT)
    return files


def same_launch(actions, text=None, restore=None, setup=lambda: None):
    """X with glo and Y with glo after the same actions launch the same."""
    results = []
    for x, y, button in (("glo", None, "X"), (None, "glo", "Y")):
        shortcuts(x=x, y=y)
        setup()
        if restore:  # A launch, then a start that comes back to its list
            handoff(restore)
            (HANDOFF / "cmd_to_run.sh").unlink()
        files = handoff(actions + button, text, keep=bool(restore))
        assert "cmd_to_run.sh" in files, (actions, button)
        results.append(files)
    assert results[0] == results[1], (actions, results)
    return results[0]


MENU.write_text(json.dumps({"menu": dict(favorites=True, games=True)}))
assert b"G01.nes" in same_launch("", restore="REEDDA")["cmd_to_run.sh"]
assert b"G00.nes" in same_launch("REEDT")["cmd_to_run.sh"]  # Start reloads the list
assert b"G01.nes" in same_launch("E")["cmd_to_run.sh"]  # Favorites
# Search results: Select, Search, "G0" typed, Start; the first result.
search = same_launch("REEDSDDDET", text="G01")
assert b"G01.nes" in search["cmd_to_run.sh"] and b"G0" in search["mainui-return.json"]
MENU.write_text(json.dumps({"menu": dict(recents=True)}))
recents = same_launch("E", setup=lambda: RECENT.write_text(SAVED))
assert b"G01.nes" in recents["cmd_to_run.sh"]

# Holding X while a menu or the keyboard opens: that screen owns the
# release, which changes no Favorite and launches nothing under it.
MENU.write_text(json.dumps({"menu": dict(favorites=True, games=True)}))
FAVORITES.write_text(SAVED)
menu, _ = run("REEDS")
keyboard, _ = run("REEDSDDDE")
for value in ("fav", "glo"):
    shortcuts(x=value)
    assert run("REED+XS-X") == (menu, None) and favorites() == ["G01"], value
    assert run("REED+XSDDDE-X") == (keyboard, None) and favorites() == ["G01"], value
print("X and Y shortcuts: fav and glo on either button, on release; X stays B otherwise")
