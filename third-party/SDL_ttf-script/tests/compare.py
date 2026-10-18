#!/usr/bin/env python3
"""Compare OnionPlus's SDL_ttf with the original 2.0.11 over every string of
Onion's language files, plus a few game titles, in four fonts and styles and
in every mode (tests/compare.c).

- Text without Arabic or Bengali: the same image and size as the original.
- The same with the script library missing, or the fonts missing: then Arabic
  and Bengali are drawn as the original draws them too.
- Text with Arabic or Bengali: an image, as large as its reported size, that
  differs from the original's.
- Every text: byte-swapped UNICODE gives the same image as UTF-8.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

EXTRA = [
    "Super Mario Bros",
    "Zelda - النسخة العربية v1.2",
    "مغامرات الفارس الشجاع في الصحراء الكبرى (عربي)",
    "রাজকুমারের অভিযান",
    "গেম ২০২৪ সংস্করণ",
    "العربية (Arabic)",
    "বাংলা (Bengali)",
    "نسبة البطارية 50%",
    "العربية (Mario [USA]) الجديد",
    "لعبة (Mario (USA)) ٢",
    "Ünïcödé — 日本語 한국어 Русский",
    "",
    "   ",
]


def complex_text(text):
    return any(0x0600 <= ord(c) <= 0x06FF or 0x0750 <= ord(c) <= 0x077F or
               0x0870 <= ord(c) <= 0x08FF or 0xFB50 <= ord(c) <= 0xFDFF or
               0xFE70 <= ord(c) <= 0xFEFC or 0x0980 <= ord(c) <= 0x09FF for c in text)


def strings(lang_dir):
    seen, out = set(), []
    for path in sorted(Path(lang_dir).glob("*.lang")):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        for value in data.values():
            if isinstance(value, str) and value not in seen:
                seen.add(value)
                out.append(value)
    for value in EXTRA:
        if value not in seen:
            seen.add(value)
            out.append(value)
    # SDL_ttf 2.0 stops at 16-bit code points; keep what it can take.
    return [s for s in out if all(ord(c) < 0x10000 for c in s)]


def run(binary, libdir, listing, fonts, env_extra, sheet=None, rows=()):
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = libdir + ":" + env.get("LD_LIBRARY_PATH", "")
    env.update(env_extra)
    args = [binary, listing, fonts]
    if sheet:
        args += [sheet] + [str(r) for r in rows]
    result = subprocess.run(args, env=env, capture_output=True, text=True, timeout=1200)
    if result.returncode:
        print(result.stdout[-2000:], result.stderr[-4000:], file=sys.stderr)
        raise SystemExit(f"compare failed with {libdir}")
    lines = {}
    for line in result.stdout.splitlines():
        p = line.split()
        lines[(int(p[0]), int(p[1]))] = p[2:]
    return lines, result.stderr


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--lang", required=True)
    parser.add_argument("--fonts", required=True)
    parser.add_argument("--sheet")
    args = parser.parse_args()
    build = Path(args.build).resolve()
    texts = strings(args.lang)
    work = Path(tempfile.mkdtemp(prefix="ttf-compare-"))
    listing = work / "strings.txt"
    listing.write_text("".join(t.encode().hex() + "\n" for t in texts))
    binary = str(build / "compare")

    original, _ = run(binary, str(build / "original"), str(listing), args.fonts, {})
    sheet_rows = [i for i, t in enumerate(texts) if t in EXTRA[1:7]]
    patched, err = run(binary, str(build / "patched"), str(listing), args.fonts,
                       {"TTF_SCRIPT_FONTS": args.fonts}, args.sheet, sheet_rows)
    assert "unavailable" not in err and "cannot open" not in err, err
    nofonts, err_fonts = run(binary, str(build / "patched"), str(listing), args.fonts,
                             {"TTF_SCRIPT_FONTS": str(work / "none")})
    lone = work / "lone"
    lone.mkdir()
    shutil.copy2(build / "patched/libSDL_ttf-2.0.so.0", lone / "libSDL_ttf-2.0.so.0")
    nolib, err_lib = run(binary, str(lone), str(listing), args.fonts,
                         {"TTF_SCRIPT_FONTS": args.fonts})

    simple = [i for i, t in enumerate(texts) if not complex_text(t)]
    shaped = [i for i, t in enumerate(texts) if complex_text(t)]
    configs = len({c for _, c in original})
    failures = []
    for (i, c), want in original.items():
        if i in simple and patched[(i, c)] != want:
            failures.append(f"text {i} config {c} changed: {texts[i]!r}")
        if nofonts[(i, c)] != want:
            failures.append(f"text {i} config {c}: no fonts differs from the original")
        if nolib[(i, c)] != want:
            failures.append(f"text {i} config {c}: no library differs from the original")
        got = patched[(i, c)]
        if got[got.index("swap") + 1] != "1":
            failures.append(f"text {i} config {c}: byte-swapped UNICODE differs")
        if got[got.index("fits") + 1] != "1":
            failures.append(f"text {i} config {c}: surface size differs from TTF_SizeUTF8")
    unchanged = [(i, c) for i in shaped for c in range(configs)
                 if patched[(i, c)][5] == original[(i, c)][5]]
    nulls = [(i, c) for i in shaped for c in range(configs)
             if patched[(i, c)][5] == "0000000000000000"]
    # Arabic or Bengali drawn: a different image, never none.
    failures += [f"text {i} config {c} not shaped: {texts[i]!r}" for i, c in unchanged]
    failures += [f"text {i} config {c} not drawn: {texts[i]!r}" for i, c in nulls]
    if "cannot open script font" not in err_fonts or err_fonts.count("cannot open") > 2:
        failures.append("missing fonts: expected one message per script, got: " + err_fonts)
    if err_lib.count("unavailable") != 1:
        failures.append("missing library: expected one message, got: " + err_lib)

    print(f"{len(texts)} strings x {configs} fonts x 3 modes: {len(simple)} without Arabic or "
          f"Bengali identical to SDL_ttf 2.0.11, {len(shaped)} shaped")
    if failures:
        print("\n".join(failures[:40]), file=sys.stderr)
        print(f"{len(failures)} failure(s)", file=sys.stderr)
        return 1
    print("missing library or fonts: identical to SDL_ttf 2.0.11 for every string")
    shutil.rmtree(work)
    return 0


if __name__ == "__main__":
    sys.exit(main())
