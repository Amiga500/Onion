# Onion Tests

This directory contains tests for the Onion project.

## Running tests

From the **root directory** of the project:

```bash
make unit-test
```

Host C unit tests (`onion_test.h`) compile with the system gcc/clang. They do
**not** need the Miyoo Mini toolchain, SDL, or Google Test.

```bash
make -C test -f Makefile.unit unit-test-san
```

Rebuilds a small subset (`hash`, `file`, `str`, `json`, `neon`) with
`-fsanitize=address,undefined` and runs those binaries.

```bash
make test
```

Builds and runs the GTest `infoPanel` integration test. That target still
needs SDL and gtest; it is **not** the suite that GitHub Actions treats as
required.

`test_images_browser` is compiled only on request (`make -f Makefile.unit test_images_browser`)
and is excluded from `TESTS` until infoPanel hardening is in tree.

## Test types

### Unit tests

Unit tests use a lightweight custom test framework (`onion_test.h`) and don't require external dependencies. See `Makefile.unit` `TESTS` for the current list.

To run only unit tests:

```bash
make unit-test
```

Or from the test directory:

```bash
make -f Makefile.unit
```

### Integration tests (gtest)

Integration tests use Google Test and require SDL libraries:

- `test_infoPanel.cpp` - InfoPanel image cache tests

To run only integration tests:

```bash
make gtest
```

Or from the test directory (requires gtest to be installed):

```bash
make
```

**Note:** If gtest is not available, the integration tests will be skipped automatically with a message indicating gtest is needed.

## Requirements

### For Unit Tests
- GCC compiler (C99)
- No SDL / gtest
- `unit-test-san` needs gcc (or clang) with AddressSanitizer and UBSan

### For Integration Tests
- Google Test (libgtest-dev)
- SDL libraries (libsdl1.2-dev, libsdl-image1.2-dev)
- C++17 compatible compiler

### Installing gtest on Ubuntu/Debian

**Modern Ubuntu (20.04+):**
```bash
sudo apt-get install libgtest-dev
```

Most recent distributions include pre-built binaries. For older versions or custom builds:

```bash
sudo apt-get install build-essential libgtest-dev cmake
cd /usr/src/gtest  # Path may vary by distribution
sudo cmake CMakeLists.txt && sudo make
sudo cp lib/*.a /usr/lib  # Output directory may vary
```

**Note:** Paths and directory structure may differ depending on your distribution and gtest version. Consult your distribution's documentation if the above paths don't exist.

## CI/CD

`.github/workflows/test.yml` runs on pull requests:

- **Host unit tests** (required): `make unit-test`, then
  `make -C test -f Makefile.unit unit-test-san`. No Miyoo toolchain.
- **GTest infoPanel** (`make test`): `continue-on-error`. infoPanel is out of
  scope for the unit-test suite; a failure there does not fail the workflow.

`pre-release.yml` and `build.yml` use the Miyoo ARM toolchain container and
are not wired to `make unit-test`. Adding host unit tests there would not
exercise the cross compiler and would lengthen pre-release; the dedicated
job in `test.yml` is the gate.

The workflow does **not** run every file under `test/` (for example
`test_images_browser` is excluded from `TESTS`).
