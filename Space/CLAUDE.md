# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

3D 空間分割・幾何クエリを提供するモジュール群（Octree/KDTree/BVH/SpaceHash/Z-Order 曲線・交差判定・距離計算等）。
`Space`（コアライブラリ）、`SpaceTest`（GoogleTest）、`SpaceView`（スタンドアロン ImGui + Vulkan ビューア）の 3 プロジェクトで構成される。単独の `.sln` は持たず、すべて上位の `Phantom2026.sln` でビルドする。

親リポジトリの CLAUDE.md（`../../CLAUDE.md`）にビルド方法・全体アーキテクチャ・命名規則が記載されているのであわせて参照すること。

**アルゴリズム・データ構造の詳細（各クラスの API・計算量・使用例）は `readme.md`（本ディレクトリ）にまとまっている。AI 向けに書かれた詳細ドキュメントなので、実装を読む前にまずそちらを参照すること。**

## Build

CMake が唯一のビルド手段（2026-08-19、`.vcxproj` は全削除済み。詳細は
内部設計メモ Phase 5、親リポジトリ `../../CLAUDE.md` の Build 節を参照）。

```powershell
# リポジトリルートから、Space単体を設定・ビルド
cmake -S CGLib/Space -B CGLib/Space/build_windows -DCMAKE_BUILD_TYPE=Debug
cmake --build CGLib/Space/build_windows

# または、ルートの CMakePresets.json 経由でリポジトリ全体を一括ビルド
cmake --preset windows-debug
cmake --build --preset windows-debug
```

依存関係: `SpaceCore` は `MathCore` のみに依存。`SpaceView` はさらに `VulkanGraphicsCore`・`UIWidgetsCore`・
`VkAppBaseCore`・`VkRendererCore` に依存する。

## Tests

```powershell
.\build\windows-debug\CGLib\Space\SpaceTest.exe

# フィルター例
.\build\windows-debug\CGLib\Space\SpaceTest.exe --gtest_filter=OctreeTest.*
.\build\windows-debug\CGLib\Space\SpaceTest.exe --gtest_filter=KDTreeTest.Insert
```

`SpaceTest/TEST_IMPROVEMENT_PLAN.md` に既知のカバレッジ改善候補がメモされている。

### シナリオテスト（SpaceView）

```powershell
.\CGLib\Space\SpaceView\run_space_scenarios.ps1 -Configuration Debug

# 単一シナリオ
.\build\windows-debug\CGLib\Space\SpaceView.exe --run-scenario CGLib\Space\SpaceView\scenarios\octree.json
```

シナリオ JSON は `CGLib\Space\SpaceView\scenarios\` にあり、`camera.json`・`octree.json`・`kdtree.json`・`spacehash.json`・`compact_spacehash.json`・`signed_distance.json` の 6 本。実装方法・JSON フォーマットはシナリオテストガイドを参照。

## Architecture

### Phantom::Space（`Space/`）— コアライブラリ

`Phantom::Space` 名前空間。空間分割構造（`Octree`/`KDTree`/`BVH`/`SpaceHash`/`CompactSpaceHash`/`ZIndexedSearcher`）と、テンプレートの静的メソッド群として実装された幾何クエリ（`IntersectionCalculator`/`DistanceCalculator`/`SignedDistanceCalculator`/`Intersection2d`）を提供する。全クラスの API・計算量・サンプルコードは `readme.md` を参照。

### SpaceView（`SpaceView/`）— スタンドアロン ImGui + Vulkan アプリ

`VkSpaceApp : VkAppBase` 直下。**`VKSpace` 名前空間**（`Phantom::Space` ではない点に注意）。

- `World`（`World.h`）— OpenGL に依存しない軽量な状態保持クラス。パネルが `SpaceResult`（ワイヤーフレーム線・サンプル点のバッファ）へ書き込み、`markDirty()` でレンダラーへ再アップロードを伝える。
- `SpaceMenuPanel` — 5 アルゴリズム（SpaceHash/CompactSpaceHash/KDTree/Octree/SignedDistance）を切り替えるメニュー。各アルゴリズムのパネル（`SpaceHashPanel`/`CompactSpaceHashPanel`/`KDTreePanel`/`OctreePanel`/`SignedDistancePanel`）は `IAlgorithmView`（`getName()`/`onImGui()`/`run()`/`setParam()`）を実装する。
- `VkSpaceCommandDispatcher : IScenarioDispatcher` — コマンド文字列ディスパッチャ。`SetAlgorithm:<name>`／`Run`／`SetParam:<name>:<value>`／`GetLineCount`／`GetPointCount`／`GetPointPositionMin`／`GetPointPositionMax`／`GetDirty`／`ResetCamera`／`GetCameraDistance`／`Scroll:<dy>` を処理する（シナリオテストガイド参照）。
- `VkSpaceRenderer` — `World` の `SpaceResult` を線・点として描画。カメラリセット (`resetCamera()`)・スクロールズーム (`handleScroll()`) を持つ。

**新しいアルゴリズムを追加する場合:** `IAlgorithmView` を実装したパネルを追加し、`SpaceMenuPanel::AlgoType` に列挙値を追加、`viewOf()`/`algoName()`/`setActive()` に配線する。

## Key Conventions

- **例外禁止**: このリポジトリ全体の規約に従い、`throw`/`try`/`catch` は使わない。エラーは `bool`/`std::optional` で返す。
- **非所有ポインタ**: `Octree`/`KDTree` は挿入されたアイテムの所有権を持たない。呼び出し元が寿命管理する。
- **Non-copyable**: `Octree`/`KDTree`/`SpaceHash`/`CompactSpaceHash`/`ZIndexedSearcher` は `UnCopyable` を継承。`unique_ptr`/`shared_ptr` で保持する。
- **KDTree の build/query 分離**: `addPoint()` を必要な回数呼んだ後に必ず `build()` を呼ぶこと。`build()` 前のクエリは未定義動作。
- **テンプレート計算クラス**: `IntersectionCalculator`/`DistanceCalculator`/`SignedDistanceCalculator`/`Intersection2d` はヘッダーオンリーのテンプレート。`float` 版の明示的インスタンス化が対応する `.cpp` にある。
