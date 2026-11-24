#!/bin/sh
# Source this file from a RedOnion game/menu launcher, AFTER setting the
# ordinary OnionOS LD_PRELOAD and BEFORE exec'ing the target program.
# No copying/replacing of /mnt/SDCARD/miyoo/lib/libpadsp.so.
# This script is source-only: it does NOT launch or change any processes.
REDAMP_AUDIO_DIR=/mnt/SDCARD/App/RedAmp
# Evidence that a RedOnion launcher actually sourced this fragment.
# A toggle alone does not prove integration is installed.
if [ -d "$REDAMP_AUDIO_DIR/data" ] && \
   [ ! -e "$REDAMP_AUDIO_DIR/data/game_music.launch_hook_seen" ]; then
    : > "$REDAMP_AUDIO_DIR/data/game_music.launch_hook_seen" 2>/dev/null || :
fi
REDAMP_AUDIO_WRAP="$REDAMP_AUDIO_DIR/lib/libpadsp_wrap.so"
REDAMP_AUDIO_BASE=/mnt/SDCARD/miyoo/lib/libpadsp.so
if [ -r "$REDAMP_AUDIO_WRAP" ] && [ -r "$REDAMP_AUDIO_BASE" ] && \
   [ -f "$REDAMP_AUDIO_DIR/data/game_music.consent" ] && \
   [ -f "$REDAMP_AUDIO_DIR/data/settings.ini" ] && \
   grep -q '^game_music=1$' "$REDAMP_AUDIO_DIR/data/settings.ini" && \
   ! grep -q 'MINIAMP_PADSP_WRAPPER' "$REDAMP_AUDIO_BASE" 2>/dev/null; then
    # The normal RedOnion library must remain after our wrapper in the linker's
    # resolution order. Avoid duplicating the normal library when already loaded.
    case " ${LD_PRELOAD:-} " in
        *"$REDAMP_AUDIO_WRAP"*) ;;  # already included
        *) case " ${LD_PRELOAD:-} " in
             *"$REDAMP_AUDIO_BASE"*)
                  LD_PRELOAD="$REDAMP_AUDIO_WRAP ${LD_PRELOAD:-}" ;;
             *) LD_PRELOAD="$REDAMP_AUDIO_WRAP $REDAMP_AUDIO_BASE ${LD_PRELOAD:-}" ;;
           esac
           export LD_PRELOAD ;;
    esac
    REDAMP_NATIVE_PRELOAD=1
    export REDAMP_NATIVE_PRELOAD
fi
unset REDAMP_AUDIO_DIR REDAMP_AUDIO_WRAP REDAMP_AUDIO_BASE
