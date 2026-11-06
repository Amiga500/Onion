# SPDX-License-Identifier: GPL-3.0-only
"""Saved Games: a RetroArch game's save states, in Onion's
Saves/CurrentProfile/states/<core name>/, by the core its launcher names.
Left on the game in a list, or the media menu in Game Info, shows their
pictures, Previous Session followed by numbered slots ascending, then Auto
Save when enabled; initially selects the rightmost available entry. A puts
chosen numbered slots into Auto, as resume_mode says, and launches
the game with Onion's force_auto_load_state flag, as GameSwitcher resumes a
state."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
subprocess.run([str(BUILD / "fixture-save_states")], check=True, capture_output=True)
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="saved-games-", dir=BUILD))
SD = OUT / "sd"
CONFIG = SD / ".tmp_update/config"
CONFIG.mkdir(parents=True)
HANDOFF = OUT / "handoff"
(SD / "Emu/SFC").mkdir(parents=True)
(SD / "Emu/SFC/config.json").write_text(json.dumps(dict(
    label="SNES", rompath="../../Roms/SFC", launch="launch.sh", extlist="sfc")))
(SD / "Emu/SFC/launch.sh").write_text(
    "#!/bin/sh\ncd /mnt/SDCARD/RetroArch/\nHOME=/mnt/SDCARD/RetroArch/ ./retroarch -v -L "
    ".retroarch/cores/test_libretro.so \"$1\"\n")
(SD / "RetroArch/.retroarch/cores").mkdir(parents=True)
(SD / "RetroArch/.retroarch/cores/test_libretro.info").write_text('corename = "Test Core"\n')
ROMS = SD / "Roms/SFC"
ROMS.mkdir(parents=True)
LONG = "Gamma " + "with a title far too long for its row " * 3
for name in ("Alpha", "Beta", LONG):
    (ROMS / (name + ".sfc")).write_bytes(b"rom")
STATES = SD / "Saves/CurrentProfile/states/Test Core"
AUTO, ONE, FOUR = (90, 90, 160), (200, 60, 60), (60, 200, 60)


def states():
    """Alpha's and Gamma's states afresh: Auto, slots 1 and 4 with pictures,
    7 without; Beta has none."""
    shutil.rmtree(STATES, ignore_errors=True)
    STATES.mkdir(parents=True)
    for game in ("Alpha", LONG):
        (STATES / f"{game}.state.auto").write_text("auto")
        Image.new("RGB", (320, 240), AUTO).save(STATES / f"{game}.state.auto.png")
        for slot, colour in ((1, ONE), (4, FOUR)):
            (STATES / f"{game}.state{slot}").write_text(f"slot{slot}")
            Image.new("RGB", (320, 240), colour).save(STATES / f"{game}.state{slot}.png")
        (STATES / f"{game}.state7").write_text("slot7")


def read(name):
    path = STATES / name
    return path.read_text() if path.exists() else None


def shot(actions, menu=None, handoff=True, blocked=False, left=True, auto=True,
         layout="overlay", system="SNES"):
    """With left and auto, game-menu.json turns on Left's Saved Games and the
    Auto Save among them, both off by default; layout sets saved_games's
    layout: overlay for the checks of the full-screen pictures, None for the
    default, framed."""
    (CONFIG / "game-menu.json").unlink(missing_ok=True)
    if left or auto or layout:
        menu = dict(menu or {})
        menu["saved_games"] = {**({"left_shortcut": True} if left else {}),
                               **({"show_auto": True} if auto else {}),
                               **({"layout": layout} if layout else {}),
                               **menu.get("saved_games", {})}
    if menu is not None:
        (CONFIG / "game-menu.json").write_text(json.dumps(menu))
    shutil.rmtree(HANDOFF, ignore_errors=True)
    HANDOFF.mkdir()
    if blocked:  # the launch cannot be published
        (HANDOFF / "cmd_to_run.sh").mkdir()
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--config-dir", str(CONFIG), "--system", system, "--input", actions,
               "--snapshot", str(OUT / "shot.bmp")]
    command += ["--handoff-dir", str(HANDOFF)] if handoff else []
    subprocess.run(command, cwd=ROOT, check=True, timeout=30, capture_output=True)
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(OUT / "shot.bmp").convert("RGB")


def centre(image):
    return image.getpixel((320, 240))


GREY = (48, 48, 48)  # the placeholder of a save without a picture
states()
listed, info = shot(""), shot("R")
# With show_auto enabled, Auto Save is rightmost and selected initially.
# Left leads to the highest numbered slot (#7), then #4 and #1. A Right
# past Auto returns to the ROM list, but a held Right does not exit.
auto = shot("L")
assert auto != listed and centre(auto) == AUTO
highest = shot("LL")
assert highest.getpixel((200, 150)) == GREY
# Without a picture the box says "No thumbnail" whichever save it is, while
# the label at the bottom left still names it: here #7 and Auto Save.
(STATES / "Alpha.state.auto.png").unlink()
bare_auto = shot("L")
Image.new("RGB", (320, 240), AUTO).save(STATES / "Alpha.state.auto.png")
BOX, LABEL = (160, 120, 480, 360), (0, 440, 200, 480)
assert len(highest.crop(BOX).getcolors(1 << 24)) > 2
assert bare_auto.crop(BOX).tobytes() == highest.crop(BOX).tobytes()
assert bare_auto.crop(LABEL).tobytes() != highest.crop(LABEL).tobytes()
four = shot("LLL")
assert centre(four) == FOUR and centre(shot("LLLL")) == ONE
assert shot("LLLLL") == shot("LLLL") and shot("LR") == listed
# The opening Left's repeats do nothing, and a held Right stops at the last.
assert shot("L+L") == auto and shot("LLR+R") == auto
# Pictures are scaled to the full screen: slot 4's 320x240 fills it, and
# the labels' boxes let it show through, darkened.
assert four.getpixel((100, 240)) == FOUR
# Saved Games uses Game Info's themed title bar, including its background,
# while the full-screen state screenshot stays visible below it.
assert four.getpixel((5, 5)) == info.getpixel((5, 5)) != FOUR
assert four.getpixel((629, 470)) not in (FOUR, (0, 0, 0)) and four.getpixel((10, 470)) not in (
    FOUR, (0, 0, 0))
# The "framed" layout fits slot 4's 4:3 picture to 640x360 between the
# header and a footer of its own: 480x360 from x 80 and y 60, the theme's
# background beside it, and in the footer its background and footer bar as
# under Game Info's hints, where only the labels differ from one save to
# another. Framed is the default; "overlay" is the full-screen one above.
framed = shot("LLL", layout="framed")
assert framed != four and shot("LLL", layout=None) == framed
assert centre(framed) == FOUR and framed.getpixel((5, 5)) == info.getpixel((5, 5))
assert framed.getpixel((82, 62)) == FOUR and framed.getpixel((557, 417)) == FOUR
assert FOUR not in (framed.getpixel((78, 240)), framed.getpixel((320, 58)),
                    framed.getpixel((320, 421)))
FOOTER, LEFT_LABEL = (200, 421, 440, 480), (0, 421, 200, 480)
one = shot("LLLL", layout="framed")
assert framed.crop(FOOTER).tobytes() == one.crop(FOOTER).tobytes()
BAR = (380, 421, 520, 480)  # clear of Game Info's hints
assert framed.crop(BAR).tobytes() == info.crop(BAR).tobytes()
assert framed.crop(LEFT_LABEL).tobytes() != one.crop(LEFT_LABEL).tobytes()
# Its placeholder sits in the frame, and the media menu over it leaves the
# footer as it is.
bare = shot("LL", layout="framed")
assert bare.getpixel((200, 160)) == GREY and bare.getpixel((200, 145)) != GREY
assert shot("LLLS", layout="framed").crop(FOOTER).tobytes() == framed.crop(FOOTER).tobytes()
# B goes back to the list.
assert shot("LB") == listed
# Beta has none: Left is as it was.
assert shot("DL") == shot("D")
# The media menu in Game Info offers Saved Games; B from them goes to Game Info.
assert shot("RS") != info and shot("RSA") == auto and shot("RSAB") == info
# Context-menu entry: both ends stop; B is the way back.
assert shot("RSAL") == highest and shot("RSARRRR") == auto
assert centre(shot("RSALL")) == FOUR and shot("RSALLLL") == shot("RSALLL")
assert shot("RSARRRRB") == info
# Right in Game Info, without a gallery, does nothing: never the menu.
assert shot("RR") == info
# Select in Saved Games: the menu without them, Game Info first.
assert shot("LS") not in (auto, listed) and shot("LSA") == info and shot("LSB") == auto
# Not listed in game-menu.json's details: no Saved Games, nor Left for them.
HIDDEN = {"details": ["game_info", "gallery", "maps"]}
assert shot("L", HIDDEN) == listed and shot("RS", HIDDEN) == info
# Without left_shortcut, the default, or with it false: Left is as it was,
# Saved Games still in the menu.
NO_LEFT = {"saved_games": {"left_shortcut": False}}
assert shot("L", left=False) == listed and shot("RSA", left=False) == auto
assert shot("L", NO_LEFT) == listed
# Without show_auto (the default), the highest numbered slot is rightmost.
assert shot("L", auto=False).getpixel((200, 150)) == GREY
assert centre(shot("LL", auto=False)) == FOUR
assert shot("LR", auto=False) == listed
# upscale_preview false: a picture that fits keeps its own size, on black.
SMALL = {"saved_games": {"upscale_preview": False}}
assert centre(shot("LLL", SMALL)) == FOUR and shot("LLL", SMALL).getpixel((100, 240)) == (0, 0, 0)
# A native 256x240 screenshot has square storage pixels but 4:3 intended
# display pixels for SNES. Corrected previews, the default, reach the left
# of the uncorrected 512x480 fit, regardless of whether enlargement is
# enabled; correct_aspect_ratio false keeps the picture's own shape.
Image.new("RGB", (256, 240), FOUR).save(STATES / "Alpha.state4.png")
OWN = {"saved_games": {"correct_aspect_ratio": False}}
assert shot("LLL", OWN).getpixel((30, 240)) == (0, 0, 0)
assert shot("LLL").getpixel((30, 240)) == FOUR
assert shot("LLL", {"saved_games": {"correct_aspect_ratio": False,
                                    "upscale_preview": False}}).getpixel((170, 240)) == (0, 0, 0)
assert shot("LLL", SMALL).getpixel((170, 240)) == FOUR
# RedOnion's other 4:3 folders by name in any case, here neogeo; an arcade
# folder keeps the picture's own shape.
CONSOLES = (("neogeo", "Neo", True), ("MAME", "Mame", False), ("ARCADE", "Arcade", False),
            ("FBNEO", "FBNeo", False))
for folder, label, widened in CONSOLES:
    (SD / f"Emu/{folder}").mkdir(parents=True, exist_ok=True)
    (SD / f"Emu/{folder}/config.json").write_text(json.dumps(dict(
        label=label, rompath=f"../../Roms/{folder}", launch="launch.sh", extlist="sfc")))
    (SD / f"Emu/{folder}/launch.sh").write_text((SD / "Emu/SFC/launch.sh").read_text())
    (SD / f"Roms/{folder}").mkdir(parents=True, exist_ok=True)
    (SD / f"Roms/{folder}/Alpha.sfc").write_bytes(b"rom")
    if folder != "ARCADE":  # its Saved Games are off by default, below
        assert (shot("LLL", system=label).getpixel((30, 240)) == FOUR) == widened, folder
# An arcade folder's portrait picture shows as it was saved, not turned
# (MAME here, as ARCADE has no Saved Games by default).
RED, BLUE = (200, 40, 40), (40, 40, 200)
portrait = Image.new("RGB", (224, 288), BLUE)
portrait.paste(RED, (0, 0, 224, 144))
portrait.save(STATES / "Alpha.state4.png")
upright = shot("LLL", system="Mame")
assert upright.getpixel((100, 240)) == (0, 0, 0) and upright.getpixel((320, 100)) == RED
Image.new("RGB", (320, 240), FOUR).save(STATES / "Alpha.state4.png")
# disabled_rom_dirs, ARCADE by default: no Saved Games there, neither by
# Left nor in Game Info's media menu (which, left without rows, does not
# open); FBNEO keeps them. A list replaces the default, [] turns none off.
def saves_off(label, menu=None):
    return (shot("L", menu, system=label) == shot("L", menu, left=False, system=label) and
            shot("RS", menu, system=label) == shot("R", menu, system=label))


assert saves_off("Arcade") and not saves_off("FBNeo")
NONE = {"saved_games": {"disabled_rom_dirs": []}}
assert not saves_off("Arcade", NONE)
CUSTOM = {"saved_games": {"disabled_rom_dirs": ["NEOGEO", "nothing here"]}}
assert saves_off("Neo", CUSTOM) and not saves_off("Arcade", CUSTOM) and not saves_off("SNES", CUSTOM)
# A scrolling title: the first Left resets it, as Left did, and the next Left
# on that game opens Saved Games; a held Left's repeats (+L) do not.
(CONFIG / ".romListTitleScroll").write_text("10,200")
reset = shot("DDL")
assert reset != shot("DDLL") and centre(shot("DDLL")) == AUTO
assert shot("DDL+L") == reset and shot("DDLDUL") == reset
(CONFIG / ".romListTitleScroll").unlink()
# A game with a gallery but no row for the menu, as game-menu.json's details
# can leave it: Select in Game Info does nothing, nor Up and Down after.
(ROMS / "media/box2dfront").mkdir(parents=True)
Image.new("RGB", (320, 240), FOUR).save(ROMS / "media/box2dfront/Beta.png")
beta, beta_listed = shot("DR"), shot("D")
for details in ([], ["game_info"], ["maps", "walkthrough"]):
    menu = {"details": details}
    assert shot("DRS", menu) == beta and shot("DRSUDD", menu) == shot("DRUDD", menu), details
    assert shot("DRSB", menu) == beta_listed, details
    assert shot("DRR", menu) != beta and shot("DRRSA", menu) == beta, details
# Saved Games opened by Left, then the gallery chosen in their menu: B goes
# back to Game Info, no longer to the list.
Image.new("RGB", (320, 240), ONE).save(ROMS / "media/box2dfront/Alpha.png")
assert centre(shot("LSDA")) == ONE and shot("LSDAB") == info
assert shot("LSDAL") == shot("LSDA")  # switching via Select removes edge exit
# The inverse switch, from Gallery into Saved Games, also loses edge exit.
assert centre(shot("RRSDA" + "L" * 3)) == ONE
assert shot("RRSDA" + "R" * 4 + "B") == info
shutil.rmtree(ROMS / "media")
print("Saved Games show a game's states from Left in a list and Game Info's media menu")

# A resumes: the default rolling backup keeps Auto, the state replaces it,
# and the game launches with Onion's flag to load Auto.
states()
shot("LLLA")
assert read("Alpha.state.auto") == "slot4" and read("Alpha.state.auto.mainui-backup") == "auto"
assert (HANDOFF / "force_auto_load_state").exists() and "Alpha.sfc" in (
    HANDOFF / "cmd_to_run.sh").read_text()
assert read("Alpha.state4") == "slot4" and read(f"{LONG}.state.auto") == "auto"
# Previous Session is at the left edge, and exchanges with Auto.
(STATES / "Alpha.state.auto").write_text("played on")
shot("L" + "L" * 4 + "A")  # Auto, 7, 4, 1, then Previous Session
assert read("Alpha.state.auto") == "auto" and read("Alpha.state.auto.mainui-backup") == "played on"
# Overwrite: Auto replaced, no backup.
states()
shot("LLA", {"saved_games": {"resume_mode": "overwrite"}})
assert read("Alpha.state.auto") == "slot7" and read("Alpha.state.auto.mainui-backup") is None
assert not (STATES / "Alpha.state.auto.png").exists()  # 7 has no picture
# Swap: Auto and the state exchange; no Previous Session.
states()
shot("LLLA", {"saved_games": {"resume_mode": "swap"}})
assert read("Alpha.state.auto") == "slot4" and read("Alpha.state4") == "auto"
assert read("Alpha.state1") == "slot1" and read("Alpha.state.auto.mainui-backup") is None
assert shot("LR", {"saved_games": {"resume_mode": "swap"}}) == listed  # Auto is at the right edge
# Without a launch (no handoff folder) nothing changes, nor when the launch
# fails after Auto was replaced: it is put back from the backup.
states()
shot("LLA", handoff=False)
assert read("Alpha.state.auto") == "auto" and read("Alpha.state7") == "slot7"
assert read("Alpha.state.auto.mainui-backup") is None
for mode in ("backup", "swap"):
    shot("LLLA", {"saved_games": {"resume_mode": mode}}, blocked=True)
    assert read("Alpha.state.auto") == "auto" and read("Alpha.state4") == "slot4", mode
    assert (STATES / "Alpha.state.auto.png").exists(), mode
    assert not (HANDOFF / "force_auto_load_state").exists(), mode
    assert not list(STATES.glob("*.mainui-new")), mode
assert read("Alpha.state.auto.mainui-backup") == "auto"
# Overwrite has nothing to put back: Auto keeps the state.
shot("LLLA", {"saved_games": {"resume_mode": "overwrite"}}, blocked=True)
assert read("Alpha.state.auto") == "slot4" and read("Alpha.state4") == "slot4"
# A file a refused rename left beside a target refuses the next resume.
states()
(STATES / "Alpha.state.auto.mainui-new").write_text("only copy")
shot("LLLA")
assert read("Alpha.state.auto") == "auto" and read("Alpha.state.auto.mainui-new") == "only copy"
assert not (HANDOFF / "force_auto_load_state").exists()
# Without the emulator's launch script Onion could not start the game: a
# core chosen for the game still finds its states, but A changes nothing.
states()
CONFIG_DIR = SD / "Roms/SFC/.game_config"
CONFIG_DIR.mkdir(exist_ok=True)
(CONFIG_DIR / "Alpha.cfg").write_text('core = "test_libretro"\n')
(SD / "RetroArch/.retroarch/cores/test_libretro.so").write_text("core")
LAUNCHER = SD / "Emu/SFC/launch.sh"
script = LAUNCHER.read_text()
LAUNCHER.unlink()
shot("LLLA")
LAUNCHER.write_text(script)
(CONFIG_DIR / "Alpha.cfg").unlink()
(SD / "RetroArch/.retroarch/cores/test_libretro.so").unlink()
assert read("Alpha.state.auto") == "auto" and read("Alpha.state4") == "slot4"
assert read("Alpha.state.auto.mainui-backup") is None
assert not (HANDOFF / "force_auto_load_state").exists()
# Nor without the game's file, moved while its console's cache still lists
# it: in Overwrite mode Auto would otherwise be lost.
states()
GAME = ROMS / "Alpha.sfc"
GAME.unlink()
shot("LLLA", {"saved_games": {"resume_mode": "overwrite"}})
GAME.write_bytes(b"rom")
assert read("Alpha.state.auto") == "auto" and read("Alpha.state4") == "slot4"
assert not (HANDOFF / "force_auto_load_state").exists()
# A on Auto Save (right of numbered slots) launches without changing files.
states()
shot("LA")
assert read("Alpha.state.auto") == "auto" and read("Alpha.state.auto.mainui-backup") is None
assert (HANDOFF / "force_auto_load_state").exists() and "Alpha.sfc" in (
    HANDOFF / "cmd_to_run.sh").read_text()
# A game with only Auto Save has Saved Games, from Left too.
for path in STATES.glob("Alpha.state[0-9]*"):
    path.unlink()
assert centre(shot("L")) == AUTO and shot("LL") == shot("L") and shot("LR") == shot("")
# Without show_auto it has none: Left is as it was.
assert shot("L", auto=False) == shot("", auto=False)
shutil.rmtree(OUT)
print("A resumes a state into Auto as resume_mode says, then launches the game")
