# Editor

Shared native editing functionality for WPF hosts and ImGui viewers.
Features include the transform gizmo in `Gizmo/` and shared input in `Input/`.

- `EditorCore` / `CGLib::Editor`: CPU-only gizmo types/geometry, input notation,
  keymaps, command/action resolution, named action state, command schemas/responses,
  reference syntax helpers, ordered selection sets, header-only console state and
  snapshot history.
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

`Commands/Registry.h`, `Response.h` and `References.h` use the
`Phantom::Editor::CommandCore` namespace. Command behavior is unchanged. CGApp headers forward to these
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

## Selection geometry and group gizmo calculations

`Selection/SelectionGeometry.h` exposes `meshSelectedInRectangle`: callers
supply local vertex positions, triangle indices, the model/view/projection
matrix and `SelectionRectangle` viewport/drag coordinates. It selects fully
contained meshes on left-to-right drags and clipped triangle intersections on
right-to-left drags. Clip depth is Vulkan's [0,w]; screen coordinates map
NDC (-1,-1) to (0,0). The host supplies its rendering projection (including its
Y convention), filters visibility/locks and resolves scene meshes and IDs.

`Gizmo/MultiSelectionGizmo.h` computes the selection centroid and applies pivot
deltas using `Phantom::Gizmo::GizmoTransform`. Translation and Euler degrees are
added; scale is multiplied by per-axis ratios (zero pivot scale leaves that axis
unchanged). Object positions are unchanged in rotate/scale modes. Hosts convert
their transform types and own drag snapshots and Undo transactions.

## Console

`Console/CommandConsoleModel.h` and `CommandInfo.h` provide header-only,
standard-library-only console state in `Phantom::Editor`. No dependency on
VkAppBase, ScenarioRunner, Vulkan or ImGui is required. The legacy global types
and `VkAppBase/ScenarioRunner` includes forward to these types.

The host supplies a command catalog and dispatches submitted lines. Manual
help/clear stay local; scenario lines reach the dispatcher unchanged after
outer whitespace trimming. Responses must arrive in dispatch order. The model
tracks each pending command's origin, logs responses and returns scenario
responses plus unclaimed responses to the host. ViewShell retains the scenario
proxy and UI connection. Clearing the log preserves pending ownership/history.
The log holds at most 2000 lines; history keeps its existing unbounded behavior.

## History

`History/UndoStack.h` provides `Phantom::Editor::UndoStack<T>`, a header-only,
standard-library-only snapshot stack. FluidStudio's `PG::FS::UndoStack<T>` is a
compatibility alias. Hosts capture state before an edit and apply the state
returned by undo/redo; the stack owns only history values. It clears redo on
push, discards the oldest undo state beyond the limit (default 100; zero disables
history), and clears both stacks at host-defined new/open/import boundaries.

Universe retains its inverse/function-based history. PointCloudStudio retains
shared-buffer snapshots and memory-budget eviction. Neither is converted to this
count-limited snapshot stack.

## Conditional migration decisions

- ViewShell remains in VkAppBase/ScenarioRunner: it already serves the viewers,
  owns ImGui windows/settings and adapts ScenarioRunner to the console. Its shared
  CPU console state is now in Editor; moving the remaining UI would require a
  separate EditorImGui target and scenario adapter without adding shared behavior.
- Universe completion remains beside CommandTable: multi-word verbs, positional
  arguments and option-key completion differ from the viewer catalog. There is
  currently no second host consuming that grammar or metadata contract.
- Universe CommandLog retains command-source semantics, delta merging,
  entity-name rewriting integration and read-only/script filtering. ConsoleModel
  is the shared viewer log/history; no common command-log contract is established.
- JobManager remains in CGApp/Common/CommandCore, shared by FluidStudio and
  PointCloudStudio. It is a generic poll/cancel registry for host-owned conversion,
  simulation and rendering work, not tied to editing. Moving it into Editor would
  add thread coordination without a new Editor consumer. Host callbacks must stay
  alive while jobs are registered; the manager does not own worker threads.

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
