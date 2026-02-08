# Onion Tests

This directory contains tests for the Onion project.

## Running All Tests

From the **root directory** of the project, run:

```bash
make test
```

This will execute both unit tests and integration tests (if gtest is available).

## Test Types

### Unit Tests

Unit tests use a lightweight custom test framework (`onion_test.h`) and don't require external dependencies:

- `test_str.c` - String utility functions
- `test_file.c` - File utility functions  
- `test_json.c` - JSON parsing and manipulation
- `test_hash.c` - Hash functions
- `test_perf.c` - Performance utilities

To run only unit tests:

```bash
make unit-test
```

Or from the test directory:

```bash
make -f Makefile.unit
```

### Integration Tests (gtest)

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
- GCC compiler
- No external dependencies

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

The GitHub Actions workflows automatically run all tests:

- `.github/workflows/test.yml` - Runs all tests on pull requests
- Tests are run in a native Ubuntu environment with gtest installed
