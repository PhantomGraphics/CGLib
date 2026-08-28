# SpaceTest 改善計画

作成日: 2026-04-17  
対象: `CGLib/Space/SpaceTest`  
調査者: Claude Code

---

## 1. 現状サマリー

| 項目 | 値 |
|------|-----|
| テストファイル数 | 16 |
| テストケース総数 | 92 |
| 推定カバレッジ | 約 45% |
| コメントアウトされたテスト | 多数（KDTree, Octree, IntersectionCalculator） |
| 完全に未テストのメソッド | `BVH::queryRay()`, `BVH::refit()`, `KDTree::findWithinRadius()` など |

---

## 2. クラス別カバレッジ評価

| クラス | 推定カバレッジ | 評価 | 優先度 |
|--------|:---:|---|:---:|
| `BVH` | ~40% | `queryRay` / `refit` が未テスト | P0 |
| `KDTree` | ~35% | ほとんどのテストがコメントアウト | P0 |
| `SpaceHash` | ~20% | 挿入のみ検証、近傍取得未検証 | P1 |
| `CompactSpaceHash` | ~25% | 主要メソッドが未テスト | P1 |
| `PolygonSampler` | ~10% | ダミーテスト1件のみ | P0 |
| `SignedDistanceCalculator` | ~40% | 境界値・Box 未検証 | P1 |
| `IndexedSortBasedSearcher` | ~30% | テスト1件、正当性未検証 | P1 |
| `DistanceCalculator` | ~50% | Ray-Sphere のみ、他プリミティブ不足 | P2 |
| `IntersectionCalculator` | ~65% | Rectangle3d・退化ケース未テスト | P2 |
| `Octree` | ~60% | Box 検索ケースがコメントアウト | P2 |
| `ZOrderCurve2d` | ~80% | `getParent` テストが少ない | P3 |
| `ZOrderCurve3d` | ~80% | 同上 | P3 |
| `LinearOctreeIndex` | ~85% | 概ね良好 | P3 |
| `IndexedParticle` | ~95% | 良好 | ― |

---

## 3. 問題の分類

### 3.1 Critical — 機能が完全に未テスト

#### BVH::queryRay() / BVH::refit()

```
ファイル: BVHTest.cpp
```

- `queryRay()` はコアメソッドだが一切テストなし
- `refit()` は動的シナリオ（物体の移動後に AABB を更新）で必須だが未テスト
- アサーションが件数チェックのみで返却オブジェクトの同一性未検証

**追加すべきテスト:**
- [x] `QueryRay_Hit` — 当たるレイ
- [x] `QueryRay_Miss` — 当たらないレイ
- [x] `QueryRay_Refit_AfterMove` — 移動後に `refit()` → 再クエリ
- [x] `QueryOverlap_VerifyIdentity` — 返却オブジェクトの ID 検証（現状は件数のみ）

---

#### KDTree::findWithinRadius() / findNearest()

```
ファイル: KDTreeTest.cpp（行 49–88 がコメントアウト）
```

- `NearestAndRadius` テスト全体がコメントアウト
- `DuplicatePoints` テストもコメントアウト
- `findWithinRadius()` は実装済みだが検証ゼロ

**追加すべきテスト:**
- [x] `FindWithinRadius_Basic` — 半径内の点を正しく返す
- [x] `FindWithinRadius_Empty` — 空ツリーで安全に返る
- [x] `FindNearest_DuplicatePoints` — 重複点での近傍探索
- [x] `FindNearest_Collinear` — 縮退分布（全点が同一直線）

---

#### PolygonSampler — 検証なし

```
ファイル: PolygonSamplerTest.cpp
```

- ハードコードされた立方体へのサンプリングのみ
- 生成点がメッシュ内部に実際に存在するか検証していない

**追加すべきテスト:**
- [x] `Sample_InsideMesh` — 生成点が AABB 内に収まるか検証
- [ ] `Sample_ConcaveMesh` — 凹メッシュへの対応
- [x] `Sample_SingleTriangle` — 縮退形状
- [x] `Sample_ZeroVolumeMesh` — 面積ゼロへの安全性

---

### 3.2 High — 近傍取得の正当性が未検証

#### SpaceHash / CompactSpaceHash

```
ファイル: SpaceHashTest.cpp, CompactSpaceHashTest.cpp
```

- 挿入後の `getNeighbors()` が**件数しか検証していない**
- 実際に返却されるインデックスが正しい近傍かどうか未確認
- 境界セルまたぎ（隣接セルへの検索）が未テスト

**追加すべきテスト:**
- [x] `GetNeighbors_VerifyIndices` — 返却インデックスが正しい粒子を指すか
- [x] `GetNeighbors_AcrossCellBoundary` — セル境界をまたぐ近傍
- [x] `Insert_Remove_Consistency` — 削除後に近傍が消えるか
- [ ] `HashCollision_Correctness` — ハッシュ衝突時の正当性

---

#### SignedDistanceCalculator

```
ファイル: SignedDistanceCalculatorTest.cpp
```

- テストが 2 件のみ（球体・平面の基本ケース）
- 点が表面上にある境界値（距離 ≒ 0）が未テスト
- Box / メッシュへの署名付き距離が未テスト

**追加すべきテスト:**
- [x] `SignedDistance_OnSurface` — 距離ゼロの境界値
- [x] `SignedDistance_InsideSphere` — 内部点（負値）
- [ ] `SignedDistance_Box` — Box プリミティブ

---

### 3.3 Medium — 既存テストの品質向上

#### Octree — コメントアウトの解消

```
ファイル: OctreeTest.cpp（行 144–146, 218–251）
```

- `containsPtr` ヘルパー関数の欠如により複数アサーションがコメントアウト
- Box 検索の多数のケースが無効化されたまま

**対応:**
- [ ] `containsPtr` ヘルパー関数を `TestHelpers.h` に実装
- [ ] コメントアウトされたアサーションを復活させる
- [ ] `FindWithBox_TouchingBoundary` — 境界接触ケース

---

#### IntersectionCalculator — 未テストのバリアント

```
ファイル: IntersectionCalculatorTest.cpp（行 54–58）
```

- `Rectangle3d` との交差テストが欠落
- 退化ケース（平行レイ、かすりレイ）が未テスト

**追加すべきテスト:**
- [ ] `Ray_Rectangle3d_Hit`
- [ ] `Ray_Rectangle3d_Miss`
- [ ] `Ray_ParallelToPlane`
- [ ] `Ray_GrazingIntersection`

---

### 3.4 Low — インフラ・品質改善

- テスト共通ヘルパー（`containsPtr` 等）が各ファイルに点在
- フィクスチャクラスが存在せず、設定コードが重複
- パラメータ化テスト（`INSTANTIATE_TEST_SUITE_P`）が未活用
- 大規模ストレステスト（1000+ オブジェクト）が全クラスで欠如
- NaN / ±Inf の入力に対する安全性テストが存在しない

---

## 4. 実施計画（フェーズ別）

### Phase 1（P0 — Critical 対応）

| タスク | 対象ファイル | 追加テスト数 (目安) |
|--------|------------|:---:|
| BVH::queryRay() テスト追加 | BVHTest.cpp | +5 |
| BVH::refit() テスト追加 | BVHTest.cpp | +3 |
| KDTree コメントアウト解除 + 追加 | KDTreeTest.cpp | +8 |
| PolygonSampler 正当性テスト | PolygonSamplerTest.cpp | +5 |

**Phase 1 完了目標カバレッジ:** BVH ~70%, KDTree ~70%, PolygonSampler ~50%

**実装反映（2026-04-17）:**
- [x] `BVHTest.cpp` に `QueryRay_Hit` / `QueryRay_Miss` / `QueryRay_Refit_AfterMove` を追加
- [x] `BVHTest.cpp` の `QueryOverlapsSimple` で返却 ID 検証を追加
- [x] `KDTreeTest.cpp` のコメントアウト済みテストを現行 API へ移植し復活
- [x] `KDTreeTest.cpp` に `FindWithinRadius_Empty` / `FindNearest_Collinear` を追加
- [x] `PolygonSamplerTest.cpp` の `Dummy` を置換し、`Sample_InsideMesh` / `Sample_SingleTriangle` / `Sample_ZeroVolumeMesh` を追加
- [ ] `Sample_ConcaveMesh` は未着手（次回対応）

---

### Phase 2（P1 — High 対応）

| タスク | 対象ファイル | 追加テスト数 (目安) |
|--------|------------|:---:|
| `TestHelpers.h` 作成（containsPtr 等） | 新規ファイル | — |
| SpaceHash 近傍正当性テスト | SpaceHashTest.cpp | +6 |
| CompactSpaceHash 拡充 | CompactSpaceHashTest.cpp | +6 |
| SignedDistanceCalculator 拡充 | SignedDistanceCalculatorTest.cpp | +5 |
| IndexedSortBasedSearcher 拡充 | IndexedSortBasedSearcherTest.cpp | +4 |
| Octree コメントアウト解除 | OctreeTest.cpp | +5 |

**実装反映（2026-04-17 追記）:**
- [x] `SpaceHashTest.cpp` に `GetNeighbors_VerifyIndices` / `GetNeighbors_AcrossCellBoundary` を追加
- [x] `CompactSpaceHashTest.cpp` に `GetNeighbors_VerifyIndices` / `GetNeighbors_AcrossCellBoundary` を追加
- [x] `CompactSpaceHashTest.cpp` の `TestRemove` を維持し、削除後の近傍消失を明示検証
- [x] `SignedDistanceCalculatorTest.cpp` に境界値（球面上 / 平面上で 0）検証を追加
- [ ] `HashCollision_Correctness` / `SignedDistance_Box` / `IndexedSortBasedSearcher` / `Octree` は未着手（次回対応）

---

### Phase 3（P2 — Medium 対応）

| タスク | 対象ファイル | 追加テスト数 (目安) |
|--------|------------|:---:|
| IntersectionCalculator バリアント追加 | IntersectionCalculatorTest.cpp | +5 |
| DistanceCalculator プリミティブ追加 | DistanceCalculatorTest.cpp | +4 |
| 全クラスにストレステスト（1000+ 件） | 各テストファイル | +16 |

---

### Phase 4（P3 — 品質向上）

- パラメータ化テストへの移行（`ZOrderCurve` 系が候補）
- NaN / Inf / ゼロベクトル の入力安全性テスト
- パフォーマンスベースライン計測の導入
- コードカバレッジレポート設定

---

## 5. 推奨ヘルパー実装

`SpaceTest/TestHelpers.h` として新規作成を推奨:

```cpp
#pragma once
#include <vector>
#include <algorithm>

// ポインタが vector に含まれるか確認
template<typename T>
bool containsPtr(const std::vector<T*>& vec, T* ptr) {
    return std::find(vec.begin(), vec.end(), ptr) != vec.end();
}

// インデックスが vector に含まれるか確認
inline bool containsIndex(const std::vector<int>& vec, int idx) {
    return std::find(vec.begin(), vec.end(), idx) != vec.end();
}

// 浮動小数点の近似比較
inline bool nearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::abs(a - b) < eps;
}
```

---

## 6. 目標カバレッジ

| フェーズ完了時 | 推定カバレッジ |
|--------------|:---:|
| 現状 | ~45% |
| Phase 1 完了 | ~58% |
| Phase 2 完了 | ~70% |
| Phase 3 完了 | ~80% |
| Phase 4 完了 | ~85%+ |

---

## 7. 参考: 調査時に発見した既存の問題コード

| ファイル | 行 | 内容 |
|---------|-----|------|
| `BVHTest.cpp` | 34–36 | 返却 ID の検証がコメントアウト |
| `KDTreeTest.cpp` | 49–88 | `NearestAndRadius` テスト全体がコメントアウト |
| `KDTreeTest.cpp` | 27 | 未使用変数 `Vector3df out` |
| `OctreeTest.cpp` | 144–146 | `containsPtr` 未定義でアサーション無効化 |
| `OctreeTest.cpp` | 218–251 | Box 検索テスト複数がコメントアウト |
| `IntersectionCalculatorTest.cpp` | 54–58 | Ray-triangle 否定テストがコメントアウト |
| `SpaceHashTest.cpp` | 全体 | 近傍インデックスの正当性が未検証 |

※ `BVHTest.cpp` / `KDTreeTest.cpp` / `PolygonSamplerTest.cpp` / `SpaceHashTest.cpp` / `CompactSpaceHashTest.cpp` / `SignedDistanceCalculatorTest.cpp` の上記項目は 2026-04-17 時点で一部対応済み。
