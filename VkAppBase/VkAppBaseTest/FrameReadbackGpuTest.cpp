#include "../../VulkanGraphics/VulkanGraphicsTest/VulkanTestFixture.h"
#include "../../VulkanGraphics/VulkanImage.h"
#include "../FrameReadback.h"

#include <filesystem>
#include <fstream>

using FrameReadbackGpuTest = VulkanTestFixture;

TEST_F(FrameReadbackGpuTest, RepeatedCaptureResizesBufferAndKeepsRecordedExtent) {
    VKG::FrameReadback readback;
    const auto path = std::filesystem::temp_directory_path() / "cglib_readback_regression.png";
    for (const uint32_t size : {2u, 8u, 1u}) {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        ASSERT_TRUE(Phantom::VKG::VulkanImage::create(ctx_, size, size, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory));
        auto cmd = pool_.beginSingleTimeCommands();
        ASSERT_NE(cmd, VK_NULL_HANDLE);
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        const VkClearColorValue white{{1.f, 1.f, 1.f, 1.f}};
        vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1, &barrier.subresourceRange);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        readback.setScreenshot(path.string(), 3);
        readback.requestPixel(size - 1, size - 1);
        const auto plan = readback.record(readback.beginFrame(3), ctx_, cmd, image,
            {size, size}, VK_FORMAT_R8G8B8A8_UNORM);
        ASSERT_TRUE(pool_.endSingleTimeCommands(cmd));
        // The source image can disappear before finish(), just like an old swapchain on resize.
        vkDestroyImage(ctx_.getDevice(), image, nullptr);
        vkFreeMemory(ctx_.getDevice(), memory, nullptr);
        readback.finish(plan);
        EXPECT_TRUE(readback.screenshotDone());
        uint8_t out[4]{};
        ASSERT_TRUE(readback.pollPixel(out));
        for (const auto channel : out) EXPECT_EQ(channel, 255);
        std::ifstream png(path, std::ios::binary);
        uint8_t header[24]{};
        ASSERT_TRUE(png.read(reinterpret_cast<char*>(header), sizeof(header)));
        EXPECT_EQ(header[19], size); // PNG IHDR width/height, big endian
        EXPECT_EQ(header[23], size);
    }
    readback.destroy(ctx_.getDevice());
    std::error_code error;
    std::filesystem::remove(path, error);
}
