#!/usr/bin/env python3
"""Writes ui_font_data.h: DisplayCal's three UI fonts as 4-bit alpha masks.

Test branch (RedOnion unified look): Exo 2 Bold Italic, the font of the rest
of the system, instead of Lato. Sizes 15, 19 and 25 px give the capital
heights of the three Lato fonts (11, 14, 18 px), and the capitals' tops are
kept where Lato had them, so the layout does not move.

Usage: python3 make_ui_font.py EXO2_TTF > ui_font_data.h
Same format as before: struct fntglyph { off (in nibbles), w, h, xo, yo, adv },
glyphs for ASCII 32-126, nibbles packed two per byte, high nibble first.
"""
import sys
from PIL import ImageFont

SIZES = (15, 19, 25)
CAP_TOP = (5, 5, 7)          # yo of 'H' in the Lato tables

nibbles = []
fonts = []
for size, cap_top in zip(SIZES, CAP_TOP):
    font = ImageFont.truetype(sys.argv[1], size)
    _, (_, h_oy) = font.getmask2('H', anchor='ls')
    baseline = cap_top - h_oy            # h_oy is negative: top above baseline
    glyphs = []
    for code in range(32, 127):
        ch = chr(code)
        mask, (ox, oy) = font.getmask2(ch, anchor='ls')
        w, h = mask.size
        adv = int(round(font.getlength(ch)))
        if ch == ' ':
            w, h = adv, 0
        glyphs.append((len(nibbles), w, h, ox, baseline + oy, adv))
        for y in range(h):
            for x in range(w):
                nibbles.append((mask.getpixel((x, y)) * 15 + 127) // 255)
    fonts.append(glyphs)
if len(nibbles) % 2:
    nibbles.append(0)
data = [(nibbles[i] << 4) | nibbles[i + 1] for i in range(0, len(nibbles), 2)]

out = ["/* Rasterized Exo 2 Bold Italic (SIL Open Font License) glyph alpha masks",
       " * for the UI, written by make_ui_font.py; no font files distributed. */",
       "struct fntglyph { u32 off; u8 w,h; signed char xo,yo; u8 adv; };",
       "static const u8 glyph_a4[] = {"]
for i in range(0, len(data), 20):
    out.append(",".join("0x%02x" % b for b in data[i:i + 20]) + ",")
out.append("};")
out.append("static const struct fntglyph ui_fonts[3][95] = {")
for glyphs in fonts:
    out.append("{")
    for i in range(0, len(glyphs), 6):
        out.append("".join("{%d,%d,%d,%d,%d,%d}," % g for g in glyphs[i:i + 6]))
    out.append("},")
out.append("};")
print("\n".join(out))
