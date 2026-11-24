#!/bin/sh
# Simulate NTP success, cancellation, offline mode, fallback and timeout.
set -eu
cd "$(dirname "$0")/.."
B="$(pwd)/build/tests/netclock"
mkdir -p "$B"
cat > "$B/ntpdate" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> calls
[ -f hang ] && exec /bin/sleep 60
[ -f slow ] && { /bin/sleep 1; : > clock-valid; exit 0; }
[ -f busy ] && exit 1
case "$*" in *pool.ntp.org) : > clock-valid; exit 0;; esac
exit 1
MOCK
cat > "$B/busybox" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> calls
[ -f hang ] && exec /bin/sleep 60
[ -f busy ] && { : > clock-valid; exit 0; }
exit 1
MOCK
chmod +x "$B/ntpdate" "$B/busybox"
${CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -I. -DM2D_HOST -DNETCLOCK_TEST \
    -DNETCLOCK_TEST_PATH="\"$B/ntpdate\"" -DNETCLOCK_TEST_BUSYBOX="\"$B/busybox\"" \
    netclock.c tools/test_netclock.c -pthread -o "$B/test"
cd "$B"
rm -f hang offline busy slow
./test success
./test offline
./test insecure
: > slow
./test switch
rm -f slow
: > busy
./test busybox
rm -f busy
./test sntp
: > hang
./test cancel
./test timeout
rm -f hang clock-valid calls
