# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

3D 空間分割・幾何クエリを提供するモジュール群（Octree/KDTree/BVH/SpaceHash/Z-Order 曲線・交差判定・距離計算等）。
`Space`（コアライブラリ `SpaceCore`）、`SpaceTest`（GoogleTest）、`SpaceView`（スタンドアロン ImGui + Vulkan ビューア）と、その CPU 側を切り出した `SpaceDemoCore`（ImGui/Vulkan 非依存、`SpaceView/SpaceDemos.*`）で構成される。ビルドは CMake のみ（`.sln` は持たない）。

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

`SpaceTest/TEST_IMPROVEMENT_PLAN.md` はテスト方針のメモ。2026-10-11 の Space リファクタリング（`docs/todo/PLAN_space_refactoring.md`）で、brute-force との固定 seed 差分テスト（`DifferentialTest`）・入力検証（`InputValidationTest`）・結果 API とワークスペース（`ResultApiTest`/`AllocationTest`）・Tree の所有契約（`TreeContractTest`）・幾何クエリの退化ケース（`GeometryContractTest`）・SpaceView の CPU 側（`SpaceDemoTest`）を追加した。
**各 API の契約（順序・境界・寿命・退化入力の結果）は `docs/spec/space_api_contracts.md`（ルート）を参照。**

注意: この環境では ninja が MSVC のヘッダー依存を追跡できないことがあり、クラスのレイアウトを変えるヘッダー変更のあとは `cmake --build ... --clean-first` が必要（`docs/spec/space_api_contracts.md` §9.5）。

### シナリオテスト（SpaceView）

```powershell
.\CGLib\Space\SpaceView\run_space_scenarios.ps1 -Configuration Debug

# 単一シナリオ
.\build\windows-debug\CGLib\Space\SpaceView.exe --run-scenario CGLib\Space\SpaceView\scenarios\octree.json
```

シナリオ JSON は `CGLib\Space\SpaceView\scenarios\` にあり、`camera.json`・`command_catalog.json`・`octree.json`・`kdtree.json`・`spacehash.json`・`compact_spacehash.json`・`signed_distance.json` の 7 本。実装方法・JSON フォーマットはシナリオテストガイドを参照。

## Architecture

### Phantom::Space（`Space/`）— コアライブラリ

`Phantom::Space` 名前空間。空間分割構造（`Octree`/`KDTree`/`BVH`/`SpaceHash`/`CompactSpaceHash`/`ZIndexedSearcher`）と、テンプレートの静的メソッド群として実装された幾何クエリ（`IntersectionCalculator`/`DistanceCalculator`/`SignedDistanceCalculator`/`Intersection2d`）を提供する。全クラスの API・計算量・サンプルコードは `readme.md` を参照。

### SpaceView（`SpaceView/`）— スタンドアロン ImGui + Vulkan アプリ

`SpaceApp : VkAppBase` 直下。**`VKSpace` 名前空間**（`Phantom::Space` ではない点に注意）。

- `World`（`World.h`）— OpenGL に依存しない軽量な状態保持クラス。パネルが `SpaceResult`（ワイヤーフレーム線・サンプル点のバッファ）へ書き込み、`markDirty()` でレンダラーへ再アップロードを伝える。
- `SpaceDemos.h/.cpp`（`SpaceDemoCore`）— 各アルゴリズムのデモの CPU 側（パラメータ構造体 → 合成サンプル → 実行 → `SpaceResult` 書き込み）。`SetParam` は全文が有限の数値であることを要求し、範囲外はクランプする。
- `SpaceMenuPanel` — 5 アルゴリズム（SpaceHash/CompactSpaceHash/KDTree/Octree/SignedDistance）を切り替えるメニュー。各アルゴリズムのパネル（`SpaceHashPanel`/`CompactSpaceHashPanel`/`KDTreePanel`/`OctreePanel`/`SignedDistancePanel`）は `IAlgorithmView`（`getName()`/`onImGui()`/`run()`/`setParam()`）を実装する薄い ImGui ラッパーで、計算は `SpaceDemos` の関数を呼ぶ。GUI の Run ボタンもコマンドの `Run` も同じ `panel->run()` を通る。
- `VkSpaceCommandDispatcher : IScenarioDispatcher` — コマンド文字列ディスパッチャ。`SetAlgorithm:<name>`／`Run`／`SetParam:<name>:<value>`／`GetLineCount`／`GetPointCount`／`GetPointPositionMin`／`GetPointPositionMax`／`GetDirty`／`ResetCamera`／`GetCameraDistance`／`Scroll:<dy>` を処理する（シナリオテストガイド参照）。
- `VkSpaceRenderer` — `World` の `SpaceResult` を線・点として描画。カメラリセット (`resetCamera()`)・スクロールズーム (`handleScroll()`) を持つ。

**新しいアルゴリズムを追加する場合:** まず `SpaceDemos` に純関数（パラメータ構造体 + 実行関数）を追加して `SpaceDemoTest` で検証し、`IAlgorithmView` を実装したパネルを追加し、`SpaceMenuPanel::AlgoType` に列挙値を追加、`viewOf()`/`algoName()`/`setActive()` に配線する。

## Key Conventions

- **例外禁止**: このリポジトリ全体の規約に従い、`throw`/`try`/`catch` は使わない。エラーは `bool`/`std::optional` で返す。
- **非所有ポインタ**: `Octree`/`BVH` は挿入されたアイテムの所有権を持たない。呼び出し元が寿命管理する（`KDTree` は位置を値で保持）。
- **Non-copyable**: `Octree`/`KDTree`/`SpaceHash`/`CompactSpaceHash`/`ZIndexedSearcher` は `UnCopyable` を継承。`unique_ptr`/`shared_ptr` で保持する。
- **KDTree の build/query 分離**: `addPoint()` は積むだけで、`build()` 時点の点だけが query に見える（`build()` 前の query は空/-1 を返す。未定義動作ではない）。`isDirty()` で未 build の点を確認できる。
- **入力検証**: 非有限の位置・不正なセル幅は拒否して `bool`/空で返す。拒否された位置も index は消費する。
- **テンプレート計算クラス**: `IntersectionCalculator`/`DistanceCalculator`/`SignedDistanceCalculator`/`Intersection2d` はテンプレートで、実装は `.cpp` にあり `float`/`double` の明示的インスタンス化を持つ（ヘッダーオンリーではない）。
