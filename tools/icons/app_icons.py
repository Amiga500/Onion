#!/usr/bin/env python3
"""App icons (74x74) in the Silky RedOnion / Default sticker style: each
part outlined, a white sticker border around them all, a drop shadow. As
tools/icons/clock_icon.py; drawn 8x and scaled down.

  python3 tools/icons/app_icons.py backup silky  "static/build/Themes/Silky RedOnion/icons/app/backup.png"
  python3 tools/icons/app_icons.py backup default "static/packages/App/Backup (Saves and settings)/Icons/Default/app/backup.png"
  python3 tools/icons/app_icons.py kidsmode silky "static/build/Themes/Silky RedOnion/icons/app/kidsmode.png"
"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

S = 8        # supersampling
N = 74       # icon size
OUT = 2.0    # outline width (px)
STICK = 3.5  # white sticker border (px)

PALETTES = {
    "silky": dict(outline=(86, 41, 51), body=(211, 98, 123), shade=(163, 64, 85),
                  face=(217, 238, 255), white=(250, 250, 250), sticker=(242, 242, 242),
                  edge=(219, 219, 219)),
    "default": dict(outline=(64, 57, 110), body=(154, 162, 230), shade=(112, 124, 192),
                    face=(217, 238, 255), white=(250, 250, 250), sticker=(242, 242, 242),
                    edge=(219, 219, 219)),
}


# ---- parts: (kind, geometry, colour name); grown by g for the outline and
# the sticker. Details (no outline, not in the sticker) have colour names
# starting with "-".
def draw_part(d, part, grow, colour):
    kind, geo = part[0], part[1]
    if kind == "circle":
        cx, cy, r = geo
        r += grow
        d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=colour)
    elif kind == "ellipse":
        cx, cy, rx, ry = geo
        rx += grow
        ry += grow
        d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=colour)
    elif kind == "rrect":
        x0, y0, x1, y1, rad = geo
        d.rounded_rectangle([(x0 - grow) * S, (y0 - grow) * S, (x1 + grow) * S, (y1 + grow) * S],
                            radius=(rad + grow) * S, fill=colour)
    elif kind == "line":
        x0, y0, x1, y1, w = geo
        w += 2 * grow
        d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=colour, width=int(round(w * S)))
        for x, y in ((x0, y0), (x1, y1)):
            d.ellipse([(x - w / 2) * S, (y - w / 2) * S, (x + w / 2) * S, (y + w / 2) * S], fill=colour)
    elif kind == "arc":  # cx, cy, r, start, end (degrees), width
        cx, cy, r, a0, a1, w = geo
        w += 2 * grow
        d.arc([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], a0, a1,
              fill=colour, width=int(round(w * S)))
        for a in (a0, a1):  # round ends
            x, y = cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))
            d.ellipse([(x - w / 2) * S, (y - w / 2) * S, (x + w / 2) * S, (y + w / 2) * S], fill=colour)
    elif kind == "poly":
        d.polygon([(x * S, y * S) for x, y in geo], fill=colour)


def render(parts, pal):
    big = N * S
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    outlined = [p for p in parts if not p[2].startswith("-")]

    sil = Image.new("L", (big, big), 0)
    ds = ImageDraw.Draw(sil)
    for part in outlined:
        draw_part(ds, part, OUT + STICK, 255)
    shadow = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 90), (0, 0), sil.transform(sil.size, Image.AFFINE, (1, 0, 0, 0, 1, -2 * S)))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(S * 0.8)))
    edge = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    edge.paste(pal["edge"] + (255,), (0, 0), sil.transform(sil.size, Image.AFFINE, (1, 0, 0, 0, 1, -S)))
    img.alpha_composite(edge)
    stick = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    stick.paste(pal["sticker"] + (255,), (0, 0), sil)
    img.alpha_composite(stick)

    d = ImageDraw.Draw(img)
    for part in parts:
        name = part[2]
        if name.startswith("-"):
            draw_part(d, part, 0, pal[name[1:]])
        else:
            draw_part(d, part, OUT, pal["outline"])
            draw_part(d, part, 0, pal[name])
    return img.resize((N, N), Image.LANCZOS)


def backup():
    """A floppy disk, a round arrow on a badge: saves kept and put back."""
    return [
        ("rrect", (10, 9, 58, 60, 5), "body"),
        ("rrect", (21, 9, 47, 26, 2), "face"),
        ("rrect", (37, 13, 43, 22, 1), "-outline"),
        ("rrect", (16, 35, 52, 60, 3), "white"),
        ("line", (22, 43, 46, 43, 2), "-shade"),
        ("line", (22, 50, 40, 50, 2), "-shade"),
        ("circle", (54, 54, 13), "shade"),
        ("arc", (54, 54, 7, 200, 470, 2.6), "-white"),
        ("poly", [(46.5, 47.5), (52, 51), (46, 53.5)], "-white"),
    ]


def kidsmode():
    """A teddy bear's head."""
    return [
        ("circle", (19, 20, 10), "body"),
        ("circle", (55, 20, 10), "body"),
        ("circle", (19, 20, 4.5), "-shade"),
        ("circle", (55, 20, 4.5), "-shade"),
        ("circle", (37, 40, 25), "face"),
        ("ellipse", (37, 50, 11, 8.5), "white"),
        ("circle", (28, 36, 3.2), "-outline"),
        ("circle", (46, 36, 3.2), "-outline"),
        ("ellipse", (37, 46.5, 4.2, 3.2), "-outline"),
        ("line", (37, 49.5, 37, 53, 1.6), "-outline"),
        ("arc", (37, 50.5, 5, 30, 150, 1.6), "-outline"),
        ("circle", (22, 47, 3.4), "-body"),
        ("circle", (52, 47, 3.4), "-body"),
    ]


ICONS = {"backup": backup, "kidsmode": kidsmode}

if __name__ == "__main__":
    name, palette, dest = sys.argv[1:4]
    render(ICONS[name](), PALETTES[palette]).save(dest, optimize=True)
