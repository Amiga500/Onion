# Third-party notices

RedAmp is an MIT-licensed fork of MiniAmp by Keith Snyder. The original license is retained
in `LICENSE`. RedAmp does not transfer upstream copyright to new maintainers.

Dependencies and assets have separate copyright and licensing obligations:

- `dr_mp3`, `dr_flac`, `dr_wav` (David Reid, and minimp3 by Lieff): public domain, MIT-0,
  or CC0 as applicable to the specific source; inspect included headers.
- `stb_vorbis`, `stb_image`, `stb_truetype` (Sean Barrett): public domain or MIT.
- `mini2d`: MIT. Existing upstream source comments and attribution are retained.
- `zlib` (Jean-loup Gailly and Mark Adler): zlib license; see the notice below.
- `Webamp` (Jordan Eldredge): referenced for classic skin layout; MIT notice below.
- `Opus` (Xiph.Org and contributors): BSD-style licensing. See `Opus-notices.txt`.
- `curl` (Daniel Stenberg and contributors): curl license. See `curl-LICENSE.txt`.
- `OpenSSL 3`: Apache License 2.0. See `Apache-2.0.txt`.
- `FFmpeg 4.4.8`: private AAC/HLS decoding, configured with `--disable-gpl`,
  `--disable-nonfree`, and `--enable-version3` for OpenSSL 3. This configuration is
  LGPLv3-compatible. See `LGPL-3.0.txt` and `src/docs/FFMPEG_AUDIO.md`.
- `Rubik` and `Chakra Petch`: SIL Open Font License 1.1. See their `OFL-*.txt` files.
- `DejaVu Sans`: Bitstream Vera license and changes as described in `LICENSE-DejaVu.txt`.
- `RedOnion`, `Breeze Dark` and `Graphite` artwork: CC0, details in
  `../skins/SKIN_CREDITS.md`. Other skin designs are not claimed as bundled.

The source/development archive does not contain ARM FFmpeg or ARM dependency shared
libraries. When redistributing those binaries, preserve exact corresponding source,
license notices, build commands, modifications and library-replacement rights required
by the LGPL. The CA bundle's provenance must also be recorded in a binary release.

Neither MiniAmp nor RedAmp is affiliated with Winamp, Nullsoft or Llama Group.

## Webamp (MIT)

Copyright (c) 2015 Jordan Eldredge

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## zlib

Copyright (C) 1995-2022 Jean-loup Gailly and Mark Adler. This software is
provided 'as-is', without any express or implied warranty. Permission is
granted to anyone to use this software for any purpose, including commercial
applications, and to alter it and redistribute it freely, subject to the
following restrictions: 1. The origin of this software must not be
misrepresented; you must not claim that you wrote the original software.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software. 3. This notice may not be
removed or altered from any source distribution.

