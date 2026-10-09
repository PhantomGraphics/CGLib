#include "ScreenshotCapture.h"

#include "../../CGLib/ThirdParty/stb/stb_image_write.h"

#include "../Graphics/FastPng.h"

#include <charconv>
#include <cstdio>
#include <filesystem>
#include <string_view>
#include <utility>

namespace VKG {

namespace {

std::optional<int> parseFrame(std::string_view text)
{
    int value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value < 0) {
        std::fprintf(stderr, "[Screenshot] ignoring invalid --screenshot-frame value '%.*s'\n",
                     static_cast<int>(text.size()), text.data());
        return std::nullopt;
    }
    return value;
}

} // namespace

ScreenshotArgs parseScreenshotArgs(int argc, char* argv[])
{
    ScreenshotArgs out;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            out.path = argv[++i];
        } else if (arg == "--screenshot-frame" && i + 1 < argc) {
            if (auto f = parseFrame(argv[++i])) out.frame = f;
        } else if (arg.starts_with("--screenshot=")) {
            out.path = std::string(arg.substr(13));
        } else if (arg.starts_with("--screenshot-frame=")) {
            if (auto f = parseFrame(arg.substr(19))) out.frame = f;
        }
    }
    return out;
}

bool isBgraFormat(VkFormat format)
{
    return format == VK_FORMAT_B8G8R8A8_SRGB  ||
           format == VK_FORMAT_B8G8R8A8_UNORM ||
           format == VK_FORMAT_B8G8R8A8_SNORM;
}

void swapRedBlue(uint8_t* rgba, size_t pixelCount)
{
    for (size_t i = 0; i < pixelCount; ++i)
        std::swap(rgba[i * 4 + 0], rgba[i * 4 + 2]);
}

bool writePng(const std::string& path, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    namespace fs = std::filesystem;
    const fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    // stb's PNG encoder needs seconds for a window-sized capture in a Debug build; FastPng takes milliseconds.
    return Phantom::Graphics::writePngFast(path, rgba, width, height, 4);
}

void recordSwapchainCopyToBuffer(VkCommandBuffer cmd, VkImage srcImage, VkBuffer dst,
                                 VkOffset2D offset, VkExtent2D extent)
{
    // PRESENT_SRC_KHR -> TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier toSrc{};
    toSrc.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSrc.oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toSrc.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSrc.image               = srcImage;
    toSrc.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toSrc.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSrc.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toSrc);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset      = { offset.x, offset.y, 0 };
    region.imageExtent      = { extent.width, extent.height, 1 };
    vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &region);

    // TRANSFER_SRC_OPTIMAL -> PRESENT_SRC_KHR
    VkImageMemoryBarrier toPresent = toSrc;
    toPresent.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toPresent.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toPresent.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toPresent);
}

} // namespace VKG
