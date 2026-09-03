# CGLib Module Reference

This reference summarizes CGLib's first-party modules and principal APIs.

## Dependency overview

```text
Math ──┬── Graphics ──┬── File
       │              └── UIWidgets ──┐
       ├── Numerics                   │
       ├── Space ──── Volume          │
       ├── Scene                      │
       └── VulkanGraphics ──┬─────────┴── VkAppBase ──┬── GltfRenderer
                            └── Renderer ─────────────┘
```

## Core modules

- **Math:** GLM-backed vectors, matrices, quaternions, parametric geometry
  interfaces, primitives, and free-function algorithms.
- **Graphics:** `Camera`, color types and conversion, color maps, `Image`, and
  STB-backed image readers and writers.
- **Numerics:** Eigen adapters plus 2D/3D symmetric eigendecomposition and
  Jacobi SVD.
- **File:** OBJ, glTF/GLB, PLY, and STL readers and writers.
- **Space:** octrees, KD-trees, BVHs, spatial hashes, Morton curves,
  intersections, closest-point queries, and distances.
- **Scene:** scene graphs and the Presenter pattern.
- **Volume:** sparse volumes, level sets, sampling, and Marching Cubes.

## Vulkan modules

- **VulkanGraphics:** RAII wrappers for instances, devices, queues, swapchains,
  buffers, images, descriptors, render passes, pipelines, commands, and
  synchronization.
- **UIWidgets:** composable Dear ImGui widgets and panels.
- **VkAppBase:** window and Vulkan lifecycle, main loop, resize handling,
  command-line processing, screenshots, and scenario execution.
- **Renderer:** reusable triangle, point, line, mesh, grid, and skybox
  subrenderers implementing the `IVkSubRenderer` lifecycle.
- **GltfRenderer:** glTF 2.0, VRM, MMD, materials, animation, and IBL.
- **Animation:** skeletal clips, poses, interpolation, and playback.
- **Particles:** GPU simulation and billboard rendering.
- **PostProcess:** bloom, SSAO, FXAA, and tone mapping.
- **Gizmo:** interactive transform manipulators.
- **Input:** device-to-action input mapping.

## Design conventions

- Public APIs use Phantom math types; third-party types stay behind module
  boundaries.
- Resource owners are non-copyable unless copying has explicit semantics.
- Registered raw pointers are non-owning; callers preserve their lifetime.
- CPU-only modules must not acquire Vulkan dependencies.
- Tests cover empty, boundary, degenerate, and deterministic-order cases.
- Regressible viewer behavior belongs in JSON scenarios.
