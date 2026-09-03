# SpaceTest Improvement Plan

## Objective

Make spatial-index and query tests complete, deterministic, and capable of
detecting missing, duplicated, or incorrectly ordered results.

## Priorities

1. Add first-class coverage for every untested structure and Morton-curve
   operation.
2. Verify exact neighbor sets for `Octree`, `KDTree`, `BVH`, `SpaceHash`, and
   `CompactSpaceHash` rather than checking counts alone.
3. Cover empty trees, duplicate positions, boundary points, large coordinates,
   degenerate bounds, and repeated rebuilds.
4. Test ray/intersection and closest-point queries with hits, misses, tangency,
   and origin-inside cases.
5. Verify deterministic output when the API promises ordering; otherwise
   compare normalized sets.
6. Add randomized differential tests against a brute-force implementation.

## Phases

- **P0:** missing classes and crash-prone edge cases.
- **P1:** neighbor correctness and update/rebuild behavior.
- **P2:** stronger assertions and shared fixtures.
- **P3:** randomized, performance-smoke, and long-run tests.

## Completion criteria

Every public query has positive and negative cases, all returned identities are
validated, and randomized checks use fixed reproducible seeds.
