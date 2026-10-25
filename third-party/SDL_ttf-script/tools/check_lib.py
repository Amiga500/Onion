#!/usr/bin/env python3
"""Check a device library: 32-bit ARM hard-float ELF, no build-machine paths
among its dependencies, and, given a symbol list, exactly those TTF_ functions
exported (the API of the SDL_ttf it replaces).

    READELF=arm-linux-gnueabihf-readelf check_lib.py LIB [SYMBOLS]
"""
import os
import subprocess
import sys


def main():
    if len(sys.argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2
    path = sys.argv[1]
    readelf = os.environ.get("READELF", "readelf")
    with open(path, "rb") as f:
        header = f.read(52)
    if header[:6] != b"\x7fELF\x01\x01" or header[18] != 40 or not header[36 + 1] & 0x04:
        print(f"{path}: not a 32-bit ARM hard-float ELF", file=sys.stderr)
        return 1
    dynamic = subprocess.run([readelf, "-d", path], capture_output=True, text=True,
                             check=True).stdout
    for line in dynamic.splitlines():
        # A run path, or a dependency named by path, is a build-machine path:
        # the device loader cannot follow it (Miyoo's own build has one).
        rpath = "(RPATH)" in line or "(RUNPATH)" in line
        if rpath or "(NEEDED)" in line:
            value = line.split("[", 1)[1].rstrip("]")
            if rpath or "/" in value:
                print(f"{path}: build-machine path in {line.split()[1]}: {value}",
                      file=sys.stderr)
                return 1
    if len(sys.argv) == 3:
        symbols = subprocess.run([readelf, "--dyn-syms", "-W", path], capture_output=True,
                                 text=True, check=True).stdout
        exported = sorted({p[7] for p in (l.split() for l in symbols.splitlines())
                           if len(p) >= 8 and p[4] == "GLOBAL" and p[6] != "UND"
                           and p[7].startswith("TTF_")})
        with open(sys.argv[2]) as f:
            wanted = sorted(line.strip() for line in f if line.strip())
        if exported != wanted:
            missing = sorted(set(wanted) - set(exported))
            extra = sorted(set(exported) - set(wanted))
            print(f"{path}: exported functions differ: missing {missing}, extra {extra}",
                  file=sys.stderr)
            return 1
        print(f"{path}: same {len(wanted)} TTF_ functions as the SDL_ttf it replaces")
    print(f"{path}: 32-bit ARM hard-float ELF, no build-machine paths")
    return 0


if __name__ == "__main__":
    sys.exit(main())
