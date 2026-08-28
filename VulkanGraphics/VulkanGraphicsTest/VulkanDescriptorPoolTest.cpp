#include "VulkanTestFixture.h"
#include "../VulkanDescriptorPool.h"

using Phantom::VKG::VulkanDescriptorPool;
using Phantom::VKG::VulkanDescriptorSetLayout;

using VulkanDescriptorPoolTest = VulkanTestFixture;

TEST_F(VulkanDescriptorPoolTest, CreateLayoutPoolAndAllocateSets) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VulkanDescriptorSetLayout layout;
    EXPECT_TRUE(layout.create(ctx_.getDevice(), {binding}));
    EXPECT_TRUE(layout.isValid());

    VulkanDescriptorPool pool;
    EXPECT_TRUE(pool.create(ctx_.getDevice(),
                {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}},
                /*maxSets=*/1));
    EXPECT_TRUE(pool.isValid());

    auto sets = pool.allocateSets(ctx_.getDevice(), {layout.get()});
    ASSERT_EQ(sets.size(), 1u);
    EXPECT_NE(sets[0], VK_NULL_HANDLE);

    pool.destroy(ctx_.getDevice());
    layout.destroy(ctx_.getDevice());
}
