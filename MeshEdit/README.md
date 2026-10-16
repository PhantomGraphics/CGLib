# MeshEdit Boolean

`MeshEditCore` provides CPU polygon-mesh editing. `MeshEditView` is its Vulkan test viewer.
Boolean operations are implemented in `Core/MeshBoolean.h` without an external CSG library.

```cpp
using namespace Phantom::MeshEdit;
EditableMesh result;
std::string error;
if (!booleanMeshes(a, b, BooleanOperation::Difference, result, {}, &error)) {
    // a, b and result are unchanged; error explains the rejection.
}
```

The API supports union, A minus B, and intersection. Supply both meshes in the same
coordinate system. Inputs must be closed, consistently oriented, non-self-intersecting
two-manifold surfaces with no isolated vertices. Planar concave quads are triangulated;
non-planar quads are rejected. Disconnected solids and nested cavity shells are supported.
Outer shells face outward; cavity boundaries face into the cavity. Empty meshes are empty
sets, including successful empty results. A point/edge-only contact that makes a union
non-manifold is rejected. Face contact and coplanar overlap are handled by BSP clipping.

Output contains triangles, per-corner interpolated UVs and optional normalized normals.
UV seams remain per-corner; subtraction reverses B's retained faces and normals. All
output element ids are new and start at A's respective next-id counters. Every successful
result has a fresh revision. Input/output aliasing is supported. No automatic repair or
quad reconstruction is performed.

## Numerical and resource limits

Geometry is centered and normalized to the combined input AABB diagonal, then computed
in double precision. `relativeTolerance` defaults to `1e-7` (accepted range `1e-12..1e-3`).
Output positions remain float. Subprecision geometry, ambiguous close vertices, collapsed
float output, invalid topology, and invalid shell orientation return an error rather than
installing a partial result. This is a tolerance-based implementation, not exact arithmetic.

`BooleanOptions` defaults to 100,000 input triangles (both operands combined), 1,000,000
cumulative input/split polygons and 1,000,000 output faces. Traversals also have a fixed
50-million-work budget; BSP construction rejects depth above 512. These are safety caps,
not promises that every input below the caps will complete. Execution is synchronous.

The pipeline validates and triangulates inputs, clips convex fragments with deterministic
plane BSPs, interpolates source corner attributes, welds shared intersection points,
splits T-junctions and triangulates convex fragments while preserving their boundary
vertices. Final float geometry is checked again for closed topology, vertex fans,
self-intersection and shell nesting before publication.

## Viewer

Open **Mesh > Show Control Window**, leave edit mode, and use the **Boolean** controls.
Create a box cutter or copy the evaluated mesh. Position/rotation/positive XYZ scale place
the cutter; rotations use degrees and `T * Ry * Rx * Rz * S`. Pink wireframe shows the
cutter. Dragging previews its placement without adding history entries; release commits
one placement change. The operation dropdown offers Union, Difference and Intersection.
Apply Boolean installs the evaluated main mesh's result and clears its modifier stack and
selection as one Undo step. Undo/Redo restores the mesh, modifier stack and cutter state.
An evaluated modifier result that has non-planar quads (e.g. subdivided curved surfaces)
is rejected with the original stack and mesh intact.

The initial cutter is a unit box at `(0.35, 0.2, 0)` with zero rotation and unit scale,
hidden to preserve the viewer's initial display. Creating a cutter shows it. New Mesh
resets the cutter and clears history. Visibility and cutter creation are undoable. The
cutter is independent of element picking and the main mesh's counts/volume.

UI, command console and scenarios use the same handlers:

```text
SetCutter:box
SetCutter:copy
SetCutterTransform:position=0.5,0,0 rotation=0,25,0 scale=1,1,1
SetCutterTransform:position=0.6,0,0 preview=1
SetCutterVisible:1
GetCutter
Boolean:union
Boolean:difference
Boolean:intersection
Undo
Redo
```

Omitted transform fields keep their committed values. `preview=1` affects only draw data;
a normal SetCutterTransform commits placement and clears preview. New Mesh, mode changes,
successful Boolean and history restoration also clear preview. UI errors appear beneath
the Boolean controls; rejected operations do not change the committed state or history.

## Validation

From the CGLib root in a Visual Studio developer shell:

```powershell
$env:VSLANG = '1033' # Keep MSVC showIncludes output compatible with Ninja's dependency scanner.
cmake --build --preset windows-debug --target MeshEditTest MeshEditView
ctest --preset windows-debug -R '^(MeshBoolean|MeshWorldBoolean|MeshEditViewBooleanCommands)\.' --output-on-failure
.\MeshEdit\MeshEditView\run_meshedit_scenarios.ps1 -Configuration Debug
```

The CPU tests cover analytic volumes, concave quads, cavities, disconnected solids,
coplanar contacts, fixed rotations, UV interpolation/seams, normals, empty results,
limits, id exhaustion, determinism, atomic failure, preview, commands and Undo/Redo.
`MeshEditView/scenarios/boolean.json` exercises all three operations and empty-result
rendering through the real viewer alongside the existing scenario suite.
