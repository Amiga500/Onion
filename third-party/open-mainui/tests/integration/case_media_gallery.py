# SPDX-License-Identifier: GPL-3.0-only
"""Right in Game Info opens a fullscreen gallery of the game's media: front and
back covers, 3D box, screenshots and video from media/ in its console's ROM
folder, by the ROM file's name; never the Imgs cover Game Info shows. Right and
Left browse, and B returns to Game Info, as Left on the first item does when
Right opened the gallery, not the media menu. Maps in media/maps and a
walkthrough in media/manuals show on their own, B going back to Game Info from
them too; Select in Game Info, the gallery, a map or the walkthrough opens a
menu of the others. Videos play with Onion's FFmpeg, loaded from
.tmp_update/lib/parasyte only then; here a stub stands in for it
(tests/ffmpeg_stub.c) under FFmpeg's sonames, with LD_LIBRARY_PATH as Onion
starts MainUI, without that folder."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
# The standalone map and walkthrough fixtures are built by the same Makefile.
for fixture in ("fixture-media_maps", "fixture-media_text"):
    subprocess.run([str(BUILD / fixture)], check=True, capture_output=True)
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="media-gallery-", dir=BUILD))
FFMPEG = ("libavutil.so.54", "libswscale.so.3", "libavcodec.so.56", "libavformat.so.56")
# Onion's FFmpeg; one without sws_scale; one without libswresample (sound).
LIBS = {"full": (BUILD / "ffstub.so", FFMPEG + ("libswresample.so.1",)),
        "noscale": (BUILD / "ffstub-noscale.so", FFMPEG),
        "noswr": (BUILD / "ffstub.so", FFMPEG)}
for name, (stub, sonames) in LIBS.items():
    (OUT / name).mkdir()
    for soname in sonames:
        (OUT / name / soname).symlink_to(stub.resolve())


def environment(path=None):
    """Onion starts MainUI without its FFmpeg folder on LD_LIBRARY_PATH."""
    env = dict(os.environ)
    env.pop("LD_LIBRARY_PATH", None)
    if path:
        env["LD_LIBRARY_PATH"] = str(OUT / path)
    return env


# The player on its own: timing, repeats, stopping, and every failure.
VIDEOS = OUT / "videos"
VIDEOS.mkdir()
for name, spec in {"ok": "320 240 6 20", "wide": "640 160 3 20", "tall": "120 240 3 20",
                   "nopts": "320 240 6 20 nopts", "noseek": "320 240 3 20 noseek",
                   "slow": "320 240 1000 30 slow", "empty": "320 240 0 20",
                   "nocodec": "320 240 5 20 nocodec", "huge": "4000 3000 5 20",
                   "long": "320 240 200 20", "sound": "320 240 200 20 audio48000",
                   "sound44": "320 240 200 20 audio44100",
                   "late": "320 240 200 20 audio48000 lag500",
                   "nosoundcodec": "320 240 200 20 audio48000 nosoundcodec",
                   "first": "320 240 200 20 audio48000 audiofirst",
                   "hugenocodec": "4000 3000 5 20 nocodec", "liar": "4000 3000 5 20 lie",
                   "odd": "320 240 200 20 audio500",
                   "nodims": "320 240 5 20 nodims", "extra": "320 240 5 20 extra",
                   "grow": "320 240 200 20 grow10",
                   "cover": "320 240 6 20 cover", "gop": "320 240 200 20 gop50",
                   "soundgop": "320 240 200 20 audio48000 gop50",
                   "big": "320 240 200 20 audio48000 packet300000",
                   "waitopen": "320 240 6 20 waitopen", "waitprobe": "320 240 6 20 waitprobe",
                   "waitread": "320 240 6 20 waitread"}.items():
    (VIDEOS / f"{name}.mp4").write_text(f"STUB {spec}\n")
(VIDEOS / "text.mp4").write_text("not a video\n")
# Found in the folder given, as in Onion's parasyte folder, or by name.
# Pause and seek; sound in step, at any rate, or none when it cannot play.
(OUT / "none").mkdir()
for mode, folder, path in (("play", "full", None), ("errors", "full", None),
                           ("errors", "-", "full"), ("missing", "noscale", None),
                           ("missing", "none", None), ("missing", "-", None),
                           ("controls", "full", None), ("sound", "full", None),
                           ("cancel", "full", None),
                           ("silent nosoundcodec.mp4", "full", None),
                           ("silent odd.mp4", "full", None),
                           ("silent sound.mp4", "noswr", None)):
    libraries = str(OUT / folder) if folder != "-" else "-"
    result = subprocess.run([str(BUILD / "fixture-video"), str(VIDEOS), libraries] + mode.split(),
                            cwd=ROOT, env=environment(path), timeout=60, capture_output=True,
                            text=True)
    assert result.returncode == 0, (mode, folder, path, result.stdout, result.stderr)
    assert "[ffstub]" not in result.stderr, result.stderr

# A console with a game that has every kind of media, one without any, and
# one in a subfolder, its media at the console's ROM folder.
SD = OUT / "sd"
(SD / "Emu/FC").mkdir(parents=True)
(SD / "Emu/FC/config.json").write_text(json.dumps(dict(
    label="NES", rompath="../../Roms/FC", imgpath="../../Roms/FC/Imgs", launch="launch.sh",
    extlist="nes")))
(SD / ".tmp_update/config").mkdir(parents=True)
ROMS = SD / "Roms/FC"
(ROMS / "Sub").mkdir(parents=True)
GAME = "Super Game$ Collection (USA)"
for rom in (GAME + ".nes", "Zero Media.nes", "Sub/Inner Game.nes"):
    (ROMS / rom).write_bytes(b"rom")
# Every kind of media, with screenshots <name>.png, -1 and -2; -4 does not
# show, -3 being missing. The list's cover in Imgs is no part of the gallery.
MEDIA = [("box2dfront", "", (200, 30, 30)), ("box2dback", "", (30, 200, 30)),
         ("box3d", "", (30, 200, 200)), ("screenshot", "", (30, 30, 200)),
         ("screenshot", "-1", (220, 220, 30)), ("screenshot", "-2", (220, 120, 30)),
         ("screenshot", "-4", (220, 30, 220))]
COLOURS = {folder + suffix: colour for folder, suffix, colour in MEDIA}
TALL = {"box2dfront", "box2dback", "box3d"}
for folder, suffix, colour in MEDIA:
    target = ROMS / "media" / folder / (GAME + suffix + ".png")
    target.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", (300, 400) if folder in TALL else (320, 240), colour).save(target)
(ROMS / "media/videos").mkdir()
# 10 s at 20 frames a second, with sound (silent in snapshots).
(ROMS / "media/videos" / (GAME + ".mp4")).write_text("STUB 320 240 200 20 audio48000\n")
COVER = (120, 60, 160)
(ROMS / "Imgs").mkdir()
Image.new("RGB", (200, 280), COVER).save(ROMS / "Imgs" / (GAME + ".png"))
# Screenshots numbered from -2, as some scrapers name them: all show.
Image.new("RGB", (50, 50), (200, 30, 30)).save(ROMS / "media/box2dfront/Inner Game.png")
Image.new("RGB", (320, 240), (90, 90, 90)).save(ROMS / "media/screenshot/Inner Game-2.png")
Image.new("RGB", (320, 240), (150, 150, 150)).save(ROMS / "media/screenshot/Inner Game-3.png")
FRAME0 = (0, 255, 100)  # the stub's first frame


PARASYTE = SD / ".tmp_update/lib/parasyte"


def shot(actions, libraries="full", system="NES"):
    """The screen after the actions, with Onion's FFmpeg folder holding the
    full stub, one without sws_scale, or nothing."""
    shutil.rmtree(PARASYTE, ignore_errors=True)
    if libraries:
        shutil.copytree(OUT / libraries, PARASYTE, symlinks=True)
    target = OUT / "shot.bmp"
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--input", actions, "--snapshot", str(target)]
    if system:
        command += ["--system", system]
    result = subprocess.run(command, cwd=ROOT, env=environment(), check=True,
                            timeout=30, capture_output=True, text=True)
    assert "[ffstub]" not in result.stderr, result.stderr
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(target).convert("RGB"), result.stderr


def colours(image):
    return {colour for _, colour in image.getcolors(1 << 24)}


def centre(image):
    return image.getpixel((320, 240))


# The list's rows: Sub, then the game, then Zero Media. Right opens Game Info,
# as before, with the cover from Imgs.
info, _ = shot("DR")
assert COVER in colours(info)
# Right again: the front cover, fitted to the height and centred on black.
box, _ = shot("DRR")
assert centre(box) == COLOURS["box2dfront"], centre(box)
assert box.getpixel((5, 240)) == (0, 0, 0) and box.getpixel((315 - 180, 240)) == (0, 0, 0)
assert box.getpixel((320 - 170, 240)) == COLOURS["box2dfront"]
# Configurable Game Info Right shortcut: default ON; OFF keeps Game Info on
# Right, but Select still opens the media menu, and Gallery remains reachable.
game_menu = SD / ".tmp_update/config/game-menu.json"
game_menu.write_text(json.dumps({"gallery": {"right_shortcut": False}}))
disabled, _ = shot("DRR")
assert disabled.tobytes() == info.tobytes()
menu_screen, _ = shot("DRS")
assert menu_screen.tobytes() != info.tobytes()
game_menu.unlink()
# Then the back cover, the 3D box and the screenshots, which fill the screen;
# then the video, the last, where Right stays. Never the Imgs cover.
order = ["box2dback", "box3d", "screenshot", "screenshot-1", "screenshot-2"]
for steps, key in enumerate(order, 1):
    image, _ = shot("DRR" + "R" * steps)
    assert centre(image) == COLOURS[key], (key, centre(image))
    assert COVER not in colours(image), key
full, _ = shot("DRRRRR")
assert full.getpixel((2, 2)) == COLOURS["screenshot"]
# With gallery upscale_images false the 320x240 screenshot keeps its own
# size, centred on black.
game_menu.write_text(json.dumps({"gallery": {"upscale_images": False}}))
native, _ = shot("DRRRRR")
game_menu.unlink()
assert centre(native) == COLOURS["screenshot"] and native.getpixel((2, 2)) == (0, 0, 0)
assert native.getpixel((165, 125)) == COLOURS["screenshot"]
assert native.getpixel((155, 240)) == (0, 0, 0) and native.getpixel((485, 240)) == (0, 0, 0)
# The counter's box lets the picture show through, darkened.
assert full.getpixel((629, 470)) not in (COLOURS["screenshot"], (0, 0, 0))
VIDEO = "DRR" + "R" * 6  # the seventh item
video, log = shot(VIDEO)
assert centre(video) == FRAME0 and video.getpixel((2, 2)) == FRAME0, centre(video)
# The counter shows over it, bottom right.
assert any(min(video.getpixel((x, y))) > 200 for x in range(560, 640) for y in range(430, 480))
# On a video A pauses, with a pause sign at the bottom left, and goes on; R and
# L (2 and 1) jump 5 s, 100 frames, forward and back, also while paused,
# from the frame shown: the video plays on between the keys, a frame or two.


def frame(image):
    """The stub frame shown in the middle."""
    r, g, b = centre(image)
    assert b == 100 and r + g == 255, (r, g, b)
    return r


def label(image):
    """The pause sign: two white bars side by side, black between them."""
    rows = [[min(image.getpixel((x, y))) > 200 for x in range(0, 60)] for y in range(430, 480)]
    for row in rows:
        runs = [x for x in range(1, 60) if row[x] and not row[x - 1]]
        if len(runs) == 2 and sum(row) <= 24:
            return True
    return False


paused, _ = shot(VIDEO + "A")
assert frame(paused) <= 2 and label(paused) and not label(video)
resumed, _ = shot(VIDEO + "AA")
assert frame(resumed) <= 2 and not label(resumed)
assert frame(shot(VIDEO + "R")[0]) <= 4  # the last: Right leaves it playing
assert 100 <= frame(shot(VIDEO + "2")[0]) <= 104
assert 0 <= frame(shot(VIDEO + "21")[0]) <= 6
assert frame(shot(VIDEO + "1")[0]) <= 2  # not before the start
assert frame(shot(VIDEO + "22")[0]) <= 4  # past the end: from the start again
later, _ = shot(VIDEO + "A2")
assert 100 <= frame(later) <= 102 and label(later)  # and still paused
# On an image A, L and R do nothing.
assert shot("DRRA12")[0] == shot("DRR")[0]
# Left goes back an item, and from the first one back to Game Info.
assert centre(shot("DRRRL")[0]) == COLOURS["box2dfront"]
assert centre(shot(VIDEO + "L")[0]) == COLOURS["screenshot-2"]
assert shot("DRRL")[0] == info and shot("DRRRLL")[0] == info
# B returns to Game Info, from an image or a video, as it was.
for actions in ("DRRB", VIDEO + "B", "DRRRRRRB", VIDEO + "BRB" + "R" * 7 + "B", "DRRLRLRRLLRB"):
    assert shot(actions)[0] == info, actions
# Up/Down and A do nothing on a still image; Select opens the context popup.
assert shot("DRRUDE")[0] == box
assert shot("DRRS")[0] != box
assert shot("DRRSB")[0] == box
assert shot("DRRSBB")[0] == info
# Select from Game Info opens the menu without Game Info: here only Gallery.
assert shot("DRS")[0] != info
assert shot("DRSA")[0] == box
assert shot("DRSB")[0] == info
# Gallery selected from the media menu stops at both ends, unlike Right
# shortcut entry; B returns to Game Info.
assert shot("DRSAL")[0] == box
assert frame(shot("DRSA" + "R" * 8)[0]) <= 4
assert shot("DRSALB")[0] == info
# Cancelling Select over a shortcut-opened gallery keeps its edge exit.
assert shot("DRRSBL")[0] == info
assert shot("DRSDA")[0] == box
# A game without media: Right and Select do nothing in Game Info.
plain, _ = shot("DDR")
assert shot("DDRR")[0] == plain
assert shot("DDRS")[0] == plain
# A game in a subfolder (below ".."): its media is at the console's ROM
# folder. Its screenshots start at -2; Right on the last stays there.
for steps, colour in enumerate([COLOURS["box2dfront"], (90, 90, 90), (150, 150, 150),
                                (150, 150, 150)]):
    assert centre(shot("EDRR" + "R" * steps)[0]) == colour, steps

# Without FFmpeg, or with one that lacks a function, the video shows why.
for libraries in (None, "noscale"):
    card, log = shot(VIDEO, libraries)
    assert centre(card) != FRAME0 and "[video]" in log, log
    text = [(x, y) for x in range(100, 540) for y in range(200, 280)
            if min(card.getpixel((x, y))) > 200]
    assert len(text) > 100, len(text)
    # Images still show, and B still returns.
    assert centre(shot("DRR", libraries)[0]) == COLOURS["box2dfront"]
    assert shot(VIDEO + "B", libraries)[0] == info

# From Favorites, the game's console is found by its launcher.
(SD / ".tmp_update/config/main-menu.json").write_text(json.dumps({"menu": dict(favorites=True)}))
(SD / "Roms/favourite.json").write_text(json.dumps(dict(
    label=GAME, rompath=f"/mnt/SDCARD/Roms/FC/{GAME}.nes", launch="/mnt/SDCARD/Emu/FC/launch.sh",
    type=5)) + "\n")
assert centre(shot("ERR", system=None)[0]) == COLOURS["box2dfront"]
# X and Y with the Favorite shortcut do nothing in the gallery.
(SD / ".tmp_update/config/keymap.json").write_text(json.dumps(dict(
    mainui_button_x="fav", mainui_button_y="fav")))
before = (SD / "Roms/favourite.json").read_text()
gallery = shot("ERR", system=None)[0]
assert shot("ERRXY", system=None)[0] == gallery
assert (SD / "Roms/favourite.json").read_text() == before
# Video discovery keeps the same gallery order but accepts common container
# extensions, regardless of extension case, without extra media folders.
original = ROMS / "media/videos" / (GAME + ".mp4")
for ext in ("MP4", "mKv", "AVI", "MoV", "WEBM", "m4v"):
    alternative = original.with_suffix("." + ext)
    original.rename(alternative)
    try:
        still_video, _ = shot(VIDEO)
        assert centre(still_video) == FRAME0 and still_video.getpixel((2, 2)) == FRAME0, ext
        # Each game still gets exactly one gallery video item.
        assert frame(shot(VIDEO + "R")[0]) <= 4, ext  # still the last item
    finally:
        alternative.rename(original)
# With two video formats present, .mp4 wins even if the other exists first.
other = original.with_suffix(".mkv")
other.write_text("STUB 120 240 200 20\n")
try:
    selected, _ = shot(VIDEO)
    assert selected.getpixel((2, 2)) == FRAME0
    assert frame(shot(VIDEO + "R")[0]) <= 4  # still the last item
finally:
    other.unlink()
# Select on a video opens the menu over it, paused; B goes back with the video
# playing on from there, not from its start, and one paused before stays so.
SEEK = VIDEO + "2"
assert shot(SEEK + "S")[0] != shot(SEEK)[0]
on_again, _ = shot(SEEK + "SB")
assert 100 <= frame(on_again) <= 104 and not label(on_again), frame(on_again)
assert 100 <= frame(shot(SEEK + "SS")[0]) <= 104  # Select closes it too
still_paused, _ = shot(VIDEO + "A2SB")
assert 100 <= frame(still_paused) <= 102 and label(still_paused)
# Game maps are separate from the existing gallery; only media/maps is read,
# and only PNG: JPEG and BMP maps are not shown.
MAPS = ROMS / "media/maps"
MAPS.mkdir(parents=True)
MAP = {"": (100, 30, 210), "-1": (210, 90, 40), "-2": (40, 190, 210)}
for suffix, rgb in MAP.items():
    Image.new("RGB", (320, 240), rgb).save(MAPS / (GAME + suffix + ".png"))
Image.new("RGB", (320, 240), (10, 10, 10)).save(MAPS / (GAME + "-3.jpg"), format="JPEG")
Image.new("RGB", (320, 240), (20, 20, 20)).save(MAPS / (GAME + "-4.bmp"))
info_now, _ = shot("DR")
# The menu leaves out the view it is opened from and always offers Game Info
# outside it: from the gallery, Game Info and Game Maps.
menu_gallery, _ = shot("DRRS")
assert menu_gallery != box
assert shot("DRRSB")[0] == box and shot("DRRSS")[0] == box
assert shot("DRRSA")[0] == info_now
map_one, _ = shot("DRRSDA")
assert centre(map_one) == MAP[""], centre(map_one)
assert centre(shot("DRRSDA2")[0]) == MAP["-1"]
assert centre(shot("DRRSDA22")[0]) == MAP["-2"]
assert centre(shot("DRRSDA221")[0]) == MAP["-1"]
assert centre(shot("DRRSDA1")[0]) == MAP["-2"]  # three maps, round the ends
# Up and Down go round the menu.
assert shot("DRRSUA")[0] == map_one and shot("DRRSDDA")[0] == info_now
# From Game Info: Gallery and Game Maps.
assert shot("DRSA")[0] == box and shot("DRSDA")[0] == map_one
# Select in a map: the menu without Game Maps; B or Select back to that map,
# Game Info and Gallery on its rows.
second_map, _ = shot("DRRSDA2")
assert shot("DRRSDA2S")[0] not in (second_map, menu_gallery)
for keys in ("SB", "SS"):
    assert shot("DRRSDA2" + keys)[0] == second_map, keys
# The menu darkens a view without a footer to the bottom, but leaves Game
# Info's footer as it is.
bottom = second_map.getpixel((320, 470))
dimmed = shot("DRRSDA2S")[0].getpixel((320, 470))
assert sum(bottom) > 150 and all(d < b // 2 for d, b in zip(dimmed, bottom)), (bottom, dimmed)
footer = (0, 421, 640, 480)
assert shot("DRS")[0].crop(footer).tobytes() == info_now.crop(footer).tobytes()
assert shot("DRRSDA2SA")[0] == info_now
assert shot("DRRSDA2SDA")[0] == box
assert shot("DRRSDA2SDAL")[0] == box  # menu-selected gallery stops at first
# B in a map goes back to Game Info.
assert shot("DRRSDAB")[0] == info_now
# A toggles native size and fit; R2 (4) shows native size and L2 (3) fits,
# each doing nothing when already so.
native, _ = shot("DRRSDAA")
assert native != map_one
assert shot("DRRSDA4")[0] == native and shot("DRRSDA44")[0] == native
assert shot("DRRSDAA3")[0] == map_one and shot("DRRSDA3")[0] == map_one
assert shot("DRRSDA43")[0] == map_one and shot("DRRSDAAA")[0] == map_one
assert centre(shot("DRRR")[0]) == COLOURS["box2dback"]

# TXT walkthroughs live only in media/manuals, not among ROMs.
MANUALS = ROMS / "media/manuals"
MANUALS.mkdir(parents=True)
(MANUALS / (GAME + ".txt")).write_text(
    "GAME WALKTHROUGH\nChapter 1\n" + "Collect everything before the boss.\n" * 40
    + "The key is in the qqq room.\n" + "Collect everything before the boss.\n" * 20
    + "A second QQQ room, in capitals.\n" + "Collect everything before the boss.\n" * 20,
    encoding="utf-8")
assert shot("DRR")[0] == box  # no control hint in the gallery
# From the gallery: Game Info, Game Maps, Walkthrough.
reader, _ = shot("DRRSDDA")
assert reader not in (menu_gallery, box)
scrolled, _ = shot("DRRSDDADD")
assert scrolled != reader
# Select in the walkthrough opens the menu without it; B or Select goes back
# to the same place, and its first row is Game Info.
assert shot("DRRSDDADDS")[0] != scrolled
assert shot("DRRSDDADDSB")[0] == scrolled
assert shot("DRRSDDADDSS")[0] == scrolled
assert shot("DRRSDDADDSA")[0] == info_now
# B in the walkthrough goes back to Game Info.
assert shot("DRRSDDAB")[0] == info_now
# Search: A opens the keyboard, here typing qqq (Down to q, A three times),
# and Start searches; the match's line goes to the top, marked. Start again
# finds the next one, in any case, and then round to the first.
MARK = (150, 110, 20)
SEARCH = "DRRSDDA" + "ADAAAT"
found, _ = shot(SEARCH)
assert found != reader and MARK in colours(found)
second, _ = shot(SEARCH + "T")
assert second != found and MARK in colours(second)
assert shot(SEARCH + "TT")[0] == found
# Nothing found ("0": Left to it, A): the page stays, nothing marked.
missing, _ = shot("DRRSDDA" + "ALAT")
assert missing != reader and MARK not in colours(missing)
assert MARK not in colours(reader)
# A game with only a walkthrough: Select opens the menu over Game Info, with
# only Walkthrough; Right, without a gallery, does nothing.
(MANUALS / "Zero Media.txt").write_text("SINGLE WALKTHROUGH\nHello world\n", encoding="utf-8")
media_only, _ = shot("DDRS")
assert media_only != plain and shot("DDRR")[0] == plain
alone, _ = shot("DDRSA")
assert alone != media_only
assert shot("DDRSAB")[0] == plain
# Select there: the menu with only Game Info.
assert shot("DDRSASA")[0] == plain and shot("DDRSASB")[0] == alone
# A TXT file beside the ROM is intentionally not discovered.
(MANUALS / "Zero Media.txt").unlink()
(ROMS / "Zero Media.txt").write_text("Not a supported path\n")
assert shot("DDRR")[0] == plain
assert shot("DDRS")[0] == plain

shutil.rmtree(OUT)
print("Game Info's gallery, maps and walkthrough, and its media menu, work; B goes back")
