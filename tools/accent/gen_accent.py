#!/usr/bin/env python3
"""Accent colours for the Silky RedOnion theme (Tweaks > Look and feel >
Accent colour).

For each colour other than red, writes the theme images that carry the red
accent, recoloured, to static/build/.tmp_update/res/accent/<name>/, with the
same paths as in the theme (skin/..., skin/extra/...). accent_color.sh copies
them into the theme overrides (Saves/CurrentProfile/theme), which Open MainUI
and the RedOnion programs read before the theme's own files.

Only the accent changes: a pixel's red hue is moved to the new colour's hue,
and its saturation and brightness are scaled by the new colour's against the
red's, so shading, outlines and transparency stay as they are. Pixels of
other hues (the greys, white, green) are left alone. Brand art (the
RedOnion logo, the boot, charging and screen-off pictures) stays red.

Run from the repository root:  python3 tools/accent/gen_accent.py
"""
import colorsys
import os
import shutil
import sys

from PIL import Image

THEME = "static/build/Themes/Silky RedOnion"
OUT = "static/build/.tmp_update/res/accent"

# name: (accent, light accent, dark accent). Red is the theme's own: the
# light and dark are the ones the programs used with it (#D65670 for text
# on panels, #782036 behind a selected slider); the others are made from
# them the same way as the pictures, so they match.
RED = ("9E2C45", "D65670", "782036")
COLOURS = {
    "blue": "2F5FB3",
    "purple": "6A3FA0",
    "teal": "1F7A75",
    "pink": "A83A78",
    "orange": "B5541C",
}
ORDER = ["red", "blue", "purple", "teal", "pink", "orange"]

# Theme images left red: the RedOnion logo (top bars, brand art) and the
# empty battery, red as a warning whatever the accent.
SKIP = {"miyoo-topbar.png", "power-0%-icon.png"}
SKIP_EXTRA_PREFIX = ("chargingState", "bootScreen", "Screen_Off", "splash-saving", "gs-legend", "gs-top-bar")


def hsv(hexrgb):
    r, g, b = (int(hexrgb[i:i + 2], 16) / 255 for i in (0, 2, 4))
    return colorsys.rgb_to_hsv(r, g, b)


def hexof(rgb):
    return "%02X%02X%02X" % tuple(max(0, min(255, round(c * 255))) for c in rgb)


RED_H, RED_S, RED_V = hsv(RED[0])


def hue_dist(a, b):
    d = abs(a - b) % 1.0
    return min(d, 1.0 - d)


def weight(h, s, v):
    """How much a pixel is the red accent: 1 for clear reds, fading to 0 for
    greys and for hues away from red, so edges blend without a seam."""
    if s < 0.12 or v < 0.06:
        return 0.0
    ws = min(1.0, (s - 0.12) / 0.18)
    d = hue_dist(h, RED_H)
    wh = 1.0 if d <= 0.07 else max(0.0, 1.0 - (d - 0.07) / 0.05)
    return ws * wh


def recolour_pixel(r, g, b, target):
    th, ts, tv = target
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    w = weight(h, s, v)
    if w <= 0:
        return r, g, b
    ns = min(1.0, s * ts / RED_S)
    nv = min(1.0, v * tv / RED_V)
    nr, ng, nb = colorsys.hsv_to_rgb(th, ns, nv)
    return (round(r + (nr * 255 - r) * w), round(g + (ng * 255 - g) * w), round(b + (nb * 255 - b) * w))


def recolour_hex(hexrgb, target):
    r, g, b = (int(hexrgb[i:i + 2], 16) for i in (0, 2, 4))
    return "%02X%02X%02X" % recolour_pixel(r, g, b, target)


def has_accent(im):
    n = 0
    for r, g, b, a in im.getdata():
        if a >= 40 and weight(*colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)) > 0.5:
            n += 1
            if n >= 20:
                return True
    return False


def theme_images():
    skin = os.path.join(THEME, "skin")
    for f in sorted(os.listdir(skin)):
        if f.endswith(".png") and f not in SKIP:
            yield "skin/" + f
    extra = os.path.join(skin, "extra")
    for f in sorted(os.listdir(extra)):
        if f.endswith(".png") and not f.startswith(SKIP_EXTRA_PREFIX):
            yield "skin/extra/" + f


def palette():
    """name -> (accent, light, dark), hex"""
    pal = {"red": RED}
    for name, base in COLOURS.items():
        t = hsv(base)
        pal[name] = (base, recolour_hex(RED[1], t), recolour_hex(RED[2], t))
    return pal


def main():
    images = [p for p in theme_images() if has_accent(Image.open(os.path.join(THEME, p)).convert("RGBA"))]
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    for name, base in COLOURS.items():
        target = hsv(base)
        for rel in images:
            src = Image.open(os.path.join(THEME, rel)).convert("RGBA")
            out = Image.new("RGBA", src.size)
            out.putdata([recolour_pixel(r, g, b, target) + (a,) for r, g, b, a in src.getdata()])
            dst = os.path.join(OUT, name, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            out.save(dst, optimize=True)
    with open(os.path.join(OUT, "palette.txt"), "w") as f:
        f.write("# name accent light dark (RRGGBB), in the Tweaks order. Made by tools/accent/gen_accent.py\n")
        pal = palette()
        for name in ORDER:
            f.write("%s %s %s %s\n" % ((name,) + pal[name]))
    print("%d images x %d colours" % (len(images), len(COLOURS)))
    for name in ORDER:
        print(name, *palette()[name])


if __name__ == "__main__":
    sys.exit(main())
