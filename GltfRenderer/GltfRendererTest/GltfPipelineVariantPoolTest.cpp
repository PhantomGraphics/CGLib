#include "gtest/gtest.h"

#include "../Renderer/GltfPipelineVariantPool.h"

using Phantom::Gltf::GltfPipelineVariantPool;
using Phantom::VKG::VulkanPipeline;

namespace {

GltfPipelineVariantPool::Key key(uint64_t h, VkCullModeFlags cull = VK_CULL_MODE_BACK_BIT,
                                 bool blend = false, bool depth = true)
{
    GltfPipelineVariantPool::Key k;
    k.fragSpvHash = h;
    k.cullMode    = cull;
    k.blendEnable = blend;
    k.depthWrite  = depth;
    return k;
}

} // namespace

TEST(GltfPipelineVariantPoolTest, HashIsDeterministicAndContentSensitive)
{
    const std::vector<uint32_t> a = {0x07230203u, 1, 2, 3};
    std::vector<uint32_t> b = a;
    EXPECT_EQ(GltfPipelineVariantPool::hashSpirv(a), GltfPipelineVariantPool::hashSpirv(b));
    b[3] = 4;
    EXPECT_NE(GltfPipelineVariantPool::hashSpirv(a), GltfPipelineVariantPool::hashSpirv(b));
    EXPECT_EQ(14695981039346656037ull, GltfPipelineVariantPool::hashSpirv(nullptr, 0)); // FNV offset basis
}

TEST(GltfPipelineVariantPoolTest, KeyDistinguishesEveryField)
{
    const auto base = key(1);
    EXPECT_TRUE(base == key(1));
    EXPECT_FALSE(base == key(2));
    EXPECT_FALSE(base == key(1, VK_CULL_MODE_NONE));
    EXPECT_FALSE(base == key(1, VK_CULL_MODE_BACK_BIT, true));
    EXPECT_FALSE(base == key(1, VK_CULL_MODE_BACK_BIT, false, false));
}

TEST(GltfPipelineVariantPoolTest, FindAddAndSharedPointers)
{
    GltfPipelineVariantPool pool;
    EXPECT_EQ(nullptr, pool.find(key(1)));
    EXPECT_EQ(0u, pool.size());

    auto* first = pool.add(key(1), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(first, pool.find(key(1)));
    EXPECT_EQ(1u, pool.size());

    auto* other = pool.add(key(1, VK_CULL_MODE_NONE), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    EXPECT_NE(first, other);
    EXPECT_EQ(2u, pool.size());

    // Pointers stay valid while the pool grows.
    for (uint64_t i = 10; i < 60; ++i)
        pool.add(key(i), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    EXPECT_EQ(first, pool.find(key(1)));
    EXPECT_EQ(52u, pool.size());
}

TEST(GltfPipelineVariantPoolTest, DuplicateKeyKeepsExistingAndNullIsRejected)
{
    GltfPipelineVariantPool pool;
    auto* first = pool.add(key(7), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    auto* again = pool.add(key(7), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    EXPECT_EQ(first, again);
    EXPECT_EQ(1u, pool.size());
    EXPECT_EQ(nullptr, pool.add(key(8), nullptr, VK_NULL_HANDLE));
    EXPECT_EQ(1u, pool.size());
}

TEST(GltfPipelineVariantPoolTest, DestroyAllEmptiesPoolAndIsRepeatable)
{
    GltfPipelineVariantPool pool;
    pool.add(key(1), std::make_unique<VulkanPipeline>(), VK_NULL_HANDLE);
    pool.destroyAll(VK_NULL_HANDLE); // pipelines were never created: every handle is null
    EXPECT_EQ(0u, pool.size());
    EXPECT_EQ(nullptr, pool.find(key(1)));
    pool.destroyAll(VK_NULL_HANDLE);
}
