#include "gtest/gtest.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Gltf/GltfAccessorBuilder.h"
#include "../Renderer/GltfPrimitiveData.h"

using namespace Phantom::Gltf;

namespace {
int addVec3(GltfDocument& doc, const std::vector<glm::vec3>& v) {
    return appendAccessor(doc, v, GltfComponentType::Float, GltfAccessorType::Vec3);
}
struct Item { glm::mat4 restWorld{1.f}; glm::vec3 localCenter{0.f}; int id = 0; };
}

TEST(GltfPrimitiveDataTest, AabbCenter)
{
    GltfDocument doc;
    const int a = addVec3(doc, {{0.f, 0.f, 0.f}, {2.f, 4.f, 6.f}, {1.f, 1.f, 1.f}});
    const glm::vec3 c = accessorAabbCenter(doc, a);
    EXPECT_FLOAT_EQ(c.x, 1.f);
    EXPECT_FLOAT_EQ(c.y, 2.f);
    EXPECT_FLOAT_EQ(c.z, 3.f);
}

TEST(GltfPrimitiveDataTest, AabbCenterInvalidAccessorIsOrigin)
{
    GltfDocument doc;
    EXPECT_EQ(accessorAabbCenter(doc, -1), glm::vec3(0.f));
    EXPECT_EQ(accessorAabbCenter(doc, 5), glm::vec3(0.f));
}

TEST(GltfPrimitiveDataTest, ReadGeometryWithAndWithoutNormals)
{
    GltfDocument doc;
    GltfPrimitive prim;
    prim.positionAccessor = addVec3(doc, {{1.f, 2.f, 3.f}, {4.f, 5.f, 6.f}});

    std::vector<glm::vec3> pos{glm::vec3(9.f)}, nrm{glm::vec3(9.f)};
    readPrimitiveGeometry(doc, prim, pos, nrm);
    ASSERT_EQ(pos.size(), 2u);
    EXPECT_EQ(pos[1], glm::vec3(4.f, 5.f, 6.f));
    ASSERT_EQ(nrm.size(), 2u);
    EXPECT_EQ(nrm[0], glm::vec3(0.f, 1.f, 0.f));

    prim.normalAccessor = addVec3(doc, {{1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}});
    readPrimitiveGeometry(doc, prim, pos, nrm);
    EXPECT_EQ(nrm[1], glm::vec3(0.f, 0.f, 1.f));

    GltfPrimitive none;
    readPrimitiveGeometry(doc, none, pos, nrm);
    EXPECT_TRUE(pos.empty());
    EXPECT_TRUE(nrm.empty());
}

TEST(GltfPrimitiveDataTest, SortFarthestFirstUsesModelAndRestWorld)
{
    Item near_, mid, far_;
    near_.id = 0; near_.localCenter = glm::vec3(1.f, 0.f, 0.f);
    mid.id = 1;   mid.localCenter = glm::vec3(5.f, 0.f, 0.f);
    far_.id = 2;  far_.restWorld = glm::translate(glm::mat4(1.f), glm::vec3(20.f, 0.f, 0.f));
    std::vector<Item*> items{&near_, &far_, &mid};
    sortFarthestFirst(items, glm::mat4(1.f), glm::vec3(0.f));
    EXPECT_EQ(items[0]->id, 2);
    EXPECT_EQ(items[1]->id, 1);
    EXPECT_EQ(items[2]->id, 0);

    // A model translation changes which item is farthest from the eye.
    sortFarthestFirst(items, glm::translate(glm::mat4(1.f), glm::vec3(-20.f, 0.f, 0.f)), glm::vec3(0.f));
    EXPECT_EQ(items[0]->id, 0);  // near: |1-20| = 19, mid: |5-20| = 15, far: 0
}
