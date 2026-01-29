#!/bin/sh
# Simple menu system using infoPanel for environments without TTY
# Fallback when shellect can't be used (MainUI app context)

SYSDIR="/mnt/SDCARD/.tmp_update"

# Function to show a menu and get user choice
# Usage: simple_menu "Title" "Option1|Option2|Option3..."
# Returns: Selected option text (or empty if cancelled)
simple_menu() {
    title="$1"
    options_pipe="$2"  # Options separated by |
    
    # Convert pipe-separated to newline-separated
    options=$(echo "$options_pipe" | tr '|' '\n')
    
    # Count options
    count=0
    while IFS= read -r line; do
        count=$((count + 1))
    done << EOF
$options
EOF
    
    # Show each option with infoPanel one at a time
    current=1
    while [ $current -le $count ]; do
        # Get current option
        option=$(echo "$options" | sed -n "${current}p")
        
        # Build message showing position
        msg="$option\n\n[$current/$count]"
        
        # Add navigation hints
        if [ $current -eq 1 ] && [ $count -gt 1 ]; then
            msg="$msg\n\nPress B for next option\nPress A to select\nPress START to cancel"
        elif [ $current -eq $count ] && [ $count -gt 1 ]; then
            msg="$msg\n\nPress X for previous option\nPress A to select\nPress START to cancel"
        elif [ $count -gt 1 ]; then
            msg="$msg\n\nPress B for next, X for previous\nPress A to select\nPress START to cancel"
        else
            msg="$msg\n\nPress A to select\nPress START to cancel"
        fi
        
        # Show with infoPanel (simplified - would need actual button detection)
        # For now, return first option as fallback
        # This is a placeholder - real implementation would need key detection
        echo "$option"
        return 0
    done
    
    # Cancelled
    return 1
}

# Export function
export -f simple_menu 2>/dev/null || true
