#!/usr/bin/env python3
"""Clock app icon (74x74) in the Silky / Default sticker style: an alarm
clock with two bells, its face at 10:10. Drawn 8x and scaled down.

  python3 tools/icons/clock_icon.py \
    silky "static/build/Themes/Silky RedOnion/icons/app/clock.png" \
    default static/build/Icons/Default/app/clock.png
"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

S = 8          # supersampling
N = 74         # icon size
OUT = 2.0      # outline width (px)
STICK = 3.5    # white sticker border (px)

PALETTES = {
    "silky": dict(outline=(86, 41, 51), body=(211, 98, 123), shade=(163, 64, 85),
                  face=(217, 238, 255), sticker=(242, 242, 242), edge=(219, 219, 219)),
    "default": dict(outline=(64, 57, 110), body=(154, 162, 230), shade=(112, 124, 192),
                    face=(217, 238, 255), sticker=(242, 242, 242), edge=(219, 219, 219)),
}

CX, CY, R = 37, 38.5, 22.5        # body
FACE = 16.5                      # face radius


def circle(d, cx, cy, r, fill):
    d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=fill)


def line(d, x1, y1, x2, y2, w, fill):
    d.line([x1 * S, y1 * S, x2 * S, y2 * S], fill=fill, width=int(round(w * S)))
    for x, y in ((x1, y1), (x2, y2)):
        circle(d, x, y, w / 2, fill)


def bell_pos(side):
    a = math.radians(135 if side < 0 else 45)
    return CX + math.cos(a) * (R - 1), CY - math.sin(a) * (R - 1)


def shapes(d, grow, col):
    """Every part of the silhouette, grown by `grow` px (outline/sticker)."""
    for side in (-1, 1):
        bx, by = bell_pos(side)
        circle(d, bx, by, 9 + grow, col("body"))
    # handle on top
    line(d, CX, CY - R - 1, CX, CY - R - 5, 3 + 2 * grow, col("outline"))
    circle(d, CX, CY - R - 5.5, 2.5 + grow, col("body"))
    # feet
    for side in (-1, 1):
        fx = CX + side * 13
        line(d, fx, CY + 16, fx + side * 4.5, CY + 24, 4 + 2 * grow, col("shade"))
    circle(d, CX, CY, R + grow, col("body"))


def render(pal):
    big = N * S
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))

    # white sticker + its grey bottom edge and drop shadow
    sil = Image.new("L", (big, big), 0)
    shapes(ImageDraw.Draw(sil), OUT + STICK, lambda c: 255)
    shadow = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 90), (0, 0), sil.point(lambda a: a).transform(
        sil.size, Image.AFFINE, (1, 0, 0, 0, 1, -2 * S)))
    shadow = shadow.filter(ImageFilter.GaussianBlur(S * 0.8))
    img.alpha_composite(shadow)
    edge = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    edge.paste(pal["edge"] + (255,), (0, 0), sil.transform(
        sil.size, Image.AFFINE, (1, 0, 0, 0, 1, -1 * S)))
    img.alpha_composite(edge)
    stick = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    stick.paste(pal["sticker"] + (255,), (0, 0), sil)
    img.alpha_composite(stick)

    d = ImageDraw.Draw(img)
    o = pal["outline"]

    # bells: outline, then fill, then a darker lower half where the body
    # casts no light
    for side in (-1, 1):
        bx, by = bell_pos(side)
        circle(d, bx, by, 9 + OUT, o)
    for side in (-1, 1):
        bx, by = bell_pos(side)
        circle(d, bx, by, 9, pal["body"])
        circle(d, bx + 2.5, by + 2.5, 5.5, pal["shade"])
        circle(d, bx - 1, by - 1, 6.5, pal["body"])
    # handle
    line(d, CX, CY - R - 1, CX, CY - R - 5, 3 + 2 * OUT, o)
    circle(d, CX, CY - R - 5.5, 2.5 + OUT, o)
    line(d, CX, CY - R - 1, CX, CY - R - 5, 3, pal["shade"])
    circle(d, CX, CY - R - 5.5, 2.5, pal["body"])
    # feet
    for side in (-1, 1):
        fx = CX + side * 13
        line(d, fx, CY + 16, fx + side * 4.5, CY + 24, 4 + 2 * OUT, o)
    for side in (-1, 1):
        fx = CX + side * 13
        line(d, fx, CY + 16, fx + side * 4.5, CY + 24, 4, pal["shade"])
    # body ring, shaded on the lower right
    circle(d, CX, CY, R + OUT, o)
    circle(d, CX, CY, R, pal["shade"])
    circle(d, CX - 1.6, CY - 1.6, R - 1.6, pal["body"])
    # face
    circle(d, CX, CY, FACE + OUT, o)
    circle(d, CX, CY, FACE, pal["face"])
    # hour marks: 12, 3, 6, 9 longer, the others dots
    for h in range(12):
        a = math.radians(90 - h * 30)
        ca, sa = math.cos(a), -math.sin(a)
        if h % 3 == 0:
            line(d, CX + ca * (FACE - 4.5), CY + sa * (FACE - 4.5),
                 CX + ca * (FACE - 2.5), CY + sa * (FACE - 2.5), 2, o)
        else:
            circle(d, CX + ca * (FACE - 3.2), CY + sa * (FACE - 3.2), 0.9, o)
    # hands at 10:10
    for h, length, w in ((10 + 10 / 60, 8.5, 2.6), (2, 11.5, 2.2)):
        a = math.radians(90 - h * 30)
        line(d, CX, CY, CX + math.cos(a) * length, CY - math.sin(a) * length, w, o)
    circle(d, CX, CY, 2.6, o)
    circle(d, CX, CY, 1.4, pal["body"])

    return img.resize((N, N), Image.LANCZOS)


if __name__ == "__main__":
    for name, dest in zip(sys.argv[1::2], sys.argv[2::2]):
        render(PALETTES[name]).save(dest, optimize=True)
