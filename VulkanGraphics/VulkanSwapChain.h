#pragma once

#include <vulkan/vulkan.h>

#include <functional>
#include <optional>
#include <vector>

namespace Phantom::VKG {

class VulkanContext;

// Test-only accessor: grants VulkanGraphicsTest access to the private
// findSupportedFormat() so its empty-candidate-list failure path can be
// exercised without widening this class's public API.
struct VulkanSwapChainTestAccess;

/// @brief Manages the swap chain, image views, depth buffer, and framebuffers.
///
/// All window-system dependency is abstracted through a FramebufferSizeFunc callback,
/// keeping this class free of any GLFW or platform headers.
///
/// Construction follows a two-phase pattern so that the caller can create the render pass
/// (which requires the color format) between the two phases:
/// @code
///   swapChain.createSwapChainAndViews();          // Phase 1 - resolves format and extent
///   VkFormat depth = swapChain.findDepthFormat();
///   renderPass.create(ctx, swapChain.getImageFormat(), depth);
///   swapChain.createDepthAndFramebuffers(rp);     // Phase 2 – depth resources + framebuffers
/// @endcode
class VulkanSwapChain {
public:
    /// @brief Callback type used to query the current framebuffer size.
    ///
    /// The implementation should write the drawable pixel dimensions into @p w and @p h.
    /// Typically wraps glfwGetFramebufferSize().
    using FramebufferSizeFunc = std::function<void(int& w, int& h)>;

    VulkanSwapChain() = default;
    VulkanSwapChain(const VulkanSwapChain&) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
    ~VulkanSwapChain() = default;

    /// @brief Stores non-owning references required for swap-chain operations.
    ///
    /// Must be called once before any creation method.
    /// @param ctx                Non-owning pointer to the logical device context.
    /// @param surface            Non-owning surface handle.
    /// @param getFramebufferSize Callback that returns the current drawable size in pixels.
    void init(VulkanContext* ctx, VkSurfaceKHR surface,
              FramebufferSizeFunc getFramebufferSize);

    /// @brief Phase 1 - creates the VkSwapchainKHR and per-image VkImageViews.
    ///
    /// After this call, getImageFormat() and getExtent() return valid values,
    /// which are needed before the render pass can be created.
    /// @return false if swap-chain or image-view creation fails.
    bool createSwapChainAndViews();

    /// @brief Phase 2 - creates depth resources and framebuffers.
    ///
    /// Must be called after the render pass has been created.
    /// @param renderPass Render pass the framebuffers will be compatible with.
    /// @return false if any resource fails to create.
    bool createDepthAndFramebuffers(VkRenderPass renderPass);

    /// @brief Convenience method that runs both phases in sequence.
    ///
    /// Use only when the render pass is already available.
    /// @param renderPass Render pass the framebuffers will be compatible with.
    /// @return false if either phase fails.
    bool create(VkRenderPass renderPass);

    /// @brief Destroys and recreates the swap chain (e.g. after a window resize).
    ///
    /// Blocks while the framebuffer size is zero (minimized window).
    /// @param renderPass  Render pass used to recreate framebuffers.
    /// @param waitEvents  Optional callback invoked while the window is minimized,
    ///                    e.g. wrapping glfwWaitEvents(). Pass nullptr to spin-wait instead.
    void recreate(VkRenderPass renderPass,
                  std::function<void()> waitEvents = nullptr);

    /// @brief Destroys all swap-chain resources.
    void destroy();

    /// @brief Set the MSAA sample count before calling createDepthAndFramebuffers().
    ///
    /// Must be called between createSwapChainAndViews() and createDepthAndFramebuffers().
    /// When samples > VK_SAMPLE_COUNT_1_BIT, a multisampled color image is allocated
    /// automatically and used as attachment 0 in the framebuffer (the resolve target is
    /// the swap-chain image at attachment 2).
    ///
    /// @param samples Sample count.  Must match the sample count of the render pass.
    void setMsaaSamples(VkSampleCountFlagBits samples) { msaaSamples_ = samples; }

    /// @name Accessors
    /// @{
    VkSwapchainKHR                    getSwapChain()    const { return swapChain_; }    ///< Returns the swap chain handle.
    VkFormat                          getImageFormat()  const { return imageFormat_; }  ///< Returns the color image format.
    VkExtent2D                        getExtent()       const { return extent_; }       ///< Returns the current swap-chain extent in pixels.
    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers_; } ///< Returns the per-image framebuffer handles.
    uint32_t                          getImageCount()   const { return (uint32_t)images_.size(); } ///< Returns the number of swap-chain images.
    const std::vector<VkImage>&       getImages()       const { return images_; }       ///< Returns the swap-chain image handles.
    VkSampleCountFlagBits             getMsaaSamples()  const { return msaaSamples_; }  ///< Returns the MSAA sample count (1 = no MSAA).
    /// @}

    /// @brief Selects the best available depth format supported by the physical device.
    ///
    /// Queries D32_SFLOAT, D32_SFLOAT_S8_UINT, and D24_UNORM_S8_UINT in preference order.
    /// @return A supported depth VkFormat, or std::nullopt if none exists.
    std::optional<VkFormat> findDepthFormat() const;

private:
    VulkanContext*      ctx_                = nullptr;
    VkSurfaceKHR        surface_            = VK_NULL_HANDLE;
    FramebufferSizeFunc getFramebufferSize_;

    VkSwapchainKHR             swapChain_   = VK_NULL_HANDLE;
    std::vector<VkImage>       images_;
    VkFormat                   imageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D                 extent_{};
    std::vector<VkImageView>   imageViews_;
    std::vector<VkFramebuffer> framebuffers_;

    // Depth attachment
    VkImage        depthImage_       = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView    depthImageView_   = VK_NULL_HANDLE;

    // MSAA color attachment (used only when msaaSamples_ > 1)
    VkSampleCountFlagBits msaaSamples_      = VK_SAMPLE_COUNT_1_BIT;
    VkImage               msaaColorImage_   = VK_NULL_HANDLE;
    VkDeviceMemory        msaaColorMemory_  = VK_NULL_HANDLE;
    VkImageView           msaaColorView_    = VK_NULL_HANDLE;

    bool createSwapChain();
    bool createImageViews();
    bool createMsaaColorResources();
    bool createDepthResources();
    bool createFramebuffers(VkRenderPass renderPass);
    void destroySwapChainObjects();

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR   choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D         chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const;

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;
    bool        createImage(uint32_t w, uint32_t h, VkFormat format,
                            VkImageTiling tiling, VkImageUsageFlags usage,
                            VkMemoryPropertyFlags props,
                            VkImage& image, VkDeviceMemory& memory,
                            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) const;
    std::optional<VkFormat> findSupportedFormat(const std::vector<VkFormat>& candidates,
                                    VkImageTiling tiling, VkFormatFeatureFlags features) const;

    friend struct VulkanSwapChainTestAccess;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
