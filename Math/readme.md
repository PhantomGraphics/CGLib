# Math Module — File Layout

The module uses the `Phantom::Math` namespace. Its types are thin GLM aliases
defined as `float` / `double` templates. See
[`../docs/module-reference.md`](../docs/module-reference.md) for API details.

```mermaid
flowchart TB
    A["MathCore\n(static library)"]

    subgraph H[Headers]
        subgraph HI[Parametric geometry interfaces]
            HI1[ICurve2d.h]
            HI2[ICurve3d.h]
            HI3[ISurface3d.h]
            HI4[IVolume3d.h]
        end

        subgraph HV[Linear algebra]
            HV1[Vector2d.h / Vector3d.h / Vector4d.h]
            HV2[Matrix2d.h / Matrix3d.h / Matrix4d.h]
            HV3[Quaternion.h]
        end

        subgraph HG[Geometric primitives]
            HG1[Line2d.h / Line3d.h / Ray2d.h / Ray3d.h]
            HG2[Plane3d.h / Triangle3d.h / Rectangle3d.h]
            HG3[Circle2d.h / Circle3d.h / Sphere3d.h]
            HG4[Box2d.h / Box3d.h]
            HG5[Cylinder3d.h / Cone3d.h / Capsule3d.h]
            HG6[Ellipse3d.h / Ellipsoid3d.h]
        end

        subgraph HU[Utilities]
            HU1[Gaussian.h]
            HU2[Statistics.h]
            HU3[glm.h / pi.h]
        end
    end

    A --> H
```

- Each geometric primitive implements its corresponding interface, such as
  `ICurve3d<T>`, and provides `float` and `double` aliases such as `Line3df`
  and `Line3dd`.
- Operations are free functions such as `getLength`, `getDistance`, and
  `areSame`, rather than member functions. Use `glm::dot`, `glm::cross`, and
  `glm::normalize` directly.
