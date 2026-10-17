# SDL_ttf with Arabic and Bengali

OnionPlus ships its own build of SDL_ttf 2.0.11, the library every Onion program
and Open MainUI draw text with. It is the original 2.0.11 plus one addition: text
with Arabic or Bengali letters is shaped and laid out properly. Every other text
is drawn by the original code, unchanged.

SDL_ttf 2.0 draws one glyph per character, left to right. Arabic letters came out
unjoined and in the wrong order, Bengali vowel signs and conjuncts in separate
pieces, and no font Onion ships has those letters: both languages showed boxes.

## How it works

- The four functions every text call ends in (`TTF_SizeUNICODE` and
  `TTF_RenderUNICODE_Solid`/`Shaded`/`Blended`, which the `Text` and `UTF8`
  variants call) first look for Arabic or Bengali letters. Without them, the text
  goes to the original code, renamed `plain_*` in `SDL_ttf.c`.
- With them, `ttf_bidi.c` splits the text into runs in display order (a reduced
  Unicode Bidirectional Algorithm, with paired brackets). Arabic and Bengali runs
  are shaped with HarfBuzz and drawn with FreeType by `libSDL_ttf-script.so`, in
  Noto Sans Arabic UI or Noto Sans Bengali UI at the font's size; the rest is
  drawn by the original code in the caller's font. `ttf_script.c` puts the runs
  together in the mode asked for, in the same surface formats as the original.
- `libSDL_ttf-script.so` (HarfBuzz, about 240 KB) is opened only the first time
  such text is drawn, from the folder `libSDL_ttf-2.0.so.0` is in. Programs that
  never draw Arabic or Bengali never load it.
- The fonts are read from `/mnt/SDCARD/miyoo/app` (`TTF_SCRIPT_FONTS` overrides
  it). Without the library or a font, the text is drawn as before and one line
  says why on stderr.

Bold faces and a bold style use the Bold files; an italic style is slanted as
SDL_ttf slants. A font with an outline (`TTF_SetFontOutline`) keeps the original
code. Glyph functions (`TTF_RenderGlyph_*`, `TTF_GlyphMetrics`) are unchanged:
they draw one character, which cannot be shaped.

## Files

| File | |
| --- | --- |
| `SDL_ttf.c`, `SDL_ttf.h` | SDL_ttf 2.0.11; the changes in `SDL_ttf.c` are marked "OnionPlus" |
| `upstream/SDL_ttf.c` | the original 2.0.11, for the comparison and for `diff` |
| `ttf_script.c`, `ttf_bidi.c` | the addition, included at the end of `SDL_ttf.c` |
| `script/` | `libSDL_ttf-script.so` |
| `harfbuzz/` | HarfBuzz 11.2.1, the files its single-file build needs |
| `upstream/miyoo-symbols.txt` | the functions Miyoo's SDL_ttf exports, which the build checks |

The SDL_ttf Onion shipped before was Miyoo's build of the same 2.0.11, without
optimisation: same 44 functions, and the same code size as 2.0.11 built at `-O0`.

## Build and check

```sh
make device    # build/device, inside the Miyoo toolchain
make install   # over static/build/miyoo/lib (three copies of SDL_ttf + the script library)
make check     # host: unit tests, then every string of Onion's language files in
               # four fonts and three modes against the original 2.0.11
```

`make check` passes when text without Arabic or Bengali gives the same images and
sizes as the original 2.0.11, when everything does without the library or the
fonts, and when Arabic and Bengali text is drawn. Run it under the sanitizers
with `make check HOST=build/asan SANITIZE="-fsanitize=address,undefined"`.

The libraries in `static/build/miyoo/lib` are committed, as Onion does for its
other libraries: rebuild them with `make install` after a change here.

## Licences

SDL_ttf and the files added to it are under the zlib licence
(`COPYING-SDL_ttf`). HarfBuzz is under the "Old MIT" licence
(`harfbuzz/COPYING`). The fonts are under the SIL Open Font License 1.1
(`static/build/miyoo/app/NotoSans-OFL.txt`).
