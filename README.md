## Inferno Renderer

WIP Vulkan renderer.

- Current status: working on PBR pipeline.

## Install (Linux)

- Prereqs: GPU driver is installed. (eg. mesa-vulkan-drivers or nvidia-driver)

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
  libvulkan-dev glslang-tools

# Fedora
sudo dnf install -y gcc gcc-c++ make cmake ninja-build pkgconf-pkg-config \
  vulkan-loader-devel glslang

# Arch Linux
sudo pacman -Sy --needed base-devel cmake ninja \
  vulkan-headers vulkan-icd-loader glslang
```

Note: 
You can also use the official LunarG Vulkan SDK tarball instead of distro Vulkan development packages.

## Install (Windows)
- Use the CMake GUI to configure and generate the .sln file.
- Open the .sln with VS, then click "Build Solution". 


## Build

From the repository root:

```bash
./build.sh
```

## Run

From the repository root:

```bash
./bin/engine
```
