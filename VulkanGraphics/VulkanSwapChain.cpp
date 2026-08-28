#include "VulkanSwapChain.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

#include <algorithm>
#include <limits>

namespace Phantom::VKG {

void VulkanSwapChain::init(VulkanContext* ctx, VkSurfaceKHR surface,
                            FramebufferSizeFunc getFramebufferSize)
{
    ctx_                = ctx;
    surface_            = surface;
    getFramebufferSize_ = std::move(getFramebufferSize);
}

// ============================================================
//  Two-phase construction
// ============================================================

bool VulkanSwapChain::createSwapChainAndViews() {
    if (!createSwapChain()) return false;
    return createImageViews();
}

bool VulkanSwapChain::createDepthAndFramebuffers(VkRenderPass renderPass) {
    if (!createMsaaColorResources()) return false;
    if (!createDepthResources()) return false;
    return createFramebuffers(renderPass);
}

bool VulkanSwapChain::create(VkRenderPass renderPass) {
    if (!createSwapChainAndViews()) return false;
    return createDepthAndFramebuffers(renderPass);
}

void VulkanSwapChain::recreate(VkRenderPass renderPass,
                                std::function<void()> waitEvents)
{
    // Wait while the window is minimized.
    int w = 0, h = 0;
    while (w == 0 || h == 0) {
        getFramebufferSize_(w, h);
        if ((w == 0 || h == 0) && waitEvents) waitEvents();
    }
    vkDeviceWaitIdle(ctx_->getDevice());
    destroySwapChainObjects();
    if (!create(renderPass))
        std::fprintf(stderr, "[VKG] VulkanSwapChain::recreate: failed to recreate swap chain\n");
}

void VulkanSwapChain::destroy() {
    destroySwapChainObjects();
}

// ============================================================
//  Internal creation
// ============================================================

bool VulkanSwapChain::createSwapChain() {
    auto support = ctx_->querySwapChainSupport(ctx_->getPhysicalDevice(), surface_);
    auto sf      = chooseSurfaceFormat(support.formats);
    auto pm      = choosePresentMode(support.presentModes);
    auto extent  = chooseExtent(support.capabilities);

    uint32_t imgCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0)
        imgCount = std::min(imgCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface_;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = sf.format;
    ci.imageColorSpace  = sf.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    auto indices = ctx_->findQueueFamilies(ctx_->getPhysicalDevice(), surface_);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    if (indices.graphicsFamily != indices.presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform   = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = pm;
    ci.clipped        = VK_TRUE;

    VKG_CHECK(vkCreateSwapchainKHR(ctx_->getDevice(), &ci, nullptr, &swapChain_),
              "Failed to create swap chain", false);

    vkGetSwapchainImagesKHR(ctx_->getDevice(), swapChain_, &imgCount, nullptr);
    images_.resize(imgCount);
    vkGetSwapchainImagesKHR(ctx_->getDevice(), swapChain_, &imgCount, images_.data());
    imageFormat_ = sf.format;
    extent_      = extent;
    return true;
}

bool VulkanSwapChain::createImageViews() {
    imageViews_.resize(images_.size());
    for (size_t i = 0; i < images_.size(); ++i) {
        imageViews_[i] = createImageView(images_[i], imageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
        if (imageViews_[i] == VK_NULL_HANDLE) return false;
    }
    return true;
}

bool VulkanSwapChain::createMsaaColorResources() {
    if (msaaSamples_ == VK_SAMPLE_COUNT_1_BIT) return true;

    if (!createImage(extent_.width, extent_.height, imageFormat_,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     msaaColorImage_, msaaColorMemory_,
                     msaaSamples_))
        return false;

    msaaColorView_ = createImageView(msaaColorImage_, imageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    return msaaColorView_ != VK_NULL_HANDLE;
}

bool VulkanSwapChain::createDepthResources() {
    auto fmt = findDepthFormat();
    if (!fmt) return false;

    if (!createImage(extent_.width, extent_.height, *fmt,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     depthImage_, depthImageMemory_,
                     msaaSamples_))
        return false;

    depthImageView_ = createImageView(depthImage_, *fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
    return depthImageView_ != VK_NULL_HANDLE;
}

bool VulkanSwapChain::createFramebuffers(VkRenderPass renderPass) {
    framebuffers_.resize(imageViews_.size());
    for (size_t i = 0; i < imageViews_.size(); ++i) {
        // MSAA: [msaaColor(0), depth(1), resolve=swapchain(2)]
        // Non-MSAA: [swapchain(0), depth(1)]
        std::vector<VkImageView> attachments;
        if (msaaSamples_ != VK_SAMPLE_COUNT_1_BIT) {
            attachments = { msaaColorView_, depthImageView_, imageViews_[i] };
        } else {
            attachments = { imageViews_[i], depthImageView_ };
        }

        VkFramebufferCreateInfo fi{};
        fi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass      = renderPass;
        fi.attachmentCount = (uint32_t)attachments.size();
        fi.pAttachments    = attachments.data();
        fi.width           = extent_.width;
        fi.height          = extent_.height;
        fi.layers          = 1;

        if (vkCreateFramebuffer(ctx_->getDevice(), &fi, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to create framebuffer\n");
            return false;
        }
    }
    return true;
}

void VulkanSwapChain::destroySwapChainObjects() {
    VkDevice dev = ctx_->getDevice();

    // Release MSAA color resources.
    if (msaaColorView_)   { vkDestroyImageView(dev, msaaColorView_,  nullptr); msaaColorView_   = VK_NULL_HANDLE; }
    if (msaaColorImage_)  { vkDestroyImage(dev, msaaColorImage_,     nullptr); msaaColorImage_  = VK_NULL_HANDLE; }
    if (msaaColorMemory_) { vkFreeMemory(dev, msaaColorMemory_,      nullptr); msaaColorMemory_ = VK_NULL_HANDLE; }

    vkDestroyImageView(dev, depthImageView_, nullptr);
    vkDestroyImage(dev, depthImage_, nullptr);
    vkFreeMemory(dev, depthImageMemory_, nullptr);
    depthImageView_   = VK_NULL_HANDLE;
    depthImage_       = VK_NULL_HANDLE;
    depthImageMemory_ = VK_NULL_HANDLE;

    for (auto fb : framebuffers_) vkDestroyFramebuffer(dev, fb, nullptr);
    framebuffers_.clear();

    for (auto iv : imageViews_) vkDestroyImageView(dev, iv, nullptr);
    imageViews_.clear();

    if (swapChain_) {
        vkDestroySwapchainKHR(dev, swapChain_, nullptr);
        swapChain_ = VK_NULL_HANDLE;
    }
}

// ============================================================
//  Selection helpers
// ============================================================

VkSurfaceFormatKHR VulkanSwapChain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (auto& sf : formats)
        if (sf.format == VK_FORMAT_B8G8R8A8_SRGB &&
            sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return sf;
    return formats[0];
}

VkPresentModeKHR VulkanSwapChain::choosePresentMode(
    const std::vector<VkPresentModeKHR>& modes) const
{
    for (auto pm : modes)
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) return pm;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return caps.currentExtent;

    int w = 0, h = 0;
    getFramebufferSize_(w, h);
    VkExtent2D ext{ (uint32_t)w, (uint32_t)h };
    ext.width  = std::clamp(ext.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

// ============================================================
//  Image / view utility
// ============================================================

VkImageView VulkanSwapChain::createImageView(
    VkImage image, VkFormat format, VkImageAspectFlags aspect) const
{
    VkImageViewCreateInfo ci{};
    ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image                           = image;
    ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ci.format                          = format;
    ci.subresourceRange.aspectMask     = aspect;
    ci.subresourceRange.baseMipLevel   = 0;
    ci.subresourceRange.levelCount     = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    VKG_CHECK(vkCreateImageView(ctx_->getDevice(), &ci, nullptr, &view),
              "Failed to create image view", VK_NULL_HANDLE);
    return view;
}

bool VulkanSwapChain::createImage(
    uint32_t w, uint32_t h, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags props,
    VkImage& image, VkDeviceMemory& memory,
    VkSampleCountFlagBits samples) const
{
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.extent        = {w, h, 1};
    ci.mipLevels     = 1;
    ci.arrayLayers   = 1;
    ci.format        = format;
    ci.tiling        = tiling;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage         = usage;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.samples       = samples;

    VKG_CHECK(vkCreateImage(ctx_->getDevice(), &ci, nullptr, &image),
              "Failed to create image", false);

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx_->getDevice(), image, &req);

    auto memType = ctx_->findMemoryType(req.memoryTypeBits, props);
    if (!memType) {
        vkDestroyImage(ctx_->getDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = *memType;

    if (vkAllocateMemory(ctx_->getDevice(), &ai, nullptr, &memory) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to allocate image memory\n");
        vkDestroyImage(ctx_->getDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(ctx_->getDevice(), image, memory, 0);
    return true;
}

std::optional<VkFormat> VulkanSwapChain::findDepthFormat() const {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

std::optional<VkFormat> VulkanSwapChain::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    for (auto fmt : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(ctx_->getPhysicalDevice(), fmt, &props);
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features)
            return fmt;
        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features)
            return fmt;
    }
    std::fprintf(stderr, "[VKG] Failed to find supported format\n");
    return std::nullopt;
}

} // namespace VKG
