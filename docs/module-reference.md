# CGLib Module Reference

This reference summarizes CGLib's first-party modules and principal APIs.

## Dependency overview

```text
Math ──┬── Graphics ──┬── File
       │              └── UIWidgets ──┐
       ├── Numerics                   │
       ├── Space ──── Volume          │
       ├── Scene                      │   (legacy, no external users: docs/scene-usage-2026-10-06.md)
       ├── GeometryNode               │
       ├── AssetCore ── SceneRuntime  │
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
- **Scene:** scene graphs and the Presenter pattern. Legacy: nothing outside the module's own tests and
  the install consumer uses it; new code uses SceneRuntime
  (`docs/scene-usage-2026-10-06.md`).
- **GeometryNode:** CPU evaluation of node-graph geometry (fields, attribute domains, terrain nodes).
- **AssetCore:** project-relative asset ids, URIs and manifests (no Math or Vulkan dependency).
- **SceneRuntime:** UUID node hierarchy, TRS, generic components and schemas, play sessions, `.universe`
  v2 read/write and merge. Uses `AssetCore`'s `AssetId` as the node id.
- **Volume:** sparse volumes, level sets, sampling, and Marching Cubes.

## Vulkan modules

- **VulkanGraphics:** RAII wrappers for instances, devices, queues, swapchains,
  buffers, images, descriptors, render passes, pipelines, commands, and
  synchronization.
- **UIWidgets:** composable Dear ImGui widgets and panels.
- **VkAppBase:** window and Vulkan lifecycle, main loop, resize handling,
  command-line processing, screenshots, and scenario execution. The frame loop is built from
  small units: `FrameSync` (semaphores, fences, per-image fence table), `FrameRecording`
  (render-pass begin, clear values, viewport/scissor), `FrameReadback` (screenshot and
  single-pixel readback state), and `ScreenshotCapture` (argument parsing, BGRA handling, PNG
  output). `ScenarioRunner/CommandQueue.h` is the thread-safe command/response queue the
  viewers' command dispatchers share.
- **Renderer:** reusable triangle, point, line, mesh, grid, and skybox
  subrenderers implementing the `IVkSubRenderer` lifecycle.
- **GltfRenderer:** glTF 2.0, VRM, MMD, materials, animation, and IBL. `GltfSceneRenderer` is the
  entry point; its parts are separate units: `GltfObjectAnimation` (clip/time/animated-node mask),
  `GltfGlobalDescriptors` (set 0/1 layouts, pools, per-frame writes), `GltfMainPipelines` (the four
  blend x double-sided pipelines), `GltfPipelineVariantPool` (`.phmat` override pipelines),
  `GltfPrimitiveData` (AABB centre, animation geometry, blend sort) and `nodeLocalMatrix()`.
- **Animation:** skeletal clips, poses, interpolation, and playback.
- **Particles:** GPU simulation and billboard rendering.
- **PostProcess:** bloom, SSAO, FXAA, and tone mapping.
- **Gizmo:** interactive transform manipulators.
- **Input:** device-to-action input mapping.

## Design conventions

- Public APIs use Phantom math types; third-party types stay behind module
  boundaries.
- Resource owners are non-copyable unless copying has explicit semantics.
- Registered raw pointers are non-owning; callers preserve their lifetime. UIWidgets children follow
  `docs/ui-widgets-ownership.md`; VulkanGraphics objects follow `docs/vulkan-ownership.md`
  (`create()` on a live object destroys first; a failed `create()` releases everything it made).
- Public headers listed for install are self-contained (the install consumer compiles each on its own).
- CPU-only modules must not acquire Vulkan dependencies.
- Tests cover empty, boundary, degenerate, and deterministic-order cases.
- Regressible viewer behavior belongs in JSON scenarios.
