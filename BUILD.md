# Building Onion from Source

This guide explains how to build Onion firmware from source code.

## ⚠️ Already Inside a Docker Container?

**If you see `root@containerid` in your prompt**, you're already inside a Docker container. Skip the Docker setup and just run:

```bash
# First, install required tools (if not already installed)
apt-get update && apt-get install -y p7zip-full

# Initialize submodules (first time only)
make git-submodules

# Build everything
make all

# Package for distribution (requires 7z)
make dist
```

**Do NOT use `sudo`** (you're already root) and **do NOT use `make with-toolchain`** (you're already in the container).

---

## Prerequisites

- Git
- Docker (for the toolchain)
- Make
- **p7zip / 7z** (for packaging - see note below)

## Quick Start

### Option 1: Using Docker Toolchain (Recommended)

If you're building on a system without the ARM cross-compiler installed:

```bash
# Clone the repository
git clone https://github.com/Amiga500/Onion.git
cd Onion

# Initialize git submodules
make git-submodules

# Build using the Docker toolchain
make with-toolchain CMD=all
```

### Option 2: Interactive Docker Session

If you prefer to work interactively inside the Docker container:

```bash
# Clone the repository
git clone https://github.com/Amiga500/Onion.git
cd Onion

# Initialize git submodules
make git-submodules

# Enter the Docker toolchain container
make toolchain

# Inside the container, run:
make all
```

### Option 3: Native Build (Advanced)

If you have the ARM cross-compiler toolchain installed on your system:

```bash
# Clone the repository
git clone https://github.com/Amiga500/Onion.git
cd Onion

# Initialize git submodules
make git-submodules

# Build directly
make all
```

## Important Notes

### About Docker and Sudo

- **If you're already inside a Docker container** (e.g., your prompt shows `root@containerid`), you do NOT need to use `make toolchain` or `make with-toolchain`. Just run `make` commands directly.

- **Do not use `sudo` when you're already root**. If you see `root@` in your prompt, you're already running as root and don't need sudo.

- The `make with-toolchain` command is designed to be run **from your host system**, not from inside a Docker container. It will launch a Docker container to run the build.

### Understanding the Makefile Targets

- `make git-submodules` - Initialize and update all git submodules (required first step)
- `make toolchain` - Start an interactive bash session inside the Docker toolchain container
- `make with-toolchain CMD=<command>` - Run a specific make command inside the Docker toolchain (from host)
- `make all` or `make` - Build all components (run this inside container or with native toolchain)
- `make dist` - Build and package everything
- `make release` - Create a release zip file
- `make clean` - Clean build artifacts
- `make deepclean` - Clean everything including cache

## Common Build Scenarios

### Building a Complete Release

```bash
# From your host system
make git-submodules
make with-toolchain CMD=release
```

Or interactively:

```bash
make git-submodules
make toolchain
# Inside container:
make release
```

### Building Just the Core Components

```bash
make with-toolchain CMD=core
```

### Running Tests

```bash
make with-toolchain CMD=test
```

### Formatting Code

```bash
make format
```

## Troubleshooting

### "sudo: command not found"

**Problem:** You ran `sudo make with-toolchain` inside a Docker container and got "bash: sudo: command not found"

**Solution:** If you're already root (check if your prompt shows `root@`), you don't need sudo. Also, if you're already inside a Docker container, you don't need `make with-toolchain`. Just run `make` directly:

```bash
# Wrong (if already in Docker as root):
sudo make with-toolchain

# Correct (if already in Docker):
make all
```

### "Cannot connect to the Docker daemon"

**Problem:** Docker is not running or you don't have permission to use it.

**Solution:** 
- Make sure Docker is installed and running
- On Linux, add your user to the docker group: `sudo usermod -aG docker $USER`
- Log out and back in for group changes to take effect

### "7z: not found" or "7z: command not found"

**Problem:** Build fails during the `make dist` or `make release` step with error "7z: not found"

**Root Cause:** The build process uses 7-Zip (`7z` command) to create compressed archives (.pak and .zip files). This tool must be installed on the host system.

**Solution:**

If you're building inside the Docker toolchain container:
```bash
# Inside the Docker container, install p7zip
apt-get update && apt-get install -y p7zip-full

# Then continue with your build
make dist
```

If you're building natively (without Docker):
```bash
# Ubuntu/Debian
sudo apt-get install p7zip-full

# Fedora/RHEL
sudo dnf install p7zip p7zip-plugins

# Arch Linux
sudo pacman -S p7zip

# macOS (with Homebrew)
brew install p7zip
```

**Note:** The repository includes an ARM version of 7z for the target device (in `static/build/.tmp_update/bin/7z`), but the build system needs a native (x86_64/aarch64) version of 7z to run on your host machine during the packaging step.

### Submodule Errors

**Problem:** Build fails with missing files or "No such file or directory" errors

**Solution:** Make sure you've initialized submodules:
```bash
make git-submodules
```

## Build Output

After a successful build:

- `build/` - Contains built binaries and resources
- `dist/` - Contains packaged distribution files
- `release/` - Contains the final release ZIP file (after `make release`)

## Development Workflow

For active development:

```bash
# One-time setup
make git-submodules
make toolchain

# Inside the container, iterate:
make clean    # Clean previous build
make core     # Build core components
make apps     # Build applications
# Test your changes
# Repeat as needed
```

## Additional Resources

- [Onion Documentation](https://onionui.github.io/docs)
- [GitHub Repository](https://github.com/OnionUI/Onion)
- [Contributing Guidelines](CONTRIBUTING.md) (if available)

## Getting Help

If you encounter issues:

1. Check this BUILD.md for common problems
2. Search existing [GitHub Issues](https://github.com/OnionUI/Onion/issues)
3. Open a new issue with:
   - Your operating system
   - Docker version (if using Docker)
   - Full error message
   - Steps to reproduce
