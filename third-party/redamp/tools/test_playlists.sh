#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
B=build/tests/playlists
mkdir -p "$B"
cat > "$B/stations.m3u" <<'M3U'
#EXTM3U
#EXTINF:-1,Jazz Station
https://example.invalid/jazz.mp3
#EXTINF:-1,Opus Station
http://example.invalid/live.opus
M3U
cat > "$B/stations.pls" <<'PLS'
[playlist]
File1=https://example.invalid/live
Title1=Test Station
NumberOfEntries=1
Version=2
PLS
printf '#EXTM3U\n# no stations\n' > "$B/saved.m3u"
${CC:-gcc} -O2 -g -I. tools/test_playlists.c playlist.c -o "$B/test_playlists" -lpthread
"$B/test_playlists" "$B/stations.m3u" "$B/stations.pls" "$B/saved.m3u"
