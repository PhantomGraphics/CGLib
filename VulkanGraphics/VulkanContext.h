#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <vector>

// Forward-declare VMA handle so callers do not need to include vk_mem_alloc.h.
// VmaAllocator == struct VmaAllocator_T* (defined by VK_DEFINE_HANDLE in VMA).
struct VmaAllocator_T;

namespace Phantom::VKG {

/// @brief Indices of queue families required by the application.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; ///< Index of the graphics queue family.
    std::optional<uint32_t> presentFamily;  ///< Index of the presentation queue family.

    /// @brief Returns true when both required queue families have been found.
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/// @brief Surface capabilities and supported formats/present-modes for swap-chain creation.
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities{}; ///< Surface capabilities reported by the driver.
    std::vector<VkSurfaceFormatKHR> formats;        ///< Supported surface formats.
    std::vector<VkPresentModeKHR>   presentModes;   ///< Supported presentation modes.
};

/// @brief Owns and manages the Vulkan instance, physical device, logical device, and queues.
///
/// Creation is split into two phases so that the caller can create a window surface
/// between them:
///   1. createInstance() - creates the VkInstance (and validation messenger if requested).
///   2. initDevice()     - selects a physical device and creates the logical device.
///
/// The surface passed to initDevice() is used only for queue-family selection;
/// ownership remains with the caller.
class VulkanContext {
public:
    VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    ~VulkanContext();

    /// @brief Phase 1 - creates the Vulkan instance.
    ///
    /// @param appName            Human-readable application name embedded in VkApplicationInfo.
    /// @param requiredExtensions Instance extensions required by the window system
    ///                           (e.g. the result of glfwGetRequiredInstanceExtensions).
    ///                           VK_EXT_debug_utils is appended automatically when @p enableValidation is true.
    /// @param enableValidation   Enables VK_LAYER_KHRONOS_validation and a debug messenger.
    /// @return false if the instance cannot be created or validation is
    ///         requested but unavailable.
    bool createInstance(const std::string& appName,
                        const std::vector<const char*>& requiredExtensions,
                        bool enableValidation = true);

    /// @brief Phase 2 - selects a physical device and creates the logical device.
    ///
    /// @param surface A valid VkSurfaceKHR used to select a present-capable queue family.
    ///                Ownership stays with the caller; the context only keeps a non-owning reference.
    /// @return false if no suitable GPU is found or device creation fails.
    bool initDevice(VkSurfaceKHR surface);

    /// @brief Destroys all owned Vulkan objects in reverse creation order.
    void destroy();

    /// @name Accessors
    /// @{
    VkInstance       getInstance()             const { return instance_; }       ///< Returns the Vulkan instance.
    VkPhysicalDevice getPhysicalDevice()       const { return physicalDevice_; } ///< Returns the selected physical device.
    VkDevice         getDevice()               const { return device_; }         ///< Returns the logical device.
    VkQueue          getGraphicsQueue()        const { return graphicsQueue_; }  ///< Returns the graphics queue.
    VkQueue          getPresentQueue()         const { return presentQueue_; }   ///< Returns the presentation queue.
    uint32_t         getGraphicsQueueFamily()  const { return graphicsQueueFamily_; } ///< Returns the graphics queue family index.
    bool             isValidationEnabled()     const { return validation_; }     ///< Returns true when validation layers are active.

    /// @brief Returns the VMA allocator.  Use this to create/destroy VMA buffers and images.
    VmaAllocator_T*  getAllocator()            const { return allocator_; }

    /// @brief Returns the maximum sample count supported by both color and depth attachments.
    ///
    /// Use this to cap the MSAA level at what the physical device supports.
    /// Returns VK_SAMPLE_COUNT_1_BIT if the device is not yet initialized.
    VkSampleCountFlagBits getMaxUsableSampleCount() const;
    /// @}

    /// @brief Finds graphics and present queue family indices for the given device and surface.
    /// @param dev     Physical device to query.
    /// @param surface Surface used to test present support.
    /// @return QueueFamilyIndices with graphics and/or present indices set where found.
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) const;

    /// @brief Queries swap-chain support details (capabilities, formats, present modes).
    /// @param dev     Physical device to query.
    /// @param surface Surface to query against.
    /// @return Populated SwapChainSupportDetails.
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface) const;

    /// @brief Finds a memory type index satisfying the given filter and property flags.
    /// @param typeFilter   Bitmask of acceptable memory type indices (from VkMemoryRequirements).
    /// @param props        Required VkMemoryPropertyFlags (e.g. DEVICE_LOCAL, HOST_VISIBLE).
    /// @return Index of a suitable memory type, or std::nullopt if none exists.
    std::optional<uint32_t> findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;

private:
    VkInstance               instance_            = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_       = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice_       = VK_NULL_HANDLE;
    VkDevice                 device_               = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue_        = VK_NULL_HANDLE;
    VkQueue                  presentQueue_         = VK_NULL_HANDLE;
    uint32_t                 graphicsQueueFamily_  = 0;
    bool                     validation_           = false;
    VmaAllocator_T*          allocator_            = nullptr;

    // Non-owning reference used only during initDevice().
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    void setupDebugMessenger();
    bool pickPhysicalDevice();
    bool createLogicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) const;
    bool checkValidationLayerSupport() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
