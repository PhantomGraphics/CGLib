#include "VulkanTestFixture.h"
#include "../VulkanCubeMap.h"

#include <array>
#include <string>

using Phantom::VKG::VulkanCubeMap;

using VulkanCubeMapTest = VulkanTestFixture;

TEST_F(VulkanCubeMapTest, CreateDummyIsValid) {
    VulkanCubeMap cube;
    EXPECT_TRUE(cube.createDummy(ctx_, pool_));
    EXPECT_TRUE(cube.isValid());
    EXPECT_NE(cube.getSampler(), VK_NULL_HANDLE);
    cube.destroy(ctx_.getDevice());
}

// Abnormal path: all six face paths are nonexistent files, so stbi_load()
// fails for the first face. Phase 1: VulkanCubeMap::create() returns false
// instead of throwing (Phase 0 recorded EXPECT_THROW).
TEST_F(VulkanCubeMapTest, CreateWithMissingFacesFails) {
    VulkanCubeMap cube;
    std::array<std::string, 6> missingFaces = {
        "no_such_face_right.png",   "no_such_face_left.png",
        "no_such_face_top.png",     "no_such_face_bottom.png",
        "no_such_face_front.png",   "no_such_face_back.png",
    };
    EXPECT_FALSE(cube.create(ctx_, pool_, missingFaces));
    EXPECT_FALSE(cube.isValid());
}
