#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Phantom::VKG {

class VulkanContext;
class VulkanCommandPool;

/// @brief Common interface for Vulkan renderers (corresponds to OpenGL IRenderer).
///
/// Derived classes initialize their pipeline and buffers in create() and record
/// draw commands into a command buffer in render().
///
/// Lifecycle:
/// @code
///   renderer.create(ctx, pool, renderPass, framesInFlight);
///   // per-frame:
///   renderer.upload(ctx, pool, buffer);   // transfer data to GPU
///   renderer.render(cmd, frameIndex);     // record draw commands
///   // on shutdown:
///   renderer.destroy(ctx.getDevice());
/// @endcode
///
/// On swap chain recreation, call destroy() followed by create() again,
/// especially when the RenderPass handle changes.
class IVkRenderer {
public:
    IVkRenderer() = default;
    IVkRenderer(const IVkRenderer&) = delete;
    IVkRenderer& operator=(const IVkRenderer&) = delete;
    virtual ~IVkRenderer() = default;

    /// @brief Create the pipeline, descriptor sets, and synchronization objects.
    ///
    /// @param ctx             Logical device context.
    /// @param pool            Command pool used for initial vertex buffer uploads.
    /// @param renderPass      Render pass this renderer will be used with.
    /// @param framesInFlight  Number of frames in flight (determines UBO count).
    virtual void create(const VulkanContext& ctx,
                        const VulkanCommandPool& pool,
                        VkRenderPass renderPass,
                        uint32_t framesInFlight = 2) = 0;

    /// @brief Destroy all Vulkan resources owned by this renderer.
    /// @param device Logical device.
    virtual void destroy(VkDevice device) = 0;

    /// @brief Record draw commands into the given command buffer.
    ///
    /// Must be called inside an active render pass.
    /// @param cmd        Command buffer to record into.
    /// @param frameIndex Current in-flight frame index (0..framesInFlight-1).
    virtual void render(VkCommandBuffer cmd, uint32_t frameIndex) = 0;

    /// @brief Returns true when the renderer is fully initialized and ready to render.
    virtual bool isValid() const = 0;
};

} // namespace Phantom::VKG
