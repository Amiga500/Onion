#!/bin/bash

mkdir -p cache
cd cache

wget -O featured.txt https://raw.githubusercontent.com/OnionUI/Themes/main/.github/data/featured.txt > /dev/null 2>&1
featured=`cat ./featured.txt`
rm -f ./featured.txt

readarray -t themes <<< "$featured"

f() { themes=("${BASH_ARGV[@]}"); }

shopt -s extdebug
f "${themes[@]}"
shopt -u extdebug

mkdir -p ../dist/Themes

# Download themes in parallel if xargs is available (much faster)
# Otherwise fall back to sequential downloads
if command -v xargs >/dev/null 2>&1; then
    # Parallel download approach
    for element in "${themes[@]}"
    do
        zipfile="$element.zip"
        if [[ ! -f "$zipfile" ]]; then
            echo "https://github.com/OnionUI/Themes/raw/main/release/$element.zip"
        fi
    done | xargs -n 1 -P 4 -I {} sh -c 'wget -O "$(basename {})" "{}" -q --show-progress 2>&1 | grep -v "^$" && echo "-- downloaded: $(basename {})"' || true
else
    # Sequential download fallback
    for element in "${themes[@]}"
    do
        zipfile="$element.zip"
        if [[ ! -f "$zipfile" ]]; then
            echo "-- downloading theme: $element"
            wget -O "$zipfile" "https://github.com/OnionUI/Themes/raw/main/release/$element.zip" -q --show-progress
        fi
    done
fi

# Extract/copy themes
for element in "${themes[@]}"
do
    zipfile="$element.zip"
    if [ "$element" == "Silky by DiMo" ]; then
        echo "-- extracting theme: $element"
        unzip -oq "$zipfile" -d ../dist/Themes
    else
        echo "-- copying theme: $element"
        cp "$zipfile" ../dist/Themes
    fi
done
