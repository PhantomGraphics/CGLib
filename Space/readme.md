# Phantom::Space

A C++ library providing 3D spatial data structures and geometric query algorithms,
intended for use in real-time graphics, simulation, and computational geometry pipelines.

This document is written for AI models and developers reading the codebase without prior context.
The exact per-API contracts (ordering, boundaries, lifetimes, what degenerate input returns) are in
`docs/spec/space_api_contracts.md` at the repository root; this file is the overview.

---

## Directory Structure

```
CGLib/Space/
├── Space/          # Core spatial algorithms library (SpaceCore, static library)
├── SpaceTest/      # Unit tests (Google Test)
└── SpaceView/      # Standalone ImGui + Vulkan viewer, plus SpaceDemoCore (its CPU side)
```

Build: CMake only (`SpaceCore`, `SpaceDemoCore`, `SpaceTest`, `SpaceView` targets). Public headers are included as
`CGLib/Space/Core/<Header>.h` (or relative to this directory inside the module); they include Math headers as
`CGLib/Math/...`, so the include root must be the directory that contains `CGLib/`.

---

## Namespace

All public symbols live in `Phantom::Space`. Math types (vectors, boxes, rays, etc.) live in `Phantom::Math`.
`SpaceView` code lives in `VKSpace`.

---

## General conventions

- **No exceptions.** Invalid input is rejected with a `bool` return or an empty result, never thrown.
- **Input validation.** NaN/Inf positions, cell sizes that are not finite and positive, and cell coordinates beyond
  ±2^30 are rejected (`SpaceValidation.h`). A rejected position still consumes its insertion index, so the indices of
  the other points do not shift. Geometric queries return finite values or an empty vector.
- **Ownership.** `KDTree`, `SpaceHash`, `CompactSpaceHash` and `IndexedSortBasedSearcher` copy positions by value.
  `Octree` and `BVH` hold **non-owning pointers** to caller-owned objects.
- **Result buffers.** Queries that return a container also have an overload taking a caller-owned `std::vector&`
  (cleared first, capacity reused) and, for the hot ones, a `forEach...` callback form. They give the same elements in
  the same order as the value-returning form. Pass one buffer per thread; const queries may run concurrently.
- **Non-copyable classes** inherit `UnCopyable`; wrap in `unique_ptr` where needed.

---

## Core Spatial Data Structures (`Space/`)

### Octree (`Octree.h/.cpp`)

Coarse 3D partition into 8 child octants, with sphere and AABB range queries.

- Items implement `ITreeItem` (`getBox()` returns an AABB). The tree stores the **pointers**; items must outlive it.
- `add()` returns `false` (and stores nothing) for a null item, a non-finite box, or a box outside the root.
- Subdivision happens only when an item straddles the root boundary, and only **one level deep**: this is not an
  adaptive octree. Items that fit the root are simply kept in a list there.
- A box's node is chosen at `add()` time; moving an item afterwards needs a new `Octree`. There is no `remove()`.

```cpp
Octree tree(Math::Box3df(...));
tree.add(&myItem);
auto hits = tree.findItems(center, radius);   // std::list<ITreeItem*>
```

### KDTree (`KDTree.h/.cpp`)

Binary space partitioning tree for nearest-neighbor and radius queries over 3D point sets.

- Points are plain `Math::Vector3df` values held by value; the tree owns its copy.
- `addPoint()` only **stages** a point; `build()` (or `build(positions)`) makes the tree. Queries see the points present
  at the last `build()`; points staged afterwards are invisible until the next build (`isDirty()` reports that).
  Querying an unbuilt tree returns nothing.
- Non-finite points keep their index but are left out of the tree (`rejectedCount()`).
- `findWithinRadius` is inclusive (`d² <= r²`); `findKNearestIndices(q, k)` returns nearest first. Ties between equidistant
  points are broken by traversal order, so compare distances rather than ids.
- Buffer forms: `findWithinRadius(q, r, out)`, `forEachWithinRadius(q, r, visit)`,
  `findKNearestIndices(q, k, out, KNearestScratch&)`.

```cpp
KDTree tree;
tree.build(myPositions);                                  // or addPoint() N times, then build()
const Vector3df* nearest = tree.findNearest(query);       // into internal storage; nullptr if empty
std::vector<int> inRange = tree.findWithinRadius(query, radius);
```

### BVH — Bounding Volume Hierarchy (`BVH.h/.cpp`)

Binary tree over `BVHObject` instances (an integer id and an AABB), for overlap, ray-candidate and pair queries.

- `queryOverlaps(box)`, `queryRay(origin, direction, tMin, tMax)` (candidates only; the exact hit test is the caller's),
  `findAllPairs()` — `(idA, idB)` with `idA < idB`, so **ids should be unique** (`hasUniqueIds()`).
- Objects are not owned or copied; null pointers are skipped. After moving objects call `refit()` (cheap, keeps the
  topology) or `rebuild()` (re-splits); `refitDegradation()` says how loose the boxes became.
- Allocation-free forms: `queryOverlaps/queryRay(..., out)`, `findAllPairs(out, stack)`, `forEachOverlap`,
  `forEachRayCandidate`.

```cpp
BVH bvh(objects);                      // std::vector<BVHObject*>
auto overlaps = bvh.queryOverlaps(queryBox);
auto pairs = bvh.findAllPairs();
```

### SpaceHash (`SpaceHash.h/.cpp`)

Uniform-grid spatial hash. Cells have side `divideLength`; `findNeighborIndices(pos)` scans the 27 surrounding cells and
returns the points **strictly closer than `divideLength`** (an exact radius result, sorted, including a stored point at
`pos`). Best for roughly uniform point clouds.

```cpp
SpaceHash hash(cellSize, tableSize);     // tableSize < 1 is raised to 1
hash.add(position);                      // false if the position/cell size was rejected
std::list<int> neighbors = hash.findNeighborIndices(queryPos);
```

### CompactSpaceHash (`CompactSpaceHash.h/.cpp`)

Same radius semantics as `SpaceHash`, with cells identified by their grid index (a Morton code is stored per cell).

- `findNeighborIndices(int index)` excludes the particle itself; the position overload includes a stored point there.
- `find(...)` returns a whole cell without a distance filter; `remove(index)` takes a particle out of its cell.
- Buffer/callback forms: `findNeighborIndices(..., out)`, `forEachNeighbor(..., visit)`. `clear()` keeps the buckets.

### CSRNeighborList / IndexedSortBasedSearcher (`CSRNeighborList.h`, `IndexedSortBasedSearcher.h`)

The SPH neighbor search used by Physics. `CSRNeighborList::build(positions, radius)` gives symmetric rows without the
particle itself, strictly closer than `radius`; row order is part of the contract (solver determinism). Rebuilding every
step reuses an internal scratch workspace (`NeighborSearchWorkspace`), so steady-state rebuilds do not allocate.
`tryBuild(...)` variants report invalid input; `at(i)` is the bounds-checked row accessor.
`IndexedParticle::gridId` is 64-bit: `iz * 2^20 + iy * 2^10 + ix`, linear in the cell index.

---

## Z-Order (Morton) Curves

### ZOrderCurve3d (`ZOrderCurve3d.h/.cpp`)

Interleaves the bits of x, y, z (10 bits per axis, so axis values `0..1023`; `isEncodable()` checks that):
`result bits: ... z2 y2 x2 z1 y1 x1 z0 y0 x0`. `encode`, `decode`, `getParent(ltd, rbd)`. `ZOrderCurve2d` is the 2D variant.

### LinearOctreeIndex (`LinearOctreeIndex.h/.cpp`)

Encodes an octree node's `(level, number)` as `index1d = (8^0 + ... + 8^(level-1)) + number`; supports parent traversal.

### ZIndexedSearcher (`ZIndexedSearcher.h/.cpp`)

Neighbor search over Morton-sorted particles: `add()` all, `sort()` once, then `findNeighbors(pos)` (strictly closer than
the search radius, 27 cells). Positions must be at or above `minPosition` and within 1022 radii of it.

---

## Geometric Query Algorithms

All calculators are **template classes** over `T` (`float` and `double` are instantiated in the `.cpp` files) with
**static** methods. A result is a list of parametric values or a scalar distance; an **empty vector means no result**.
Non-finite input, zero-length directions/segments and zero-area triangles/rectangles always give an empty result.

### IntersectionCalculator

| Query | Result |
|---|---|
| Ray vs Sphere / Box | the `DistanceCalculator` parameters that land on the surface (infinite line: `t < 0` is reported) |
| Ray vs Triangle / Rectangle | `t >= 0` along the ray |
| Ray vs Plane | the **origin's signed distance** if the origin is in front of the plane (a compatibility quirk, not a ray parameter) |
| Line (segment) vs Plane | `u` in `[0,1]`, **front-to-back crossings only** |
| Line vs Sphere / Triangle | `u` in `(0,1]` |
| Sphere vs Plane | the centre's signed distance if `|d| <= r + tolerance` |

### DistanceCalculator

| Query | Result |
|---|---|
| Point to Triangle | scalar distance (0 within `tolerance`; finite for degenerate triangles) |
| Ray vs Sphere | up to 2 `t` values; direction may be non-unit (`t` is in direction lengths) |
| Ray vs Triangle | 0 or 1 `t >= 0` (Möller–Trumbore, absolute `det` tolerance) |
| Ray vs Box | `{tmin, tmax}` of the slab test; axis-parallel rays handled explicitly |
| `closestSegments` | closest points of two segments (capsule contacts) |

### SignedDistanceCalculator

Sphere: `|p - center| - radius` (negative inside). Plane: `dot(p - origin, normal)` — the normal is **not normalized**,
so pass a unit normal for a metric distance.

### Intersection2d

Line2d × Circle2d → up to 2 parametric `t` values; a zero-length segment gives an empty result.

### PolygonSampler

Samples points inside a closed triangle mesh (ray casting, odd/even rule); `generateUniformGrid(spacing)`.

---

## Interfaces

| Interface | Used By | Must Provide |
|---|---|---|
| `ITreeItem` | Octree | `getBox() → Box3d<float>` (non-const) |
| `BVHObject` | BVH | `id` (int), `box` (Box3df) — extend or use directly |

`KDTree`, `SpaceHash` and `CompactSpaceHash` take plain `Vector3df` positions.

---

## SpaceView and SpaceDemoCore

`SpaceView` is the standalone ImGui + Vulkan viewer (`VKSpace` namespace). Its CPU side — synthetic samples, one algorithm
run, the wireframe/points written into a `SpaceResult` — is `SpaceDemos.h/.cpp` in the `SpaceDemoCore` library (no ImGui,
no Vulkan), verified without a window by `SpaceDemoTest`. The `*Panel` classes only edit parameters and call `run()`; the
`SetAlgorithm`/`Run`/`SetParam` scenario commands go through the same `panel->run()`.

---

## Dependencies

```
Phantom::Math (CGLib)  — Vector3d, Box3d, Ray3d, Plane3d, Triangle3d, Sphere3d, Rectangle3d, Line3d, Line2d, Circle2d
Phantom::Space (SpaceCore) → MathCore only
SpaceDemoCore → SpaceCore, glm headers
SpaceView → SpaceDemoCore, VkAppBase, VkRenderer, UIWidgets, Vulkan, GLFW
```
