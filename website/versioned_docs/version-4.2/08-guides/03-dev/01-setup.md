---
slug: /dev/setup
---

# Setup

*![](https://user-images.githubusercontent.com/7110113/184558441-dc2783c1-0447-489d-9bde-b99d63b6d4b7.png)*


## Environment

The build environment is based on [Shaun Inman's docker image](https://github.com/shauninman/union-miyoomini-toolchain).

We recommend to use a Linux VM with Docker installed. For example you can use [VMware Workstation Player](https://www.vmware.com/fr/products/workstation-player.html) or [VirtualBox](https://www.virtualbox.org/wiki/Downloads).

You can find pre-installed Linux images on [linuxvmimages.com](https://www.linuxvmimages.com/)


## Development Workflow

**Important:** You edit and work with the code on your **host machine** (outside Docker). Docker is only used for building/compiling the code.

### Typical workflow:

1. **Clone the repository** on your host machine
2. **Edit code** using your favorite editor/IDE on your host machine
3. **Build** using Docker (which has the ARM cross-compilation toolchain)
4. **Test** the built binaries on your device or emulator

**You do NOT need to work inside the Docker container** for normal development.


## Building

Docker must be installed and running. 

The following command lines will install the required Docker image to get the preconfigured Toolchain for compilation (credits: [Shaun Inman](https://github.com/shauninman/union-miyoomini-toolchain)), compile all the sources and make a release.

Open a Terminal and type : 

`git clone https://github.com/OnionUI/Onion.git`

`cd Onion/`

`make git-submodules`

`make with-toolchain`

Done!

### Common Build Commands

You can pass different make targets using the `CMD` parameter:

- `make with-toolchain` - Default: builds everything (equivalent to `make dist`)
- `make with-toolchain CMD=clean` - Clean build artifacts
- `make with-toolchain CMD=core` - Build core binaries only
- `make with-toolchain CMD=dev` - Build with debug logging enabled
- `make with-toolchain CMD=dist` - Build distribution package
- `make with-toolchain CMD=release` - Build release package

**Note:** The `CMD` parameter should contain only the make target name (e.g., `clean`, `core`), NOT the word "make" itself.


## Advanced: Using the Toolchain Interactively

If you need to run commands directly inside the Docker container (for debugging build issues or running specific toolchain commands), you can use:

`make toolchain`

This opens an interactive bash shell inside the Docker container. From there you can run any commands, like `make dev`.

**Note:** For regular development, you don't need this. Just edit code on your host machine and use `make with-toolchain` to build.


## Troubleshooting

### Error: "No rule to make target 'make'"

If you see this error:
```
make: *** No rule to make target 'make'.  Stop.
```

**Cause:** You included the word "make" in the CMD parameter.

**Wrong:** `make with-toolchain CMD="make clean"`  
**Correct:** `make with-toolchain CMD=clean`

The `CMD` parameter should only contain the target name, not the "make" command itself.
