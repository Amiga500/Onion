#!/usr/bin/env python3
"""Silky RedOnion's Quick Guide pages (640x480): one background colour all
over, and "< > PAGE" after the A/B hints, as the pages also turn with
LEFT/RIGHT.

The header and footer bands were (30, 30, 30) on a (28, 28, 30) page
((29, 29, 29) on page 3): a lighter bar showed at the bottom on the
device. Both become the page colour of the apps, CUI_BG (28, 28, 30).

  python3 tools/artwork/quick_guide_footer.py page1.png [page2.png ...]

Run once on the pages as they were (it finds the hints by their light
pixels and adds the pill after the last one).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

FONT = "static/build/miyoo/app/Exo-2-Bold-Italic.ttf"  # the theme's hint font
BG = (28, 28, 30)          # CUI_BG
BANDS = ((30, 30, 30), (29, 29, 29))
ACCENT = (158, 44, 69)     # the A/B buttons
WHITE = (244, 244, 245)
CY = 449.5                 # the buttons' centre
S = 4                      # supersampling for the pill


def flatten(im):
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            p = px[x, y]
            for b in BANDS:
                if max(abs(p[i] - b[i]) for i in range(3)) <= 1:
                    px[x, y] = BG
                    break


def hints_end(im):
    """Right end of the last hint label in the footer."""
    end = 0
    for x in range(im.width):
        if any(sum(im.getpixel((x, y))) > 380 for y in range(430, 470)):
            end = x
    return end


def pill(x, w=40, h=28):
    big = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    d.rounded_rectangle([0, 0, w * S - 1, h * S - 1], radius=h * S // 2, fill=ACCENT + (255,))
    cx, cy, arm, half, lw = w * S / 2, h * S / 2, 5 * S, 5 * S, int(2.6 * S)
    for tip, back in ((cx - 9 * S + lw / 2, cx - 9 * S + arm + lw / 2),
                      (cx + 9 * S - lw / 2, cx + 9 * S - arm - lw / 2)):
        pts = [(back, cy - half), (tip, cy), (back, cy + half)]
        d.line(pts, fill=WHITE + (255,), width=lw, joint="curve")
        for p in (pts[0], pts[2]):
            d.ellipse([p[0] - lw / 2, p[1] - lw / 2, p[0] + lw / 2, p[1] + lw / 2], fill=WHITE + (255,))
    return big.resize((w, h), Image.LANCZOS)


def main(paths):
    font = ImageFont.truetype(FONT, 21)
    for path in paths:
        im = Image.open(path).convert("RGB")
        flatten(im)
        x = hints_end(im) + 21          # the gap between two hints
        p = pill(x)
        im.paste(p, (x, int(CY - p.height / 2 + 0.5)), p)
        ImageDraw.Draw(im).text((x + p.width + 7, 437), "PAGE", font=font, fill=WHITE)
        im.save(path, optimize=True)


if __name__ == "__main__":
    main(sys.argv[1:])
