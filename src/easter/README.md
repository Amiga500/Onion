# Cucuziello's Game

Onion/RedOnion ten-level Easter egg (SDL 1.2, SDL_ttf, C11), by Amiga500.

A warehouse puzzle for Onion/RedOnion on the Miyoo Mini family: push every
crate onto a marker. It takes the place of Neon Breaker and keeps its look
(palette, HUD, panels, rectangles only), its sound code and the way it starts.

## Controls

* D-PAD moves; hold to keep walking
* B takes the last move back; hold to keep undoing
* X starts the level again (press it twice)
* A starts, and continues after a level
* LEFT/RIGHT on the title screen picks the level to start from
* MENU exits; B on the title screen exits too

## Rules

Walk into a crate to push it. One crate at a time, and never pull: a crate in
a corner stays there, so B (undo) goes back as far as you like, to the start of
the level. A level is done when every crate is on a marker. Crates on a marker
turn green; the count under the board says how many are home.

The top bar shows the moves and pushes of the level. After the tenth level the
game shows the totals of the levels you finished in that run.

Nothing is saved between runs: use LEFT/RIGHT on the title screen to go back to
the level you were on.

## Levels

The ten levels are Microban 1 to 10 by **David W. Skinner**, unchanged. Microban
(155 levels, April 2000) is his set for beginners: small levels, each built
around one idea. They are credited on the title screen.

To add levels, add them to the `levels` table in `easter.c` in the usual
notation (`#` wall, `@` player, `$` crate, `.` marker, `*` crate on a marker,
`+` player on a marker). A level can be up to 16 by 12 cells; cells are 40
pixels and shrink when a level needs it. `LEVEL_COUNT` follows the table.

## Testing on a PC

`test/` holds a small stand-in for SDL 1.2 (software only, text through
FreeType) and a script that solves every level and plays the solutions through
the game itself:

```sh
gcc -std=c11 -Wall -Wextra -O2 -Itest/shim -I/usr/include/freetype2 \
    easter.c test/shim/shim.c -o easter_host -lfreetype
python3 test/check.py easter.c ./easter_host
```

The game takes `--input KEYS` (one key per frame: `L R U D` move, `B` undo,
`X` restart, `A`, `M` quit, `.` waits), `--frames N`, `--snapshot file.bmp` and
`--level N`, as well as Neon Breaker's `--mute`, `--no-music`, `--audio-test`
and `--benchmark`. With `--input` it prints its final state on exit.

With Onion's logging on, it writes `.tmp_update/logs/easter.log`.

## Credits

* Game: Amiga500
* Levels: David W. Skinner (Microban)
* Sound, text and start-up code: from Neon Breaker, the easter egg it replaces

## License

GPL-3.0-only for the code, matching Onion. The levels are David W. Skinner's
and are included with credit to him.
