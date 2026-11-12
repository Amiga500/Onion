#!/bin/sh
romdir="$1"

# Only files that exist: a missing one was made, with one empty line in it
# (an /appconfigs/romwinidx.json MainUI could not read, a junk state.json).
if [ -f /tmp/state.json ]; then
    current_state="$(echo "$(cat /tmp/state.json)
" | tac)"
    echo "$current_state" \
        | sed "1,/\"currpos\"/s/:\s*[0-9]*,/:\t0,/" \
        | sed "1,/\"pagestart\"/s/:\s*[0-9]*,/:\t0,/" \
        | sed "1,/\"pageend\"/s/:\s*[0-9]*,/:\t5,/" \
        | tac > /tmp/state.json
fi

if [ -f /appconfigs/romwinidx.json ]; then
    current_idx="$(cat /appconfigs/romwinidx.json | awk -v search="$romdir" '$0 ~ search{n=4}; n {n--; next}; 1')"
    echo "$current_idx" > /appconfigs/romwinidx.json
fi

rm -f "$romdir/$(basename "$romdir")_cache2.db" 2> /dev/null
rm -f "$romdir/$(basename "$romdir")_cache6.db" 2> /dev/null
