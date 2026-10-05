# CGLib Standalone Repository Plan

> Status: the standalone top-level build, presets, dependency options, tests,
> examples, and viewer gates are implemented. Install/export and an external
> `find_package` consumer check exist for the CPU-only components (2026-10-05,
> `cmake/CGLibInstall.cmake`, `tests/run_install_consumer.ps1`). Vulkan-dependent
> components are not yet exported. Keep this document until they are.

## Goal

Make CGLib independently clonable, configurable, buildable, testable, and
installable while preserving its use as a Phantom submodule.

## Current constraints

- Only the CPU components are installable (`CGLIB_INSTALL`, default ON when CGLib is the
  top-level project): Math, Graphics, Numerics, Space, Scene, Volume, File, Animation, Terrain,
  Asset, SceneRuntime, exported as `CGLib::<Component>`. Headers keep the source-relative
  layout under `include/CGLib/` and bundle glm, nlohmann/json, Eigen and cgltf, because public
  headers reach them by relative path. Vulkan components (VulkanGraphics, UI, VkAppBase,
  VkRenderer, GltfRenderer, ...) are not exported yet.

## Implementation phases

1. **Complete:** establish Windows and Linux configure/build/test baselines.
2. **Complete:** move CMake helpers and presets into CGLib and remove
   parent-relative assumptions.
3. **Complete:** centralize dependency options and keep Vulkan optional.
4. **Mostly complete:** define consistent targets and usage requirements.
5. **Complete:** register tests with CTest and gate viewers behind options.
6. **Mostly complete:** install rules, exported targets, package configuration and an external
   consumer test exist for the CPU components; the Vulkan components remain.
7. **Complete:** update Phantom integration without duplicate target setup.

## Completion criteria

- A clean clone builds without the Phantom parent repository.
- CPU-only builds require no Vulkan SDK.
- Enabled tests pass on supported Windows and Linux toolchains.
- An external project consumes installed targets through `find_package`.
- Phantom continues to build CGLib as a pinned submodule.
