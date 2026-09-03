# Math モジュール — ファイル構成

`Phantom::Math` 名前空間。すべての型は GLM の薄いエイリアスで、`float` / `double` の
テンプレートとして定義される。API 詳細は [`../docs/module-reference.md`](../docs/module-reference.md) を参照。

```mermaid
flowchart TB
    A["MathCore\n(静的ライブラリ)"]

    subgraph H[ヘッダ]
        subgraph HI[パラメトリック幾何インターフェース]
            HI1[ICurve2d.h]
            HI2[ICurve3d.h]
            HI3[ISurface3d.h]
            HI4[IVolume3d.h]
        end

        subgraph HV[線形代数]
            HV1[Vector2d.h / Vector3d.h / Vector4d.h]
            HV2[Matrix2d.h / Matrix3d.h / Matrix4d.h]
            HV3[Quaternion.h]
        end

        subgraph HG[幾何プリミティブ]
            HG1[Line2d.h / Line3d.h / Ray2d.h / Ray3d.h]
            HG2[Plane3d.h / Triangle3d.h / Rectangle3d.h]
            HG3[Circle2d.h / Circle3d.h / Sphere3d.h]
            HG4[Box2d.h / Box3d.h]
            HG5[Cylinder3d.h / Cone3d.h / Capsule3d.h]
            HG6[Ellipse3d.h / Ellipsoid3d.h]
        end

        subgraph HU[ユーティリティ]
            HU1[Gaussian.h]
            HU2[Statistics.h]
            HU3[glm.h / pi.h]
        end
    end

    A --> H
```

- 各幾何プリミティブは対応するインターフェース（`ICurve3d<T>` など）を実装し、
  `float` / `double` 版のエイリアス（`Line3df` / `Line3dd` など）を持つ。
- 演算はメンバー関数ではなくフリー関数（`getLength`, `getDistance`, `areSame` など）。
  `glm::dot` / `glm::cross` / `glm::normalize` はそのまま利用する。
