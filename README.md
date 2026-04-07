## Inferno Renderer

<img width="1140" height="964" alt="Screenshot from 2026-04-07 01-55-53" src="https://github.com/user-attachments/assets/09e072b8-bb7e-44df-b2a9-6bbabe114392" />
<img width="1423" height="1073" alt="Screenshot from 2026-04-07 01-57-54" src="https://github.com/user-attachments/assets/662d5ffa-8f65-4157-bb4c-8f39bd68f008" />
<img width="1083" height="916" alt="image" src="https://github.com/user-attachments/assets/11cf5f08-0c3a-49c5-9ec6-8111928ccf55" />


Inferno Renderer is a cross platform realtime renderer built with Vulkan, that supports the glTF 2.0 PBR standard.


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

