## Inferno Renderer

<img width="1791" height="1413" alt="image" src="https://github.com/user-attachments/assets/e5a86bc4-d491-4f85-893e-cc98cab3802c" />

WIP Vulkan renderer.


## Linux
- Make sure a Vulkan-capable GPU driver is installed and working on your system.
- Download and extract the Vulkan SDK tarball.
- Activate the SDK environment in your current shell.

Then to build, from the repository root:

```bash
./build.sh
```

From the repository root:

```bash
./bin/engine
```


## Windows
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
- Click the run button.

