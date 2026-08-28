#include "VulkanTestFixture.h"
#include "../VulkanSwapChain.h"

#include <optional>
#include <vector>

using Phantom::VKG::VulkanSwapChain;

namespace Phantom::VKG {
// Thin accessor granted friendship in VulkanSwapChain.h so this test can
// reach the otherwise-private findSupportedFormat().
struct VulkanSwapChainTestAccess {
    static std::optional<VkFormat> call(const VulkanSwapChain& sc,
                          const std::vector<VkFormat>& candidates,
                          VkImageTiling tiling, VkFormatFeatureFlags features) {
        return sc.findSupportedFormat(candidates, tiling, features);
    }
};
} // namespace Phantom::VKG

using VulkanSwapChainTest = VulkanTestFixture;

TEST_F(VulkanSwapChainTest, FindDepthFormatSucceeds) {
    VulkanSwapChain sc;
    sc.init(&ctx_, surface_, [](int& w, int& h) { w = 1; h = 1; });
    auto fmt = sc.findDepthFormat();
    ASSERT_TRUE(fmt.has_value());
    EXPECT_NE(*fmt, VK_FORMAT_UNDEFINED);
}

// Abnormal path: an empty candidate list can never match, regardless of the
// device. Phase 1: findSupportedFormat() returns std::nullopt instead of
// throwing (Phase 0 recorded EXPECT_THROW).
TEST_F(VulkanSwapChainTest, FindSupportedFormatWithEmptyCandidatesReturnsNullopt) {
    VulkanSwapChain sc;
    sc.init(&ctx_, surface_, [](int& w, int& h) { w = 1; h = 1; });

    auto fmt = Phantom::VKG::VulkanSwapChainTestAccess::call(
        sc, {}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    EXPECT_FALSE(fmt.has_value());
}
