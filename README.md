# CGLib

**CGLib** は [Phantom](https://github.com/PhantomGraphics/Phantom) フレームワークの基盤となる
C++17 グラフィックス・数値計算ライブラリ群です。CG・物理シミュレーション研究用のフレームワークから
切り出した、再利用可能なモジュールの集合として公開しています。

- 線形代数・幾何プリミティブ（GLM の薄いラッパー、`float` / `double` テンプレート）
- Vulkan + [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) を RAII で包んだ薄い抽象化層
- [Dear ImGui](https://github.com/ocornut/imgui) ベースの Composite パターン UI ウィジェット
- メッシュ／点群 I/O（OBJ・glTF/GLB・PLY・STL）
- シーングラフと Presenter パターン
- 空間データ構造（Octree・KD-Tree・BVH・空間ハッシュ・Morton 曲線）
- スパースボリューム・レベルセット・Marching Cubes
- glTF 2.0 / VRM / MMD (PMX・VMD) レンダリング
- [Eigen](https://eigen.tuxfamily.org/) を Phantom の数学型に橋渡しする数値計算層

各モジュールは個別にリンクできる静的ライブラリで、それぞれ GoogleTest スイートを備えています。
主要なビューアアプリには JSON シナリオによる自動テストも用意しています。
ビルドは Phantom スーパープロジェクト経由で行います（[ビルド](#ビルド)参照）。

---

## モジュール一覧

| モジュール | 名前空間 | 概要 | Vulkan 依存 |
|---|---|---|:---:|
| `Math` | `Phantom::Math` | ベクトル・行列・クォータニオン・幾何プリミティブ | – |
| `Graphics` | `Phantom::Graphics` | カメラ・色空間・画像 I/O（STB） | – |
| `Numerics` | `Phantom::Numerics` | Eigen による固有値分解・SVD | – |
| `File` | `Phantom::File` | OBJ / glTF / PLY / STL の読み書き | – |
| `Space` | `Phantom::Space` | 空間分割・交差判定・距離計算 | – |
| `Scene` | `Phantom::Scene` | シーングラフ + Presenter パターン | – |
| `Volume` | `Phantom::Volume` | スパースボリューム・Marching Cubes・レベルセット | – |
| `VulkanGraphics` | `VKG` | Vulkan オブジェクトの抽象化（コンテキスト・バッファ・パイプライン等） | ✔ |
| `UIWidgets` | `Phantom::UI` | ImGui ベースの UI フレームワーク | ✔ |
| `VkAppBase` | `VKG` | Vulkan アプリケーション基盤（ウィンドウ・メインループ・スクリーンショット） | ✔ |
| `Renderer` | `VKG` | 三角形／点／線／スカイボックス等のサブレンダラー | ✔ |
| `GltfRenderer` | – | glTF / VRM / MMD 専用レンダラーと IBL 前計算 | ✔ |
| `Animation` | `Phantom::Animation` | スケルタルアニメーション | ✔ |
| `Particles` | `Phantom::Particles` | GPU パーティクルシミュレーション・ビルボード描画 | ✔ |
| `PostProcess` | `Phantom::PostProcess` | Bloom・SSAO・FXAA・トーンマッピング | ✔ |
| `Gizmo` | `Phantom::Gizmo` | トランスフォームギズモ | ✔ |
| `Input` | `Phantom::Input` | 入力マッピング | – |

各モジュールのクラス構成・API・設計方針は **[`docs/module-reference.md`](docs/module-reference.md)** を参照してください。

### 依存関係（抜粋）

```
Math ──┬── Graphics ──┬── File
       │              └── UIWidgets ──┐
       ├── Numerics                   │
       ├── Space ──── Volume          │
       ├── Scene                      │
       └── VulkanGraphics ──┬─────────┴── VkAppBase ──┬── GltfRenderer
                            └── Renderer ─────────────┘
```

---

## 動作環境

- C++17 対応コンパイラ（MSVC v143 以降 / Clang 15 以降 / GCC 11 以降）
- CMake 3.20 以降 + [Ninja](https://ninja-build.org/)
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.3 以降 — `VulkanGraphics` 以降の Vulkan 依存モジュールのみ必要。
  `Math` / `Graphics` / `Numerics` / `File` / `Space` / `Scene` / `Volume` はヘッダ・ローダともに不要
- [GoogleTest](https://github.com/google/googletest) — テストをビルドする場合のみ

GLM・Dear ImGui・VMA・STB・nlohmann/json・tinyfiledialogs・cgltf は `ThirdParty/` に同梱しています。
Eigen は `Numerics/ThirdParty/eigen-3.4.0/` に同梱しています。

---

## ビルド

CGLib は [Phantom スーパープロジェクト](https://github.com/PhantomGraphics/Phantom) の
サブモジュールとしてビルドします。ソースは `#include "CGLib/..."` 形式でヘッダを参照し、
共有 CMake モジュール（`cmake/`）とビルドプリセットはスーパープロジェクト側が保持しているため、
このリポジトリ単体のクローンは設定できません。Phantom を再帰クローンしてください。

```bash
git clone --recurse-submodules https://github.com/PhantomGraphics/Phantom.git
cd Phantom

# Windows
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug

# Linux
cmake --preset linux-debug \
    -DVULKAN_INCLUDE_DIR=/path/to/Vulkan-Headers/include \
    -DVULKAN_LIBRARY=/path/to/libvulkan.so \
    -DGLFW_LIBRARY=/path/to/libglfw.so
cmake --build --preset linux-debug
ctest --preset linux-debug
```

成果物は `build/<preset>/CGLib/` 以下に出力されます。Vulkan ヘッダ・ローダ・GLFW ローダが
見つからない場合、Vulkan 依存ターゲットは設定失敗ではなく警告付きでスキップされ、
`Math` / `Graphics` などの非 Vulkan モジュールだけがビルドされます。

Phantom チェックアウト内であれば、各モジュールを個別に設定することもできます:

```bash
cmake -S CGLib/Space -B CGLib/Space/build -DCMAKE_BUILD_TYPE=Debug
cmake --build CGLib/Space/build
```

---

## テスト

すべて GoogleTest（`ctest` で一括実行、または `.exe` を直接実行）。

```powershell
ctest --preset windows-debug

# 個別実行の例（build\windows-debug\CGLib\ 以下）
.\build\windows-debug\CGLib\MathTest.exe
.\build\windows-debug\CGLib\GraphicsTest.exe
.\build\windows-debug\CGLib\VulkanGraphicsTest.exe
.\build\windows-debug\CGLib\Numerics\NumericsTest.exe
.\build\windows-debug\CGLib\File\FileTest.exe
.\build\windows-debug\CGLib\Scene\SceneTest.exe
.\build\windows-debug\CGLib\Space\SpaceTest.exe
.\build\windows-debug\CGLib\Volume\VolumeTest.exe
.\build\windows-debug\CGLib\Animation\AnimationTest.exe
.\build\windows-debug\CGLib\GltfRenderer\GltfRendererTest.exe

# 単一ケースのみ
.\build\windows-debug\CGLib\Space\SpaceTest.exe --gtest_filter=OctreeTest.Insert
```

- 命名規則: `TEST(ClassName, MethodName)` — クラス単位でグループ化
- 許容誤差: `float` は `1e-5f`〜`1e-6f`、`double` は `1e-12`〜`1e-14`

---

## サンプルビューア

`VkAppBase` を土台にした ImGui + Vulkan の単体アプリを同梱しています。

| ビューア | 内容 |
|---|---|
| `Animation/AnimationView` | スケルタルアニメーション（MMD / glTF） |
| `GltfViewer` | glTF 2.0 / VRM シーンビューア |
| `Space/SpaceView` | 空間分割アルゴリズムの可視化 |
| `Volume/VolumeView` | スパースボリューム・等値面抽出の可視化 |
| `Renderer/VkRendererView` | サブレンダラーのデモ |

各ビューアは以下の共通コマンドライン引数をサポートします。

```powershell
# 指定フレームで PNG を保存して実行を継続（ウィンドウのフォーカス・最小化に依存しない）
.\build\windows-debug\CGLib\Space\SpaceView.exe --screenshot out.png --screenshot-frame 10

# JSON シナリオをヘッドレスで実行（自動テスト用）
.\build\windows-debug\CGLib\Space\SpaceView.exe --run-scenario Space/SpaceView/scenarios/octree.json
```

シナリオランナー（`run_*_scenarios.ps1`）とシナリオ JSON は各ビューアのディレクトリにあります。

---

## ディレクトリ構成

```
CGLib/
├── Math/ Graphics/ Numerics/ File/ Space/ Scene/ Volume/   # 非 Vulkan コアモジュール
├── VulkanGraphics/ UIWidgets/ VkAppBase/ Renderer/         # Vulkan 基盤
├── GltfRenderer/ Animation/ Particles/ PostProcess/ Gizmo/ Input/
├── GltfViewer/                                             # スタンドアロンビューア
├── ThirdParty/                                             # 同梱サードパーティ
├── docs/
│   └── module-reference.md                                 # 全モジュールのクラス／API リファレンス
├── CMakeLists.txt / CMakePresets.json
└── LICENSE
```

---

## ドキュメント

- **[`docs/module-reference.md`](docs/module-reference.md)** — 全モジュールのクラス構成・API・設計パターン
- 各モジュールの `readme.md` / `CLAUDE.md` — モジュール個別の詳細

---

## ライセンス

本リポジトリのソースコードは [MIT License](LICENSE) で公開しています。

同梱するサードパーティライブラリはそれぞれのライセンスに従います。

| ライブラリ | ライセンス | 所在 |
|---|---|---|
| GLM | MIT | `ThirdParty/glm-0.9.9.8/` |
| Dear ImGui | MIT | `ThirdParty/imgui/` |
| VulkanMemoryAllocator | MIT | `ThirdParty/VulkanMemoryAllocator/` |
| stb | MIT / Public Domain | `ThirdParty/stb/` |
| nlohmann/json | MIT | `ThirdParty/nlohmann/` |
| tinyfiledialogs | zlib | `ThirdParty/tinyfiledialogs/` |
| pugixml | MIT | `ThirdParty/pugixml/` |
| GLFW | zlib/libpng | `ThirdParty/glfw-3.3.8/` |
| cgltf | MIT | `File/ThirdParty/cgltf/` |
| Eigen | MPL2 | `Numerics/ThirdParty/eigen-3.4.0/` |
