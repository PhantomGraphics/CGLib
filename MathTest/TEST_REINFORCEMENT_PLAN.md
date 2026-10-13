# Math Test Reinforcement Plan

## Objective

Strengthen tests for public math APIs, especially geometric edge cases and
template parity between `float` and `double` variants.

## Completed baseline

Existing coverage includes vectors, matrices, transformations, `Box3d`,
`Sphere3d`, statistics, and Gaussian utilities.

## Remaining priorities

1. Add focused suites for `Cylinder3d`, `Ellipse3d`, `Ellipsoid3d`,
   `Cone3d`, and `Capsule3d`.
2. Cover degenerate geometry, zero-length directions, tangency, points on
   boundaries, negative/zero dimensions, and near-parallel inputs.
3. Verify all aliases and templated algorithms for both scalar precisions.
4. Add algebraic invariants: normalization, inverse round trips, symmetry,
   containment, and distance sign conventions.
5. Replace duplicated literals with named tolerances and shared fixtures.

## Quality rules

- Use `TEST(ClassName, MethodName)`.
- Prefer behavior and invariants over implementation details.
- Use appropriate absolute/relative tolerances and document unusually loose
  values.
- Every fixed defect receives a regression test.

## Completion criteria

All public primitives have nominal, boundary, degenerate, and precision-parity
coverage, and the suite passes deterministically in Debug and Release builds.

## Status (2026-10-13)

Items 1-4 of "Remaining priorities" are done for `Cylinder3d`, `Ellipse3d`,
`Ellipsoid3d`, `Cone3d`, `Capsule3d`, `Triangle3d`, `Rectangle3d`, `Torus3d`,
`Statistics` and the shared invariants (`MathInvariantTest.cpp`), as typed
`float` / `double` tests. Shared tolerances live in `PrimitiveTestSupport.h`
(item 5). Still open: typed parity for the older suites (`Box2d`, `Circle*`,
`Line*`, `Plane3d`, `Ray3d`, `Sphere3d`, `Gaussian`, `SphericalHarmonics`).
See `docs/todo/PLAN_math_refactoring.md` in the superproject.
