#include "gtest/gtest.h"

#include "../SceneRuntime/SceneGraph.h"

#include <filesystem>
#include <random>

using namespace Phantom::SceneRuntime;

namespace {

SceneNode makeNode(const std::string& id, const std::string& name, const NodeId& parent = NodeId())
{
    SceneNode n;
    n.id = NodeId(id);
    n.name = name;
    n.parent = parent;
    return n;
}

std::filesystem::path tempDir()
{
    static std::mt19937 rng{ std::random_device{}() };
    const auto dir = std::filesystem::temp_directory_path()
        / ("SceneGraphTest_" + std::to_string(rng()));
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST(SceneGraph, AddNodeRootAndChild)
{
    SceneGraph g;
    EXPECT_EQ(g.size(), 0u);
    ASSERT_TRUE(g.addNode(makeNode("root", "Root")));
    EXPECT_EQ(g.size(), 1u);
    ASSERT_TRUE(g.addNode(makeNode("child", "Child", NodeId("root"))));
    EXPECT_EQ(g.size(), 2u);

    const SceneNode* child = g.find(NodeId("child"));
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->name, "Child");
    EXPECT_EQ(child->parent, NodeId("root"));
}

TEST(SceneGraph, AddNodeRejectsInvalidIdDuplicateAndUnknownParent)
{
    SceneGraph g;
    EXPECT_FALSE(g.addNode(makeNode("", "NoId")));
    EXPECT_FALSE(g.addNode(makeNode("orphan", "Orphan", NodeId("missing-parent"))));

    ASSERT_TRUE(g.addNode(makeNode("a", "A")));
    EXPECT_FALSE(g.addNode(makeNode("a", "ADuplicate")));
    EXPECT_EQ(g.size(), 1u);
}

TEST(SceneGraph, ChildrenReturnsDirectChildrenOnly)
{
    SceneGraph g;
    ASSERT_TRUE(g.addNode(makeNode("root", "Root")));
    ASSERT_TRUE(g.addNode(makeNode("a", "A", NodeId("root"))));
    ASSERT_TRUE(g.addNode(makeNode("b", "B", NodeId("root"))));
    ASSERT_TRUE(g.addNode(makeNode("aa", "AA", NodeId("a"))));

    std::vector<NodeId> roots = g.children(NodeId());
    ASSERT_EQ(roots.size(), 1u);
    EXPECT_EQ(roots[0], NodeId("root"));

    std::vector<NodeId> rootChildren = g.children(NodeId("root"));
    EXPECT_EQ(rootChildren.size(), 2u);

    std::vector<NodeId> aChildren = g.children(NodeId("a"));
    ASSERT_EQ(aChildren.size(), 1u);
    EXPECT_EQ(aChildren[0], NodeId("aa"));
}

TEST(SceneGraph, RemoveNodeCascadesToDescendants)
{
    SceneGraph g;
    ASSERT_TRUE(g.addNode(makeNode("root", "Root")));
    ASSERT_TRUE(g.addNode(makeNode("a", "A", NodeId("root"))));
    ASSERT_TRUE(g.addNode(makeNode("aa", "AA", NodeId("a"))));
    ASSERT_TRUE(g.addNode(makeNode("b", "B", NodeId("root"))));
    EXPECT_EQ(g.size(), 4u);

    EXPECT_TRUE(g.removeNode(NodeId("a")));
    EXPECT_EQ(g.size(), 2u); // root + b; a and aa both gone
    EXPECT_EQ(g.find(NodeId("a")), nullptr);
    EXPECT_EQ(g.find(NodeId("aa")), nullptr);
    EXPECT_NE(g.find(NodeId("b")), nullptr);

    EXPECT_FALSE(g.removeNode(NodeId("a"))); // already gone
}

TEST(SceneGraph, ReparentMovesNodeAndRejectsCycles)
{
    SceneGraph g;
    ASSERT_TRUE(g.addNode(makeNode("root", "Root")));
    ASSERT_TRUE(g.addNode(makeNode("a", "A", NodeId("root"))));
    ASSERT_TRUE(g.addNode(makeNode("b", "B", NodeId("root"))));
    ASSERT_TRUE(g.addNode(makeNode("aa", "AA", NodeId("a"))));

    EXPECT_TRUE(g.reparent(NodeId("b"), NodeId("a")));
    EXPECT_EQ(g.find(NodeId("b"))->parent, NodeId("a"));

    // Cycle: can't reparent an ancestor under its own descendant.
    EXPECT_FALSE(g.reparent(NodeId("a"), NodeId("aa")));
    // Cycle: can't reparent a node under itself.
    EXPECT_FALSE(g.reparent(NodeId("a"), NodeId("a")));
    // Unknown target.
    EXPECT_FALSE(g.reparent(NodeId("a"), NodeId("missing")));
    // Unknown source.
    EXPECT_FALSE(g.reparent(NodeId("missing"), NodeId("root")));

    // Reparenting to an invalid id makes it a root.
    EXPECT_TRUE(g.reparent(NodeId("a"), NodeId()));
    EXPECT_FALSE(g.find(NodeId("a"))->parent.isValid());
}

TEST(SceneGraph, WorldTransformComposesAncestorChain)
{
    SceneGraph g;
    SceneNode root = makeNode("root", "Root");
    root.local.translation = { 10.0f, 0.0f, 0.0f };
    ASSERT_TRUE(g.addNode(root));

    SceneNode child = makeNode("child", "Child", NodeId("root"));
    child.local.translation = { 0.0f, 5.0f, 0.0f };
    ASSERT_TRUE(g.addNode(child));

    SceneNode grandchild = makeNode("grandchild", "Grandchild", NodeId("child"));
    grandchild.local.translation = { 0.0f, 0.0f, 2.0f };
    ASSERT_TRUE(g.addNode(grandchild));

    auto worldRoot = g.worldTransform(NodeId("root"));
    ASSERT_TRUE(worldRoot.has_value());
    glm::vec4 originRoot = *worldRoot * glm::vec4(0, 0, 0, 1);
    EXPECT_FLOAT_EQ(originRoot.x, 10.0f);

    auto worldGrandchild = g.worldTransform(NodeId("grandchild"));
    ASSERT_TRUE(worldGrandchild.has_value());
    glm::vec4 origin = *worldGrandchild * glm::vec4(0, 0, 0, 1);
    EXPECT_FLOAT_EQ(origin.x, 10.0f);
    EXPECT_FLOAT_EQ(origin.y, 5.0f);
    EXPECT_FLOAT_EQ(origin.z, 2.0f);
}

TEST(SceneGraph, WorldTransformOnUnknownNodeReturnsNullopt)
{
    SceneGraph g;
    EXPECT_FALSE(g.worldTransform(NodeId("missing")).has_value());
}

TEST(SceneGraph, JsonRoundTripPreservesHierarchyTransformAndComponents)
{
    SceneGraph g;
    SceneNode root = makeNode("root", "Root");
    root.enabled = false;
    root.local.translation = { 1.0f, 2.0f, 3.0f };
    root.local.scale = { 2.0f, 2.0f, 2.0f };
    ComponentRecord mesh;
    mesh.type = "mesh";
    mesh.data = { { "shape", "box" }, { "size", 1.5 } };
    root.components.push_back(mesh);
    ASSERT_TRUE(g.addNode(root));

    SceneNode child = makeNode("child", "Child", NodeId("root"));
    ASSERT_TRUE(g.addNode(child));

    const std::string json = g.toJson();
    EXPECT_NE(json.find("\"schema\":\"phantom.scene/1\""), std::string::npos);

    bool ok = false;
    SceneGraph loaded = SceneGraph::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(loaded.size(), 2u);

    const SceneNode* loadedRoot = loaded.find(NodeId("root"));
    ASSERT_NE(loadedRoot, nullptr);
    EXPECT_EQ(loadedRoot->name, "Root");
    EXPECT_FALSE(loadedRoot->enabled);
    EXPECT_EQ(loadedRoot->local, root.local);
    ASSERT_EQ(loadedRoot->components.size(), 1u);
    EXPECT_EQ(loadedRoot->components[0], mesh);
    EXPECT_FALSE(loadedRoot->parent.isValid());

    const SceneNode* loadedChild = loaded.find(NodeId("child"));
    ASSERT_NE(loadedChild, nullptr);
    EXPECT_EQ(loadedChild->parent, NodeId("root"));
}

TEST(SceneGraph, JsonRoundTripHandlesForwardReferencedParent)
{
    // Hand-write a "child before parent" payload -- fromJson() must not depend on array order.
    const std::string json = R"({"schema":"phantom.scene/1","nodes":[)"
        R"({"id":"child","parent":"root","name":"Child","enabled":true,)"
        R"("transform":{"pos":[0,0,0],"rot":[0,0,0,1],"scale":[1,1,1]},"components":[]},)"
        R"({"id":"root","parent":"","name":"Root","enabled":true,)"
        R"("transform":{"pos":[0,0,0],"rot":[0,0,0,1],"scale":[1,1,1]},"components":[]})"
        R"(]})";

    bool ok = false;
    SceneGraph g = SceneGraph::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(g.size(), 2u);
    EXPECT_EQ(g.find(NodeId("child"))->parent, NodeId("root"));
}

TEST(SceneGraph, FromJsonRejectsWrongSchema)
{
    bool ok = true;
    SceneGraph g = SceneGraph::fromJson(R"({"schema":"something.else/1","nodes":[]})", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(g.size(), 0u);
}

TEST(SceneGraph, FromJsonRejectsGarbage)
{
    bool ok = true;
    SceneGraph g = SceneGraph::fromJson("not json at all", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(g.size(), 0u);
}

TEST(SceneGraph, FromJsonRejectsUnknownParent)
{
    const std::string json = R"({"schema":"phantom.scene/1","nodes":[)"
        R"({"id":"a","parent":"missing","name":"A","enabled":true,)"
        R"("transform":{"pos":[0,0,0],"rot":[0,0,0,1],"scale":[1,1,1]},"components":[]})"
        R"(]})";
    bool ok = true;
    SceneGraph g = SceneGraph::fromJson(json, &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(g.size(), 0u);
}

TEST(SceneGraph, SaveLoadFileRoundTrip)
{
    SceneGraph g;
    ASSERT_TRUE(g.addNode(makeNode("root", "Root")));
    const std::filesystem::path dir = tempDir();
    const std::filesystem::path path = dir / "scene.json";

    ASSERT_TRUE(g.saveToFile(path));
    bool ok = false;
    SceneGraph loaded = SceneGraph::loadFromFile(path, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_NE(loaded.find(NodeId("root")), nullptr);

    std::filesystem::remove_all(dir);
}

TEST(SceneGraph, LoadFromFileMissingFileFails)
{
    bool ok = true;
    SceneGraph g = SceneGraph::loadFromFile("Z:/does/not/exist.json", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(g.size(), 0u);
}
