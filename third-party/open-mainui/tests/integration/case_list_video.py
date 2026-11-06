# SPDX-License-Identifier: GPL-3.0-only
"""Video previews in ROM lists: with .tmp_update/config/.romListVideoPreview,
a game selected for its delay (the file's number of ms, 1000 when it holds
none) plays media/videos/<name>.mp4 silently in place of its cover, fitted to
the cover's 250x360 area, or with "fullscreen" after the delay over the whole
screen, where B closes it and any other key closes it and acts on the list.
Without the file, or for a game without a video or without Onion's FFmpeg,
the list shows as before; nothing plays over Game Info. A stub stands in for
FFmpeg (tests/ffmpeg_stub.c), as for the gallery. A live run checks every
partial frame of the playing video against a full frame, also while a long
title bounces."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix="list-video-", dir=BUILD))
FFMPEG = ("libavutil.so.54", "libswscale.so.3", "libavcodec.so.56", "libavformat.so.56",
          "libswresample.so.1")
STUBS = OUT / "stubs"
STUBS.mkdir()
for soname in FFMPEG:
    (STUBS / soname).symlink_to((BUILD / "ffstub.so").resolve())
SD = OUT / "sd"
(SD / "Emu/FC").mkdir(parents=True)
(SD / "Emu/FC/config.json").write_text(json.dumps(dict(
    label="NES", rompath="../../Roms/FC", imgpath="../../Roms/FC/Imgs", launch="launch.sh",
    extlist="nes")))
CONFIG = SD / ".tmp_update/config"
CONFIG.mkdir(parents=True)
SETTING = CONFIG / ".romListVideoPreview"
PARASYTE = SD / ".tmp_update/lib/parasyte"
ROMS = SD / "Roms/FC"
(ROMS / "Imgs").mkdir(parents=True)
(ROMS / "media/videos").mkdir(parents=True)
# Rows: Alpha (cover and a wide video), Beta (cover only), Gamma (a tall
# video, no cover).
for name in ("Alpha", "Beta", "Gamma"):
    (ROMS / (name + ".nes")).write_bytes(b"rom")
COVER = (120, 60, 160)
for name in ("Alpha", "Beta"):
    Image.new("RGB", (200, 280), COVER).save(ROMS / "Imgs" / (name + ".png"))
(ROMS / "media/videos/Alpha.mp4").write_text("STUB 320 240 200 20\n")
(ROMS / "media/videos/Gamma.mp4").write_text("STUB 120 240 200 20\n")
FRAME0 = (0, 255, 100)  # the stub's first frame
EDGE = 640 - Image.open(ONION_THEME / "skin/preview-bg.png").width


def environment():
    env = dict(os.environ)
    env.pop("LD_LIBRARY_PATH", None)
    return env


def setting(text):
    SETTING.unlink(missing_ok=True)
    if text is not None:
        SETTING.write_text(text)


HANDOFF = OUT / "handoff"
HANDOFF.mkdir()
COMMAND = HANDOFF / "cmd_to_run.sh"


def shot(actions="", elapsed=0, libraries=True):
    shutil.rmtree(PARASYTE, ignore_errors=True)
    if libraries:
        shutil.copytree(STUBS, PARASYTE, symlinks=True)
    target = OUT / "shot.bmp"
    COMMAND.unlink(missing_ok=True)
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--system", "NES", "--elapsed", str(elapsed), "--snapshot", str(target),
               "--handoff-dir", str(HANDOFF)]
    if actions:
        command += ["--input", actions]
    result = subprocess.run(command, cwd=ROOT, env=environment(), check=True, timeout=30,
                            capture_output=True, text=True)
    assert "[ffstub]" not in result.stderr, result.stderr
    (SD / "appconfigs/romwinidx.json").unlink(missing_ok=True)
    return Image.open(target).convert("RGB")


def video_box(image):
    """The box of the video's first frame in the cover area, or None."""
    points = [(x, y) for x in range(EDGE, 640) for y in range(60, 420)
              if image.getpixel((x, y)) == FRAME0]
    if not points:
        return None
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def cover_shown(image):
    return any(image.getpixel((x, y)) == COVER for x in range(EDGE, 640, 5)
               for y in range(60, 420, 5))


# Without the file nothing plays, however long the game stays selected.
setting(None)
assert cover_shown(shot(elapsed=60000)) and not video_box(shot(elapsed=60000))
# Empty: after 1000 ms the video plays in place of the cover, fitted to its
# 250x360 area (320x240 becomes 250x188) and centred in it.
setting("")
before = shot(elapsed=999)
assert cover_shown(before) and not video_box(before)
playing = shot(elapsed=1000)
box = video_box(playing)
assert box and box[2] - box[0] == 250 and box[3] - box[1] == 187, box
assert abs((box[0] + box[2]) / 2 - (EDGE + 125)) <= 1 and abs((box[1] + box[3]) / 2 - 240) <= 1
assert not cover_shown(playing)
# A number of ms sets the delay; anything else is 1000 ms.
setting("300\n")
assert not video_box(shot(elapsed=299)) and video_box(shot(elapsed=300))
setting("soon")
assert not video_box(shot(elapsed=999)) and video_box(shot(elapsed=1000))
setting("0")
# Another game: its own video, or its cover when it has none.
beta = shot("D", elapsed=60000)
assert cover_shown(beta) and not video_box(beta)
gamma = video_box(shot("DD", elapsed=60000))
assert gamma and gamma[2] - gamma[0] == 180 and gamma[3] - gamma[1] == 360, gamma
# Without Onion's FFmpeg the cover stays, with no message.
missing = shot(elapsed=60000, libraries=False)
assert cover_shown(missing) and not video_box(missing)
# Game Info shows its cover, not the video; back in the list it plays again.
info = shot("R", elapsed=60000)
assert not video_box(info)
assert video_box(shot("RB", elapsed=60000))

# Fullscreen: after the delay the video fills the screen, header and footer
# too, fitted (320x240 becomes 640x480) and centred on black.
setting("500,fullscreen")
assert cover_shown(shot(elapsed=499)) and not video_box(shot(elapsed=499))


def fullscreen(image):
    return image.getpixel((320, 240)) == FRAME0 and image.getpixel((320, 2)) == FRAME0


full = shot(elapsed=500)
assert fullscreen(full) and full.getpixel((2, 2)) == FRAME0 and full.getpixel((637, 477)) == FRAME0
# Fullscreen waits at least 100 ms, whatever the file says.
setting("0,fullscreen")
assert not fullscreen(shot(elapsed=99)) and fullscreen(shot(elapsed=100))
setting("0 fullscreen")
# A tall video (120x240 becomes 240x480), black beside it.
tall = shot("DD", elapsed=60000)
assert fullscreen(tall) and tall.getpixel((199, 240)) == (0, 0, 0)
assert tall.getpixel((201, 240)) == FRAME0 and tall.getpixel((439, 240)) == FRAME0
assert tall.getpixel((441, 240)) == (0, 0, 0)
# B closes it and stays in the list, with the cover, without playing it again;
# held (no release between) it does nothing more; released and pressed again
# it leaves the list as B does.
closed = shot("B", elapsed=60000)
assert not fullscreen(closed) and cover_shown(closed) and not video_box(closed)
held = shot("BB", elapsed=60000)
assert cover_shown(held) and not fullscreen(held)
left = shot("B-BB", elapsed=60000)
assert not cover_shown(left) and not fullscreen(left)
# Select, as Menu does, closes it and opens the context menu; once the menu
# closes, the same game keeps its cover, also after B closed the video before
# the menu, while another game selected plays its own.
for actions in ("SB", "B-BSB", "X-XSB"):
    after = shot(actions, elapsed=60000)
    assert cover_shown(after) and not fullscreen(after), actions
assert fullscreen(shot("SBDD", elapsed=60000))
# Any other key closes it and acts on the list: Down selects Beta, without a
# video, and Down again Gamma, whose video plays; Up from there shows the list.
beta = shot("D", elapsed=60000)
assert cover_shown(beta) and not fullscreen(beta)
assert fullscreen(shot("DD", elapsed=60000))
up = shot("DDU", elapsed=60000)
assert cover_shown(up) and not fullscreen(up)
# Right opens Game Info; A launches the game.
assert not fullscreen(shot("R", elapsed=60000))
shot("A", elapsed=60000)
assert COMMAND.exists() and b"Alpha.nes" in COMMAND.read_bytes(), COMMAND.exists()
# The X shortcut, which acts when released, closes it too: with "fav", X
# adds the game to Favorites, and the list shows again.
KEYMAP = CONFIG / "keymap.json"
KEYMAP.write_text(json.dumps({"mainui_button_x": "fav"}))
favourites = SD / "Roms/favourite.json"
favourites.unlink(missing_ok=True)
starred = shot("X", elapsed=60000)
assert cover_shown(starred) and not fullscreen(starred)
assert favourites.exists() and "Alpha.nes" in favourites.read_text()
favourites.unlink()
KEYMAP.unlink()
# Without Onion's FFmpeg the list stays.
missing = shot(elapsed=60000, libraries=False)
assert cover_shown(missing) and not fullscreen(missing)
setting("0")
# ROM-list previews use the gallery's same single-video discovery rules.
original = ROMS / "media/videos/Alpha.mp4"
for ext in ("MP4", "mKv", "AVI", "MoV", "WEBM", "m4v"):
    alternate = original.with_suffix("." + ext)
    original.rename(alternate)
    try:
        assert video_box(shot(elapsed=60000)), ext
    finally:
        alternate.rename(original)
# A different basename is never accepted, even when its extension matches.
other = ROMS / "media/videos/Alpha Game.WEBM"
other.write_text("STUB 320 240 200 20\n")
try:
    original.rename(ROMS / "media/videos/Alpha.hidden")
    assert not video_box(shot(elapsed=60000))
finally:
    (ROMS / "media/videos/Alpha.hidden").rename(original)
    other.unlink()

# Live: every partial frame of the playing video, and of a long title bouncing
# beside it, equals a full frame (MAINUI_VERIFY_PARTIAL aborts otherwise).
long_title = "Alpha " + "Long Title " * 6
(ROMS / "Alpha.nes").rename(ROMS / (long_title.strip() + ".nes"))
(ROMS / "media/videos/Alpha.mp4").rename(ROMS / "media/videos" / (long_title.strip() + ".mp4"))
(ROMS / "Imgs/Alpha.png").rename(ROMS / "Imgs" / (long_title.strip() + ".png"))
(CONFIG / ".romListTitleScroll").write_text("100,200,bounce")
shutil.rmtree(PARASYTE, ignore_errors=True)
shutil.copytree(STUBS, PARASYTE, symlinks=True)
for refresh in (True, False):
    env = environment()
    env["MAINUI_VERIFY_PARTIAL"] = "1"
    command = [str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme", str(ONION_THEME),
               "--system", "NES"] + (["--refresh-caches"] if refresh else [])
    process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, text=True)
    time.sleep(2.5)
    alive = process.poll() is None
    process.kill()
    _, errors = process.communicate(timeout=10)
    assert alive, errors
    assert "differs" not in errors, errors
    assert "a video preview frame matches" in errors, errors
# Without a title scrolling, each new frame is drawn alone over the last one:
# the same pixels as a full frame.
(CONFIG / ".romListTitleScroll").unlink()
process = subprocess.Popen([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme",
                            str(ONION_THEME), "--system", "NES"], cwd=ROOT, env=env,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
time.sleep(2.5)
alive = process.poll() is None
process.kill()
_, errors = process.communicate(timeout=10)
assert alive and "differs" not in errors, errors
assert "a video frame drawn alone matches" in errors, errors
# Fullscreen live: full frames only, and nothing differs when the list returns.
setting("0,fullscreen")
process = subprocess.Popen([str(BUILD / "MainUI-dev"), "--sd-root", str(SD), "--theme",
                            str(ONION_THEME), "--system", "NES"], cwd=ROOT, env=env,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
time.sleep(2)
alive = process.poll() is None
process.kill()
_, errors = process.communicate(timeout=10)
assert alive and "differs" not in errors and "a video preview frame matches" not in errors, errors
print("video previews play in place of the cover or fullscreen after the delay, only on lists; "
      "keys close a fullscreen one, and partial frames match full frames")
