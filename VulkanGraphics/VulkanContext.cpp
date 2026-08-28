#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <cstring>
#include <iostream>
#include <set>

namespace Phantom::VKG {

static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ============================================================
//  Debug messenger helper
// ============================================================

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pInfo,
    const VkAllocationCallbacks* pAlloc,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return fn ? fn(instance, pInfo, pAlloc, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAlloc)
{
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn) fn(instance, messenger, pAlloc);
}

// ============================================================
//  VulkanContext
// ============================================================

VulkanContext::~VulkanContext() {
    destroy();
}

bool VulkanContext::createInstance(const std::string& appName,
                                    const std::vector<const char*>& requiredExtensions,
                                    bool enableValidation)
{
    validation_ = enableValidation;

    if (validation_ && !checkValidationLayerSupport()) {
        std::fprintf(stderr, "[VKG] Validation layers requested but not available\n");
        return false;
    }

    VkApplicationInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName   = appName.c_str();
    ai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.pEngineName        = "VKG";
    ai.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    ai.apiVersion         = VK_API_VERSION_1_3;

    // Append debug extension to caller-supplied extension list if validation is enabled.
    std::vector<const char*> exts = requiredExtensions;
    if (validation_) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &ai;
    ci.enabledExtensionCount   = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();

    if (validation_) {
        ci.enabledLayerCount   = (uint32_t)VALIDATION_LAYERS.size();
        ci.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VKG_CHECK(vkCreateInstance(&ci, nullptr, &instance_),
              "Failed to create Vulkan instance", false);

    if (validation_) setupDebugMessenger();
    return true;
}

bool VulkanContext::initDevice(VkSurfaceKHR surface) {
    surface_ = surface;
    if (!pickPhysicalDevice()) return false;
    return createLogicalDevice();
}

void VulkanContext::destroy() {
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = nullptr;
    }
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (validation_ && debugMessenger_) {
        DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
    CreateDebugUtilsMessengerEXT(instance_, &ci, nullptr, &debugMessenger_);
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        std::fprintf(stderr, "[VKG] No Vulkan-capable GPU found\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto& d : devices) {
        if (isDeviceSuitable(d)) {
            physicalDevice_ = d;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[VKG] Failed to find a suitable GPU\n");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    std::cout << "[VKG] GPU: " << props.deviceName << "\n";
    return true;
}

bool VulkanContext::createLogicalDevice() {
    auto indices = findQueueFamilies(physicalDevice_, surface_);
    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    for (auto f : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = f;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueCIs.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.largePoints       = VK_TRUE; // required for gl_PointSize
    features.fillModeNonSolid  = VK_TRUE; // required for VK_POLYGON_MODE_LINE (wireframe)

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = (uint32_t)queueCIs.size();
    ci.pQueueCreateInfos       = queueCIs.data();
    ci.pEnabledFeatures        = &features;
    ci.enabledExtensionCount   = (uint32_t)DEVICE_EXTENSIONS.size();
    ci.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    // Device Layers are deprecated since Vulkan 1.0; only Instance Layers
    // (see createInstance()) are used for validation.

    VKG_CHECK(vkCreateDevice(physicalDevice_, &ci, nullptr, &device_),
              "Failed to create logical device", false);

    graphicsQueueFamily_ = indices.graphicsFamily.value();
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(),  0, &presentQueue_);

    // VMA allocator initialization
    VmaVulkanFunctions vmaFns{};
    vmaFns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCI{};
    allocatorCI.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCI.physicalDevice   = physicalDevice_;
    allocatorCI.device           = device_;
    allocatorCI.instance         = instance_;
    allocatorCI.pVulkanFunctions = &vmaFns;

    VKG_CHECK(vmaCreateAllocator(&allocatorCI, &allocator_),
              "Failed to create VMA allocator", false);
    return true;
}

// ============================================================
//  Query helpers
// ============================================================

VkSampleCountFlagBits VulkanContext::getMaxUsableSampleCount() const {
    if (!physicalDevice_) return VK_SAMPLE_COUNT_1_BIT;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);

    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(
    VkPhysicalDevice dev, VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) indices.presentFamily = i;

        if (indices.isComplete()) break;
    }
    return indices;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(
    VkPhysicalDevice dev, VkSurfaceKHR surface) const
{
    SwapChainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
    if (fmtCount) {
        d.formats.resize(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, d.formats.data());
    }

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, nullptr);
    if (pmCount) {
        d.presentModes.resize(pmCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, d.presentModes.data());
    }
    return d;
}

std::optional<uint32_t> VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    std::fprintf(stderr, "[VKG] Failed to find suitable memory type\n");
    return std::nullopt;
}

// ============================================================
//  Device suitability check
// ============================================================

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice dev) const {
    if (!findQueueFamilies(dev, surface_).isComplete()) return false;
    if (!checkDeviceExtensionSupport(dev)) return false;
    auto sup = querySwapChainSupport(dev, surface_);
    if (sup.formats.empty() || sup.presentModes.empty()) return false;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(dev, &features);
    return features.largePoints == VK_TRUE;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice dev) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());
    std::set<std::string> required(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

bool VulkanContext::checkValidationLayerSupport() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (auto* name : VALIDATION_LAYERS) {
        bool found = false;
        for (auto& lp : layers)
            if (strcmp(name, lp.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "[VKG Validation] " << data->pMessage << "\n";
    return VK_FALSE;
}

} // namespace VKG
