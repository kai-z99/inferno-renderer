## Inferno Renderer

WIP Vulkan renderer.

- Current status: working on PBR pipeline.

## Install And Build (Linux)

This project builds most third-party libraries, but you still need:

- A C++ toolchain and CMake/Ninja.
- Vulkan headers/loader 
- Shader tools

Note: You can also download the offical LunarG Vulkan SDK tarball instead of downloading the Vulkan tools from distro packages. 

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
  libvulkan-dev glslang-tools mesa-vulkan-drivers
```

### Fedora

```bash
sudo dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config \
  vulkan-loader-devel glslang vulkan-tools vulkan-validation-layers
```

### Arch 

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf \
  vulkan-headers vulkan-icd-loader glslang vulkan-tools
```



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