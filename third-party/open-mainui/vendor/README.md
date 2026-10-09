# Source provenance

`cjson/cJSON.c` and `cjson/cJSON.h` are copied without modification from Onion's `include/cjson/` directory. Each file retains its MIT license notice and copyright. They are compiled into both the host and device builds; there is no system copy to link against on the device.

New project source is GPL-3.0-only, with the license text in the root `LICENSE`. Onion is GPLv3 as well, so the licences match.

## SQLite is not vendored

SQLite is a library dependency rather than vendored source:

- The **device build** links `$(ONION_ROOT)/lib/libsqlite3.so` and compiles against `$(ONION_ROOT)/include/sqlite3/`. This is deliberate and is most of the reason the device image is around 200 KB: the launcher uses the exact SQLite that Onion already ships, so nothing is duplicated on the card.
- The **host build** links the system `libsqlite3`. Distributions ship a newer release than Onion's 3.39.0, which means `make check` exercises a different version than the device. SQLite's compatibility record makes that a reasonable trade for a development build, but it is a real difference: questions about on-device database behaviour are answered by `make check-device` and a device run, not by the host suite.

Nothing in the code depends on particular SQLite compile-time options. WAL caches are rejected by reading the database header directly (bytes 18 and 19, which hold the read/write version; 2 means WAL) in both `journal_is_wal` on the read path and `cache_needs_repair` on the rebuild path. A WAL reader creates a `-shm` companion file even on a read-only open, which would write to the user's card, so such a cache is never opened, replaced or repaired - and that holds whether the linked SQLite has WAL support or was built with `SQLITE_OMIT_WAL`. The caller falls back to a directory scan. See `docs/CATALOG_CACHE.md`.
