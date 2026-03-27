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

## Build (Linux)

From the repository root:

```bash
./build.sh
```

## Run (Linux)

From the repository root:

```bash
./bin/engine
```


## Install/Build (Windows)
- Install the Vulkan SDK
- In the CMake GUI, set source code path to:
```bash
path/to/inferno-renderer
```
- Set "Where to build binaries" to:
```bash
path/to/inferno-renderer/build
```
- Click "Configure", then click "Generate" to create the .sln file.
- Open the .sln with VS, then click "Build Solution".

## Run (Windows)
- Set "engine" as the startup project in VS.
- Click the run button.
