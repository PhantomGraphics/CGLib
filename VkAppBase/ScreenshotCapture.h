#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace VKG {

// Pieces of VkAppBase's built-in screenshot / pixel readback that do not need the app: command-line
// parsing, BGRA -> RGBA conversion, PNG writing and the swapchain-image copy recording. They are
// free functions so the CPU parts can be unit-tested without a window or a GPU.

struct ScreenshotArgs {
    std::string        path;   // empty = no screenshot requested
    std::optional<int> frame;  // explicit --screenshot-frame, if any (only meaningful with a path)
};

// Understands `--screenshot <path>`, `--screenshot=<path>`, `--screenshot-frame <N>` and
// `--screenshot-frame=<N>` in any order; the last occurrence of each wins. A frame that is not a
// non-negative integer is ignored with a message on stderr (it never throws). Unknown arguments
// are skipped, argv[0] is not inspected.
ScreenshotArgs parseScreenshotArgs(int argc, char* argv[]);

// True for the BGRA surface formats whose first and third byte must be swapped to get RGBA.
bool isBgraFormat(VkFormat format);

// Swaps the R and B bytes of `pixelCount` 4-byte pixels in place.
void swapRedBlue(uint8_t* rgba, size_t pixelCount);

// Writes tightly packed RGBA8 as PNG, creating parent directories. Returns false on failure.
bool writePng(const std::string& path, const uint8_t* rgba, uint32_t width, uint32_t height);

// Records PRESENT_SRC -> TRANSFER_SRC, copy of the whole image (or one pixel region) into `dst`,
// and TRANSFER_SRC -> PRESENT_SRC again. `region` is the rectangle to copy.
void recordSwapchainCopyToBuffer(VkCommandBuffer cmd, VkImage srcImage, VkBuffer dst,
                                 VkOffset2D offset, VkExtent2D extent);

} // namespace VKG
