# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

CGLib は Crystal2024 フレームワークの基盤ライブラリ群。数学・グラフィックス・UI・Vulkan 抽象化の 4 モジュール
（Math/Graphics/VulkanGraphics/UIWidgets、本ファイルが扱う範囲）に加え、2026-08-21 に旧 `Crystal` submodule
（`Animation`/`File`/`Gizmo`/`GltfRenderer`/`GltfViewer`/`Input`/`Numerics`/`Particles`/`PostProcess`/`Renderer`/
`Scene`/`Space`/`VkAppBase`/`Volume`）がフラットに統合された（`docs/todo/PLAN_merge_crystal_into_cglib.md`）。
モジュール一覧・依存関係・各モジュールのクラス仕様は `CGLib/README.md` を参照（本ファイルは Math/Graphics/
VulkanGraphics/UIWidgets のビルド・規約のみを扱う）。

## Build

CMake が唯一のビルド手段（2026-08-19、`.vcxproj` は全削除済み。詳細は
`docs/todo/PLAN_crossplatform_non_cgapp_build.md` Phase 5、親リポジトリ `../CLAUDE.md` の Build 節を参照）。

```powershell
# リポジトリルートから、CGLib単体を設定・ビルド
cmake -S CGLib -B CGLib/build_windows -DCMAKE_BUILD_TYPE=Debug
cmake --build CGLib/build_windows

# または、ルートの CMakePresets.json 経由でリポジトリ全体を一括ビルド
cmake --preset windows-debug
cmake --build --preset windows-debug
```

ターゲット: `MathCore`, `MathTest`, `GraphicsCore`, `GraphicsTest`, `VulkanGraphicsCore`,
`VulkanGraphicsTest`, `UIWidgetsCore`, `PugixmlCore`。

## Tests

```powershell
# ルートから一括ビルドした場合（build\windows-debug\CGLib\ 配下）
.\build\windows-debug\CGLib\MathTest.exe
.\build\windows-debug\CGLib\GraphicsTest.exe

# フィルター例
.\build\windows-debug\CGLib\MathTest.exe --gtest_filter=Vector3dTest.*

# ctest 経由（リポジトリルートから）
ctest --preset windows-debug -R "MathTest|GraphicsTest"
```

## Architecture

### モジュール依存関係

```
Math  ←  Graphics
  ↓           ↓
VulkanGraphics  ←  UIWidgets (Phantom2026.sln 側)
```

### Crystal::Math（`Math/`）

Math 型はすべて **GLM の薄いエイリアス**。`Vector3d<T>` = `glm::vec<3,T>`。演算はメンバー関数ではなく**フリー関数**（`getLengthSquared`, `getDistance`, `areSame` など）で提供される。GLM の `glm::dot`, `glm::cross`, `glm::normalize` はそのまま使う。

```cpp
Vector3df v(1, 2, 3);               // float 版
Vector3dd vd(1.0, 2.0, 3.0);       // double 版
auto len = getLength(v);            // Crystal::Math フリー関数
auto n   = glm::normalize(v);       // GLM 直接使用 OK
```

幾何プリミティブ（`Ray3d`, `Plane3d`, `Box3d`, `Sphere3d` など）も同様にテンプレート型。

### Crystal::Graphics（`Graphics/`）

`Camera` クラスが中心。`getViewMatrix()` / `getProjectionMatrix()` で GLM ベースの行列を返す。  
画像 I/O は `ImageFileReader` / `ImageFileWriter`（STB 経由）。`ColorMap` は科学可視化用のカラーテーブル管理。

### Crystal::UI（`UIWidgets/`）

**Composite パターン**。`IWindow` が基底、`IView` がデフォルト実装（子を順に `show()`）。  
`children` は**非所有の生ポインタ**リスト。ライフタイム管理は呼び出し元の責任。コピー禁止 (`UnCopyable` 継承)。

```
IWindow  (onShow() = 0)
  └── IView  (onShow() で children を順に show())
        ├── Panel
        ├── Vector3dView
        ├── Matrix4dView
        └── ... （各型に対応した View クラス群）
```

### VKG / VulkanGraphics（`VulkanGraphics/`）

**2 フェーズ初期化**が必須:
1. `VulkanContext::createInstance()` — VkInstance 作成
2. `VulkanContext::initDevice(surface)` — 物理/論理デバイス選択

メモリは **VMA（Vulkan Memory Allocator）** で管理。`VulkanContext::getAllocator()` で取得。

**バッファパターン**:
- デバイスローカル → `VulkanBuffer::create(ctx, pool, size, usage, data)` — staging 転送自動
- ユニフォームバッファ → `VulkanBuffer::createMapped(ctx, size, usage)` + `write()` でフレーム毎更新

**パイプライン作成**:
```cpp
PipelineConfig cfg;
cfg.vertSpv  = loadSPV("vert.spv");
cfg.fragSpv  = loadSPV("frag.spv");
cfg.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
// cfg.depthTest/depthWrite/blendEnable/cullMode/samples etc.
if (!pipeline.create(ctx, renderPass, cfg)) { /* ハンドルは未設定のまま */ }
```
Viewport/scissor は dynamic state なので `PipelineConfig` に含めない。

**オフスクリーンレンダリング** (`VulkanOffscreen`): FBO 相当。`create()` → `beginRenderPass()` / `endRenderPass()` → `getColorImageView()` でサンプリング。

## Key Conventions

- `VulkanBuffer` はコピー禁止・ムーブ可。他の VKG クラスはコピー禁止・ムーブ未定義（値として保持せず `unique_ptr` か直接メンバーで使う）。
- Math の `Vector3df`/`Vector3dd`/`Matrix4df` 等のエイリアスを使い、`glm::vec3` 等のプリミティブを直接コードに書かない。
- エラーは `bool` 戻り値で表現する（`VK_SUCCESS` チェック後）。失敗時は `stderr` にログを出し、ハンドルは未設定（`VK_NULL_HANDLE` 等の既定値）のまま返す。呼び出し元は戻り値または `isValid()` で確認する。「見つからなかった」系（`findMemoryType`/`findDepthFormat` 等）は `std::optional<T>` を返す — `.value()` は使わず、`.value_or(default)` か `if (opt)` で取り出すこと（`std::bad_optional_access` の送出を避けるため）。例外は使わない。
- SPIR-V は `std::vector<uint32_t>` で受け渡し。`VulkanSPVLoader.h` の `loadSPV()` で読み込む。
