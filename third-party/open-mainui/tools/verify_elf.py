#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check a device binary really is a 32-bit ARM hard-float Linux ELF.

Run by `make device` after stripping. The DT_NEEDED check matters because a
shared object linked by path and without DT_SONAME records that build-machine
path as its dependency, which the device loader cannot resolve.
"""
import struct
import sys
from pathlib import Path


def verify_elf(path):
    data = path.read_bytes()
    header = data[:52]
    if (len(header) != 52 or header[:6] != b"\x7fELF\x01\x01" or
            struct.unpack_from("<H", header, 18)[0] != 40 or
            not struct.unpack_from("<I", header, 36)[0] & 0x400):
        raise ValueError("Output must be a 32-bit little-endian ARM hard-float ELF")

    # A shared object without DT_SONAME inherits its link argument in DT_NEEDED.
    # Reject build-machine paths: they cannot be resolved by the device loader.
    phoff = struct.unpack_from("<I", header, 28)[0]
    phsize, phcount = struct.unpack_from("<HH", header, 42)
    if phsize < 32 or phoff + phsize * phcount > len(data):
        raise ValueError("Output has an invalid ELF program header table")
    segments = [struct.unpack_from("<8I", data, phoff + i * phsize)
                for i in range(phcount)]
    for segment in segments:
        if segment[0] != 2:  # PT_DYNAMIC
            continue
        offset, length = segment[1], segment[4]
        if offset + length > len(data) or length % 8:
            raise ValueError("Output has an invalid ELF dynamic table")
        entries = []
        for position in range(offset, offset + length, 8):
            tag, value = struct.unpack_from("<II", data, position)
            if tag == 0:
                break
            entries.append((tag, value))
        needed = [value for tag, value in entries if tag == 1]
        if not needed:
            continue
        strings = next((value for tag, value in entries if tag == 5), None)
        size = next((value for tag, value in entries if tag == 10), 0)
        load = next((item for item in segments if item[0] == 1 and
                     strings is not None and item[2] <= strings and
                     strings + size <= item[2] + item[4]), None)
        if load is None or not size:
            raise ValueError("Output has an invalid ELF dynamic string table")
        start = load[1] + strings - load[2]
        table = data[start:start + size]
        for index in needed:
            end = table.find(b"\0", index)
            if index >= len(table) or end < 0:
                raise ValueError("Output has an invalid ELF library dependency")
            name = table[index:end].decode("utf-8", errors="replace")
            if "/" in name or "\\" in name:
                raise ValueError("Output contains a path-based library dependency: " + name)



def main():
    if len(sys.argv) != 2:
        print("usage: verify_elf.py <binary>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    try:
        verify_elf(path)
    except (OSError, ValueError) as error:
        print(f"{path}: {error}", file=sys.stderr)
        return 1
    print(f"{path}: 32-bit ARM hard-float ELF, no path-based dependencies")
    return 0


if __name__ == "__main__":
    sys.exit(main())
