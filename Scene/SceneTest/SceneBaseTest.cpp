#include "gtest/gtest.h"

#include "../Scene/SceneBase.h"
#include "../Scene/SceneGroup.h"

using namespace Phantom::Scene;
using namespace Phantom::Math;

namespace {
    class TestScene : public SceneBase {
    public:
        TestScene() = default;
        explicit TestScene(int id) : SceneBase(id) {}
        TestScene(int id, const std::string& name) : SceneBase(id, name) {}
    };
}

TEST(SceneBaseTest, DefaultConstructor)
{
    TestScene s;
    EXPECT_EQ(s.getId(), -1);
    EXPECT_TRUE(s.getName().empty());
    EXPECT_TRUE(s.isRoot());
    EXPECT_TRUE(s.isLeaf());
    EXPECT_EQ(s.getParent(), nullptr);
}

TEST(SceneBaseTest, ConstructorWithId)
{
    TestScene s(42);
    EXPECT_EQ(s.getId(), 42);
}

TEST(SceneBaseTest, ConstructorWithIdAndName)
{
    TestScene s(7, "root");
    EXPECT_EQ(s.getId(), 7);
    EXPECT_EQ(s.getName(), "root");
}

TEST(SceneBaseTest, SetGetName)
{
    TestScene s(1);
    s.setName("renamed");
    EXPECT_EQ(s.getName(), "renamed");
}

TEST(SceneBaseTest, SetGetId)
{
    TestScene s;
    s.setId(42);
    EXPECT_EQ(s.getId(), 42);
}

TEST(SceneBaseTest, AddScene_SetsParentAndChild)
{
    TestScene parent(1, "parent");
    auto* child = new TestScene(2, "child");

    parent.addScene(child);

    EXPECT_FALSE(parent.isLeaf());
    EXPECT_EQ(parent.getChildren().size(), 1u);
    EXPECT_EQ(child->getParent(), &parent);
    EXPECT_FALSE(child->isRoot());
}

TEST(SceneBaseTest, FindSceneById_Self)
{
    TestScene s(10);
    EXPECT_EQ(s.findSceneById(10), &s);
}

TEST(SceneBaseTest, FindSceneById_DirectChild)
{
    TestScene root(1);
    auto* child = new TestScene(2);
    root.addScene(child);

    EXPECT_EQ(root.findSceneById(2), child);
}

TEST(SceneBaseTest, FindSceneById_Grandchild)
{
    TestScene root(1);
    auto* child = new TestScene(2);
    auto* grandchild = new TestScene(3);
    child->addScene(grandchild);
    root.addScene(child);

    EXPECT_EQ(root.findSceneById(3), grandchild);
}

TEST(SceneBaseTest, FindSceneById_ReturnsNullWhenNotFound)
{
    TestScene root(1);
    EXPECT_EQ(root.findSceneById(99), nullptr);
}

// Regression test for bug 1.1: findSceneByName returned c (the child iterated)
// instead of the recursive result when the match was deeper in the subtree.
TEST(SceneBaseTest, FindSceneByName_Self)
{
    TestScene s(1, "self");
    EXPECT_EQ(s.findSceneByName("self"), &s);
}

TEST(SceneBaseTest, FindSceneByName_DirectChild)
{
    TestScene root(1, "root");
    auto* child = new TestScene(2, "child");
    root.addScene(child);

    EXPECT_EQ(root.findSceneByName("child"), child);
}

TEST(SceneBaseTest, FindSceneByName_Grandchild)
{
    TestScene root(1, "root");
    auto* child = new TestScene(2, "child");
    auto* grandchild = new TestScene(3, "grandchild");
    child->addScene(grandchild);
    root.addScene(child);

    SceneBase* found = root.findSceneByName("grandchild");
    EXPECT_EQ(found, grandchild);
    EXPECT_NE(found, child);  // old bug would return child
}

TEST(SceneBaseTest, FindSceneByName_ReturnsNullWhenNotFound)
{
    TestScene root(1, "root");
    EXPECT_EQ(root.findSceneByName("missing"), nullptr);
}

TEST(SceneBaseTest, DeleteSceneById_DeletesDirectChild)
{
    TestScene root(1);
    root.addScene(new TestScene(2));
    root.addScene(new TestScene(3));

    root.deleteSceneById(2);

    EXPECT_EQ(root.getChildren().size(), 1u);
    EXPECT_EQ(root.findSceneById(2), nullptr);
    EXPECT_NE(root.findSceneById(3), nullptr);
}

// Regression test for bug 1.2: old deleteSceneById searched only direct children.
TEST(SceneBaseTest, DeleteSceneById_DeletesGrandchild)
{
    TestScene root(1);
    auto* child = new TestScene(2);
    child->addScene(new TestScene(3));
    root.addScene(child);

    root.deleteSceneById(3);

    EXPECT_TRUE(child->isLeaf());
    EXPECT_EQ(root.findSceneById(3), nullptr);
}

TEST(SceneBaseTest, DeleteSceneById_IgnoresNonExistentId)
{
    TestScene root(1);
    root.addScene(new TestScene(2));

    EXPECT_NO_FATAL_FAILURE(root.deleteSceneById(99));
    EXPECT_EQ(root.getChildren().size(), 1u);
}

TEST(SceneBaseTest, DeleteSceneById_IgnoresRoot)
{
    TestScene root(1);
    EXPECT_NO_FATAL_FAILURE(root.deleteSceneById(1));
    EXPECT_TRUE(root.isRoot());
}

TEST(SceneBaseTest, ClearAll_RemovesAllChildren)
{
    TestScene root(1);
    root.addScene(new TestScene(2));
    root.addScene(new TestScene(3));

    root.clearAll();

    EXPECT_TRUE(root.isLeaf());
    EXPECT_EQ(root.getChildren().size(), 0u);
}

TEST(SceneBaseTest, Clear_IsSameAsClearAll)
{
    TestScene root(1);
    root.addScene(new TestScene(2));

    root.clear();

    EXPECT_TRUE(root.isLeaf());
}

TEST(SceneBaseTest, IsRoot_TrueForTopLevel)
{
    TestScene s(1);
    EXPECT_TRUE(s.isRoot());
}

TEST(SceneBaseTest, IsRoot_FalseForChild)
{
    TestScene root(1);
    auto* child = new TestScene(2);
    root.addScene(child);
    EXPECT_FALSE(child->isRoot());
}

TEST(SceneBaseTest, GetRoot_ReturnsTopLevelAncestor)
{
    TestScene root(1);
    auto* child = new TestScene(2);
    auto* grandchild = new TestScene(3);
    child->addScene(grandchild);
    root.addScene(child);

    EXPECT_EQ(grandchild->getRoot(), &root);
    EXPECT_EQ(child->getRoot(), &root);
    EXPECT_EQ(root.getRoot(), &root);
}

TEST(SceneBaseTest, IsLeaf_TrueWhenNoChildren)
{
    TestScene s(1);
    EXPECT_TRUE(s.isLeaf());
}

TEST(SceneBaseTest, IsLeaf_FalseWhenHasChildren)
{
    TestScene root(1);
    root.addScene(new TestScene(2));
    EXPECT_FALSE(root.isLeaf());
}

TEST(SceneBaseTest, SetGetVisible)
{
    TestScene s(1);
    EXPECT_TRUE(s.isVisible());

    s.setVisible(false);
    EXPECT_FALSE(s.isVisible());

    s.setVisible(true);
    EXPECT_TRUE(s.isVisible());
}

TEST(SceneBaseTest, FindScenesByType_FindsMatchingType)
{
    TestScene root(1);
    root.addScene(new TestScene(2));
    root.addScene(new TestScene(3));

    auto results = root.findScenesByType(typeid(TestScene));
    EXPECT_EQ(results.size(), 3u);  // root + 2 children
}

TEST(SceneBaseTest, FindScenesByType_EmptyForUnrelatedType)
{
    TestScene root(1);

    auto results = root.findScenesByType(typeid(SceneGroup));
    EXPECT_TRUE(results.empty());
}

TEST(SceneBaseTest, FindScenesByType_MixedTree)
{
    SceneGroup root;
    root.addScene(new TestScene(1));
    root.addScene(new TestScene(2));

    auto groups = root.findScenesByType(typeid(SceneGroup));
    EXPECT_EQ(groups.size(), 1u);  // only root

    auto scenes = root.findScenesByType(typeid(TestScene));
    EXPECT_EQ(scenes.size(), 2u);
}

TEST(SceneBaseTest, MultipleChildren_AllReachable)
{
    TestScene root(1);
    auto* a = new TestScene(2, "a");
    auto* b = new TestScene(3, "b");
    auto* c = new TestScene(4, "c");
    root.addScene(a);
    root.addScene(b);
    root.addScene(c);

    EXPECT_EQ(root.getChildren().size(), 3u);
    EXPECT_EQ(root.findSceneById(2), a);
    EXPECT_EQ(root.findSceneById(3), b);
    EXPECT_EQ(root.findSceneById(4), c);
    EXPECT_EQ(root.findSceneByName("a"), a);
    EXPECT_EQ(root.findSceneByName("b"), b);
    EXPECT_EQ(root.findSceneByName("c"), c);
}
