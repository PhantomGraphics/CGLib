# Editor

Shared native editing functionality for WPF hosts and ImGui viewers.
The first feature is the transform gizmo in `Gizmo/`.

- `EditorCore` / `CGLib::Editor`: CPU-only gizmo types and generated handle geometry.
- `EditorVulkanCore` / `CGLib::EditorVulkan`: existing Vulkan transform gizmo,
  including picking and dragging. Requires Vulkan headers; no WPF or ImGui dependency.

Include `CGLib/Editor/Gizmo/GizmoTypes.h`, `GizmoGeometry.h`, or
`VkTransformGizmo.h`. The existing `Phantom::Gizmo` namespace is retained.
Applications supply camera/pointer state, apply returned transforms, and own
selection, undo transactions, UI panels and scene lifetime. Shader binaries are
supplied by the host through `VkTransformGizmo::Shaders`.

The Vulkan class currently owns both interaction and rendering. A future second
rendering backend should extract the interaction state into a CPU controller.
Do not add WPF/ImGui event types to the shared API. Add selection or command
functionality when multiple applications need a concrete shared implementation.

Standalone CPU build:

```sh
cmake -S Editor -B build/Editor -DCGLIB_ENABLE_VULKAN=OFF
cmake --build build/Editor
```

With a Vulkan SDK available, use `-DCGLIB_ENABLE_VULKAN=ON`.
