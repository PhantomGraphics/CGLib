# 互換層・残置物の利用残数と削除条件（2026-10-06 時点）

`docs/todo/PLAN_cglib_refactoring.md` Phase 6。「ファイル名や古い文書だけで死んだコードと判定しない」ため、
削除候補ごとに**利用元の数え方と実測値**・**削除条件**・**削除するまでの扱い**を記録する。数値は静的な
`git grep` 相当の調査（`#include`・CMake の link/include・vcxproj）で、ビルド・実行による確認ではない。
削除の実施順は CGLib → Phantom → 親リポジトリ（submodule 更新順）。

| # | 互換層・残置物 | 利用残数（実測） | 削除条件 | 現在の扱い |
|---|---|---|---|---|
| 1 | `Phantom/cmake/PhantomCoreLibs.cmake`・`PhantomVulkanApp.cmake`・`PhantomGTest.cmake`・`PhantomPch.cmake`（CGLib への転送） | Physics/PointCloud/RayTracer/Physics-examples の CMake から `include(Phantom…)` 13 件 | 上記 4 モジュールが `CMAKE_MODULE_PATH` に `${CGLIB_ROOT}/cmake` を積んで直接 `include` するよう変えた後。`phantom_add_runtime_shaders`/`phantom_find_glslc`（`PhantomVulkanApp.cmake` 内、Phantom 固有）は先に新ファイル（例 `Phantom/cmake/PhantomRuntimeShaders.cmake`）へ分離する | 転送のみ・定義は持たない（2026-10-06）。残す |
| 2 | ビルドツリーの転送ヘッダー `<build>/_cglib_headers/`（`cmake/CGLibCommon.cmake`。`#include "CGLib/..."` を無改変ソースへ解決） | `#include "CGLib/` が CGLib 198・Physics 183・PointCloud 31・RayTracer 1・CGApp 64 | 利用側が `CGLib::<Component>` のターゲット経由の include だけで解決できる状態（= ソース内の `CGLib/` プレフィックス include を、実ディレクトリ配置と一致させるか install 後レイアウトに統一）になった後 | 維持（ビルド用の互換機構。install には絶対パスを持ち込まない検査あり） |
| 3 | VulkanGraphics / VkAppBase の namespace | `Phantom::VKG` に統一済み。グローバル `::VKG` の互換宣言は撤去し、Phantom と CGApp の参照を更新（2026-10-14） | 外部利用側も `Phantom::VKG` を使用する | 撤去済み |
| 4 | 旧 `CGLib/Scene` | 実アプリ利用者 0（専用テストと install consumer のみ） | 2026-10-14、ユーザー指示により本体・テスト・ビルド・配布・CI 設定を削除 | 削除済み。新規は SceneRuntime を使う |
| 5 | `CGLib/ThirdParty/glew-2.2.0`（5.5 MB、git 追跡 9 ファイル） | CMake・vcxproj・ソースからの参照 0（`glew-2.2.0/include/GL/*.h` 自身のみ。CGApp の `RANSACCommandsTest.cpp` の一致は `RectangleWith` の部分一致（`Rectan` + `gleW` + `ith`）） | `docs/third-party.md` の「GLEW はどのターゲットからも参照されない残置」を再確認（`git grep -i glew`）し、外部利用者がいないこと（公開ヘッダーが GL/glew.h を include していない）を確認したうえで `ThirdParty/glew-2.2.0` を削除し、`third-party.md` から GLEW を外す | 削除済み（2026-10-07、参照 0 を再確認のうえ `ThirdParty/glew-2.2.0` と `third-party.md`・README の記述を同時に除去） |
| 6 | 旧名前空間名 `Crystal::` の記述 | コード内の言及はコメント 2 件（`CGApp/Universe/ImportReport.cpp`、`Rendering/GltfRenderer.h`）。実装の名前空間は `Phantom::*`。`docs/guide/architecture.md` の名前空間表は 2026-10-06 に実名前空間へ更新 | コメント 2 件を `Phantom::Gltf::…` に直す（CGApp リポジトリの変更） | コメントのみ・機能影響なし |
| 7 | `Phantom/CGLib/.github/workflows/ci.yml` の旧テスト一覧 | CPU テスト実行ファイルの存在検査が `GeometryNodeTest`/`AssetCoreTest`/`SceneRuntimeTest` を欠いていた（2026-10-06 に追加） | — | 解消済み |

## 追加・変更時のルール

- 互換層を足すときは、この表に「利用元の数え方」「削除条件」を同時に書く。
- 利用残数が 0 になったら、ここに記録した条件を確認してから削除する（削除前に `git grep` で再計測する。
  ビルド登録の有無・vcxproj・CGApp 側の参照も含める）。
- 削除は機械的移動と挙動変更を混ぜない（計画の変更単位の原則）。
