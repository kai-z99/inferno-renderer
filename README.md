## Inferno Renderer

WIP Vulkan renderer.

- Current status: working on PBR pipeline.

## Install (Ubuntu/Debian)

- Prereqs: GPU driver is installed. (eg. mesa-vulkan-drivers or nvidia-driver)

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
  libvulkan-dev glslang-tools
```

Note: 
You can also use the official LunarG Vulkan SDK tarball instead of distro Vulkan development packages.


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
