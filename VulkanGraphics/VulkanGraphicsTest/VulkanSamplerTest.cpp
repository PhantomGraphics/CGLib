#include "VulkanTestFixture.h"
#include "../VulkanSampler.h"

using Phantom::VKG::VulkanSampler;

using VulkanSamplerTest = VulkanTestFixture;

TEST_F(VulkanSamplerTest, CreateDefault) {
    VulkanSampler sampler;
    EXPECT_TRUE(sampler.create(ctx_.getDevice()));
    EXPECT_TRUE(sampler.isValid());
    sampler.destroy(ctx_.getDevice());
}
