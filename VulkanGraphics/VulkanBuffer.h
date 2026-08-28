#pragma once

#include <vulkan/vulkan.h>

// Forward-declare VMA handles so callers do not need to include vk_mem_alloc.h.
struct VmaAllocator_T;
struct VmaAllocation_T;

namespace Phantom::VKG {

class VulkanContext;
class VulkanCommandPool;

/// @brief Owns a VkBuffer and its backing VMA allocation.
///
/// Supports two common usage patterns:
///
/// **Device-local (fast GPU memory)** - created via create() with an optional
/// staging upload:
/// @code
///   VulkanBuffer vb;
///   vb.create(ctx, pool, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data());
/// @endcode
///
/// **Host-visible (persistently mapped)** - created via createMapped(), suited for
/// data that changes every frame (e.g. uniform buffers):
/// @code
///   VulkanBuffer ubo;
///   ubo.createMapped(ctx, sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
///   ubo.write(&data, sizeof(data));   // called each frame
/// @endcode
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    ~VulkanBuffer() = default;

    VulkanBuffer(VulkanBuffer&& o) noexcept
        : buffer_(o.buffer_), alloc_(o.alloc_), mapped_(o.mapped_),
          size_(o.size_), allocator_(o.allocator_)
    {
        o.buffer_    = VK_NULL_HANDLE;
        o.alloc_     = nullptr;
        o.mapped_    = nullptr;
        o.size_      = 0;
        o.allocator_ = nullptr;
    }

    VulkanBuffer& operator=(VulkanBuffer&& o) noexcept {
        if (this != &o) {
            destroy();
            buffer_    = o.buffer_;    o.buffer_    = VK_NULL_HANDLE;
            alloc_     = o.alloc_;     o.alloc_     = nullptr;
            mapped_    = o.mapped_;    o.mapped_    = nullptr;
            size_      = o.size_;      o.size_      = 0;
            allocator_ = o.allocator_; o.allocator_ = nullptr;
        }
        return *this;
    }

    /// @brief Creates a device-local buffer, optionally uploading initial data via a staging buffer.
    ///
    /// The buffer is allocated with VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE.
    /// When @p initialData is non-null, a temporary host-visible staging buffer is created,
    /// the data is copied into it, and a one-shot transfer command copies it to the device.
    ///
    /// @param ctx         Logical device context (owns the VMA allocator).
    /// @param pool        Command pool used for the optional staging transfer.
    /// @param size        Size in bytes of the buffer to allocate.
    /// @param usage       Buffer usage flags (e.g. VK_BUFFER_USAGE_VERTEX_BUFFER_BIT).
    ///                    VK_BUFFER_USAGE_TRANSFER_DST_BIT is added automatically when data is provided.
    /// @param initialData Pointer to the data to upload, or nullptr to skip the upload.
    /// @return false if buffer or staging-buffer creation fails; the buffer stays invalid.
    bool create(const VulkanContext& ctx, const VulkanCommandPool& pool,
                VkDeviceSize size, VkBufferUsageFlags usage,
                const void* initialData = nullptr);

    /// @brief Creates a persistently mapped, host-visible buffer (e.g. for uniform buffers).
    ///
    /// The buffer is allocated with VMA_ALLOCATION_CREATE_MAPPED_BIT and remains mapped
    /// until destroy() is called.  Use write() to update its contents each frame.
    ///
    /// @param ctx   Logical device context.
    /// @param size  Size in bytes of the buffer to allocate.
    /// @param usage Buffer usage flags (e.g. VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).
    /// @return false if buffer creation fails; the buffer stays invalid.
    bool createMapped(const VulkanContext& ctx,
                      VkDeviceSize size, VkBufferUsageFlags usage);

    /// @brief Uploads data to a device-local buffer via a temporary staging buffer.
    ///
    /// Blocks until the transfer is complete.
    /// @param ctx  Logical device context.
    /// @param pool Command pool used for the transfer command.
    /// @param data Pointer to the source data.
    /// @param size Number of bytes to copy.
    /// @return false if staging-buffer creation fails.
    bool upload(const VulkanContext& ctx, const VulkanCommandPool& pool,
                const void* data, VkDeviceSize size);

    /// @brief Copies data directly into a persistently-mapped host-visible buffer.
    ///
    /// The buffer must have been created with createMapped().
    /// @param data Pointer to the source data.
    /// @param size Number of bytes to copy.
    void write(const void* data, VkDeviceSize size);

    /// @brief Destroys the buffer and frees the associated VMA allocation.
    /// @param device Logical device (kept for API compatibility; VMA allocator is used internally).
    void destroy(VkDevice device = VK_NULL_HANDLE);

    /// @name Accessors
    /// @{
    VkBuffer        get()            const { return buffer_; }        ///< Returns the VkBuffer handle.
    VkBuffer        getBuffer()      const { return buffer_; }        ///< Alias for get().
    VmaAllocation_T* getVmaAllocation() const { return alloc_; }      ///< Returns the VMA allocation handle.
    void*           getMapped()      const { return mapped_; }        ///< Returns the persistent host pointer, or nullptr for device-local buffers.
    VkDeviceSize    getSize()        const { return size_; }          ///< Returns the allocated size in bytes.
    bool            isValid()        const { return buffer_ != VK_NULL_HANDLE; } ///< Returns true when the buffer has been created.
    /// @}

    /// @brief Records and submits a one-shot copy command from @p src to @p dst.
    ///
    /// Blocks until the copy completes.
    /// @param ctx  Logical device context.
    /// @param pool Command pool used for the transfer command.
    /// @param src  Source buffer.
    /// @param dst  Destination buffer.
    /// @param size Number of bytes to copy.
    static void copyBuffer(const VulkanContext& ctx, const VulkanCommandPool& pool,
                           VkBuffer src, VkBuffer dst, VkDeviceSize size);

private:
    VkBuffer         buffer_    = VK_NULL_HANDLE;
    VmaAllocation_T* alloc_     = nullptr;       ///< VMA allocation backing this buffer.
    void*            mapped_    = nullptr;
    VkDeviceSize     size_      = 0;
    VmaAllocator_T*  allocator_ = nullptr;       ///< Non-owning reference to the VMA allocator.
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
