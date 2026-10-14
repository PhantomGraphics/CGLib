# Effects

Reusable Vulkan visual effects, built as `EffectsCore` (`CGLib::Effects`).

- `Particles/`: emitter configuration, GPU particle storage and compute updates,
  and billboard rendering. Namespace: `Phantom::Effects::Particles`.
- `PostProcess/`: image-space Bloom, SSAO, tone mapping and FXAA, plus
  `PostProcessChain`. Namespace: `Phantom::Effects::PostProcess`.

Effects uses VulkanGraphics for GPU resources and pipelines. The application
provides shader SPIR-V, command buffers, camera/input images and execution order.
`IPostEffect` is specific to image-space processing; particle simulation and
rendering have their own interfaces. Physics solvers and scene ownership belong
to their respective modules.

Build from the CGLib root:

```sh
cmake -S Effects -B build/Effects -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Effects
```

The top-level Vulkan-enabled CGLib build also includes this module. Vulkan
headers are required; CPU-only configurations omit it. Effects is a build-tree
component and is not currently exported by the installed CGLib package.

Shaders and their compilation scripts remain under each subdirectory's
`shaders/`. Shader lookup and packaging are managed by the application.
