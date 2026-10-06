#include "gtest/gtest.h"

#include "../Renderer/GltfMainPipelines.h"

using namespace Phantom::Gltf;

TEST(GltfMainPipelinesTest, OpaqueStateWritesDepthWithoutBlend)
{
    const auto s = gltfFixedFunctionState(false, false, VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(s.cullMode, VK_CULL_MODE_BACK_BIT);
    EXPECT_FALSE(s.blendEnable);
    EXPECT_TRUE(s.depthWrite);
}

TEST(GltfMainPipelinesTest, BlendStateDisablesDepthWrite)
{
    const auto s = gltfFixedFunctionState(true, false, VK_CULL_MODE_FRONT_BIT);
    EXPECT_EQ(s.cullMode, VK_CULL_MODE_FRONT_BIT);
    EXPECT_TRUE(s.blendEnable);
    EXPECT_FALSE(s.depthWrite);
}

TEST(GltfMainPipelinesTest, DoubleSidedOverridesCullOnlyOnTheCullAxis)
{
    const auto o = gltfFixedFunctionState(false, true, VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(o.cullMode, VK_CULL_MODE_NONE);
    EXPECT_TRUE(o.depthWrite);
    const auto b = gltfFixedFunctionState(true, true, VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(b.cullMode, VK_CULL_MODE_NONE);
    EXPECT_FALSE(b.depthWrite);
}

TEST(GltfMainPipelinesTest, SelectIndexesAreDistinct)
{
    GltfMainPipelines p;
    EXPECT_NE(&p.select(false, false), &p.select(false, true));
    EXPECT_NE(&p.select(false, false), &p.select(true, false));
    EXPECT_NE(&p.select(true, false), &p.select(true, true));
    EXPECT_EQ(&p.select(true, true), &p.select(true, true));
}
