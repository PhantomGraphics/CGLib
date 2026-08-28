#include "gtest/gtest.h"

#include "../Scene/SceneGroup.h"

using namespace Phantom::Scene;
using namespace Phantom::Math;

namespace {
    class BoxScene : public SceneBase {
    public:
        BoxScene(int id, const Box3df& box) : SceneBase(id), box_(box) {}
        Box3df getBoundingBox() const override { return box_; }
    private:
        Box3df box_;
    };
}

TEST(SceneGroupTest, GetBoundingBox_EmptyGroup)
{
    SceneGroup group;
    auto bb = group.getBoundingBox();
    auto ref = Box3df::createDegeneratedBox();
    EXPECT_TRUE(bb.isSame(ref, 1e-6f));
}

TEST(SceneGroupTest, GetBoundingBox_SingleChild)
{
    SceneGroup group;
    group.addScene(new BoxScene(1, Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1))));

    auto bb = group.getBoundingBox();
    EXPECT_TRUE(bb.isSame(Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1)), 1e-6f));
}

TEST(SceneGroupTest, GetBoundingBox_MultipleChildren)
{
    SceneGroup group;
    group.addScene(new BoxScene(1, Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1))));
    group.addScene(new BoxScene(2, Box3df(Vector3df(2, 3, 4), Vector3df(5, 6, 7))));

    auto bb = group.getBoundingBox();
    const auto min = bb.getMin();
    const auto max = bb.getMax();

    EXPECT_FLOAT_EQ(min.x, 0.0f);
    EXPECT_FLOAT_EQ(min.y, 0.0f);
    EXPECT_FLOAT_EQ(min.z, 0.0f);
    EXPECT_FLOAT_EQ(max.x, 5.0f);
    EXPECT_FLOAT_EQ(max.y, 6.0f);
    EXPECT_FLOAT_EQ(max.z, 7.0f);
}

TEST(SceneGroupTest, GetBoundingBox_NestedGroups)
{
    SceneGroup outer;
    auto* inner = new SceneGroup();
    inner->addScene(new BoxScene(1, Box3df(Vector3df(-1, -1, -1), Vector3df(0, 0, 0))));
    outer.addScene(inner);
    outer.addScene(new BoxScene(2, Box3df(Vector3df(1, 1, 1), Vector3df(3, 3, 3))));

    auto bb = outer.getBoundingBox();
    const auto min = bb.getMin();
    const auto max = bb.getMax();

    EXPECT_FLOAT_EQ(min.x, -1.0f);
    EXPECT_FLOAT_EQ(min.y, -1.0f);
    EXPECT_FLOAT_EQ(min.z, -1.0f);
    EXPECT_FLOAT_EQ(max.x, 3.0f);
    EXPECT_FLOAT_EQ(max.y, 3.0f);
    EXPECT_FLOAT_EQ(max.z, 3.0f);
}

TEST(SceneGroupTest, GetBoundingBox_NegativeCoords)
{
    SceneGroup group;
    group.addScene(new BoxScene(1, Box3df(Vector3df(-5, -3, -1), Vector3df(-2, -1, 0))));
    group.addScene(new BoxScene(2, Box3df(Vector3df(-10, 0, 2), Vector3df(-8, 4, 6))));

    auto bb = group.getBoundingBox();
    const auto min = bb.getMin();
    const auto max = bb.getMax();

    EXPECT_FLOAT_EQ(min.x, -10.0f);
    EXPECT_FLOAT_EQ(min.y, -3.0f);
    EXPECT_FLOAT_EQ(min.z, -1.0f);
    EXPECT_FLOAT_EQ(max.x, -2.0f);
    EXPECT_FLOAT_EQ(max.y, 4.0f);
    EXPECT_FLOAT_EQ(max.z, 6.0f);
}
