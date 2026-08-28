#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace Phantom::VKG {

class VulkanContext;

/// @brief Manages a VkCommandPool and provides helpers for single-time command submission.
///
/// Typical usage:
/// @code
///   VulkanCommandPool pool;
///   pool.init(&ctx, surface);
///
///   // Allocate per-frame command buffers
///   auto cmds = pool.allocateCommandBuffers(2);
///
///   // One-shot GPU upload
///   auto cmd = pool.beginSingleTimeCommands();
///   // ... record transfer commands ...
///   pool.endSingleTimeCommands(cmd);   // submits and waits
/// @endcode
class VulkanCommandPool {
public:
    VulkanCommandPool() = default;
    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
    ~VulkanCommandPool() = default;

    /// @brief Creates the command pool on the graphics queue family.
    /// @param ctx     Non-owning pointer to the logical device context.
    /// @param surface Surface used to resolve the graphics queue family index.
    /// @return false if pool creation fails.
    bool init(VulkanContext* ctx, VkSurfaceKHR surface);

    /// @brief Destroys the command pool (implicitly frees all allocated command buffers).
    void destroy();

    /// @brief Returns the underlying VkCommandPool handle.
    VkCommandPool get() const { return pool_; }

    /// @brief Allocates primary command buffers from the pool.
    /// @param count Number of command buffers to allocate.
    /// @return Vector of newly allocated VkCommandBuffer handles, or an empty vector on failure.
    std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t count) const;

    /// @brief Frees the given command buffers back to the pool.
    /// @param bufs Command buffers to free. The vector is cleared on return.
    void freeCommandBuffers(std::vector<VkCommandBuffer>& bufs) const;

    /// @brief Begins a one-shot command buffer for a transient GPU operation.
    ///
    /// Must be paired with a call to endSingleTimeCommands().
    /// @return A newly allocated, already-begun VkCommandBuffer.
    VkCommandBuffer beginSingleTimeCommands() const;

    /// @brief Ends, submits, and waits for the one-shot command buffer to complete.
    ///
    /// Blocks until the graphics queue is idle, then frees the command buffer.
    /// @param cmd Command buffer returned by beginSingleTimeCommands().
    void endSingleTimeCommands(VkCommandBuffer cmd) const;

private:
    VulkanContext* ctx_  = nullptr;
    VkCommandPool  pool_ = VK_NULL_HANDLE;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
