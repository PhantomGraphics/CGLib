#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Phantom::VKG {

class VulkanContext;

/// @brief オフスクリーンレンダリング用の補助クラス（OpenGL FBO 相当）。
///
/// カラーアタッチメント（SAMPLED 可能）とデプスアタッチメントを持つ
/// レンダーパス + フレームバッファを所有する。
/// レンダリング結果はカラーイメージビュー経由でサンプラーに渡せる。
///
/// 用途例:
///   - ポストプロセス（ブルーム、被写界深度）
///   - ID ピッキングパス
///   - シャドウマップ
///
/// 使い方:
/// @code
///   VulkanOffscreen offscreen;
///   offscreen.create(ctx, 1280, 720,
///                    VK_FORMAT_R8G8B8A8_UNORM,
///                    swapChain.findDepthFormat());
///
///   // コマンドバッファ記録中:
///   offscreen.beginRenderPass(cmd);
///   // ... draw calls ...
///   offscreen.endRenderPass(cmd);
///
///   // オフスクリーン結果をサンプリング:
///   VkImageView colorView = offscreen.getColorImageView();
/// @endcode
class VulkanOffscreen {
public:
    VulkanOffscreen() = default;
    VulkanOffscreen(const VulkanOffscreen&) = delete;
    VulkanOffscreen& operator=(const VulkanOffscreen&) = delete;
    ~VulkanOffscreen() = default;

    /// @brief オフスクリーンリソースを生成する。
    ///
    /// @param ctx         論理デバイスコンテキスト。
    /// @param width       レンダーターゲット幅（ピクセル）。
    /// @param height      レンダーターゲット高さ（ピクセル）。
    /// @param colorFormat カラーアタッチメントフォーマット。
    /// @param depthFormat デプスアタッチメントフォーマット。
    /// @return リソース生成に失敗した場合は false。
    bool create(const VulkanContext& ctx,
                uint32_t width, uint32_t height,
                VkFormat colorFormat,
                VkFormat depthFormat);

    /// @brief すべての Vulkan リソースを解放する。
    void destroy(const VulkanContext& ctx);

    /// @brief カラー/デプスイメージとフレームバッファを新しいサイズで再生成する。
    ///
    /// **`getRenderPass()` は変わらない**（`create()`と違い、レンダーパス自体は破棄・再生成
    /// しない — フォーマット/アタッチメント記述はサイズに依存しないため）。このオフスクリーンの
    /// レンダーパスに対して作成済みの `VulkanPipeline` は本メソッド呼び出し後も引き続き有効
    /// （このプロジェクトの全パイプラインは viewport/scissor を dynamic state にしており
    /// （`VulkanPipeline::create()`）、かつ `beginRenderPass()` が毎回新しい `extent_` で
    /// 両方を再設定するため、パイプライン再構築は一切不要——ウィンドウリサイズのたびに多数の
    /// サブレンダラーのパイプラインを作り直す必要がある、という制約はここでは生じない）。
    /// ただし `getColorImageView()`/`getDepthImageView()` が返すハンドルは新しいイメージの
    /// ものに変わるため、これらを（コピー済みの値として）サンプラーと一緒にディスクリプタへ
    /// 書き込んでいる呼び出し元は、本メソッドの後で書き込みをやり直す必要がある。
    ///
    /// `isValid()` が false（`create()`未実行、または前回の `resize()` が失敗して破棄済み）
    /// の場合は何もせず false を返す。生成に失敗した場合は `destroy()` 相当まで巻き戻し
    /// （`renderPass_` を含め全ハンドルを未設定に戻し）false を返す——中途半端な状態を残さない。
    ///
    /// @param width  新しい幅（ピクセル）。0 の場合は何もせず false を返す（呼び出し元は
    ///               最小化ウィンドウ等、有効なサイズが得られるまで呼び出しを控えること）。
    /// @param height 新しい高さ（ピクセル）。
    bool resize(const VulkanContext& ctx, uint32_t width, uint32_t height);

    /// @brief レンダーパスを開始し、カラー/デプスをクリアする。
    ///
    /// @param cmd        記録対象のコマンドバッファ。
    /// @param clearColor カラークリア値 (R, G, B, A)。
    /// @param clearDepth デプスクリア値（通常 1.0f）。
    void beginRenderPass(VkCommandBuffer cmd,
                         const std::array<float, 4>& clearColor = {0.f, 0.f, 0.f, 1.f},
                         float clearDepth = 1.0f) const;

    /// @brief レンダーパスを終了する。
    void endRenderPass(VkCommandBuffer cmd) const;

    /// @name アクセサ
    /// @{
    VkRenderPass  getRenderPass()     const { return renderPass_; }     ///< レンダーパスハンドル。
    VkFramebuffer getFramebuffer()    const { return framebuffer_; }    ///< フレームバッファハンドル。
    VkImage       getColorImage()     const { return colorImage_; }     ///< カラーイメージハンドル（転送/同期用途）。
    VkImageView   getColorImageView() const { return colorView_; }      ///< カラーイメージビュー（サンプリング用）。
    VkImageView   getDepthImageView() const { return depthView_; }      ///< デプスイメージビュー（サンプリング用。シャドウマップ等）。
    VkExtent2D    getExtent()         const { return extent_; }         ///< レンダーターゲットサイズ。
    VkFormat      getColorFormat()    const { return colorFormat_; }    ///< カラーフォーマット。
    /// @}

    bool isValid() const { return renderPass_ != VK_NULL_HANDLE; }

private:
    VkExtent2D extent_{};
    VkFormat   colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat   depthFormat_ = VK_FORMAT_UNDEFINED; // cached so resize() can rebuild the depth image without it being re-passed

    // Shared by create()/resize(): (re)builds colorImage_/View_ and depthImage_/View_ at
    // width x height using colorFormat_/depthFormat_. Caller must ensure any previous images
    // were already destroyed. Leaves all four handles VK_NULL_HANDLE and returns false on
    // failure (matching create()'s existing per-step cleanup below).
    bool createImages(const VulkanContext& ctx, uint32_t width, uint32_t height);

    // Shared by create()/resize(): builds framebuffer_ from the current colorView_/depthView_
    // against renderPass_ (which must already exist) at width x height.
    bool createFramebuffer(VkDevice device, uint32_t width, uint32_t height);

    // カラーアタッチメント
    VkImage        colorImage_  = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory_ = VK_NULL_HANDLE;
    VkImageView    colorView_   = VK_NULL_HANDLE;

    // デプスアタッチメント
    VkImage        depthImage_  = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView    depthView_   = VK_NULL_HANDLE;

    VkRenderPass  renderPass_  = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
