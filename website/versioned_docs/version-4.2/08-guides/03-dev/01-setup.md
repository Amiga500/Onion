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

`make with-toolchain` or  
`make with-toolchain CMD=dev` (to enable debug logging )

Done!


## Advanced: Using the Toolchain Interactively

If you need to run commands directly inside the Docker container (for debugging build issues or running specific toolchain commands), you can use:

`make toolchain`

This opens an interactive bash shell inside the Docker container. From there you can run any commands, like `make dev`.

**Note:** For regular development, you don't need this. Just edit code on your host machine and use `make with-toolchain` to build.
