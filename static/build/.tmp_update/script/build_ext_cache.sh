#!/bin/sh
radir="$1"
ext_cache_path=$radir/cores/cache/core_extensions.cache

get_info_value() {
    echo "$1" | grep "$2\b" | awk '{split($0,a,"="); print a[2]}' | awk -F'"' '{print $2}' | tr -d '\n'
}

mkdir -p $radir/cores/cache

# Collect all entries first, then sort once at the end (instead of O(n²) sorting)
temp_file=$(mktemp)
for entry in "$radir/cores"/*.info; do
    tmp_info=$(cat "$entry")
    tmp_core=$(basename "$entry" .info)

    if [ ! -f "$radir/cores/$tmp_core.so" ]; then
        continue
    fi

    tmp_corename=$(get_info_value "$tmp_info" "corename")
    tmp_extensions=$(get_info_value "$tmp_info" "supported_extensions")

    echo "$tmp_corename;$tmp_core;$tmp_extensions" >> "$temp_file"
done

# Sort once at the end and write to final destination
sort -f -o "$ext_cache_path" "$temp_file"
rm -f "$temp_file"
