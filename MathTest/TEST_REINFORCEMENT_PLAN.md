# ドキュメントに基づく Math テスト強化プラン

作成日: 2026-04-20
更新日: 2026-04-20

## 現状サマリー

- テストファイル: 22 個
- テストケース数: 約 80 件（アクティブ）
- テストフレームワーク: Google Test v1.8.1.7
- 未テストクラス: `Cylinder3d`, `Ellipse3d`, `Ray2d`
- コメントアウトされたテスト: なし

---

## 実装済み

### 1. `Box3d` の既存テスト復活と修正

- `TestAdd`
- `TestIsInside` 相当の `contains` テスト
- `TestContains`
- `TestIsSame`

### 2. `Vector2d` / `Vector3d` の演算テスト追加

- `TestNormalize`
- `TestDotProduct`
- `TestAddSubtract`
- `Vector3d` では `TestCrossProduct` を追加

### 3. `Matrix2d` / `Matrix3d` の基本演算テスト追加

- `TestIdentity`
- `TestMultiplication`
- `TestDeterminant`

### 4. `Matrix4d` の変換テスト拡充

- `Scale`
- `TranslationThenRotation`
- `InverseOfTranslation`

### 5. `Sphere3d` の不足テスト追加

- `TestContains`

`TestContains` は実装済み。`getBoundingBox()` は今回は保留。

### 6. `Statistics` / `Gaussian` の境界値テスト追加

- `StatisticsTest.cpp`: `TestSingleValue`
- `GaussianTest.cpp`: `TestSymmetry`, `TestPeak`

---

## 優先度 HIGH — 既存コードの空白を埋める

### 1. `Cylinder3d` — 新規テストファイル作成

`Cylinder3dTest.cpp` を新規作成。

```
テスト案:
- DefaultConstructor        : デフォルト値の確認
- ParameterizedConstructor  : 半径・高さ・中心の設定
- TestGetPosition           : パラメトリック位置 (u, v) のサンプリング
- TestGetNormal             : 側面法線の確認
- TestGetLength             : 高さの取得
```

### 2. `Ellipse3d` — 新規テストファイル作成

`Ellipse3dTest.cpp` を新規作成。

```
テスト案:
- DefaultConstructor        : デフォルト値
- ParameterizedConstructor  : 長軸・短軸の設定
- TestGetPosition_Curve     : ICurve3d としての getPosition(u)
- TestGetPosition_Surface   : ISurface3d としての getPosition(u, v)
- TestGetNormal             : 法線ベクトルの確認
```

### 3. `Line2d` / `Line3d` — 既存ファイルへのケース追加

現在テストなし。

```
Line2dTest.cpp (新規):
- DefaultConstructor
- ParameterizedConstructor
- TestGetPosition           : パラメトリック位置 (u=0, 0.5, 1.0)
- TestGetLength             : 線分の長さ

Line3dTest.cpp (新規):
- 同上（3D 版）
```

### 4. `Circle3d` — `getPosition` テストの追加

`Circle3dTest.cpp` に追加:

```
- TestGetPosition           : u=0, 0.25, 0.5, 0.75 での位置を検証
                              (Circle2d と対称的なカバレッジ)
```

---

## 優先度 MEDIUM — 演算・変換テストの拡充

### 5. `Vector2d` / `Vector3d` — 演算テストの追加

現在はノルム・距離のみ。

```
Vector2dTest.cpp に追加:
- TestNormalize             : 正規化後の長さが 1.0
- TestDotProduct            : 直交ベクトルの内積 = 0
- TestAddSubtract           : 加算・減算

Vector3dTest.cpp に追加:
- TestNormalize
- TestDotProduct
- TestCrossProduct          : 標準基底ベクトルの外積 (e.g., X×Y = Z)
- TestAddSubtract
```

### 6. `Matrix2d` / `Matrix3d` — 基本演算テスト

現在は回転行列のみ。

```
Matrix2dTest.cpp に追加:
- TestIdentity
- TestMultiplication        : 回転×逆回転 = 単位行列
- TestDeterminant           : 回転行列の行列式 = 1

Matrix3dTest.cpp に追加:
- TestIdentity
- TestMultiplication
- TestDeterminant
```

### 7. `Matrix4d` — 変換テストの拡充

```
Matrix4dTest.cpp に追加:
- Scale                     : スケール変換行列の検証
- TranslationThenRotation   : 平行移動→回転の合成
- InverseOfTranslation      : 平行移動の逆行列
```

### 8. `Sphere3d` — 不足テストの追加

```
Sphere3dTest.cpp に追加:
- TestContains              : 内部点・外部点・境界点の判定
```

### 9. `Plane3d` — `isSame` テスト

```
Plane3dTest.cpp に追加:
- TestIsSame                : 同一平面と異なる平面の比較
```

`Plane3d` の `calculateD` / `isSame` は実装待ちのため、現在は保留。

---

## 優先度 LOW — 品質向上・型バリアント

### 10. double 精度バリアントのカバレッジ

現在 `Circle2d` / `Circle3d` 程度しか double テストがない。

対象: `Vector2d<double>`, `Vector3d<double>`, `Box3d<double>`, `Sphere3d<double>`

各クラスに `DoubleType` テストを追加し、float/double 両方の精度で動作することを確認する。

### 11. `Ray2d` の状況確認

ヘッダのみ存在、実装・テストともに無し。  
→ 実装する予定があるか確認。あれば `Ray2dTest.cpp` を作成する。

### 12. 統計・Gaussian の境界値テスト

```
StatisticsTest.cpp に追加:
- TestSingleValue           : 要素 1 つの場合の分散 = 0
- TestEmptyOrZero           : 空データのガード確認

GaussianTest.cpp に追加:
- TestSymmetry              : ガウス関数の対称性 (f(mu+x) == f(mu-x))
- TestPeak                  : mu での値が最大
```

`StatisticsTest.cpp` / `GaussianTest.cpp` の追加分は実装済み。空データのガードは未実装のため保留。

---

## テスト追加の実装方針

1. **既存ファイルを修正する場合**: 対応する `*Test.cpp` ファイルに `TEST()` ブロックを追記
2. **新規ファイルを作成する場合**: 同ディレクトリに `<ClassName>Test.cpp` として作成し、`MathTest.vcxproj` に追加
3. **数値比較**: 浮動小数点には `EXPECT_NEAR(a, b, 1e-5)` を使用（`EXPECT_EQ` は使わない）
4. **境界値**: 0, ±1, π/2, π を基本テスト値とする

---

## 優先度別作業リスト

| # | タスク | 優先度 | 工数目安 |
|---|---|---|---|
| 1 | Box3d コメントアウトテストの復活 | HIGH | 1h |
| 2 | Plane3d コメントアウトテストの復活 | HIGH | 0.5h |
| 3 | Cylinder3dTest.cpp 新規作成 | HIGH | 2h |
| 4 | Ellipse3dTest.cpp 新規作成 | HIGH | 2h |
| 5 | Line2dTest.cpp / Line3dTest.cpp 新規作成 | HIGH | 1.5h |
| 6 | Circle3d getPosition テスト追加 | HIGH | 0.5h |
| 7 | Vector2d/3d 演算テスト追加 | MEDIUM | 1h |
| 8 | Matrix2d/3d 演算テスト追加 | MEDIUM | 1h |
| 9 | Matrix4d Scale/合成テスト追加 | MEDIUM | 1h |
| 10 | Sphere3d BoundingBox/Contains テスト追加 | MEDIUM | 1h |
| 11 | Plane3d isSame テスト追加 | MEDIUM | 0.5h |
| 12 | double 型バリアントの拡充 | LOW | 2h |
| 13 | Ray2d の方針確認・対応 | LOW | 0.5h〜 |
| 14 | 統計・Gaussian 境界値テスト追加 | LOW | 1h |

**合計見積もり: HIGH 約 7.5h / MEDIUM 約 4.5h / LOW 約 3.5h**
