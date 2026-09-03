# CGLib

CGLib is a collection of reusable C++20 graphics and numerical-computing
libraries that form the foundation of the
[Phantom](https://github.com/PhantomGraphics/Phantom) framework.

It includes linear algebra and geometry, Vulkan RAII wrappers, ImGui widgets,
asset I/O, scene graphs, spatial data structures, sparse volumes,
glTF/VRM/MMD rendering, animation, particles, and Eigen adapters.

## Modules

| Module | Namespace | Purpose | Vulkan |
|---|---|---|:---:|
| `Math` | `Phantom::Math` | Vectors, matrices, quaternions, geometry | No |
| `Graphics` | `Phantom::Graphics` | Cameras, color spaces, image I/O | No |
| `Numerics` | `Phantom::Numerics` | Eigenvalue decomposition and SVD | No |
| `File` | `Phantom::File` | OBJ, glTF, PLY, and STL I/O | No |
| `Space` | `Phantom::Space` | Spatial indexing and geometric queries | No |
| `Scene` | `Phantom::Scene` | Scene graph and Presenter pattern | No |
| `Volume` | `Phantom::Volume` | Sparse volumes and Marching Cubes | No |
| `VulkanGraphics` | `VKG` | Vulkan object abstractions | Yes |
| `UIWidgets` | `Phantom::UI` | ImGui UI framework | Yes |
| `VkAppBase` | `VKG` | Window, main loop, screenshots | Yes |
| `Renderer` | `VKG` | Reusable subrenderers | Yes |
| `GltfRenderer` | — | glTF, VRM, MMD, and IBL rendering | Yes |
| `Animation` | `Phantom::Animation` | Skeletal animation | Yes |
| `Particles` | `Phantom::Particles` | GPU particles and billboards | Yes |
| `PostProcess` | `Phantom::PostProcess` | Bloom, SSAO, FXAA, tone mapping | Yes |
| `Gizmo` | `Phantom::Gizmo` | Transform gizmos | Yes |
| `Input` | `Phantom::Input` | Input mapping | No |

See [the module reference](docs/module-reference.md) for public APIs and design
conventions.

## Requirements

- A C++20 compiler: MSVC v143+, Clang 15+, or GCC 11+
- CMake 3.20+ and Ninja
- Vulkan SDK 1.3+ for Vulkan-dependent modules
- GoogleTest when building tests

GLM, Dear ImGui, VMA, STB, nlohmann/json, tinyfiledialogs, cgltf, and Eigen are
vendored.

## Build

```bash
git clone https://github.com/PhantomGraphics/CGLib.git
cd CGLib
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Use `-DCGLIB_ENABLE_VULKAN=OFF` for an explicit CPU-only build. Missing Vulkan
dependencies cause Vulkan targets to be skipped; CPU-only modules remain
buildable. CGLib can also be consumed as the `CGLib/` submodule of Phantom.
A module can be configured directly:

```bash
cmake -S CGLib/Space -B CGLib/Space/build -DCMAKE_BUILD_TYPE=Debug
cmake --build CGLib/Space/build
```

## Tests and viewers

Run all tests with `ctest --preset windows-debug` or execute a test binary
under `build/windows-debug/CGLib/`. Viewer applications include
`AnimationView`, `GltfViewer`, `SpaceView`, `VolumeView`, and
`VkRendererView`. They support frame-based screenshots and JSON scenarios.

## Documentation and license

- [Module reference](docs/module-reference.md)
- [Standalone-repository plan](docs/standalone-repository-plan.md)
- Module README files under `Math/`, `Graphics/`, `Numerics/`, and `Space/`

CGLib is distributed under the [MIT License](LICENSE). Bundled dependencies
remain subject to their respective licenses.
