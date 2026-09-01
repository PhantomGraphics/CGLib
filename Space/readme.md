# Phantom::Space

A C++ library providing 3D spatial data structures and geometric query algorithms,
intended for use in real-time graphics, simulation, and computational geometry pipelines.

This document is written for AI models and developers reading the codebase without prior context.

---

## Directory Structure

```
CGLib/Space/
├── Space/          # Core spatial algorithms library (static library)
├── SpaceTest/      # Unit tests (Google Test)
└── SpaceView/      # UI visualization layer (dialog + renderer)
```

---

## Namespace

All public symbols live in `Phantom::Space`.
Math types (vectors, boxes, rays, etc.) live in `Phantom::Math`.

---

## Core Spatial Data Structures (`Space/`)

### Octree (`Octree.h/.cpp`)

Hierarchical 3D space partitioning into up to 8 child octants.

- Items must implement `ITreeItem` (provide an AABB via `getBox()`).
- Children are created lazily when items are inserted.
- Supports sphere-radius and AABB range queries.

```cpp
Octree tree(Math::Box3df(...));
tree.add(myItem);
auto hits = tree.findItems(center, radius);
```

### KDTree (`KDTree.h/.cpp`)

Binary space partitioning tree for nearest-neighbor and radius queries over 3D point sets.

- Points are plain `Math::Vector3df` values, held by value in a `Math::Vector3dfVector`
  (`std::vector<Vector3df>`) — no item interface or virtual dispatch involved.
- Build phase is separate from insertion: call `addPoint()` N times then `build()`, or pass a
  whole `Vector3dfVector` to `build(positions)` directly (replaces any previously added points).
- Axes cycle X→Y→Z at each depth; partitioning uses `nth_element` for O(n log n) build.
- The tree copies positions into its own storage; callers don't need to keep anything alive
  after `build()`.
- `findKNearestIndices(query, k)` returns up to k indices sorted by ascending distance, using a
  bounded max-heap of size k with the same splitting-plane pruning as `findNearest`.

```cpp
KDTree tree;
tree.build(myPositions); // or: addPoint() N times, then build()
const Vector3df* nearest = tree.findNearest(query); // pointer into internal storage, or nullptr
std::vector<int> inRange = tree.findWithinRadius(query, radius);
std::vector<int> kNearest = tree.findKNearestIndices(query, 5); // nearest first
```

### BVH — Bounding Volume Hierarchy (`BVH.h/.cpp`)

Binary tree over a set of `BVHObject` instances (each with an integer ID and an AABB).
Designed for overlap/collision queries.

- `queryOverlaps(box)` — returns all objects overlapping a query AABB.
- `findAllPairs()` — returns all (idA, idB) pairs where idA < idB and the two objects overlap.
- `refit()` — updates node AABBs bottom-up after objects have moved (topology unchanged).
- Tree is built once in the constructor; use `refit()` for dynamic scenes with small motion.

```cpp
BVH bvh(objects);
auto overlaps = bvh.queryOverlaps(queryBox);
auto pairs = bvh.findAllPairs();
```

### SpaceHash (`SpaceHash.h/.cpp`)

Uniform-grid spatial hash for O(1) average neighbor lookup.

- Space is divided into cubic cells of side `divideLength`.
- Hash function: `(ix*73856093) XOR (iy*19349663) XOR (iz*83492791)` mod `tableSize`.
- `findNeighborIndices(pos)` searches the 3×3×3 = 27 neighboring cells.
- Best suited for uniformly distributed point clouds.

```cpp
SpaceHash hash(cellSize, tableSize);
hash.add(position);
std::list<int> neighbors = hash.findNeighborIndices(queryPos);
```

### CompactSpaceHash (`CompactSpaceHash.h/.cpp`)

Memory-efficient spatial hash using Z-order (Morton) curve encoding for cell IDs.

- Same interface as `SpaceHash` but uses `ZOrderCurve3d` internally for cell identification.
- Supports removal by index, and coordinate conversion utilities.
- Prefer over `SpaceHash` when spatial locality in memory matters or when cells are sparse.

```cpp
CompactSpaceHash hash(cellSize, tableSize);
hash.add(position);
hash.remove(index);
std::vector<int> neighbors = hash.findNeighborIndices(queryPos);
std::array<int,3> gridIdx = hash.toIndex(position);
unsigned int morton = hash.toZIndex(gridIdx);
```

---

## Z-Order (Morton) Curves

### ZOrderCurve3d (`ZOrderCurve3d.h/.cpp`)

Encodes/decodes 3D unsigned integer grid coordinates to/from a single Morton value
by interleaving the bits of x, y, z:

```
result bits: ... z2 y2 x2 z1 y1 x1 z0 y0 x0
```

- `encode({x,y,z})` → `uint`
- `decode(uint)` → `{x,y,z}`
- `getParent(ltd, rbd)` → parent node index in a linear octree.

### ZOrderCurve2d (`ZOrderCurve2d.h/.cpp`)

2D variant. Interleaves bits of x and y.

---

## Particle Search Utilities

### IndexedParticle (`IndexedParticle.h/.cpp`)

A particle that caches a scalar `gridId` derived from its position and a cell size.
Sorting a container of `IndexedParticle` by `gridId` groups spatially nearby particles,
enabling efficient pair searches without a hash table.

### ZIndexedParticle / ZIndexedSearcher (`ZIndexedSearcher.h/.cpp`)

Similar to `IndexedParticle` but uses Z-order Morton encoding as the sort key.
`ZIndexedSearcher` manages a sorted array of `ZIndexedParticle` and supports
neighbor queries after a one-time `sort()` call.

---

## Geometric Query Algorithms

All calculators are **template classes** parameterized on `T` (typically `float` or `double`).
All methods are **static**. Return values are parametric `t` values along the ray/line,
or a direct scalar distance. An **empty vector** means no intersection.

### IntersectionCalculator (`IntersectionCalculator.h/.cpp`)

3D intersection tests (parametric `t` along ray or line):

| Query | Primitives |
|-------|-----------|
| Ray | Sphere, Plane, Triangle, Rectangle, Box (AABB) |
| Line | Plane, Sphere, Triangle |
| Sphere vs Plane | returns intersection circle description |

### DistanceCalculator (`DistanceCalculator.h/.cpp`)

Distance queries:

| Query | Result |
|-------|--------|
| Point to Triangle | scalar minimum distance |
| Ray vs Sphere | up to 2 parametric `t` values |
| Ray vs Triangle | 0 or 1 parametric `t` (Möller–Trumbore) |
| Ray vs Box (AABB) | up to 2 parametric `t` values (slab method) |

### SignedDistanceCalculator (`SignedDistanceCalculator.h/.cpp`)

Signed distance from a point to a primitive (negative = inside, positive = outside):

| Primitive | Notes |
|-----------|-------|
| Sphere | `dist = \|p - center\| - radius` |
| Plane  | Signed by plane normal direction |

### Intersection2d (`Intersection2d.h/.cpp`)

2D intersection tests. Currently supports:
- Line2d × Circle2d → up to 2 parametric `t` values.

---

## Other Utilities

### LinearOctreeIndex (`LinearOctreeIndex.h/.cpp`)

Encodes an octree node's (level, number) pair as a single `uint`:
`index1d = (8^0 + ... + 8^(level-1)) + number`.
Supports parent traversal and use in `std::set` / `std::map`.

### PolygonSampler (`PolygonSampler.h/.cpp`)

Samples points inside a closed triangle mesh.
Uses ray casting (odd/even crossing rule) to determine inside/outside.
`generateUniformGrid(spacing)` returns all grid points inside the mesh.

---

## Interfaces

| Interface | Used By | Must Provide |
|-----------|---------|-------------|
| `ITreeItem` | Octree | `getBox() → Box3d<float>` |
| `BVHObject` | BVH | `id` (int), `box` (Box3df) — extend or use directly |

`KDTree` takes plain `Vector3df` positions directly (via `addPoint()`/`build()`); it has no item
interface of its own.

---

## Dependencies

```
Phantom::Math (CGLib)
  ├── Vector3d<T>, Box3d<T>, Ray3d<T>, Plane3d<T>
  ├── Triangle3d<T>, Sphere3d<T>, Rectangle3d<T>
  └── Line3d<T>, Line2d<T>, Circle2d<T>

Phantom::Space (this library)
  └── consumed by SpaceView (UI / visualization layer)
```

---

## Design Notes for AI Readers

- **Non-copyable classes**: `Octree`, `KDTree`, `SpaceHash`, `CompactSpaceHash`, `ZIndexedSearcher`
  all inherit from `UnCopyable`. Wrap in `unique_ptr` or `shared_ptr` where needed.
- **Pointer ownership**: `Octree` does **not** own the pointers passed to it; the caller is
  responsible for item lifetimes. `KDTree` instead copies positions by value into its own
  storage, so it owns no external pointers.
- **Build/query separation**: `KDTree` requires an explicit `build()` call after all `addPoint()`
  calls (or use `build(positions)` to add and build in one step). Querying before `build()` is
  undefined behavior.
- **Template instantiation**: `IntersectionCalculator`, `DistanceCalculator`,
  `SignedDistanceCalculator`, `Intersection2d` are header-only templates.
  Explicit instantiations for `float` are defined in the corresponding `.cpp` files.
- **SpaceView** is a separate Visual C++ project (`SpaceView.vcxproj`) and depends on
  `Phantom::UI`, `Phantom::Renderer`, and `Phantom::Scene` frameworks in addition to `Space`.
