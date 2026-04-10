## Inferno Renderer

<img width="1121" height="957" alt="image" src="https://github.com/user-attachments/assets/35d53fd0-7138-4243-ac21-4d45294cecec" />
<img width="1423" height="1073" alt="Screenshot from 2026-04-07 01-57-54" src="https://github.com/user-attachments/assets/662d5ffa-8f65-4157-bb4c-8f39bd68f008" />
<img width="1017" height="691" alt="image" src="https://github.com/user-attachments/assets/7b57f037-e52e-4511-a031-3434fb904f7e" />
<img width="1630" height="525" alt="Screenshot from 2026-04-10 01-19-07" src="https://github.com/user-attachments/assets/6cffe16f-f782-44bb-82ef-e44108de5ece" />



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

