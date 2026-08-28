#include "VulkanTestFixture.h"
#include "../VulkanComputePipeline.h"
#include "../VulkanSPVLoader.h"

using Phantom::VKG::ComputePipelineConfig;
using Phantom::VKG::VulkanComputePipeline;
using Phantom::VKG::loadSPV;

using VulkanComputePipelineTest = VulkanTestFixture;

TEST_F(VulkanComputePipelineTest, CreateWithDummySpirv) {
    ComputePipelineConfig cfg;
    cfg.compSpv = loadSPV("shaders/test.comp.spv");
    ASSERT_FALSE(cfg.compSpv.empty());

    VulkanComputePipeline pipeline;
    EXPECT_TRUE(pipeline.create(ctx_, cfg));
    EXPECT_TRUE(pipeline.isValid());
    EXPECT_NE(pipeline.getLayout(), VK_NULL_HANDLE);

    pipeline.destroy(ctx_.getDevice());
}
