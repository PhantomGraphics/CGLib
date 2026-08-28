# Mathプロジェクト構成（Mermaid）

対象: `CGLib/Math/Math.vcxproj`

```mermaid
flowchart TB
    A["Math.vcxproj\nStatic Library"]

    subgraph H[Header Files]
        H0[framework.h]
        H1[pch.h]
        H2[pi.h]

        subgraph HI[Interfaces]
            HI1[ICurve2d.h]
            HI2[ICurve3d.h]
            HI3[ISurface3d.h]
            HI4[IVolume3d.h]
        end

        subgraph HV[Linear Algebra]
            HV1[Vector2d.h]
            HV2[Vector3d.h]
            HV3[Vector4d.h]
            HV4[Matrix2d.h]
            HV5[Matrix3d.h]
            HV6[Matrix4d.h]
            HV7[Quaternion.h]
        end

        subgraph HG[Geometry]
            HG1[Line2d.h]
            HG2[Line3d.h]
            HG3[Plane3d.h]
            HG4[Ray3d.h]
            HG5[Triangle3d.h]
            HG6[Rectangle3d.h]
            HG7[Circle2d.h]
            HG8[Circle3d.h]
            HG9[Sphere3d.h]
            HG10[Box2d.h]
            HG11[Box3d.h]
            HG12[Cylinder3d.h]
            HG13[Ellipse3d.h]
            HG14[Ellipsoid3d.h]
        end

        subgraph HU[Utility / Math]
            HU1[Gaussian.h]
            HU2[Statistics.h]
            HU3[glm.h]
        end
    end

    subgraph C[Source Files]
        C0[pch.cpp]
        C1[Math.cpp]
        C2[Line2d.cpp]
        C3[Line3d.cpp]
        C4[Plane3d.cpp]
        C5[Triangle3d.cpp]
        C6[Rectangle3d.cpp]
        C7[Circle2d.cpp]
        C8[Circle3d.cpp]
        C9[Sphere3d.cpp]
        C10[Box2d.cpp]
        C11[Box3d.cpp]
        C12[Cylinder3d.cpp]
        C13[Ellipse3d.cpp]
        C14[Ellipsoid3d.cpp]
        C15[Gaussian.cpp]
        C16[Statistics.cpp]
        C17[glm.cpp]
    end

    A --> H
    A --> C
```

## 補足
- この図は `Math.vcxproj` の `ClInclude` / `ClCompile` を基に作成しています。
- AI向けの把握しやすさを優先して、`Interfaces` / `Linear Algebra` / `Geometry` / `Utility` に分類しています。
