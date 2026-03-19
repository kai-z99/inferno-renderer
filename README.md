## Inferno Renderer

WIP Vulkan renderer.

- Current status: working on PBR pipeline.

## Install (Linux)

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

#### Note
You can also download the offical LunarG Vulkan SDK tarball instead of downloading the Vulkan tools from distro packages. 

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
