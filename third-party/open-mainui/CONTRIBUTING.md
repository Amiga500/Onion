# Contributing

This is a handheld game launcher. Prefer small, direct changes and focused tests over new abstractions. Add infrastructure only for a concrete launcher need.

## Before sending a patch

```sh
make check               # unit suites and integration cases
python3 tools/format.py  # apply .clang-format to src/ and tests/
```

`python3 tools/format.py --check` reports formatting problems without rewriting, which is what CI runs. Use clang-format 14, the version CI installs; other versions can lay out some code differently. If your system has a different version, install 14 in a virtual environment:

```sh
python3 -m venv ~/.venvs/clang-format14
~/.venvs/clang-format14/bin/pip install "clang-format==14.*"
PATH=~/.venvs/clang-format14/bin:$PATH python3 tools/format.py
```

A green host build is not proof of device compatibility: if a change touches byte layouts, integer widths or anything platform-specific, say so, and run `make check-device` if you can.

## C conventions

- C11, formatted with the committed `.clang-format`: four-space indent, Stroustrup braces, 100 columns. Format `src/` and `tests/` only.
- Public functions use the `mainui_` prefix and live in a domain header. Keep UI rendering, data access and platform behaviour in separate files; [src/README.md](src/README.md) describes the domains.
- Document interfaces: purpose, valid inputs, ownership, lifetime, error behaviour, side effects. Comment thread boundaries, ownership transfers, bounds and recovery behaviour. Don't narrate obvious assignments.
- Check every allocation, I/O result, parse and database call. Use one cleanup path for owned resources and publish new state only after it is fully valid. Never truncate a path silently - `snprintf` results are checked here.
- Bind SQL values and quote identifiers. Keep expensive work off the UI thread.
- Where a behaviour exists for compatibility with the original launcher, say so in a comment near the code, and say why it is kept.
- Leave `vendor/` alone. Upstream files keep their own notices and are not reformatted with project sources.

## Translations

New user-visible strings go through mainui_translate(id, "English text"), with the English literal as the fallback. Register the ID in lang/README.md; IDs Onion does not define show the fallback until they land upstream.

## Tests

Two kinds, split by whether a test can call the code directly:

- `tests/test_*.c` linked into `build/unit-tests` - self-contained, no files on disk, and cross-compilable to the device.
- `tests/integration/case_*.py` - build an SD tree, run a binary against it, assert on the result. Add a case file and the runner finds it.

Adding a fixture harness in C means adding its name to `FIXTURES` in the Makefile. Prefer tests that could fail independently of the implementation: invariants, malformed input, failure injection.

## Licence

GPL-3.0-only. New files carry `/* SPDX-License-Identifier: GPL-3.0-only */`.
