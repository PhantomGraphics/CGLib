#include "gtest/gtest.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Gltf/GltfNodeTransform.h"

using namespace Phantom::Gltf;

TEST(GltfNodeTransformTest, MatrixFormWinsOverTrs)
{
    GltfNode n;
    n.hasMatrix = true;
    n.matrix = glm::translate(glm::mat4(1.f), glm::vec3(1.f, 2.f, 3.f));
    n.translation = glm::vec3(9.f);
    EXPECT_EQ(nodeLocalMatrix(n), n.matrix);
}

TEST(GltfNodeTransformTest, TrsComposesTranslateRotateScale)
{
    GltfNode n;
    n.translation = glm::vec3(1.f, 0.f, 0.f);
    n.scale = glm::vec3(2.f);
    n.rotation = glm::vec4(0.f, 0.f, 0.f, 1.f);  // identity (x,y,z,w)
    const glm::vec4 p = nodeLocalMatrix(n) * glm::vec4(1.f, 1.f, 1.f, 1.f);
    EXPECT_NEAR(p.x, 3.f, 1e-6f);  // scaled first, then translated
    EXPECT_NEAR(p.y, 2.f, 1e-6f);
    EXPECT_NEAR(p.z, 2.f, 1e-6f);
}

TEST(GltfNodeTransformTest, DefaultNodeIsIdentity)
{
    GltfNode n;
    const glm::mat4 m = nodeLocalMatrix(n);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_NEAR(m[c][r], c == r ? 1.f : 0.f, 1e-6f);
}
