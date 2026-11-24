#!/bin/sh
# Legacy audio-library recovery before removing RedAmp. Do not remove the app
# until recovery succeeds; this is for older installs which replaced libpadsp.
set -eu
DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec "$DIR/redamp-recover.sh" "$@"
