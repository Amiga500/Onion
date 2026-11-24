#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/art-test/folder build/art-test/mp3 build/art-test/flac
python3 - <<'PY'
from PIL import Image
from pathlib import Path
p=Path('build/art-test')
for name,wh in [('folder/cover.png',16),('mp3/cover.png',24),('flac/cover.png',32)]:
 im=Image.new('RGB',(wh,wh),'red');im.save(p/name)
# MP3 APIC has a MIME string, picture type, description, image data.
img=(p/'mp3/cover.png').read_bytes()
apic=b'\0image/png\0\3\0'+img
frame=b'APIC'+len(apic).to_bytes(4,'big')+b'\0\0'+apic
sz=len(frame)
head=b'ID3\3\0\0'+bytes(((sz>>21)&127,(sz>>14)&127,(sz>>7)&127,sz&127))
(p/'mp3/sample.mp3').write_bytes(head+frame)
# FLAC Picture (block type 6, last-block flag)
img=(p/'flac/cover.png').read_bytes()
pict=b'\0\0\0\3'+len(b'image/png').to_bytes(4,'big')+b'image/png'+(0).to_bytes(4,'big')+b''.join((32).to_bytes(4,'big') for i in range(3))+(0).to_bytes(4,'big')+len(img).to_bytes(4,'big')+img
flac=b'fLaC'+bytes([0x86])+len(pict).to_bytes(3,'big')+pict
(p/'flac/sample.flac').write_bytes(flac)
# Cache and folder cover precedence: a user folder cover wins over a downloaded cache.
folder=p/'priority'
folder.mkdir(exist_ok=True)
(folder/'song.mp3').write_bytes(b'placeholder')
h=14695981039346656037
for ch in b'sample artist\0sample album\0':h=((h^ch)*1099511628211)&((1<<64)-1)
cache=p/'prioritydata'/'covers';cache.mkdir(parents=True,exist_ok=True)
Image.new('RGB',(35,35),'blue').save(cache/f'album-{h:016x}.jpg')
Image.new('RGB',(42,42),'red').save(folder/'cover.jpg')
mixed=p/'mixed';mixed.mkdir(exist_ok=True)
Image.new('RGB',(42,42),'red').save(mixed/'cover.jpg')
for name in ('first','second','missing'):(mixed/(name+'.mp3')).write_bytes(b'placeholder')
for album,size in [('Sample Album',35),('Second Album',36)]:
 h=14695981039346656037
 for b in b'sample artist\0'+album.lower().encode()+b'\0':h=((h^b)*1099511628211)&((1<<64)-1)
 Image.new('RGB',(size,size),'blue').save(cache/f'album-{h:016x}.jpg')
PY
${CC:-gcc} -O1 -g -Ithird_party -I. -fsanitize=address,undefined -fno-omit-frame-pointer \
 album_art.c tools/test_album_art.c -o build/test_album_art -lm
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/test_album_art \
 build/art-test/folder/sample.mp3 build/art-test/mp3/sample.mp3 build/art-test/flac/sample.flac build/art-test/prioritydata build/art-test/priority/song.mp3 build/art-test/mixed/first.mp3 build/art-test/mixed/second.mp3 build/art-test/mixed/missing.mp3
