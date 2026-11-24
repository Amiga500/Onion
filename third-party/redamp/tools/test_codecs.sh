#!/bin/sh
# Desktop-only local Opus + streaming format regression suite.
set -eu
cd "$(dirname "$0")/.."
command -v ffmpeg >/dev/null || { echo 'ffmpeg needed for host codec tests' >&2; exit 1; }
command -v python3 >/dev/null || exit 1
mkdir -p build/tests/codecs
B=build/tests/codecs
ffmpeg -nostdin -loglevel error -f lavfi -i 'sine=frequency=480:duration=2.3' \
    -c:a libopus -b:a 64k -metadata title='Opus Unit Test' -metadata artist=RedAmp \
    -metadata album=Testing -y "$B/test.opus"
ffmpeg -nostdin -loglevel error -f lavfi -i 'sine=frequency=600:duration=3.0' \
    -c:a libmp3lame -b:a 96k -y "$B/test.mp3"
ffmpeg -nostdin -loglevel error -f lavfi -i 'sine=frequency=730:duration=3.0' \
    -c:a libvorbis -q:a 3 -y "$B/test.ogg"
${CC:-gcc} -O2 -g -I. -Ithird_party tools/test_decode.c \
    decode.c meta.c opus_ogg.c radio.c ffaudio.c netclock.c -DM2D_HOST -o "$B/test_decode" -lm -ldl -lpthread -lcurl -lz
"$B/test_decode" "$B/test.opus" > "$B/opus-decode.txt"
grep -q 'RedAmp - Opus Unit Test' "$B/opus-decode.txt"
grep -q 'full decode frames 110400' "$B/opus-decode.txt"
${CC:-gcc} -O0 -g \
    -I. -Ithird_party tools/test_radio.c decode.c meta.c opus_ogg.c radio.c ffaudio.c netclock.c -DM2D_HOST \
    -o "$B/test_radio" -lm -ldl -lpthread -lcurl -lz
rm -f "$B/port"
python3 tools/local_radio_server.py "$B" "$B/port" > "$B/server.log" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null || true' EXIT INT TERM
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    [ -s "$B/port" ] && break
    sleep 0.1
done
[ -s "$B/port" ] || { echo 'HTTP test server failed to start' >&2; exit 1; }
PORT=$(cat "$B/port")
ASAN_OPTIONS=detect_leaks=0 "$B/test_radio" \
    "http://127.0.0.1:$PORT/mp3" "http://127.0.0.1:$PORT/ogg" \
    "http://127.0.0.1:$PORT/opus" "http://127.0.0.1:$PORT/icy" \
    "http://127.0.0.1:$PORT/redirect" "http://127.0.0.1:$PORT/rawicy"
# An HTTP 200 HTML error page must not reach the decoder.
if "$B/test_radio" "http://127.0.0.1:$PORT/html" > "$B/html.log" 2>&1; then
    echo "HTML response unexpectedly decoded" >&2; exit 1
fi
grep -q "server returned non-audio content" "$B/html.log"
# Real chained Ogg logical bitstreams: validate stream transition and titles.
ffmpeg -nostdin -loglevel error -f lavfi -i 'sine=frequency=1200:duration=2.5' \
    -ac 2 -c:a libvorbis -metadata title=Second -y "$B/test2.ogg"
ffmpeg -nostdin -loglevel error -f lavfi -i 'sine=frequency=1200:duration=2.5' \
    -ac 2 -c:a libopus -metadata title=Second -y "$B/test2.opus"
cat "$B/test.ogg" "$B/test2.ogg" > "$B/oggchain.ogg"
cat "$B/test.opus" "$B/test2.opus" > "$B/opuschain.opus"
${CC:-gcc} -O1 -g -I. -Ithird_party tools/test_ogg_chain.c \
    decode.c meta.c opus_ogg.c radio.c ffaudio.c netclock.c -DM2D_HOST -o "$B/test_ogg_chain" -lm -ldl -lpthread -lcurl -lz
"$B/test_ogg_chain" "http://127.0.0.1:$PORT/oggchain"
"$B/test_ogg_chain" "http://127.0.0.1:$PORT/opuschain"
echo 'Opus metadata/decode; HTTP MP3/Vorbis/Opus/ICY; chained Ogg streams: PASS' 
