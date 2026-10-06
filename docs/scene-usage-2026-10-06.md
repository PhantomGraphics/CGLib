# Scene / SceneRuntime 利用表（2026-10-06）

`docs/todo/PLAN_cglib_refactoring.md` Phase 5 の「Scene と SceneRuntime の利用表」。ソースの `#include`・
CMake の link・vcxproj の `AdditionalDependencies` を `git grep` 相当で照合した静的調査で、実行結果ではない。
対象は `Phantom/`（CGLib/Physics/PointCloud/RayTracer）と親リポジトリの `CGApp/`。

## 2 つのモジュールは別物（名前が似ているだけ）

| | `CGLib/Scene`（`Phantom::Scene`、`SceneCore`） | `CGLib/SceneRuntime`（`Phantom::SceneRuntime`、`SceneRuntimeCore`） |
|---|---|---|
| 役割 | 旧来のレンダリング向けシーン: `SceneBase`/`SceneGroup`（ID 付きツリー）、`ParticleSystem`/`TriangleMesh`/`WireFrame` とその Scene/Builder/Presenter | UUID ノード階層・TRS・汎用コンポーネント・schema・PlaySession・`.universe` v2 の読み書き・マージ |
| ID | 整数 ID | `AssetId`（UUID）を `NodeId` として再利用 |
| 依存 | `MathCore` | `MathCore` + `AssetCore` |
| 仕様 | なし | `docs/spec/phantom_scene_runtime.md` |

## 利用者

### `Scene`（`SceneCore`）

| 利用者 | 内容 |
|---|---|
| `CGLib/Scene/SceneTest` | 自モジュールのテスト（`SceneBaseTest`・`SceneGroupTest`） |
| `CGLib/tests/install_consumer` | `SceneBase.h` を include する install 検証 |
| `CGLib/Volume/VolumeView`（link のみ） | **`Scene/` のヘッダーを 1 つも include していない**（`VolumeScene.h`・`SceneListPanel.h` は VolumeView 自身のクラス）。link は痕跡で、2026-10-06 に外した（ビルド・シナリオ通過） |
| `Physics`・`PointCloud`・`RayTracer`・`CGApp` | 使用なし（`Scene/Scene/*` の include 0 件） |

`Scene/Scene/*Presenter.*`・`TriangleMeshScene.h` は `SceneCore` のビルド対象外（Renderer/Vulkan ヘッダーを引くため。
`cmake/PhantomCoreLibs.cmake` の注記どおり）。

### `SceneRuntime`（`SceneRuntimeCore`）

| 利用者 | 内容 |
|---|---|
| `CGApp/Universe`（`UniverseScene.h`・`UniverseSceneIO.cpp`・`CommandDispatcher.cpp`・`AssetSidecar.*`・`SceneMerge.h`・`Rendering/GltfRenderer.cpp`） | `.universe` のシーン本体 |
| `CGApp/PhantomStudio/PhantomStudioEngine`（`PhantomStudioSession.*`・`TransactionManager.h`） | Studio のシーンモデル・Undo |
| `CGApp/Common/RigidSim`（`ClipSimWorld`・`RigidBodyCore`・`RigidSimWorld`） | 剛体シミュレーションのノード/コライダー記述 |
| `CGApp/CMakeLists.txt`（`CGAPP_NATIVE_DEPS`）・`Universe.vcxproj`・`PhantomStudio*.vcxproj`・`UniverseTest/CMakeLists.txt` | 上記のリンク |
| `CGLib/tests/install_consumer` | install 検証 |
| `Physics`・`PointCloud`・`RayTracer` | 使用なし |

## 結論と方針

- **共有すべきデータ契約は現時点で存在しない。** `Scene` に外部の実利用者がなく、`SceneRuntime` は `Scene` を参照しない
  ため、「最小の共有契約」を決める対象自体がない。計画にあった互換アダプタは不要。
- 両者を名前だけで統合しない（計画の原則どおり）。統合・削除の判断材料は次のとおり:
  - `Scene` を残す理由: install consumer が公開 API として検証している（`CGLibInstall.cmake` の CPU コンポーネント
    `Scene`）。外部の第三者が使う可能性があり、利用者がいる間は絞らない方針（Phase 1）に従い **今は削除しない**。
  - 削除するなら条件: ①CGLib を単独配布した利用者に `Scene` 利用者がいないことを確認、②`install_consumer` から
    `SceneBase` の検査を外す、③`SceneCore`・テスト・`CGLibInstall.cmake` の `Scene=SceneCore` を一括で外す。
    これは Phase 6（互換層の利用残数と削除条件）で扱う。
- 命名の混乱を避けるため、新規コードは `SceneRuntime` を使い、`Scene` に機能を足さない。
- 派生（Scene とは別件、Phase 6 へ）: `Phantom/cmake/Phantom{CoreLibs,VulkanApp,GTest,Pch}.cmake` は `CGLib/cmake/` の
  同名ファイルと**別内容のまま二重に存在**する（行差分 183/202/66/122）。Physics/PointCloud/RayTracer の各
  `CMakeLists.txt` は `${REPO_ROOT}/cmake`（= `Phantom/cmake`）を `CMAKE_MODULE_PATH` に積んで `include()` するため
  未使用ではない。Phantom ルートは先に `add_subdirectory(CGLib)` するので、CGLib のターゲットは CGLib 側の定義で作られる
  （`if(TARGET X) return()` による冪等ガード）が、関数（例: `phantom_use_pch`）は後から `include` された側が上書きする。
  CGLib 側にだけ入れた変更（今回の GltfRenderer/VkAppBase のソース追加、`cglib_add_test` 等）が Physics 側の
  コピーに無いのはこのため。解消案は `Phantom/cmake/*.cmake` を `CGLib/cmake/*.cmake` への薄い転送にすること
  （Phantom リポジトリ側の変更で、Physics/PointCloud/RayTracer の単独 configure と統合ビルドの再検証が必要）。
