# MeshEdit（`Phantom::MeshEdit`）

ポリゴンメッシュ編集の CPU ライブラリ（`MeshEditCore`）とそのテストビュー（`MeshEditView`）。Vulkan・シーン・描画メッシュ型に依存しない。
PhantomStudio（`CGApp/PhantomStudio`）が使う編集マスタ・編集操作・ピッキング・選択ステートはここが正で、Studio 側は
転送ヘッダー（`PhantomStudioCore/EditableMesh.h` など）と `MeshData` との変換（`EditableMeshConvert.cpp`）だけを持つ。

## 構成

| ディレクトリ | 内容 |
|---|---|
| `MeshEdit/` | `EditableMesh.h/.cpp`（論理トポロジ。永続 id の頂点・辺・面、コーナーごとの UV と法線、`validate()`、`makeEditableBox/Plane`、`newellNormal`）、`MeshEditOperations.h/.cpp`（移動/回転/拡縮、押し出し、Inset、ベベル、ループカット、結合、削除、シェーディング、Mirror・Subdivision モディファイア）、`MeshEditPicking.h/.cpp`（画面空間の点・矩形ピック）、`MeshEditState.h/.cpp`（編集セッションの選択ステート、`ids=` / ピクセル / ミラー軸の引数パース） |
| `MeshEditView/` | テストビュー。`MeshWorld`（メッシュ・モディファイア・選択・Undo・描画データ。ImGui/Vulkan なし）と `CommandDispatcher`（コマンド言語）は `MeshEditDemoCore` に入り、`MeshEditTest` が窓なしで検証する。`MeshEditApp`/`Renderer`/`MeshMenuPanel` が Vulkan + ImGui 側 |
| `MeshEditTest/` | GoogleTest。`EditableMeshTest`、`MeshEditOperationsTest`、`MeshEditPickingTest`、`MeshEditStateTest`、`MeshEditViewTest` |

## 規約

- **巻き順**: すべての面は外側から見て反時計回り（CCW）。三角形の外積 `(b-a)x(c-a)` が宣言法線と同じ向き。面のループ（`EditableFace::loop`）も
  描画三角形（ループ順の扇形分割）も同じ向きで、閉じたメッシュの符号付き体積は正になる（`MeshWorld::signedVolume`）。
- **id**: 頂点・辺・面の id は 1 つのメッシュの履歴の中で再利用されない。編集は不変の入力から新しいメッシュを作り、失敗したら出力に触れない。
- **例外禁止**: 失敗は `bool` と `std::string* error`、引数エラーは `MeshArgError{code, message}`。
- 画面空間のカメラ規約: `proj` は Vulkan の Y 反転込み、ピクセルは左上原点（`MeshPickView`）。

## MeshEditView

```powershell
# 起動（Vulkan SDK + GLFW が必要）。クリック = 要素選択（編集モード）、Shift で追加、Ctrl でトグル、ドラッグ = 回転、ホイール = ズーム
.\build\windows-debug\CGLib\MeshEdit\MeshEditView.exe
# スクリーンショット（VkAppBase 内蔵）
.\build\windows-debug\CGLib\MeshEdit\MeshEditView.exe --run-scenario CGLib\MeshEdit\MeshEditView\scenarios\showcase.json --no-exit-on-complete --screenshot out.png --screenshot-frame 40
# シナリオ一括
.\CGLib\MeshEdit\MeshEditView\run_meshedit_scenarios.ps1 -Configuration Debug
```

操作はすべてコマンド（`Verb` または `Verb:先頭引数 key=value ...`）で、Mesh メニュー・Control ウィンドウ・Command ウィンドウ・シナリオが同じ経路を通る
（`CommandDispatcher::route`）。`Select:0,1,4-6 mode=add`、`Extrude:distance=0.4`、`Inset:thickness=0.1 depth=0`、`Bevel:width=0.1`、`LoopCut:factor=0.5`、
`Shade:smooth angle=60`、`Merge:center`、`Move:0,0.2,0`、`AddModifier:mirror axis=x` / `AddModifier:subdivision levels=2`、`Undo`/`Redo`、
`SetDisplay:orientation=1 normals=1`（面の向き: 表=青・裏=赤、法線の線）。確認用: `GetCounts`、`GetSelection`、`GetVolume`（符号付き体積）、
`GetOrientation`（カメラから見た表裏の三角形数）、`ProjectVertex:id`（頂点のピクセル。`store_as` で `Pick:${px}` に渡せる）。全コマンドは `help`（Command ウィンドウ）か
`commandCatalog()` で見られる。
