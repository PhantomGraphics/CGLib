#include "gtest/gtest.h"

#include "../Renderer/GltfGlobalDescriptors.h"

using Phantom::Gltf::GltfGlobalDescriptors;

namespace {

const VkDescriptorSetLayoutBinding* find(const std::vector<VkDescriptorSetLayoutBinding>& v, uint32_t index)
{
    for (const auto& b : v)
        if (b.binding == index) return &b;
    return nullptr;
}

} // namespace

// The shaders (gltf.vert/gltf.frag) hard-code these slots; the table is the single source of truth
// for the layout, so pin it.
TEST(GltfGlobalDescriptorsTest, GlobalBindingTable)
{
    const auto b = GltfGlobalDescriptors::globalBindings();
    ASSERT_EQ(8u, b.size());
    for (uint32_t i = 0; i < b.size(); ++i) {
        ASSERT_NE(nullptr, find(b, i)) << "missing binding " << i;
        EXPECT_EQ(1u, find(b, i)->descriptorCount);
    }

    for (uint32_t ubo : {0u, 5u, 6u})
        EXPECT_EQ(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, find(b, ubo)->descriptorType) << ubo;
    for (uint32_t tex : {1u, 2u, 3u, 4u, 7u})
        EXPECT_EQ(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, find(b, tex)->descriptorType) << tex;

    EXPECT_EQ(VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), find(b, 0)->stageFlags);
    EXPECT_EQ(VkShaderStageFlags(VK_SHADER_STAGE_VERTEX_BIT), find(b, 5)->stageFlags); // BoneUBO
    EXPECT_EQ(VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT), find(b, 6)->stageFlags);
}

TEST(GltfGlobalDescriptorsTest, MaterialBindingTable)
{
    const auto b = GltfGlobalDescriptors::materialBindings();
    ASSERT_EQ(6u, b.size());
    EXPECT_EQ(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, find(b, 0)->descriptorType);
    for (uint32_t i = 1; i <= 5; ++i) {
        ASSERT_NE(nullptr, find(b, i));
        EXPECT_EQ(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, find(b, i)->descriptorType) << i;
        EXPECT_EQ(VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT), find(b, i)->stageFlags) << i;
    }
}

TEST(GltfGlobalDescriptorsTest, DestroyWithoutCreateIsSafeNoDevice)
{
    GltfGlobalDescriptors d;
    d.destroy(VK_NULL_HANDLE); // nothing was created: every handle is null, no Vulkan call is made
    d.destroyMaterialPool(VK_NULL_HANDLE);
    EXPECT_EQ(0u, d.frameCount());
    EXPECT_EQ(VK_NULL_HANDLE, d.globalLayout());
    EXPECT_EQ(VK_NULL_HANDLE, d.materialPool());
}
