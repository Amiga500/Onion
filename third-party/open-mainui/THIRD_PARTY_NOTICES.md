# Third-party notices

Open MainUI is GPL-3.0-only; see LICENSE. The following components retain their own upstream terms.

## cJSON

cJSON 1.7.19 is included in vendor/cjson under the MIT license. Its source headers retain the complete upstream notice, reproduced below.

Upstream: https://github.com/DaveGamble/cJSON/tree/v1.7.19

Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## SQLite

SQLite is dedicated to the public domain by its authors. The device build links the SQLite library supplied by Onion; SQLite source is not vendored here. Host builds link the host's SQLite library.

Upstream dedication: https://www.sqlite.org/copyright.html

## FFmpeg

Game Info's media gallery plays videos with the FFmpeg 2.4 libraries Onion ships in `.tmp_update/lib/parasyte` (libavformat, libavcodec, libavutil, libswscale and, for sound, libswresample), loaded at run time with `dlopen()`.
FFmpeg is neither linked nor distributed with Open MainUI, and no FFmpeg source is vendored here. [src/platform/ffmpeg_abi.h](src/platform/ffmpeg_abi.h) declares the few FFmpeg 2.4.14 structure fields and functions used, to match those libraries.

FFmpeg is licensed under the LGPL 2.1 or later, or the GPL depending on its build: https://ffmpeg.org/legal.html
