# CGLib public-header classification and consumer usage (PLAN_cglib_refactoring Phase 0)

Generated 2026-10-05 by `python tools/header_usage.py` (static `#include` analysis of CGLib,
Phantom/Physics, Phantom/PointCloud, Phantom/RayTracer and every CGApp sub-project; ThirdParty
and build trees skipped). Re-run the script to refresh; the tables below are a snapshot.

## How to read this

Classification is by **who actually includes a header today**, i.e. the de-facto API surface, not
the intended one:

- **public**: included by a consumer outside CGLib (Phantom modules or CGApp).
- **cross**: not used outside CGLib, but included by another CGLib module.
- **internal**: included only inside its own module.
- **unused**: included nowhere. Candidates only: may be an entry point, referenced by a build
  script or non-C++ code, or dead. Verify before removing (Phase 6).
- **test**: lives in a `*Test` directory; never public.

Limits: includes that rely on a target's extra include directories are resolved by unique file
name, so a few mappings may be missed; non-C++ consumers (C#, Python, Wasm glue) are not scanned
except for `#include` lines in C/C++ files. "public" therefore means "needed by at least one
known consumer", and a header marked **internal** may still be intended API for external users
(e.g. `Scene`, `Terrain`, which have no Phantom/CGApp consumer other than the ones listed).

Known imprecision: generic file names such as `Renderer.h` are matched by name when an include is
not path-qualified, so a few entries (e.g. `Space/SpaceView/Renderer.h` listed as public) may be false
positives from a same-named header in a consumer; check with `--list <Module>` before acting.

Decision aid for Phase 1/2: the installed CPU package (`cmake/CGLibInstall.cmake`) currently ships
every header of the CPU modules; this table shows how much of that is actually used externally.

### Header classification (counts)

| Module | public | cross | internal | unused | test | total |
|---|---|---|---|---|---|---|
| Animation | 3 | 7 | 9 | 2 | 0 | 21 |
| AssetCore | 2 | 3 | 0 | 0 | 0 | 5 |
| File | 14 | 5 | 6 | 0 | 0 | 25 |
| Gizmo | 1 | 0 | 1 | 0 | 0 | 2 |
| GltfRenderer | 17 | 3 | 10 | 0 | 0 | 30 |
| GltfViewer | 1 | 0 | 8 | 0 | 0 | 9 |
| Graphics | 6 | 4 | 1 | 0 | 0 | 11 |
| GraphicsTest | 0 | 0 | 0 | 0 | 1 | 1 |
| Input | 2 | 0 | 0 | 0 | 0 | 2 |
| Math | 13 | 20 | 2 | 1 | 0 | 36 |
| MathTest | 0 | 0 | 0 | 0 | 1 | 1 |
| Numerics | 1 | 0 | 3 | 0 | 1 | 5 |
| Particles | 1 | 0 | 4 | 0 | 0 | 5 |
| PostProcess | 2 | 0 | 5 | 0 | 0 | 7 |
| Renderer | 6 | 0 | 4 | 0 | 0 | 10 |
| Scene | 0 | 0 | 18 | 1 | 0 | 19 |
| SceneRuntime | 7 | 0 | 6 | 0 | 0 | 13 |
| Space | 9 | 1 | 20 | 1 | 1 | 32 |
| Terrain | 2 | 0 | 0 | 0 | 0 | 2 |
| UIWidgets | 30 | 0 | 17 | 1 | 0 | 48 |
| Util | 1 | 0 | 0 | 0 | 0 | 1 |
| VkAppBase | 6 | 0 | 2 | 0 | 0 | 8 |
| Volume | 12 | 1 | 41 | 1 | 1 | 56 |
| VulkanGraphics | 14 | 2 | 1 | 0 | 1 | 18 |
| **total** | 150 | 46 | 158 | 7 | 6 | 367 |

### Consumer usage (distinct CGLib headers included / total include directives)

| CGLib module | CGApp/Common | CGApp/FluidStudio | CGApp/PhantomStudio | CGApp/PointCloudStudio | CGApp/Universe | CGApp/VDBIO | CGApp/Wasm | CGApp/blender | CGApp/e2e_fixtures | CGApp/screenshots | CGApp/tools | Phantom/Physics | Phantom/PointCloud | Phantom/RayTracer |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Animation | - | - | - | - | 3 / 4 | - | - | - | - | - | - | - | - | - |
| AssetCore | - | - | - | - | 2 / 2 | - | - | - | - | - | - | - | - | - |
| File | - | 9 / 21 | 11 / 15 | - | 2 / 2 | - | - | - | - | - | - | 4 / 5 | 3 / 8 | 2 / 2 |
| Gizmo | - | - | 1 / 1 | - | - | - | - | - | - | - | - | - | - | - |
| GltfRenderer | 4 / 5 | 3 / 3 | 8 / 20 | - | 16 / 29 | - | - | - | - | - | - | 8 / 21 | - | 6 / 10 |
| GltfViewer | - | - | - | - | - | - | - | - | - | - | - | 1 / 2 | - | - |
| Graphics | - | - | 1 / 1 | 2 / 3 | 1 / 1 | - | - | - | - | - | - | 2 / 3 | 1 / 3 | 3 / 4 |
| Input | - | - | - | - | 2 / 5 | - | - | - | - | - | - | - | - | - |
| Math | - | 3 / 6 | 4 / 6 | 1 / 2 | 2 / 4 | - | 2 / 2 | 5 / 7 | - | - | - | 7 / 85 | 7 / 55 | 2 / 3 |
| Numerics | - | - | - | - | - | - | - | - | - | - | - | 1 / 1 | 1 / 4 | - |
| Particles | - | - | - | - | 1 / 1 | - | - | - | - | - | - | - | - | - |
| PostProcess | - | - | - | - | 2 / 2 | - | - | - | - | - | - | - | - | - |
| Renderer | - | 4 / 4 | 1 / 1 | 4 / 4 | 2 / 2 | - | - | - | - | - | - | 4 / 5 | 3 / 4 | - |
| SceneRuntime | 3 / 4 | - | 4 / 4 | - | 5 / 7 | - | - | - | - | - | - | - | - | - |
| Space | - | - | 1 / 1 | - | 1 / 3 | - | - | - | - | - | - | 6 / 14 | 2 / 17 | 1 / 1 |
| Terrain | - | - | 2 / 3 | - | - | - | - | - | - | - | - | - | - | - |
| UIWidgets | - | - | - | - | 1 / 1 | - | - | - | - | - | - | 26 / 89 | 4 / 12 | 1 / 1 |
| Util | 1 / 1 | - | - | - | 1 / 14 | 1 / 2 | - | - | - | - | - | 1 / 22 | - | - |
| VkAppBase | - | 2 / 6 | 1 / 2 | - | 6 / 17 | - | - | - | - | - | - | 6 / 26 | 6 / 13 | 6 / 7 |
| Volume | - | 3 / 4 | - | - | 3 / 4 | 1 / 3 | - | - | - | - | - | 7 / 13 | 1 / 2 | 1 / 2 |
| VulkanGraphics | - | 13 / 36 | 9 / 24 | 6 / 6 | 9 / 30 | - | - | - | - | - | - | 11 / 79 | 7 / 38 | 5 / 5 |

### Most-used headers by consumers (top 25)

- Math/Vector3d.h: 99 (CGApp/FluidStudio:2, CGApp/PhantomStudio:2, CGApp/PointCloudStudio:2, CGApp/Universe:3, CGApp/Wasm:1, CGApp/blender:2, Phantom/Physics:41, Phantom/PointCloud:44, Phantom/RayTracer:2)
- VulkanGraphics/VulkanContext.h: 61 (CGApp/FluidStudio:8, CGApp/PhantomStudio:5, CGApp/PointCloudStudio:1, CGApp/Universe:9, Phantom/Physics:29, Phantom/PointCloud:8, Phantom/RayTracer:1)
- VkAppBase/IVkSubRenderer.h: 42 (CGApp/FluidStudio:5, CGApp/PhantomStudio:2, CGApp/Universe:12, Phantom/Physics:18, Phantom/PointCloud:3, Phantom/RayTracer:2)
- VulkanGraphics/VulkanCommandPool.h: 39 (CGApp/FluidStudio:6, CGApp/PhantomStudio:5, CGApp/PointCloudStudio:1, CGApp/Universe:4, Phantom/Physics:16, Phantom/PointCloud:6, Phantom/RayTracer:1)
- Util/UnCopyable.h: 39 (CGApp/Common:1, CGApp/Universe:14, CGApp/VDBIO:2, Phantom/Physics:22)
- VulkanGraphics/VulkanBuffer.h: 33 (CGApp/FluidStudio:7, CGApp/PhantomStudio:3, CGApp/PointCloudStudio:1, CGApp/Universe:3, Phantom/Physics:12, Phantom/PointCloud:7)
- Math/Box3d.h: 31 (CGApp/FluidStudio:3, CGApp/Universe:1, CGApp/Wasm:1, CGApp/blender:2, Phantom/Physics:22, Phantom/PointCloud:2)
- GltfRenderer/Gltf/GltfDocument.h: 24 (CGApp/Common:1, CGApp/PhantomStudio:8, CGApp/Universe:5, Phantom/Physics:7, Phantom/RayTracer:3)
- VulkanGraphics/VulkanDescriptorPool.h: 18 (CGApp/FluidStudio:3, CGApp/PhantomStudio:2, CGApp/Universe:3, Phantom/Physics:5, Phantom/PointCloud:5)
- VulkanGraphics/VulkanSPVResolver.h: 17 (CGApp/FluidStudio:1, CGApp/PhantomStudio:4, CGApp/PointCloudStudio:1, CGApp/Universe:4, Phantom/Physics:2, Phantom/PointCloud:4, Phantom/RayTracer:1)
- VulkanGraphics/VulkanPipeline.h: 16 (CGApp/FluidStudio:3, CGApp/PhantomStudio:2, CGApp/Universe:3, Phantom/Physics:3, Phantom/PointCloud:5)
- Math/Matrix3d.h: 15 (CGApp/blender:1, Phantom/Physics:9, Phantom/PointCloud:5)
- Volume/Volume/SparseVolumeTree/SparseVolume.h: 12 (CGApp/FluidStudio:2, CGApp/Universe:1, CGApp/VDBIO:3, Phantom/Physics:6)
- UIWidgets/Button.h: 12 (Phantom/Physics:6, Phantom/PointCloud:6)
- GltfRenderer/Renderer/GltfSceneRenderer.h: 12 (CGApp/FluidStudio:1, CGApp/PhantomStudio:1, CGApp/Universe:2, Phantom/Physics:5, Phantom/RayTracer:3)
- UIWidgets/Label.h: 11 (Phantom/Physics:11)
- Space/Space/CompactSpaceHash.h: 11 (Phantom/Physics:3, Phantom/PointCloud:8)
- UIWidgets/IView.h: 9 (Phantom/Physics:9)
- Space/Space/KDTree.h: 9 (Phantom/PointCloud:9)
- Math/Quaternion.h: 9 (CGApp/blender:1, Phantom/Physics:8)
- VulkanGraphics/VulkanSampler.h: 8 (CGApp/FluidStudio:1, CGApp/Universe:2, Phantom/Physics:4, Phantom/RayTracer:1)
- VulkanGraphics/VulkanOffscreen.h: 8 (CGApp/FluidStudio:2, CGApp/Universe:1, Phantom/Physics:5)
- VkAppBase/ScenarioRunner/IScenarioDispatcher.h: 8 (CGApp/FluidStudio:1, CGApp/Universe:1, Phantom/Physics:3, Phantom/PointCloud:2, Phantom/RayTracer:1)
- UIWidgets/Separator.h: 8 (Phantom/Physics:8)
- Math/Triangle3d.h: 8 (CGApp/FluidStudio:1, CGApp/PhantomStudio:2, CGApp/blender:1, Phantom/Physics:3, Phantom/PointCloud:1)


## Per-module header lists (public = used outside CGLib; unused = included nowhere)

**Animation** public (3): `Animation/AnimationClip.h`, `Animation/Animator.h`, `Animation/Skeleton.h`

**AssetCore** public (2): `AssetCore/ContentHash.h`, `AssetCore/FileWatcher.h`

**File** public (14): `File/GLTFFile.h`, `File/GLTFFileReader.h`, `File/GLTFFileWriter.h`, `File/MTLFile.h`, `File/MTLFileWriter.h`, `File/OBJFile.h`, `File/OBJFileReader.h`, `File/OBJFileWriter.h`, `File/PLYFile.h`, `File/PLYFileReader.h`, `File/PLYFileWriter.h`, `File/STLFile.h`, `File/STLFileReader.h`, `File/STLFileWriter.h`

**Gizmo** public (1): `VkTransformGizmo.h`

**GltfRenderer** public (17): `Gltf/GltfAccessorBuilder.h`, `Gltf/GltfAccessorView.h`, `Gltf/GltfAnimationEvaluator.h`, `Gltf/GltfBounds.h`, `Gltf/GltfDocument.h`, `Gltf/GltfLightsCameras.h`, `Gltf/GltfMorphApply.h`, `Gltf/GltfReader.h`, `Gltf/GltfTypes.h`, `Gltf/MmdToGltfConverter.h`, `Gltf/ObjToGltfConverter.h`, `Gltf/StlToGltfConverter.h`, `IBL/GltfEnvironmentCubemap.h`, `Renderer/GltfLightShadowState.h`, `Renderer/GltfSceneRenderer.h`, `Renderer/LightManager.h`, `Renderer/ShadowMapPass.h`

**GltfViewer** public (1): `ControlPanel.h`

**Graphics** public (6): `Camera.h`, `ColorMap.h`, `EnsembleLodController.h`, `Image.h`, `ImageFileReader.h`, `ImageFileWriter.h`

**Input** public (2): `GameAction.h`, `InputManager.h`

**Math** public (13): `Box3d.h`, `Gaussian.h`, `Matrix3d.h`, `Plane3d.h`, `Quaternion.h`, `Ray3d.h`, `Statistics.h`, `Triangle3d.h`, `Vector2d.h`, `Vector3d.h`, `framework.h`, `glm.h`, `pi.h`

**Numerics** public (1): `Numerics/SVD3d.h`

**Particles** public (1): `ParticleEmitter.h`

**PostProcess** public (2): `FXAAEffect.h`, `ToneMappingEffect.h`

**Renderer** public (6): `VkRenderer/RenderBufferTypes.h`, `VkRenderer/VkLineRenderer.h`, `VkRenderer/VkPointBatchRenderer.h`, `VkRenderer/VkPointRenderer.h`, `VkRenderer/VkSkyBoxRenderer.h`, `VkRenderer/VkTriangleRenderer.h`

**SceneRuntime** public (7): `SceneRuntime/AssetSidecar.h`, `SceneRuntime/ClipPlayback.h`, `SceneRuntime/ColliderDesc.h`, `SceneRuntime/PlaySession.h`, `SceneRuntime/SceneV2Editor.h`, `SceneRuntime/SceneV2Merge.h`, `SceneRuntime/UniverseSceneV2.h`

**Space** public (9): `Space/BVH.h`, `Space/CompactSpaceHash.h`, `Space/IntersectionCalculator.h`, `Space/KDTree.h`, `Space/NeighborIndexView.h`, `Space/NeighborList.h`, `Space/Octree.h`, `Space/SignedDistanceCalculator.h`, `SpaceView/Renderer.h`

**Terrain** public (2): `Terrain/TerrainGenerator.h`, `Terrain/TerrainTypes.h`

**UIWidgets** public (30): `BoolView.h`, `Box3dView.h`, `Button.h`, `ComboBox.h`, `DisableScope.h`, `FileOpenDialog.h`, `FileOpenView.h`, `FileSaveDialog.h`, `FileSaveView.h`, `FloatDrag.h`, `FloatSlider.h`, `FloatView.h`, `IView.h`, `IWindow.h`, `IdScope.h`, `Immediate.h`, `IntSlider.h`, `IntView.h`, `Label.h`, `MainMenuBar.h`, `Menu.h`, `MenuItem.h`, `Row.h`, `Section.h`, `Separator.h`, `Spacing.h`, `StringView.h`, `Vector3dView.h`, `Window.h`, `tinyfiledialogs.h`

**Util** public (1): `UnCopyable.h`

**VkAppBase** public (6): `IVkSubRenderer.h`, `ScenarioRunner/IScenarioDispatcher.h`, `ScenarioRunner/IScenarioHost.h`, `ScenarioRunner/ScenarioBrowserPanel.h`, `ScenarioRunner/ScenarioRunner.h`, `VkAppBase.h`

**Volume** public (12): `Volume/LevelSet.h`, `Volume/MCSurfaceBuilder.h`, `Volume/ScalarGrid3D.h`, `Volume/SparseVolumeTree/Interpolator.h`, `Volume/SparseVolumeTree/SparseVolume.h`, `Volume/SurfaceVoxelizer.h`, `VolumeRaymarch/VolumePbvrGpu.h`, `VolumeRaymarch/VolumeRaymarchGpu.h`, `VolumeRenderer/IPBVRDataSource.h`, `VolumeRenderer/PBVRRenderer.h`, `VolumeView/MenuPanel.h`, `VolumeView/SceneListPanel.h`

**VulkanGraphics** public (14): `IGpuProfiler.h`, `VulkanBuffer.h`, `VulkanCommandPool.h`, `VulkanComputePipeline.h`, `VulkanContext.h`, `VulkanCubeMap.h`, `VulkanDescriptorPool.h`, `VulkanImage.h`, `VulkanOffscreen.h`, `VulkanPipeline.h`, `VulkanRenderPass.h`, `VulkanSPVResolver.h`, `VulkanSampler.h`, `VulkanSwapChain.h`

### Unused header candidates

- `Animation/AnimationRenderer/BoneWireRenderer.h`
- `Animation/AnimationRenderer/SkinnedMeshRenderer.h`
- `Math/Ray2d.h`
- `Scene/Scene/IAnimator.h`
- `Space/SpaceView/PanelHelpers.h`
- `UIWidgets/IOkCancelView.h`
- `Volume/Volume/VolumeCell.h`
