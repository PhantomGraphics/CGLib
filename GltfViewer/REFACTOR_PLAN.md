# GltfViewer リファクタリング計画

`vulkan_test_app_guide.md` に基づき、GltfViewer をフレームワーク準拠の構成に整理し、
再利用可能な描画ロジックを `GltfRenderer` 静的ライブラリとして切り出す。

---

## 現状の問題点

| 問題 | 場所 | ガイドの期待 |
|------|------|------------|
| `GltfSceneRenderer` が `IVkSubRenderer` を実装していない | `GltfSceneRenderer.h` | `onInit/onUpdate/onRender/onCleanup` の標準インタフェース準拠 |
| ライフサイクルをアプリが手動管理 | `GltfViewerApp.cpp` | `add()` に登録して `VkAppBase` に委譲 |
| `onSwapChainCreated()` でフルdestroy/recreate | `GltfViewerApp.cpp` | `setExtent()` 呼び出しのみで足りる |
| カメラ計算・ImGui がアプリに混在 | `GltfViewerApp.cpp` | 責務を分離 |
| `main.cpp` に Test Engine サポートがない | `main.cpp` | ガイド §6-5 のパターン |
| `GltfShaderSPV.h` がアプリ側にある | `GltfViewer/` | ライブラリ内部に移動すべき |

---

## 目標アーキテクチャ

```
CGLib/
├── GltfRenderer/               ← 新規: 静的ライブラリ
│   ├── GltfRenderer.vcxproj
│   ├── Gltf/                   ← 既存から移動
│   │   ├── GltfDocument.h
│   │   ├── GltfTypes.h
│   │   ├── GltfReader.h / .cpp
│   │   └── GltfAccessorView.h / .cpp
│   └── Renderer/               ← 既存から移動・改修
│       ├── CameraUBO.h         ← 新規: UBO 定義を独立ヘッダへ
│       ├── GltfMesh.h / .cpp
│       ├── GltfMaterial.h / .cpp
│       ├── GltfSceneRenderer.h / .cpp  ← IVkSubRenderer 準拠に改修
│       └── GltfShaderSPV.h     ← GltfViewer/ から移動
│
GltfViewer/                     ← 既存アプリ（薄いシェルに整理）
├── GltfViewer.vcxproj          ← GltfRenderer を ProjectReference 追加
├── GltfViewerApp.h / .cpp      ← add() パターンに整理
├── GltfViewerPanel.h / .cpp    ← 新規: IVkUIPanel として ImGui を分離
├── GltfViewerTests.h / .cpp    ← 新規: ImGui Test Engine テスト登録
└── main.cpp                    ← Test Engine 対応に更新
```

---

## Phase 1 — GltfRenderer ライブラリの作成

### 1-1. ディレクトリと vcxproj 新規作成

`CGLib/GltfRenderer/GltfRenderer.vcxproj` を作成する。

設定チェックリスト（ガイド §8 準拠）：

| 項目 | 値 |
|------|-----|
| 出力種別 | `StaticLibrary` |
| C++ 標準 | `stdcpp17` |
| `/utf-8` | `AdditionalOptions` に追加 |
| インクルードパス | `../../CGLib/VkAppBase/imgui`, `../../CGLib/VulkanGraphics`, glm パス |
| プロジェクト参照 | `VulkanGraphics`, `VkAppBase` |

### 1-2. ファイルの移動

| 移動元 | 移動先 |
|--------|--------|
| `GltfViewer/Gltf/GltfDocument.h` | `GltfRenderer/Gltf/GltfDocument.h` |
| `GltfViewer/Gltf/GltfTypes.h` | `GltfRenderer/Gltf/GltfTypes.h` |
| `GltfViewer/Gltf/GltfReader.h/.cpp` | `GltfRenderer/Gltf/GltfReader.h/.cpp` |
| `GltfViewer/Gltf/GltfAccessorView.h/.cpp` | `GltfRenderer/Gltf/GltfAccessorView.h/.cpp` |
| `GltfViewer/Renderer/GltfMesh.h/.cpp` | `GltfRenderer/Renderer/GltfMesh.h/.cpp` |
| `GltfViewer/Renderer/GltfMaterial.h/.cpp` | `GltfRenderer/Renderer/GltfMaterial.h/.cpp` |
| `GltfViewer/Renderer/GltfSceneRenderer.h/.cpp` | `GltfRenderer/Renderer/GltfSceneRenderer.h/.cpp` |
| `GltfViewer/GltfShaderSPV.h` | `GltfRenderer/Renderer/GltfShaderSPV.h` |

インクルードパスの相対参照はすべて移動後のパスに更新する。

### 1-3. `CameraUBO.h` の新規作成

`CameraUBO` と `MaterialUBO` は複数のファイルから参照される共有型なので独立ヘッダに切り出す。

```cpp
// GltfRenderer/Renderer/CameraUBO.h
#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct CameraUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 camPos;
    float     padding = 0.f;
};
```

`GltfSceneRenderer.h` と `GltfViewerApp.h` はこのヘッダを `#include` する。
`GltfMaterial.h` の `MaterialUBO` も同ファイルにまとめる。

### 1-4. `GltfSceneRenderer` を `IVkSubRenderer` に準拠させる

#### 変更前 (現在のインタフェース)

```cpp
void init(const VulkanContext&, const VulkanCommandPool&, VkRenderPass, const GltfDocument&);
void updateCamera(uint32_t frameIndex, const CameraUBO& cam);
void render(VkCommandBuffer cmd, uint32_t frameIndex);
void destroy(VkDevice device);
bool isReady() const;
```

#### 変更後 (IVkSubRenderer 準拠)

```cpp
class GltfSceneRenderer : public VKG::IVkSubRenderer {
public:
    // --- IVkSubRenderer ---
    void onInit(VKG::VulkanContext& ctx, const VKG::VulkanCommandPool& pool,
                VkRenderPass rp, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;   // カメラUBO書き込みをここへ
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;
    void onImGui() override;                        // 統計表示など（任意）

    // --- アプリから呼ぶ設定 API ---
    void setDocument(const GltfDocument& doc);      // onInit の前に呼ぶ
    void setExtent(VkExtent2D ext);                 // onSwapChainCreated から呼ぶ

    // --- カメラ入力ハンドラ ---
    void handleMouseButton(bool pressed);
    void handleMouseMove(double x, double y);
    void handleScroll(double dy);

private:
    // カメラ球面座標 (アプリから移動)
    float     camTheta_ = 0.6f;
    float     camPhi_   = 0.4f;
    float     camDist_  = 3.0f;
    glm::vec3 camTarget_{0.f, 0.f, 0.f};
    double    lastX_ = 0.0, lastY_ = 0.0;
    bool      isDragging_ = false;
    VkExtent2D extent_ = {1280, 720};

    const GltfDocument* doc_ = nullptr;
    // ... 既存プライベートメンバ ...
};
```

**`onInit()` での変更点**:
- 引数から `doc` を削除し `doc_` ポインタを使用
- `framesInFlight` を `MAX_FRAMES` の代わりに使用（または static constexpr を維持）

**`onUpdate()` の実装**:
```cpp
void GltfSceneRenderer::onUpdate(uint32_t frameIndex) {
    // カメラ位置計算
    float x = camDist_ * std::sin(camTheta_) * std::cos(camPhi_);
    float y = camDist_ * std::cos(camTheta_);
    float z = camDist_ * std::sin(camTheta_) * std::sin(camPhi_);
    glm::vec3 eye = camTarget_ + glm::vec3(x, y, z);

    float aspect = (extent_.height > 0)
        ? (float)extent_.width / (float)extent_.height : 1.f;

    CameraUBO cam{};
    cam.model  = glm::mat4(1.f);
    cam.view   = glm::lookAt(eye, camTarget_, glm::vec3(0.f, 1.f, 0.f));
    cam.proj   = glm::perspective(glm::radians(45.f), aspect, 0.001f, 1000.f);
    cam.proj[1][1] *= -1.f;
    cam.camPos = eye;
    cameraUbos_[frameIndex].write(&cam, sizeof(CameraUBO));
}
```

**`onImGui()` の実装** (任意の軽量統計):
```cpp
void GltfSceneRenderer::onImGui() {
    // アプリ側 GltfViewerPanel から呼ばれるため、ここでは描画しない
    // 必要であれば内部デバッグ情報を ImGui::Text で表示
}
```

> `onImGui()` は `IVkSubRenderer` に登録されているため、アプリ側で重複しないよう設計する。
> メインUIは後述の `GltfViewerPanel` が担う。

### 1-5. `onSwapChainCreated()` の扱い方

ガイド §5 のパターンに従い、`onSwapChainCreated()` では **destroy/recreate せず `setExtent()` を呼ぶだけ** にする。

```
現在:  onSwapChainCreated() → renderer_->destroy() → renderer_->init()  [重い]
変更後: onSwapChainCreated() → renderer_.setExtent(getExtent())            [軽い]
```

ただし **RenderPass が変わるリサイズ** (MSAA 切り替えなど) には再初期化が必要になる場合がある。
通常のウィンドウリサイズは extent 更新のみで対応可能。

---

## Phase 2 — GltfViewerApp の整理

### 2-1. `add()` パターンへの移行

```cpp
// GltfViewerApp.h
class GltfViewerApp : public VKG::VkAppBase {
public:
    GltfViewerApp(const std::filesystem::path& gltfPath);

protected:
    void onInit()             override;
    void onSwapChainCreated() override;
    void onCleanup()          override;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    void onImGuiReady() override;
    void onPostSwap()   override;
#endif

private:
    GltfDocument       doc_;
    GltfSceneRenderer  renderer_;   // IVkSubRenderer — add() で登録
    GltfViewerPanel    panel_;      // IVkUIPanel    — add() で登録

    void setupCallbacks();
};
```

```cpp
// GltfViewerApp.cpp
GltfViewerApp::GltfViewerApp(const std::filesystem::path& gltfPath)
    : VKG::VkAppBase(1280, 720, gltfPath.empty()
          ? "glTF Viewer"
          : "glTF Viewer - " + gltfPath.filename().string())
{
    if (!gltfPath.empty()) {
        doc_ = GltfReader::load(gltfPath);
        renderer_.setDocument(doc_);
    }
    panel_.setRenderer(&renderer_);   // パネルにドキュメント情報を渡す
    add(&renderer_);
    add(&panel_);
}

void GltfViewerApp::onInit() {
    VKG::VkAppBase::onInit();          // → renderer_.onInit() が呼ばれる
    renderer_.setExtent(getExtent());
    setupCallbacks();
}

void GltfViewerApp::onSwapChainCreated() {
    renderer_.setExtent(getExtent());  // extent 更新のみ
}

void GltfViewerApp::onCleanup() {
#ifdef IMGUI_ENABLE_TEST_ENGINE
    if (testEngine_) ImGuiTestEngine_Stop(testEngine_);
#endif
    VKG::VkAppBase::onCleanup();       // → renderer_.onCleanup() が呼ばれる
}

void GltfViewerApp::setupCallbacks() {
    auto& win = getWindow();
    win.onMouseButton = [this](int btn, int action, int) {
        if (btn == 0) renderer_.handleMouseButton(action == 1);
    };
    win.onCursorPos = [this](double x, double y) {
        renderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        renderer_.handleScroll(dy);
    };
}
```

**削除するメンバ**:
- カメラ変数 (`camTheta_`, `camPhi_` など) — レンダラーに移動済み
- `renderer_` を `unique_ptr` でなく値メンバに変更
- `onUpdate()`, `onRender()` のオーバーライド — デフォルト実装 (add 委譲) で十分

### 2-2. `GltfViewerPanel` の新規作成

```cpp
// GltfViewerPanel.h
#pragma once
#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
class GltfSceneRenderer;
struct GltfDocument;

class GltfViewerPanel : public VKG::IVkUIPanel {
public:
    void setRenderer(GltfSceneRenderer* r) { renderer_ = r; }
    void onImGui() override;
private:
    GltfSceneRenderer* renderer_ = nullptr;
};
```

```cpp
// GltfViewerPanel.cpp
#include "GltfViewerPanel.h"
#include "GltfSceneRenderer.h"
#include <imgui/imgui.h>

void GltfViewerPanel::onImGui() {
    if (!renderer_) return;
    ImGui::Begin("glTF Viewer");
    ImGui::SliderFloat("Camera Distance", renderer_->camDistPtr(), 0.1f, 100.f);
    ImGui::SliderFloat3("Camera Target",  renderer_->camTargetPtr(), -10.f, 10.f);
    ImGui::Separator();
    // 統計（ドキュメント情報を renderer_ 経由で取得）
    ImGui::End();
}
```

> `camDistPtr()` / `camTargetPtr()` はレンダラーが公開するアクセサ。
> ImGui の `SliderFloat` に直接ポインタを渡せるよう `float*`/`glm::vec3*` を返す。

---

## Phase 3 — ImGui Test Engine の追加

### 3-1. `GltfViewerTests.h/.cpp` の作成

ガイド §6-3 のパターンに従う。

```cpp
// GltfViewerTests.h
#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "imgui_test_engine/imgui_test_engine/imgui_te_engine.h"
class GltfViewerApp;
void RegisterGltfViewerTests(ImGuiTestEngine* e, GltfViewerApp* app);
#endif
```

```cpp
// GltfViewerTests.cpp
#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "GltfViewerTests.h"
#include "GltfViewerApp.h"
#include "imgui_test_engine/imgui_test_engine/imgui_te_context.h"

void RegisterGltfViewerTests(ImGuiTestEngine* e, GltfViewerApp* app) {
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "gltf_viewer", "panel_open");
        t->TestFunc = [app](ImGuiTestContext* ctx) {
            ctx->Yield(2);
            ctx->ItemClick("glTF Viewer");
            IM_CHECK(ImGui::FindWindowByName("glTF Viewer") != nullptr);
        };
    }
}
#endif
```

### 3-2. `main.cpp` の更新

```cpp
#include "GltfViewerApp.h"
#include "GltfViewerTests.h"

int main(int argc, char* argv[]) {
    std::filesystem::path modelPath = (argc >= 2) ? argv[1] : std::filesystem::path{};
    GltfViewerApp app(modelPath);

#ifdef IMGUI_ENABLE_TEST_ENGINE
    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& io   = ImGuiTestEngine_GetIO(engine);
    io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    io.ConfigLogToFunc = [](ImGuiTestEngine*, ImGuiTestContext*,
                             ImGuiTestVerboseLevel, const char* msg, void*) {
        puts(msg);
    };
    app.setTestEngine(engine);

    bool autoRun = (argc > 1 && std::string(argv[1]) != /* modelPath check */ "");
    // ※ モデルパスとテストフィルタを区別するため引数設計を要検討
    if (autoRun) {
        app.queueTests(ImGuiTestGroup_Tests, "");
        app.setExitCondition([engine]() {
            return ImGuiTestEngine_IsTestQueueEmpty(engine);
        });
    }
#endif

    app.run();

#ifdef IMGUI_ENABLE_TEST_ENGINE
    int countTested = 0, countSuccess = 0;
    ImGuiTestEngine_GetResult(engine, countTested, countSuccess);
    ImGuiTestEngine_DestroyContext(engine);
    if (autoRun) {
        printf("[tests] %d / %d passed\n", countSuccess, countTested);
        return (countTested > 0 && countSuccess == countTested) ? 0 : 1;
    }
#endif
    return 0;
}
```

> **引数設計の注意**: モデルパスとテストフィルタが argv[1] を競合する。
> `--test` フラグで切り分けるか、`argc == 1` のときのみテスト自動実行とする設計が安全。

### 3-3. `GltfViewer.vcxproj` の Test Engine 設定

ガイド §6-1 に従い Debug|x64 の `PreprocessorDefinitions` に追加:

```
IMGUI_ENABLE_TEST_ENGINE
IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION=1
IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
```

`GltfViewerTests.cpp` の `PrecompiledHeader` を `NotUsing` に設定。

---

## Phase 4 — vcxproj の更新

### GltfViewer.vcxproj

1. `Gltf/`, `Renderer/` のソースファイル参照を `GltfRenderer` プロジェクトへの `ProjectReference` に置換
2. `GltfShaderSPV.h` の参照先を `GltfRenderer/Renderer/` に更新
3. `GltfViewerPanel.cpp`, `GltfViewerTests.cpp` をソースリストに追加
4. Phase 3-3 の Debug プリプロセッサ定義を追加

### GltfRenderer.vcxproj (新規)

1. 移動した全ソースファイルを追加
2. `ProjectReference`: `VulkanGraphics`, `VkAppBase`
3. インクルードパス: VkAppBase/imgui, CGLib/VulkanGraphics, glm

---

## 実装順序

```
[DONE] Step 1  GltfRenderer.vcxproj 新規作成 + ファイル移動
               → CGLib/GltfRenderer/ 以下に全ファイル配置済み
               → Phantom2026.sln に追加済み
               → Debug|x64 ビルド確認済み (2026-04-28)

[DONE] Step 2  CameraUBO.h を切り出し、インクルードを更新
               → GltfRenderer/Renderer/CameraUBO.h に CameraUBO / MaterialUBO を定義
               → GltfMaterial.h から MaterialUBO 定義を削除し CameraUBO.h を include

[DONE] Step 3  GltfSceneRenderer を IVkSubRenderer に準拠させる
               → カメラ変数をレンダラーへ移動
               → onInit / onUpdate / onRender / onCleanup に改名
               → handleMouse* メソッドを追加
               → setDocument / setExtent を追加
               → GltfRenderer.lib として単体ビルド成功

Step 4  GltfViewerPanel.h/.cpp 新規作成
Step 5  GltfViewerApp を add() パターンに整理
        - onUpdate / onRender のオーバーライドを削除
        - onSwapChainDestroying を削除
        - setupCallbacks を追加
Step 6  GltfViewerTests.h/.cpp 新規作成
Step 7  main.cpp を Test Engine 対応に更新
Step 8  GltfViewer.vcxproj を更新（ProjectReference 追加、Test Engine 設定）
Step 9  ビルド確認・既存 glTF ファイルで動作確認
```

---

## 注意点 / 落とし穴

| 項目 | 内容 |
|------|------|
| `onSwapChainCreated()` での再初期化 | 通常リサイズは `setExtent()` のみ。RenderPass 再生成が必要な場合のみ `onCleanup` + `onInit` の順で再実行 |
| `getDescriptorSet()` の一時オブジェクト | ガイド §3 参照。値返しの戻り値に `&` を直接取らずローカル変数を経由 |
| `onCleanup()` の順序 | `ImGuiTestEngine_Stop()` は `cleanupImGui()` より**前**に呼ぶ。ガイド §2 参照 |
| GLM ヘッダ | `glm::lookAt` / `glm::perspective` は `<glm/gtc/matrix_transform.hpp>` が必要 |
| ImGui コンテキストの二重インクルード | `AdditionalIncludeDirectories` の先頭に `../../CGLib/VkAppBase/imgui` を配置 |
| `doc_` の生存期間 | `GltfSceneRenderer::doc_` は `GltfViewerApp::doc_` を指す生ポインタ。アプリの解体順に注意 |
| カメラ API の公開 | `camDistPtr()` 等で `float*` を返すと ImGui から直接変更可能。スレッドセーフティは不要（シングルスレッドのメインループ内のみ使用） |

## その他
文字コードに注意せよ．基本的にコメントは英語とせよ．