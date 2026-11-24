#!/bin/sh
radir=$1
[ -n "$radir" ] || exit 1
cache_dir="$radir/cores/cache"
ext_cache_path="$cache_dir/core_extensions.cache"

mkdir -p "$cache_dir" || exit 1

# Select installed cores once. Passing all .info files to one awk avoids the
# old cat/grep/awk/tr pipeline for every core. Missing .so files stay excluded.
set --
for info in "$radir"/cores/*.info; do
    [ -f "$info" ] || continue
    core=${info##*/}
    core=${core%.info}
    [ -f "$radir/cores/$core.so" ] || continue
    set -- "$@" "$info"
done

# As before, do not create a cache if no installed cores exist. A later
# installation can then trigger a fresh build.
[ "$#" -gt 0 ] || exit 0

# Work in the same directory as the final cache, so rename is atomic and an
# interrupted build never leaves a half-written core_extensions.cache.
raw=$(mktemp "$cache_dir/.core_extensions_raw.XXXXXX") || exit 1
sorted=$(mktemp "$cache_dir/.core_extensions_sorted.XXXXXX") || {
    rm -f "$raw"
    exit 1
}
trap 'rm -f "$raw" "$sorted"' 0
trap 'exit 1' 1 2 3 15

awk '
        function quoted_value(line, split_equals, split_quotes) {
            split(line, split_equals, "=")
            split(split_equals[2], split_quotes, "\"")
            return split_quotes[2]
        }
        function emit() {
            if (core != "") print corename ";" core ";" extensions
        }
        FNR == 1 {
            if (NR > 1) emit()
            core = FILENAME
            sub(/^.*\//, "", core)
            sub(/\.info$/, "", core)
            corename = ""
            extensions = ""
        }
        /corename([[:space:]]|=|$)/ {
            corename = corename quoted_value($0)
        }
        /supported_extensions([[:space:]]|=|$)/ {
            extensions = extensions quoted_value($0)
        }
        END { if (NR > 0) emit() }
    ' "$@" > "$raw" || exit 1

sort -f "$raw" > "$sorted" || exit 1
mv -f "$sorted" "$ext_cache_path" || exit 1
