#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run clang-format over every project source, in place.

Uses .clang-format at the repository root. vendor/ is upstream code and is left
alone. With --check, report files that need formatting and exit non-zero.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sources():
    for directory in ("src", "tests"):
        for pattern in ("*.c", "*.h"):
            yield from sorted((ROOT / directory).rglob(pattern))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="Report unformatted files instead of rewriting them")
    arguments = parser.parse_args()
    binary = shutil.which("clang-format")
    if not binary:
        print("clang-format not found; see docs/BUILDING.md", file=sys.stderr)
        return 2
    files = [str(path.relative_to(ROOT)) for path in sources()]
    if arguments.check:
        result = subprocess.run([binary, "--dry-run", "--Werror", *files], cwd=ROOT)
        return result.returncode
    subprocess.run([binary, "-i", *files], cwd=ROOT, check=True)
    print(f"formatted {len(files)} file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
