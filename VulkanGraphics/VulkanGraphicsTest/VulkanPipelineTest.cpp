#include "VulkanTestFixture.h"
#include "../VulkanPipeline.h"
#include "../VulkanRenderPass.h"
#include "../VulkanSPVLoader.h"

using Phantom::VKG::PipelineConfig;
using Phantom::VKG::VulkanPipeline;
using Phantom::VKG::VulkanRenderPass;
using Phantom::VKG::loadSPV;

using VulkanPipelineTest = VulkanTestFixture;

TEST_F(VulkanPipelineTest, CreateWithDummySpirv) {
    VulkanRenderPass rp;
    EXPECT_TRUE(rp.create(ctx_, VK_FORMAT_B8G8R8A8_UNORM, depthFormat_));

    PipelineConfig cfg;
    cfg.vertSpv = loadSPV("shaders/test.vert.spv");
    cfg.fragSpv = loadSPV("shaders/test.frag.spv");
    ASSERT_FALSE(cfg.vertSpv.empty());
    ASSERT_FALSE(cfg.fragSpv.empty());

    VulkanPipeline pipeline;
    EXPECT_TRUE(pipeline.create(ctx_, rp.get(), cfg));
    EXPECT_NE(pipeline.getPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(pipeline.getLayout(), VK_NULL_HANDLE);

    pipeline.destroy(ctx_.getDevice());
    rp.destroy(ctx_.getDevice());
}
