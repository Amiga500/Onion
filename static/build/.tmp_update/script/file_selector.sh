#!/bin/sh
prompt_title="$1"
home_dir="$2"
file_extensions="$3"
# Optional footer hint: the PDF Reader adds how to close a PDF (MENU), since
# START, which closes this list, does something else inside a PDF.
hint="${4:-Start: Exit      A: Open}"
current_dir=$(cd -- "$home_dir" >/dev/null 2>&1 && pwd -P) || exit 1
selected_file=""

# Filesystem tests here are shell builtins. Previously awk ran a separate
# shell for every directory and seen-marker check, making large folders slow.
# Keep the historical menu order (shell globs sort names) and exclusions.
show_files() {
    [ "$current_dir" = /mnt/SDCARD ] || printf '../\n'

    for path in "$current_dir"/*; do
        [ -e "$path" ] || [ -L "$path" ] || continue
        name=${path##*/}
        case "$name" in
            ' '* | '-'* | *'=='*) continue ;;
        esac

        if [ -d "$path" ]; then
            printf '%s/\n' "$name"
            continue
        fi

        if [ -n "$file_extensions" ]; then
            extension=${name##*.}
            # Most extensions are already lowercase. Run tr only for the
            # uncommon uppercase extension rather than for every file.
            case "$extension" in
                *[A-Z]*) extension=$(printf '%s' "$extension" | tr 'A-Z' 'a-z') ;;
            esac
            case " $file_extensions " in
                *" $extension "*) ;;
                *) continue ;;
            esac
        fi

        if [ -n "$SHELLECT_NATIVE" ]; then
            # New UI: the name, "(seen)" after the ones opened before
            if [ -f "$current_dir/.state_seen/$name._seen" ]; then
                printf '%s (seen)\n' "$name"
            else
                printf '%s\n' "$name"
            fi
        elif [ -f "$current_dir/.state_seen/$name._seen" ]; then
            printf '[x] %s\n' "$name"
        else
            printf '[ ] %s\n' "$name"
        fi
    done
}

# New UI (SHELLECT_NATIVE, set by the apps' launchers): the list is drawn
# by prompt, "PDF Reader > PDF" in the header; B goes up a folder, and
# leaves from the first one; MENU leaves (SHELLECT_NATIVE_PID).
if [ -n "$SHELLECT_NATIVE" ]; then
    SHELLECT_PARENT=$prompt_title
    SHELLECT_NATIVE_PID=$$
    export SHELLECT_PARENT SHELLECT_NATIVE_PID
fi
top_dir=$current_dir

while :; do
    dir_short=${current_dir##*/}
    if [ -n "$SHELLECT_NATIVE" ]; then
        selected_file=$(show_files | /mnt/SDCARD/.tmp_update/script/shellect.sh \
            -t "$dir_short")
        if [ -z "$selected_file" ]; then
            # B: up a folder while inside the one the app started in, else
            # leave (after "../" above it, B went on up to / and never left)
            case "$current_dir" in
                "$top_dir"/*) selected_file=../ ;;
                *) break ;;
            esac
        fi
        case "$selected_file" in
            */) ;;
            *' (seen)') selected_file="[x] ${selected_file% (seen)}" ;;
            *) selected_file="[ ] $selected_file" ;;
        esac
    else
        menu_title=$(printf '%s' "$prompt_title: $dir_short" | tr 'a-z' 'A-Z')
        selected_file=$(show_files | /mnt/SDCARD/.tmp_update/script/shellect.sh \
            -t "--== $menu_title ==--" -b "$hint")
    fi

    if [ -d "$current_dir/$selected_file" ]; then
        current_dir=$(cd -- "$current_dir/$selected_file" >/dev/null 2>&1 && pwd -P) || exit 1
        continue
    fi

    case "$selected_file" in
        '[x] '* | '[ ] '*) selected_file=${selected_file#????} ;;
        *) selected_file='' ;;
    esac

    if [ -f "$current_dir/$selected_file" ]; then
        mkdir -p "$current_dir/.state_seen"
        touch "$current_dir/.state_seen/$selected_file._seen"
        printf '%s\n' "$current_dir/$selected_file" > "$home_dir/.selected.pls"
        break
    fi
done
