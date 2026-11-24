#!/usr/bin/env python3
"""Verify that built-in EQ profiles are varied and leave digital headroom.

Matches the cascaded peaking-biquad design in audio.c. This is a frequency-
response test only; it is not a substitute for listening on Miyoo hardware.
"""
import cmath
import math
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = (ROOT / 'main.c').read_text()
FREQ = [60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000]
RATE = 48000


def gain_db(bands, pre, f):
    omega = 2 * math.pi * f / RATE
    z = cmath.exp(-1j * omega)
    h = complex(1)
    for center, db in zip(FREQ, bands):
        if not db:
            continue
        A = 10 ** (db / 40)
        w0 = 2 * math.pi * center / RATE
        alpha = math.sin(w0) / (2 * 1.2)
        c = math.cos(w0)
        a0 = 1 + alpha / A
        b0 = (1 + alpha * A) / a0
        b1 = -2 * c / a0
        b2 = (1 - alpha * A) / a0
        a1 = -2 * c / a0
        a2 = (1 - alpha / A) / a0
        h *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * math.log10(abs(h)) + pre


def main():
    source = SRC.split('static const eq_preset PRESETS[] = {', 1)[1].split('};', 1)[0]
    entries = re.findall(r'\{\s*"([^"]+)",\s*([-+\d.]+),\s*\{([^{}]+)\}\s*\}', source)
    assert len(entries) == 11, f'expected 11 EQ presets, got {len(entries)}'
    curves = {}
    for name, pre_str, bands_str in entries:
        pre = float(pre_str)
        bands = [float(x.strip()) for x in bands_str.split(',')]
        assert len(bands) == 10, f'{name}: expected ten bands'
        assert all(-12 <= v <= 12 for v in [pre, *bands]), f'{name}: out of range'
        logfreq = [25 * (20000 / 25) ** (i / 512) for i in range(513)]
        top = max(gain_db(bands, pre, f) for f in logfreq)
        assert top <= 0.5, f'{name}: possible clipping: {top:.1f} dB peak gain'
        curves[name] = [gain_db(bands, 0, f) for f in [100, 250, 700, 1500, 3500, 7500]]
    assert all(v == 0 for v in curves['Flat']), 'Flat must be neutral'
    for name, curve in curves.items():
        if name == 'Flat':
            continue
        assert max(curve) - min(curve) >= 5.0, f'{name}: little tonal contrast'
    assert curves['Bass Boost'][0] > curves['Bass Boost'][4] + 7
    assert curves['Treble Boost'][4] > curves['Treble Boost'][0] + 5
    assert curves['Vocal'][2] > curves['Vocal'][0] + 5
    print('PASS 11 EQ presets: pronounced tonal profiles within available headroom')


if __name__ == '__main__':
    main()
