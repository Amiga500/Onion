#!/bin/sh
scriptlabel="Clean recents (remove apps)"
scriptinfo="Removes all apps\nfrom the recents list."

tools recents --clean_all

# Non-zero: GLO goes back to the list. Zero would start the selected game.
exit 1
