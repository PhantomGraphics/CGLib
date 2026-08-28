# VkRendererView 実装計画

既存の `RendererView`（OpenGL + Crystal::UI フレームワーク）を
`VkAppBase` フレームワーク上の Vulkan 版として作り直す計画。
参照: `VkSpaceView_plan.md`、`CGLib/VkAppBase/`、`Crystal/VkRenderer/`

---

## 1. 概要・方針

| 項目 | RendererView (OpenGL) | VkRendererView (Vulkan) |
|------|----------------------|------------------------|
| フレームワーク | `Crystal::UI::Window / Canvas` | `VKG::VkAppBase` |
| レンダリング | OpenGL (`IScreenShader` 派生クラス群) | Vulkan (`IVkRenderer` 派生クラス群) |
| UI | `Crystal::UI::IMenu` / `MenuItem` | ImGui メニューバー |
| カメラ | `Crystal::Graphics::Camera` | GLM + GLFW コールバック |
| レンダラー切替 | `IScreenShader* activeRenderer` ポインタ | `IVkRenderer* activeRenderer_` ポインタ |

対応するレンダラー一覧:

| OpenGL (RendererView) | Vulkan (VkRenderer ライブラリ) |
|----------------------|-------------------------------|
| `PointShader`        | `VKG::VkPointRenderer`        |
| `LineShader`         | `VKG::VkLineRenderer`         |
| `TriangleShader`     | `VKG::VkTriangleRenderer`     |
| `TexShader`          | `VKG::VkTexRenderer`          |
| `SkyBoxShader`       | `VKG::VkSkyBoxRenderer`       |

---

## 2. ディレクトリ構成

```
CGLib/Renderer/VkRendererView/
├── VkRendererView.vcxproj          ← 新規プロジェクトファイル (Application)
├── VkRendererView.vcxproj.filters
├── main.cpp                        ← エントリポイント
│
├── VkRendererApp.h / .cpp          ← VkAppBase 派生、カメラ・メニュー管理
├── VkRendererSubRenderer.h / .cpp  ← IVkSubRenderer、全レンダラーを束ねる
│
└── shaders/                        ← Crystal/VkRenderer/Shaders/ からコピー
    ├── point.vert.spv / point.frag.spv
    ├── line.vert.spv  / line.frag.spv
    ├── triangle.vert.spv / triangle.frag.spv
    ├── tex.vert.spv   / tex.frag.spv
    ├── skybox.vert.spv / skybox.frag.spv
    └── compile_shaders.bat         ← (参照用コピー)
```

---

## 3. クラス設計

### 3-1. VkRendererSubRenderer (IVkSubRenderer)

5種類の `IVkRenderer` を所有し、`activeRenderer_` が指すレンダラーだけ描画する。
`RendererView/main.cpp` の `Renderer` クラスに相当。

```cpp
// VkRendererSubRenderer.h
#pragma once
#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "Crystal/VkRenderer/VkPointRenderer.h"
#include "Crystal/VkRenderer/VkLineRenderer.h"
#include "Crystal/VkRenderer/VkTriangleRenderer.h"
#include "Crystal/VkRenderer/VkTexRenderer.h"
#include "Crystal/VkRenderer/VkSkyBoxRenderer.h"
#include <glm/glm.hpp>

namespace VKRenderer {

class VkRendererSubRenderer : public VKG::IVkSubRenderer {
public:
    VkRendererSubRenderer();

    void setExtent(VkExtent2D ext) { extent_ = ext; }

    // アクティブレンダラーの切替 (メニューから呼ぶ)
    enum class Mode { Point, Line, Triangle, Tex, SkyBox };
    void setMode(Mode m) { activeMode_ = m; }
    Mode getMode() const { return activeMode_; }

    // IVkSubRenderer
    void onInit(VKG::VulkanContext& ctx, const VKG::VulkanCommandPool& pool,
                VkRenderPass rp, uint32_t frames) override;
    void onUpdate(uint32_t frame)                        override;
    void onRender(VkCommandBuffer cmd, uint32_t frame)   override;
    void onCleanup(VkDevice dev)                         override;

    // カメラ行列の設定 (VkRendererApp から毎フレーム更新)
    void setCamera(const glm::mat4& proj, const glm::mat4& view) {
        proj_ = proj; view_ = view;
    }

    // キューブマップの設定 (SkyBox 用)
    void setCubeMap(VkDevice dev, VkImageView view, VkSampler sampler);

private:
    VkExtent2D extent_    = {1280, 720};
    Mode       activeMode_= Mode::Point;

    glm::mat4  proj_{1.f};
    glm::mat4  view_{1.f};

    const VKG::VulkanContext*     ctx_  = nullptr;
    const VKG::VulkanCommandPool* pool_ = nullptr;
    uint32_t                      frames_ = 2;
    VkRenderPass                  renderPass_ = VK_NULL_HANDLE;

    VKG::VkPointRenderer    pointRenderer_;
    VKG::VkLineRenderer     lineRenderer_;
    VKG::VkTriangleRenderer triangleRenderer_;
    VKG::VkTexRenderer      texRenderer_;
    VKG::VkSkyBoxRenderer   skyBoxRenderer_;

    bool skyBoxReady_ = false;

    // サンプルジオメトリの生成 (デモ用固定データ)
    void uploadSamplePoint();
    void uploadSampleLine();
    void uploadSampleTriangle();
    void uploadSampleTex();

    glm::mat4 computeMVP() const;
};

} // namespace VKRenderer
```

`onInit()` ではSPIR-Vを読み込み全5レンダラーを `create()` する。
`onRender()` では `activeMode_` に応じて1つだけ `render()` を呼ぶ。

---

### 3-2. VkRendererApp (VkAppBase)

カメラ制御・ImGui メニューバー・`VkRendererSubRenderer` の管理を担う。
`RendererView/main.cpp` の `RendererMenu` + `Window` に相当。

```cpp
// VkRendererApp.h
#pragma once
#include "CGLib/VkAppBase/VkAppBase.h"
#include "VkRendererSubRenderer.h"
#include <string>

namespace VKRenderer {

class VkRendererApp : public VKG::VkAppBase {
public:
    VkRendererApp(int w, int h, const std::string& title);

protected:
    void onInit()             override;
    void onSwapChainCreated() override;
    void onImGui()            override;   // メニューバー「Renderer」を描画
    void onCleanup()          override;

private:
    VkRendererSubRenderer renderer_;

    // カメラ状態 (軌道カメラ)
    float azimuth_   = 0.f;
    float elevation_ = 20.f;
    float distance_  = 3.f;
    double prevMouseX_ = 0.0, prevMouseY_ = 0.0;
    bool   dragging_   = false;

    void setupWindowCallbacks();
    glm::mat4 computeViewMatrix() const;
    glm::mat4 computeProjMatrix() const;
};

} // namespace VKRenderer
```

`onImGui()` に ImGui メニューバー「Renderer > Point / Line / Triangle / Tex / SkyBox」を実装し、
`renderer_.setMode()` を呼び出す。

---

### 3-3. main.cpp

```cpp
// main.cpp
#include "VkRendererApp.h"

int main() {
    VKRenderer::VkRendererApp app(1280, 720, "VkRendererView");
    app.run();
    return 0;
}
```

---

## 4. シェーダー

`Crystal/VkRenderer/Shaders/` に既存 SPIR-V が存在する。
`VkRendererView/shaders/` へコピーし、`loadShader()` で読み込む。

```cpp
static std::vector<uint32_t> loadShader(const std::string& name) {
    const std::array<std::string, 3> cands = {
        "shaders/" + name,
        "../../Crystal/VkRenderer/Shaders/" + name,
        "../VkRenderer/Shaders/" + name,
    };
    for (const auto& p : cands)
        if (std::filesystem::exists(p)) return VKG::loadSPV(p);
    throw std::runtime_error("SPIR-V not found: " + name);
}
```

必要な SPIR-V ファイル:

| モード | SPIR-V |
|--------|--------|
| Point    | `point.vert.spv` / `point.frag.spv` |
| Line     | `line.vert.spv` / `line.frag.spv` |
| Triangle | `triangle.vert.spv` / `triangle.frag.spv` |
| Tex      | `tex.vert.spv` / `tex.frag.spv` |
| SkyBox   | `skybox.vert.spv` / `skybox.frag.spv` |

---

## 5. .vcxproj 設定チェックリスト

`CGLib/Space/VkSpaceView/VkSpaceView.vcxproj` をベースに調整する。

| 項目 | 設定値 |
|------|--------|
| `ConfigurationType` | `Application` |
| C++ 標準 | `stdcpp17` |
| UTF-8 | `AdditionalOptions: /utf-8` |
| インクルードパス | `..\..\CGLib\VkAppBase\imgui;..\..\Crystal\VkRenderer;..\..\CGLib\UI` |
| プロジェクト参照 | `VulkanGraphics`, `VkAppBase`, `VkRenderer` |
| 追加ライブラリ | `vulkan-1.lib; glfw3.lib` |
| GLFW targets | `glfw.targets` インポート |
| サブシステム | `Console` (Debug) / `Windows` (Release) |
| PCH | 使用しない (`NotUsing`) |

---

## 6. サンプルジオメトリ

各レンダラーの動作確認用に `VkRendererSubRenderer::onInit()` 内でハードコードした
固定データを `upload()` する。

| モード | ジオメトリ内容 |
|--------|--------------|
| Point    | 立方体頂点8点 (各色) |
| Line     | 立方体ワイヤフレーム12辺 |
| Triangle | 正三角形1枚 |
| Tex      | オフスクリーンからテクスチャ表示 (または単色テクスチャ) |
| SkyBox   | HDR キューブマップがあれば表示、なければスキップ |

---

## 7. 実装フェーズと工数

### Phase 1 — プロジェクト骨格とレンダリング (1〜2日)

1. `VkRendererView/` ディレクトリ作成
2. `VkRendererSubRenderer.h / .cpp` 実装 (Point のみ動作確認)
3. `VkRendererApp.h / .cpp` 実装 (カメラ回転・ズーム含む)
4. `main.cpp` 実装
5. `.vcxproj` 作成・ビルド確認

### Phase 2 — 全レンダラー実装 (1〜2日)

6. `VkLineRenderer` 統合・動作確認
7. `VkTriangleRenderer` 統合・動作確認
8. `VkTexRenderer` 統合・動作確認 (単色テクスチャで最低限確認)
9. `VkSkyBoxRenderer` 統合・動作確認 (キューブマップ読み込み含む)

### Phase 3 — 品質向上 (0.5〜1日)

10. ImGui メニューバーのレンダラー切替確認
11. ウィンドウリサイズ対応 (onSwapChainCreated/Destroying)
12. 各レンダラーの切替時リソース解放確認 (バリデーションレイヤーエラーがないこと)

| Phase | 内容 | 工数 |
|-------|------|------|
| Phase 1 | 骨格・基本レンダリング | 1〜2日 |
| Phase 2 | 全レンダラー統合 | 1〜2日 |
| Phase 3 | 品質向上 | 0.5〜1日 |
| **合計** | | **2.5〜5日** |

---

## 8. 注意点・落とし穴

### create()/destroy() のタイミング
`VkTexRenderer` と `VkSkyBoxRenderer` はテクスチャ設定 (`setTexture()` / `setCubeMap()`) を
`create()` 完了後に呼ぶ必要がある。`onInit()` 内で `create()` → `setTexture()` の順を守る。

### レンダラー切替時の destroy/create
モード切替では `destroy()` / `create()` は不要。全レンダラーを `onInit()` で
一括 `create()` し、`onRender()` で activeRenderer のみ呼び出せばよい。
ただし `VkTexRenderer` は `setTexture()` が未設定だと `hasTexture_` フラグで skip される。

### Extent の更新
`onInit()` と `onSwapChainCreated()` の両方で `renderer_.setExtent(getExtent())` を呼ぶ。
どちらか一方だけでは、リサイズ後にアスペクト比が歪む。

### SkyBox の深度テスト
`VkSkyBoxRenderer` 用パイプラインは深度テストを `VK_COMPARE_OP_LESS_OR_EQUAL` にする
(LESS だとスカイボックスが near clip されて消える)。

### GLM ヘッダー
`glm::lookAt` / `glm::perspective` は `<glm/gtc/matrix_transform.hpp>` が必要。
`<glm/glm.hpp>` だけでは未定義エラーになる。

### クリーンアップ順序
`onCleanup()` 内で `renderer_.onCleanup(getDevice())` または
`VKG::VkAppBase::onCleanup()` (登録ベース) → ImGui cleanup の順を守る。

---

## 9. 既存コードの再利用まとめ

| 既存ファイル / クラス | 再利用先 | 方法 |
|----------------------|---------|------|
| `RendererView/main.cpp` の `Renderer` | `VkRendererSubRenderer` | 構造を移植、GL呼出をVK呼出に変換 |
| `RendererView/main.cpp` の `RendererMenu` | `VkRendererApp::onImGui()` | ImGui メニューバーへ書き直し |
| `VkSpaceView/VkSpaceApp.cpp` | `VkRendererApp.cpp` | カメラコールバックパターン流用 |
| `Crystal/VkRenderer/Shaders/*.spv` | `shaders/` | コピー |
| `VkAppBase/VkRendererBase` | (参考) | オフスクリーン2パスが必要な場合に継承候補 |
