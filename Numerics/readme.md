# CGLib/Numerics

## Overview

`CGLib/Numerics` is a static library that wraps [Eigen 3.4.0](ThirdParty/eigen-3.4.0)
to provide eigenvalue decomposition and singular value decomposition (SVD) for small
fixed-size matrices, expressed in Crystal's own math types (`CGLib/Math`).

It is intentionally thin: no new algorithms are implemented here.
The value it adds is a **type-bridging layer** that allows the rest of the Crystal
codebase to call Eigen solvers without taking a direct dependency on Eigen headers.

---

## File map

```
CGLib/Numerics/
  Numerics/
    Converter.h / .cpp   -- Crystal <-> Eigen type conversion utilities
    SVD2d.h   / .cpp     -- Eigenvalue decomposition for 2x2 symmetric matrices
    SVD3d.h   / .cpp     -- Eigenvalue / SVD decomposition for 3x3 matrices
    pch.h     / .cpp     -- Precompiled header
  NumericsTest/
    SVD2dTest.cpp        -- Google Test unit tests for SVD2d
    SVD3dTest.cpp        -- Google Test unit tests for SVD3d
  ThirdParty/
    eigen-3.4.0/         -- Bundled Eigen header-only library
```

---

## Classes

### `Crystal::Numerics::Converter`

A pure-static utility class.  Converts element-by-element between:

| Crystal type    | Eigen type      |
|-----------------|-----------------|
| `Math::Matrix2dd` | `Eigen::Matrix2d` |
| `Math::Matrix3dd` | `Eigen::Matrix3d` |
| `Math::Matrix4dd` | `Eigen::Matrix4d` |
| `Math::Vector2dd` | `Eigen::Vector2d` |
| `Math::Vector3dd` | `Eigen::Vector3d` |
| `Math::Vector4dd` | `Eigen::Vector4d` |

The `dd` suffix denotes double-precision.
Crystal matrices index as `[row][col]`; Eigen matrices index as `(row, col)`.

---

### `Crystal::Numerics::SVD2d`

Eigenvalue decomposition for **2x2 symmetric** matrices.

- **Algorithm**: `Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d>`
- **Input requirement**: symmetric matrix (undefined behaviour for non-symmetric input)
- **Output**: `Result::eigenValues` (ascending order), `Result::eigenVectors` (columns = unit eigenvectors)

```cpp
Math::Matrix2dd M(1.0, 2.0, 2.0, 3.0);
SVD2d svd;
auto r = svd.calculate(M);
// r.eigenValues[0] ~= -0.2361  (smallest eigenvalue)
// r.eigenValues[1] ~=  4.2361  (largest  eigenvalue)
```

Despite the class name, this is **not** a general SVD — it is an eigendecomposition
restricted to symmetric matrices.

---

### `Crystal::Numerics::SVD3d`

Provides two decomposition methods for 3x3 matrices:

#### `calculate(const Matrix3dd&)` — symmetric eigendecomposition

- **Algorithm**: `Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d>`
- **Input requirement**: symmetric matrix
- **Output**: eigenvalues in **ascending order**, eigenvector columns

#### `calculateJacobi(const Matrix3dd&)` — true SVD via Jacobi iterations

- **Algorithm**: `Eigen::JacobiSVD<Eigen::Matrix3d>` with `ComputeFullU`
- **Input requirement**: any real matrix
- **Output**:
  - `Result::eigenValues`  → singular values in **descending order**
  - `Result::eigenVectors` → left singular matrix **U** (orthogonal, 3x3)
  - The right singular matrix V is **not** computed (only `ComputeFullU` is passed)

---

## Result struct (shared by SVD2d and SVD3d)

```cpp
struct Result {
    bool isOk;              // true if the solver converged
    Math::VectorNdd eigenValues;   // eigenvalues or singular values
    Math::MatrixNdd eigenVectors;  // eigenvector matrix or U matrix
};
```

**Important caveat**: In the current implementation of both `SVD2d::calculate()`
and `SVD3d::calculate()`, `isOk` is unconditionally set to `true` after the
failure check, so the flag does not reliably reflect solver failure.
Callers should not rely solely on `isOk`.

---

## Value ordering summary

| Method                  | Value type      | Order      |
|-------------------------|-----------------|------------|
| `SVD2d::calculate()`    | Eigenvalues     | Ascending  |
| `SVD3d::calculate()`    | Eigenvalues     | Ascending  |
| `SVD3d::calculateJacobi()` | Singular values | Descending |

This difference follows Eigen's convention:
`SelfAdjointEigenSolver` sorts ascending; `JacobiSVD` sorts descending.

---

## Dependencies

| Dependency | Version | Location | Usage |
|---|---|---|---|
| Eigen | 3.4.0 | `ThirdParty/eigen-3.4.0/` | Core linear algebra solver |
| CGLib/Math | (internal) | `CGLib/Math/` | Matrix/Vector types used in the public API |
| Google Test | (NuGet) | `packages.config` | Unit testing only |

---

## Notes for AI models

- The `dd` suffix on Crystal math types means **double-double** precision
  (in practice, `double`-precision; the naming convention is from the library).
- `SVD2d` and `SVD3d` reuse the name "SVD" loosely.  `calculate()` is an
  eigendecomposition (symmetric-only), while `calculateJacobi()` is a true SVD.
- `Converter` is the only class that includes Eigen headers.  All other classes
  in the public API use only Crystal math types, keeping the Eigen dependency
  internal.
- All types live in the `Crystal::Numerics` namespace.
