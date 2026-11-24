#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
python3 - <<'PY'
import zipfile, struct
from pathlib import Path
root=Path('build/tests')
with zipfile.ZipFile(root/'good.zip','w',compression=zipfile.ZIP_STORED) as z:
    z.writestr('main.bmp', b'test')
v=bytearray((root/'good.zip').read_bytes())
p=v.find(b'PK\x01\x02'); assert p >= 0
# Central directory says 256 output bytes, but only 4 bytes are stored.
bad=v[:]; struct.pack_into('<I',bad,p+24,256)
(root/'bad-store.zip').write_bytes(bad)
with zipfile.ZipFile(root/'deflate.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
    z.writestr('main.bmp',b'test')
v=bytearray((root/'deflate.zip').read_bytes())
p=v.find(b'PK\x01\x02'); assert p>=0
bad=v[:]; struct.pack_into('<I',bad,p+24,999)
(root/'bad-deflate.zip').write_bytes(bad)
PY
${CC:-gcc} -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
  -I. -Ithird_party tools/test_hardening.c zip.c -Wl,--gc-sections -lz -lm -o build/tests/test_hardening
ASAN_OPTIONS=detect_leaks=0 build/tests/test_hardening \
  build/tests/good.zip build/tests/bad-store.zip build/tests/bad-deflate.zip
