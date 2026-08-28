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
