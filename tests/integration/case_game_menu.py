# SPDX-License-Identifier: GPL-3.0-only
"""game-menu.json reorders and hides the game-list context menus.

Without the file, with a malformed one and with the default order the menus
are drawn exactly as before. The selection still decides what a menu can
offer, and every action works as it did from its new place."""
import json
from pathlib import Path
import subprocess
import tempfile
from env import BUILD, ONION_THEME, require_onion_theme, unlink_if_exists  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="game-menu-", dir=BUILD))
SD = OUT / "sd"
HANDOFF = OUT / "handoff"
HANDOFF.mkdir()
(SD / ".tmp_update/config").mkdir(parents=True)
(SD / ".tmp_update/config/main-menu.json").write_text(
    json.dumps({"menu": dict(recents=True, favorites=True, games=True)}))
GAME_MENU = SD / ".tmp_update/config/game-menu.json"
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
DEFAULT = {"roms": ["launch", "favorite", "delete", "search", "refresh"],
           "favorites": ["move", "create_folder", "sort", "remove_favorite", "rename_folder",
                         "delete_folder"],
           "recents": ["launch", "remove_recent", "clear_recents"],
           "search_results": ["launch", "favorite"]}
# Screens with a game-list context menu open: from the main menu (Recents,
# Favorites, Games), the console's folder, a game, Favorites' game and
# folder, Recents, and Search results for "G01".
MENUS = {"rom-folder": ("RREES", None), "rom-game": ("RREEDS", None),
         "favorite-folder": ("RES", None), "favorite-game": ("REDS", None),
         "recent": ("ES", None), "search": ("RREEDSDDDETS", "G01")}
FOLDERS = "".join(json.dumps(row) + "\n" for row in (
    dict(schema=1, generation=1), dict(kind="folder", id="a", parent="", name="First", order=0)))


def run(actions, text=None, game_menu=None):
    """The screen after the actions, and the launch command they left."""
    if game_menu is None:
        unlink_if_exists(GAME_MENU)
    else:
        GAME_MENU.write_text(game_menu if isinstance(game_menu, str) else json.dumps(game_menu))
    FAVORITES.write_text(SAVED)
    RECENT.write_text(SAVED)
    (SD / "Roms/favourite-folders.json").write_text(FOLDERS)
    for name in ("cmd_to_run.sh", "mainui-return.json", "state.json"):
        unlink_if_exists(HANDOFF / name)
    shot = OUT / "shot.bmp"
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--input", actions, "--snapshot", str(shot), "--handoff-dir", str(HANDOFF)]
    if text is not None:
        command += ["--text", text]
    result = subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    unlink_if_exists(SD / "appconfigs/romwinidx.json")
    return (shot.read_bytes(), COMMAND.read_bytes() if COMMAND.exists() else None,
            result.stderr.decode())


def screens(game_menu=None):
    return {name: run(actions, text, game_menu)[0] for name, (actions, text) in MENUS.items()}


before = screens()
# A Favorites folder exists for its menu, and every screen shows a menu.
assert len(set(before.values())) == len(before)
# The default order, a malformed file and one without sections: as before.
assert screens(DEFAULT) == before
assert screens("{\"roms\": [") == before
assert screens({"Roms": ["delete"], "other": []}) == before
assert "game-menu.json is not a JSON object" in run("", game_menu="[1]")[2]
assert "game-menu.json" not in run("", game_menu=DEFAULT)[2]

# ROM lists: Refresh first, then Launch; Favorite, Delete and Search hidden.
menu = {"roms": ["refresh", "bogus", "launch", "refresh"]}
changed = screens(menu)
assert changed["rom-game"] != before["rom-game"]
# Search results are reached through the ROM menu; the others are unchanged.
assert all(changed[k] == before[k] for k in ("favorite-folder", "favorite-game", "recent"))
assert run("RREEDSE", game_menu=menu)[1] is None  # Refresh, no launch
assert b"G00.nes" in run("RREEDSDE", game_menu=menu)[1]  # Launch, second
assert b"G00.nes" in run("RREEDSDDDE", game_menu=menu)[1]  # wraps: no third row
# A folder offers no Launch: Refresh alone.
assert run("RREES", game_menu=menu)[0] == run("RREES", game_menu={"roms": ["refresh"]})[0]
# Search first; then on its results Favorite only, which still adds or removes.
menu = {"roms": ["search"], "search_results": ["favorite"]}
_, launched, _ = run("RREEDSETSE", text="G00", game_menu=menu)
assert launched is None
assert [json.loads(line)["label"] for line in FAVORITES.read_text().splitlines()] == \
    ["G01", "G00"], FAVORITES.read_text()

# Favorites: Remove first, Create folder hidden; a folder shows only Rename.
menu = {"favorites": ["remove_favorite", "rename_folder", "move"]}
changed = screens(menu)
assert changed["favorite-game"] != before["favorite-game"]
assert changed["rom-game"] == before["rom-game"] and changed["recent"] == before["recent"]
# Its first row asks to remove G01, as Remove Favorite did from its old place.
assert run("REDSE", game_menu=menu)[0] == run("REDSDDDE")[0]

# Recents: an empty list means no menu, so Select does nothing.
menu = {"recents": []}
assert run("ES", game_menu=menu)[0] == run("E", game_menu=menu)[0]
assert run("REDS", game_menu=menu)[0] == before["favorite-game"]
print("game-menu.json reorders and hides game-list menus; without it they are unchanged")
