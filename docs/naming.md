# Class and file naming

Public classes and structs use PascalCase. A header containing one primary type
uses that type's name; its implementation uses the same stem. Headers grouping
several related types use a descriptive plural name such as `GltfUniformTypes.h`.

- Use `Gltf` for the glTF format in C++ names, including `GltfFileReader`.
- Keep algorithm acronyms uppercase: `DBSCAN`, `FPFH`, `ICP`, `MLS`, `RANSAC`,
  `PBVR`, `SPH`, and `XPBD`. For example, `RANSACPlaneDetector` and
  `VolumePBVRGpu`.
- Write dimensional suffixes as `2d` and `3d`: `ConcaveHull2d`, `ScalarGrid3d`.
- Use a trailing `Gpu` for GPU renderer implementations: `SkinnedRendererGpu`.
  GPU resource types can qualify the resource itself, as in `GltfGpuMesh`.
- Use `CubeMap` for cube maps, as in `GltfEnvironmentCubeMap`.
- Preserve the full represented type name in widgets: `Rectangle3dView`.
- Use matching names for GoogleTest suites and corresponding test source files.

## October 2026 migration

These source-level renames require consumers to update includes and C++ type
names. Python bindings exposing renamed classes also use the new spelling and
must be rebuilt. Serialized keys, scenario commands, and file formats are unchanged.

| Previous header | Current header |
|---|---|
| `DBSCAN.h` | `DBSCANClustering.h` |
| `Renderer/GltfMesh.h` | `Renderer/GltfGpuMesh.h` |
| `Renderer/GltfMaterial.h` | `Renderer/GltfGpuMaterial.h` |
| `Renderer/CameraUBO.h` | `Renderer/GltfUniformTypes.h` |
| `NeighborList.h` | `CSRNeighborList.h` |
| `SparseVolumeTree/Interpolator.h` | `SparseVolumeTree/TrilinearInterpolator.h` |
| `VolumeView/VolumeScene.h` | `VolumeView/SparseVolumeScene.h` |
| `CloudParticle.h` | `CloudParticleSoA.h` |
| `Commands/Registry.h` | `Commands/CommandRegistry.h` |
| `UniverseSceneV2.h` | `SceneV2.h` |

Classes following these headers retain their names. Additional type and file
renames follow the conventions above, including `GLTFFile*` to `GltfFile*`,
`Ransac*` to `RANSAC*`, `Pbvr*` rendering types to `PBVR*`, `Rect3dView` to
`Rectangle3dView`, and `GpuSkinnedRenderer` to `SkinnedRendererGpu`.
