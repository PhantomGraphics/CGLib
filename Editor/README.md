# Editor

Shared native editing functionality for WPF hosts and ImGui viewers.
Features include the transform gizmo in `Gizmo/` and shared input in `Input/`.

- `EditorCore` / `CGLib::Editor`: CPU-only gizmo types/geometry, input notation,
  keymaps, command/action resolution, named action state, command schemas/responses,
  reference syntax helpers, and ordered selection sets.
- `EditorVulkanCore` / `CGLib::EditorVulkan`: existing Vulkan transform gizmo,
  including picking and dragging. Requires Vulkan headers; no WPF or ImGui dependency.

Include `CGLib/Editor/Gizmo/GizmoTypes.h`, `GizmoGeometry.h`, or
`VkTransformGizmo.h`. The existing `Phantom::Gizmo` namespace is retained.
Applications supply camera/pointer state, apply returned transforms, and own
selection liveness/primary-ID policy, undo transactions, UI panels and scene lifetime. Shader binaries are
supplied by the host through `VkTransformGizmo::Shaders`.

The Vulkan class currently owns both interaction and rendering. A future second
rendering backend should extract the interaction state into a CPU controller.
Do not add WPF/ImGui event types to the shared API. Add selection or command
functionality when multiple applications need a concrete shared implementation.

## Commands and selection

`Commands/Registry.h`, `Response.h` and `References.h` retain the existing
`PG::CommandCore` namespace and command behavior. CGApp headers forward to these
headers; Registry and reference helpers are compiled only into `EditorCore`.
`Response.h` exposes nlohmann JSON. The bundled header-only JSON include directory
is a PUBLIC EditorCore dependency; no JSON binary library is linked.

`Selection/SelectionSet.h` provides `Phantom::Editor::SelectionSet<Id>` with
ordered replace/add/toggle/prune operations. Integer and string IDs use the same
implementation. Repeated IDs in toggle are processed in sequence. Hosts retain
ID resolution, scene ownership, liveness validation and primary selection policy.
PointCloudStudio keeps the last selected ID and activeCloudId synchronization.

Future native PromptSession logic should live in `Editor/Commands`, using the
schema and tokenizer here without a dependency on CGApp or UI. The existing
CommandCore include location can remain a compatibility facade. This migration
does not add prompt metadata or change execution routes.

## Input

`Phantom::Editor` provides `InputSpec`, `BindingTable`, keymap parsing/formatting,
`triggerCommands`, `deltaCommands`, `resolveActions`, and `ActionState` through
`CGLib/Editor/Input/`. Tokenization and script/keymap line reading live in
`CGLib/Editor/Commands/`. These APIs require only the C++ standard library.

Hosts translate WPF/GLFW events into input specs, apply UI capture/modifier
checks, and supply device values to `resolveActions`. Commands resolve the
active context before the global fallback. Actions retain the existing Universe
policy of combining active/global bindings, inverting axes, scaling, clamping
to [0,1], and taking the maximum for each arbitrary action name.
`ActionState` stores the resolved values and persistent digital scenario overrides.

Editor has no GLFW dependency or device backend. Universe owns its gamepad
polling, `GameAction` vocabulary, input tracking/replay and default keymap in
`CGApp/Universe`. The former standalone `InputCore` library is removed.
Legacy `CGApp/Common/CommandCore` input, tokenizer and script headers forward
to the Editor API; their implementation is compiled only into `EditorCore`.

Standalone CPU build:

```sh
cmake -S Editor -B build/Editor -DCGLIB_ENABLE_VULKAN=OFF
cmake --build build/Editor
```

With a Vulkan SDK available, use `-DCGLIB_ENABLE_VULKAN=ON`.
