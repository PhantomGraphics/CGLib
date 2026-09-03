# CGLib 独立リポジトリ化・単独ビルド改良計画

作成日: 2026-09-03  
対象: 現在の `CGLib` ディレクトリをリポジトリのルートとして公開する構成

## 実装状況

- **M1（Phase 0-1）実装済み（2026-09-03）** — 親ディレクトリ非依存の自己完結 CMake ビルド。
  - 親の CMake helper 3 本を `cmake/`（`CGLibCommon.cmake` / `PhantomGTest.cmake` /
    `PhantomCoreLibs.cmake` / `PhantomVulkanApp.cmake`）へ移設し、`REPO_ROOT = CGLib/..` を撤去。
  - `#include "CGLib/..."` 規約は `CGLibCommon.cmake` がビルドツリーに生成する転送ヘッダー
    （`<build>/_cglib_headers/CGLib/...` → ソースの絶対パス）で解決。ソース側は無改変。
  - トップレベル `CMakeLists.txt` が全 CPU モジュール（Math/Graphics/Numerics/Space/Scene/
    File/Animation/Volume）＋ Pugixml を構成し、GoogleTest は system → pinned FetchContent の順で解決
    （親の NuGet `packages/` 参照を撤去）。Vulkan/GLFW モジュールは `CGLIB_ENABLE_VULKAN`
    （SDK 自動検出、明示 ON で未検出時は `FATAL_ERROR`）でゲート。
  - オプション: `CGLIB_BUILD_TESTING` / `CGLIB_BUILD_EXAMPLES` / `CGLIB_BUILD_VIEWERS` /
    `CGLIB_ENABLE_VULKAN` / `CGLIB_FETCH_GTEST`。公開ターゲットに `CGLib::<Component>` alias。
  - `CMakePresets.json` に `linux-*` / `*-cpu-*` プリセット追加。`.github/workflows/ci.yml`
    が Windows/Linux の CPU-only build+test（テストスイート数の暗黙 skip 検出付き）と
    Linux の Vulkan compile を回す。`examples/` に `CGLib::Math` / `CGLib::File` の consumer サンプル。
  - 検証: Windows/MSVC で `cmake --preset windows-cpu-debug` → build → `ctest` が 458 テスト全通過。
- M2 以降（依存管理の再設計、install/export、CI 拡充、ライセンス監査、公開準備）は未着手。

## 1. 目的

CGLib を Phantom 親リポジトリの配置、ビルド補助ファイル、NuGet 復元結果に依存せず、取得直後から CMake だけで構成・ビルド・テスト・インストールできる独立 C++ ライブラリにする。

公開可能と判断する最終条件は次のとおりとする。

- CGLib の親ディレクトリにあるファイルを一切参照しない。
- Windows と Linux のクリーン環境で、文書化されたコマンドにより構成、ビルド、テストが成功する。
- Vulkan 非搭載環境でも CPU モジュールだけを明示的にビルドできる。
- Vulkan 機能を有効にした場合、必要な依存が欠けていれば黙ってターゲットを省略せず、構成時に原因が明確になる。
- インストール後、別プロジェクトから `find_package(CGLib CONFIG REQUIRED)` と `CGLib::<component>` で利用できる。
- 同梱する第三者コードについて、出所、バージョン、ライセンス、改変有無を追跡できる。

## 2. 現状評価

現状には CMake ビルドの土台があるものの、独立リポジトリとしては未完成である。

### 2.1 独立性を妨げる重大項目

1. トップレベル `CMakeLists.txt` が `REPO_ROOT` を `CGLib/..` と定義し、親側の `cmake/PhantomGTest.cmake`、`PhantomCoreLibs.cmake`、`PhantomVulkanApp.cmake` を読み込む。CGLib だけを clone した配置では configure できない。
2. 各モジュールの `CMakeLists.txt` も `../..` を Phantom ルート、そこにある `CGLib` を本体と仮定する。このため `cmake -S File ...` のような「モジュール単独」手順も、独立公開後のディレクトリ構造では成立しない。
3. トップレベルが直接定義するのは主に Math、Graphics、VulkanGraphics、UIWidgets、Pugixml とその一部テストであり、Animation、File、Gizmo、GltfRenderer、Input、Numerics、Particles、PostProcess、Renderer、Scene、Space、Volume などを一括構成する `add_subdirectory()` がない。
4. Windows の GoogleTest 検出は親リポジトリの `packages/` に復元済みであることを前提とする。初回 clone だけではテスト環境を再現できない。
5. 公開・インストール用ルールがない。`install()`、export set、`CGLibConfig.cmake`、version file、名前空間付き公開ターゲットがないため、外部プロジェクトが通常の CMake package として利用できない。

### 2.2 設計上の改善項目

- 内部ターゲット名が `MathCore`、`GraphicsCore` などで統一されておらず、公開 API 名、コンポーネント境界、依存の可視性が確定していない。
- 多くの公開ヘッダーが `#include "CGLib/..."` を使用する。ビルドツリーでは親ディレクトリを include path に加えることで成立しているが、インストールツリーの配置規約がない。
- source list に `file(GLOB ...)` と明示列挙が混在する。公開パッケージでは意図しないファイル混入を避けるため、ライブラリ本体は明示列挙を基本としたい。
- Vulkan/GLFW が見つからない場合にターゲットを warning だけで省略する箇所が多い。利用者が「要求した機能がビルドされた」と誤認しやすい。
- ライブラリ、テスト、ビューアー、サンプルのビルド制御が分離されていない。
- C++17 と C++20 の指定がディレクトリ単位・ターゲット単位で混在する。実際に必要な最低標準と公開 compile feature を決める必要がある。
- GLFW は Windows 用バイナリを同梱する一方、Linux はシステムライブラリを要求するなど、依存取得方針がプラットフォーム間で非対称である。
- ThirdParty 配下では GLFW、GLEW、GLM のライセンス文書は確認できるが、imgui、nlohmann、pugixml、stb、tinyfiledialogs、VulkanMemoryAllocator などを含め、公開前にライセンスファイルと NOTICE の網羅性を監査する必要がある。
- CI、最小利用例、ABI/バージョニング方針、リリース作成手順がない。

## 3. 目標アーキテクチャ

トップレベル CMake を唯一の製品ビルド入口とする。モジュール配下の CMake は原則としてターゲット定義だけを持ち、単独の `project()` やリポジトリ位置の推測を行わない。

```text
CGLib/
  CMakeLists.txt
  CMakePresets.json
  cmake/
    CGLibDependencies.cmake
    CGLibConfig.cmake.in
    modules/*.cmake            # 必要な場合のみ
  Math/CMakeLists.txt
  Graphics/CMakeLists.txt
  ...
  tests/ または各モジュールの Test/
  examples/
  docs/
  ThirdParty/
```

公開ターゲットは `CGLib::Math`、`CGLib::Graphics`、`CGLib::File` のような名前に統一する。ビルドツリーでも同じ名前を alias として提供し、利用例とインストール後の挙動を一致させる。

機能は最低限、次のオプションで分離する。

| オプション | 既定値 | 役割 |
|---|---:|---|
| `CGLIB_BUILD_TESTING` | top-level では `ON` | CTest/GoogleTest を有効化 |
| `CGLIB_BUILD_EXAMPLES` | `OFF` | 小さな利用例をビルド |
| `CGLIB_BUILD_VIEWERS` | `OFF` | Vulkan/GLFW を使う実行アプリをビルド |
| `CGLIB_ENABLE_VULKAN` | 依存検出または明示指定 | Vulkan 系ライブラリを有効化 |
| `CGLIB_BUILD_SHARED_LIBS` または標準 `BUILD_SHARED_LIBS` | `OFF` | 初期公開では static を基準にし、shared 対応は検証後に提供 |
| `CGLIB_USE_BUNDLED_DEPS` | 方針決定後 | 同梱依存と system/package-manager 依存を切替 |

ライブラリの依存グラフは CPU-only と GPU/UI を明確に分ける。

- CPU-only: Math → Graphics、Numerics、Space → Volume、File、Scene、Animation
- GPU/UI: VulkanGraphics → UIWidgets/VkAppBase → VkRenderer、GltfRenderer、VolumeRenderer、Gizmo、Particles、PostProcess
- Apps: GltfViewer、AnimationView、SpaceView、VolumeView、VkRendererView

正確な辺は既存の共通 CMake 関数と実ソースの include/link を照合して確定し、循環依存を CI で防ぐ。

## 4. 実施フェーズ

### Phase 0: 再現可能な基準線を作る

作業前に、現行 Phantom 配下でビルド可能なターゲットとテスト結果を保存する。

- Windows/MSVC と Linux/Clang または GCC のコンパイラ・SDK バージョンを記録する。
- 全ターゲット一覧、成功/skip/失敗したテスト、Vulkan 実行テストに必要な GPU 条件を記録する。
- 親リポジトリに依存する全パスを機械検索し、移行チェックリストに固定する。
- `.vcxproj` の source list と CMake の source list の差分を確認する。

完了条件: 現行挙動を比較できるログがあり、移行対象ターゲットの一覧に漏れがない。

### Phase 1: CMake 自己完結化

最優先で configure 時の親依存をなくす。

- 親の 3 つの CMake helper を `cmake/` へ移し、`CMAKE_CURRENT_LIST_DIR` と `PROJECT_SOURCE_DIR` を基準に書き直す。
- `REPO_ROOT`、`${REPO_ROOT}/CGLib`、`${REPO_ROOT}/packages` といった配置依存を廃止する。
- トップレベルから全モジュールを構成する。各モジュール CMake は top-level/subproject の双方で安全な target definition に整理する。
- 重複する `project()`、言語標準、warning、依存探索をトップレベルと共通 helper に集約する。
- `include(CTest)` と `BUILD_TESTING` の標準的な制御へ寄せる。
- 必須コンポーネントの依存不足は `FATAL_ERROR`、無効な任意コンポーネントは明示的な status と configure summary で示す。

完了条件: CGLib ディレクトリだけを一時ディレクトリへコピーした状態で、親にアクセスせず CPU-only configure/build/test が成功する。

### Phase 2: 依存管理を再設計する

依存ごとに「同梱」「`find_package`」「FetchContent/package manager」のいずれを公式経路にするか決める。初期案は次のとおり。

- Vulkan: `find_package(Vulkan)` を正式経路とする。
- GLFW: viewer/test 実行時のみ `find_package(glfw3 CONFIG)` を優先し、必要なら明示的な fallback を用意する。
- GoogleTest: 製品依存から分離し、テスト有効時のみ `find_package(GTest CONFIG)`、任意でバージョン固定の FetchContent fallback を提供する。親の NuGet `packages/` は参照しない。
- GLM、Eigen、nlohmann_json、pugixml、imgui、stb、VMA、cgltf 等: 実際の公開 API 露出、改変の有無、上流 CMake package の品質を調べて個別決定する。
- FetchContent を使う場合も、オフライン/ディストリビューションビルド向けに system package のみで構成できるモードを残す。
- 全依存をバージョン固定し、`CGLibDependencies.cmake` に集約する。

完了条件: クリーン Windows/Linux 環境で依存の入手手順が一意であり、ネットワーク利用の有無を利用者が制御できる。

### Phase 3: ターゲットと公開ヘッダーを整備する

- `CGLib::<Component>` を正式ターゲット名にする。
- `PUBLIC`、`PRIVATE`、`INTERFACE` の link/include 定義を見直し、利用者が不要な include path やライブラリを引き継がないようにする。
- `BUILD_INTERFACE` と `INSTALL_INTERFACE` を設定する。
- `#include <CGLib/...>` を正式な include 規約とし、インストール先を `${CMAKE_INSTALL_INCLUDEDIR}/CGLib` に合わせる。
- 公開ヘッダーと内部ヘッダーを分類する。必要なら `include/CGLib/` への段階移行を行う。
- source list を明示化し、除外理由がある古いソースや未使用ヘッダーは整理する。
- C++ 標準は最低要件を検証して統一する。C++20 を全体要件にする場合は README と package config に明記する。
- shared library を提供するなら export macro、Windows DLL symbol、runtime dependency を別タスクとして実装する。初回リリースを static-only とする判断も明記する。

完了条件: build tree と install tree の双方で同一の consumer サンプルがビルドでき、公開ヘッダー単体の compile test が通る。

### Phase 4: install/package 対応

- GNUInstallDirs を使用し、ライブラリ、ヘッダー、必要な shader/resource を適切な場所へ install する。
- `install(TARGETS ... EXPORT CGLibTargets)` を導入する。
- `CMakePackageConfigHelpers` で `CGLibConfig.cmake` と `CGLibConfigVersion.cmake` を生成する。
- component ごとの依存を package config に伝播させる。
- `cmake --install` の staging directory に対し、別 build directory の consumer を構成する smoke test を追加する。
- CPack または GitHub Releases 用アーカイブは、install tree が安定してから追加する。

完了条件: source tree を include path に含めず、インストール成果物だけで consumer が link・実行できる。

### Phase 5: テストと CI

推奨 CI matrix:

- Windows: MSVC、Debug/Release、CPU-only と Vulkan compile。
- Ubuntu: GCC と Clang、Debug/Release、CPU-only と Vulkan compile。
- configure-only: tests/viewers の各 option combination。
- install-consumer: install 後の `find_package` smoke test。
- hygiene: format/lint は段階導入し、第三者コードを対象外にする。

Vulkan テストは次の 3 層に分ける。

1. ヘッダーだけで成立する core の compile test。
2. loader まで使う headless test。利用可能な CI runner のみ実行する。
3. surface/GPU/画面が必要な integration test。通常 CI の必須判定から分離し、対応 runner または手動 release gate で実行する。

テストが依存不足で登録されなかった場合を成功扱いにしない。要求した profile に必要な test 数または target の存在を CI で検証する。

完了条件: pull request ごとに主要 matrix が自動実行され、必須ターゲットの暗黙 skip を検出できる。

### Phase 6: 公開準備

- README のビルド手順を独立リポジトリ前提に全面更新する。
- `CONTRIBUTING.md`、サポート対象表、互換性/バージョニング方針、リリース手順を追加する。
- `LICENSE` に加え、`THIRD_PARTY_NOTICES.md` と必要な原ライセンス全文を整備する。
- バイナリ同梱物の再配布条件、shader、テスト fixture、サンプルモデル/画像の権利を監査する。
- Phantom や社内パス、私有プロジェクト名、内部設計文書への参照を README、コメント、fixture、履歴に残してよいか確認する。
- GitHub Actions、issue/PR template、Dependabot または同等の依存更新フローを追加する。
- SemVer の初期バージョンを決め、タグから source archive と checksum を生成する。

完了条件: ライセンスレビューと secret/large-file scan が完了し、第三者が README だけで build と consumer 利用を再現できる。

## 5. 推奨する実装順序とマイルストーン

| マイルストーン | 範囲 | リリース可能性 |
|---|---|---|
| M1: Self-contained configure | Phase 0-1、CPU-only の Math/Graphics/Numerics/Space/File/Scene/Animation/Volume | 開発版として評価可能 |
| M2: Reproducible dependencies | Phase 2、VulkanGraphics/UIWidgets/Input 等を追加 | 全ソースの CI ビルド可能 |
| M3: Consumable package | Phase 3-4、install/export/consumer test | ライブラリ利用者向け preview 可能 |
| M4: Public release | Phase 5-6、CI、ライセンス、文書、タグ | 独立公開可能 |

最初の変更セットは「親 CMake helper の移設」「パス基準の修正」「CPU-only top-level build」「その CI」に限定する。依存管理や include tree の大規模変更を同時に行わないことで、既存動作との差分を追跡しやすくする。

## 6. 検証コマンドの目標形

実装後は、少なくとも次の操作を正式に保証する。

```powershell
# Windows: CPU-only
cmake --preset windows-msvc-debug -DCGLIB_ENABLE_VULKAN=OFF
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug

# Windows: install と consumer test
cmake --install build/windows-msvc-release --prefix stage
cmake -S examples/consumer -B build/consumer -DCMAKE_PREFIX_PATH="$PWD/stage"
cmake --build build/consumer --config Release
```

```bash
# Linux: CPU-only
cmake --preset linux-gcc-debug -DCGLIB_ENABLE_VULKAN=OFF
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug

# Vulkan を必須として構成。見つからなければ configure を失敗させる。
cmake --preset linux-clang-release -DCGLIB_ENABLE_VULKAN=ON
```

Presets は開発者のローカル SDK 絶対パスを含めず、vendor preset や `CMakeUserPresets.json` で上書きできるようにする。

## 7. リスクと判断事項

### 公開前に必ず決める事項

- 全モジュールを一つの package として公開するか、CPU core と Vulkan/viewer を別 component/package にするか。
- bundled dependency を維持するか、vcpkg/Conan/system package を正式サポートするか。
- 初回リリースを static-only にするか。
- C++20 を全体の最低要件にするか。
- Windows、Linux 以外、特に macOS をサポート対象に含めるか。
- API/ABI 安定性を保証するバージョンから開始するか、0.x として変更を許容するか。

### 主なリスク

- `CGLib/...` include の変更はソース全体に波及する。互換 include shim または一括移行と consumer test が必要になる。
- GPU ターゲットは compile 成功と実行可能性が異なる。CI の合格条件を混同しない。
- vendored library を package-manager 版へ置換すると、バージョン差や compile definition 差によって ABI・描画結果が変わり得る。
- モジュール CMake の source list と既存 `.vcxproj` に差があるため、CMake 成功だけでは既存 Windows 製品との等価性を保証できない。
- test fixture や shader の相対パス依存は build tree では動いても install tree で壊れやすい。resource locator の設計が必要になる可能性がある。

## 8. 完了チェックリスト

- [ ] CGLib 外への相対パス参照がゼロである。
- [ ] top-level configure が全選択モジュールを生成する。
- [ ] CPU-only build/test が Windows と Linux で成功する。
- [ ] Vulkan build と要求時の依存エラーが Windows と Linux で検証されている。
- [ ] tests/examples/viewers を個別に無効化できる。
- [ ] 全公開ターゲットに `CGLib::` 名前空間がある。
- [ ] build/install interface と公開ヘッダー配置が確定している。
- [ ] `cmake --install` と外部 consumer test が成功する。
- [ ] CI が暗黙の target/test skip を検出する。
- [ ] README の手順をクリーン環境で第三者が再現している。
- [ ] 第三者依存、fixture、shader、画像等のライセンス監査が完了している。
- [ ] release archive に不要なビルド成果物、内部情報、秘密情報が含まれない。

このチェックリストがすべて完了した時点を、CGLib を独立リポジトリとして公開できる状態とする。
