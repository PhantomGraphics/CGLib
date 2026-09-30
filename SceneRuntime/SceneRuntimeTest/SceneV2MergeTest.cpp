#include "gtest/gtest.h"

#include "../SceneRuntime/SceneV2Merge.h"

#include <fstream>

using namespace Phantom::SceneRuntime;
using namespace Phantom::SceneRuntime::Universe;
using Json = nlohmann::json;

namespace {

Json body(double mass, double friction)
{
    return { { "version", 3 }, { "mass", mass }, { "restitution", 0.1 }, { "friction", friction },
             { "twoWay", false },
             { "collider", { { "shape", "box" }, { "halfExtents", { 0.5, 0.5, 0.5 } } } } };
}

struct Fixture {
    std::filesystem::path dir;
    std::filesystem::path scene;
    Json gltfNodes = Json::array();
    Json sidecarObjects = Json::array();

    Fixture()
    {
        dir = std::filesystem::temp_directory_path() / ("v2merge_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(dir);
        scene = dir / "scene.universe";
    }
    ~Fixture() { std::error_code ec; std::filesystem::remove_all(dir, ec); }

    void node(const std::string& name, double y, const std::vector<int>& children = {})
    {
        Json n = { { "name", name }, { "translation", { 0, y, 0 } } };
        if (!children.empty()) n["children"] = children;
        gltfNodes.push_back(n);
    }
    void object(const std::string& name, const std::string& uuid, const Json* rigidBody)
    {
        Json o = { { "name", name }, { "uuid", uuid }, { "type", "MESH" } };
        if (rigidBody) o["components"] = { { "rigidBody", *rigidBody } };
        sidecarObjects.push_back(o);
    }
    void write()
    {
        std::ofstream(dir / "asset.gltf") << Json({ { "asset", { { "version", "2.0" } } }, { "nodes", gltfNodes } }).dump();
        std::ofstream(dir / "asset.phantom.json")
            << Json({ { "schema", "phantom.asset/1" }, { "objects", sidecarObjects } }).dump();
    }
    void clear() { gltfNodes = Json::array(); sidecarObjects = Json::array(); }
};

// What Universe saves right after importing the fixture's asset.
SceneV2 importedDoc(const Fixture& f)
{
    SceneV2 doc;
    doc.assetRoot = ".";
    Phantom::Asset::AssetManifestEntry entry;
    entry.id = Phantom::Asset::AssetId("asset-1");
    entry.uri = *Phantom::Asset::AssetUri::parse("asset.gltf");
    doc.assets.upsert(entry);

    std::vector<SidecarObject> objects;
    std::string diag;
    EXPECT_TRUE(readSidecarObjects(f.dir / "asset.phantom.json", objects, diag)) << diag;
    std::unordered_map<std::string, Transform> worlds;
    EXPECT_TRUE(readGltfNodeWorldTransforms(f.dir / "asset.gltf", worlds, diag)) << diag;
    for (const auto& o : objects) {
        SceneNode n;
        n.id = NodeId(o.uuid);
        n.name = o.name;
        n.local = worlds.at(o.name);
        n.components.push_back({ "meshAsset", { { "assetId", "asset-1" }, { "nodeId", o.uuid }, { "nodeName", o.name },
                                                 { "authoredTransform", transformToJson(worlds.at(o.name)) } } });
        if (o.rigidBody) n.components.push_back({ "rigidBody", rigidBodyComponentData(*o.rigidBody, worlds.at(o.name), o.name) });
        EXPECT_TRUE(doc.scene.addNode(n));
    }
    return doc;
}

const SceneNode* byName(const SceneV2& doc, const std::string& name)
{
    for (const SceneNode* n : listNodes(doc)) if (n->name == name) return n;
    return nullptr;
}

const Json* comp(const SceneNode& n, const std::string& type)
{
    for (const auto& c : n.components) if (c.type == type) return &c.data;
    return nullptr;
}

double mass(const SceneNode& n) { return (*comp(n, "rigidBody"))["mass"].get<double>(); }

} // namespace

TEST(SceneV2Merge, NoSidecarLeavesDocumentUntouched)
{
    Fixture f;
    f.node("Crate", 1);
    f.object("Crate", "u-crate", nullptr);
    f.write();
    SceneV2 doc = importedDoc(f);
    std::filesystem::remove(f.dir / "asset.phantom.json");
    const std::string before = doc.toJson();
    MergeReport report;
    EXPECT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_TRUE(report.empty());
    EXPECT_EQ(doc.toJson(), before);
}

TEST(SceneV2Merge, UnchangedReexportIsNoOpAndRepeatable)
{
    Fixture f;
    const Json rb = body(2, 0.5);
    f.node("Crate", 1);
    f.object("Crate", "u-crate", &rb);
    f.write();
    SceneV2 doc = importedDoc(f);
    MergeReport report;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_TRUE(report.empty());
    EXPECT_NEAR(byName(doc, "Crate")->local.translation.y, 1.f, 1e-5f);
}

TEST(SceneV2Merge, RemoteChangeIsTakenLocalOverrideIsKeptBothChangedConflicts)
{
    Fixture f;
    const Json rbCrate = body(2, 0.5), rbBall = body(1, 0.5);
    f.node("Crate", 1);
    f.node("Ball", 2);
    f.object("Crate", "u-crate", &rbCrate);
    f.object("Ball", "u-ball", &rbBall);
    f.write();
    SceneV2 doc = importedDoc(f);

    // Studio-style local edits: Crate moved + mass 3 + friction 0.9; Ball untouched.
    SceneNode* crate = doc.scene.find(NodeId("u-crate"));
    crate->local.translation.y = 5.f;
    for (auto& c : crate->components) if (c.type == "rigidBody") { c.data["mass"] = 3.0; c.data["friction"] = 0.9; }

    // Blender re-export: Crate mass 2 -> 2.5 (conflict), Crate moves to y=4 (conflict),
    // Ball friction 0.5 -> 0.7 and Ball moves to y=3 (taken).
    f.clear();
    const Json rbCrate2 = body(2.5, 0.5), rbBall2 = body(1, 0.7);
    f.node("Crate", 4);
    f.node("Ball", 3);
    f.object("Crate", "u-crate", &rbCrate2);
    f.object("Ball", "u-ball", &rbBall2);
    f.write();

    MergeReport report;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_NEAR(byName(doc, "Crate")->local.translation.y, 5.f, 1e-5f);
    EXPECT_NEAR(mass(*byName(doc, "Crate")), 3.0, 1e-9);
    EXPECT_NEAR((*comp(*byName(doc, "Crate"), "rigidBody"))["friction"].get<double>(), 0.9, 1e-6);
    EXPECT_NEAR((*comp(*byName(doc, "Crate"), "rigidBody"))["authored"]["mass"].get<double>(), 2.5, 1e-9);
    EXPECT_NEAR((*comp(*byName(doc, "Crate"), "meshAsset"))["authoredTransform"]["pos"][1].get<double>(), 4.0, 1e-6);
    EXPECT_NEAR(byName(doc, "Ball")->local.translation.y, 3.f, 1e-5f);
    EXPECT_NEAR((*comp(*byName(doc, "Ball"), "rigidBody"))["friction"].get<double>(), 0.7, 1e-6);
    EXPECT_EQ(report.count("conflicts"), 2); // Crate.transform, Crate.rigidBody.mass

    // The same re-export again changes nothing and reports no conflict.
    MergeReport again;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, again));
    EXPECT_EQ(again.count("conflicts"), 0) << "the base moved to the re-exported value";
    EXPECT_EQ(again.count("renamed"), 0);
    EXPECT_EQ(again.count("added"), 0);
    EXPECT_EQ(again.count("removed"), 0);
}

TEST(SceneV2Merge, RenameAddRemoveFollowUuidAndChildrenKeepWorld)
{
    Fixture f;
    const Json rb = body(2, 0.5);
    f.node("Crate", 1);
    f.node("Marker", 0);
    f.node("Ball", 2);
    f.object("Crate", "u-crate", &rb);
    f.object("Marker", "u-marker", nullptr);
    f.object("Ball", "u-ball", &rb);
    f.write();
    SceneV2 doc = importedDoc(f);
    // Ball is parented to Marker in the document.
    ASSERT_TRUE(doc.scene.reparent(NodeId("u-ball"), NodeId("u-marker")));
    doc.scene.find(NodeId("u-ball"))->local.translation = { 0.f, 2.f, 0.f };

    f.clear();
    f.node("Crate_Renamed", 1);
    f.node("Ball", 2);
    f.node("New_Ball", 7);
    f.object("Crate_Renamed", "u-crate", &rb);
    f.object("Ball", "u-ball", &rb);
    f.object("New_Ball", "u-new", &rb);
    f.write();

    MergeReport report;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_EQ(report.renamed, std::vector<std::string>{ "Crate -> Crate_Renamed" });
    EXPECT_EQ(report.added, (std::vector<std::string>{ "New_Ball", "New_Ball.rigidBody" }));
    EXPECT_EQ(report.removed, std::vector<std::string>{ "Marker" });
    EXPECT_EQ(byName(doc, "Crate"), nullptr);
    EXPECT_EQ(byName(doc, "Crate_Renamed")->id.value(), "u-crate");
    EXPECT_EQ(byName(doc, "Marker"), nullptr);
    const SceneNode* ball = byName(doc, "Ball");
    ASSERT_NE(ball, nullptr);
    EXPECT_FALSE(ball->parent.isValid()) << "deleted parent must not dangle";
    EXPECT_NEAR(ball->local.translation.y, 2.f, 1e-5f) << "world transform kept";
    const SceneNode* added = byName(doc, "New_Ball");
    ASSERT_NE(added, nullptr);
    EXPECT_EQ(added->id.value(), "u-new");
    EXPECT_NEAR(added->local.translation.y, 7.f, 1e-5f);
    EXPECT_NE(comp(*added, "rigidBody"), nullptr);
}

TEST(SceneV2Merge, RemovedRigidBodyDropsUnlessOverridden)
{
    Fixture f;
    const Json rb = body(2, 0.5);
    f.node("Crate", 1);
    f.node("Ball", 2);
    f.object("Crate", "u-crate", &rb);
    f.object("Ball", "u-ball", &rb);
    f.write();
    SceneV2 doc = importedDoc(f);
    for (auto& c : doc.scene.find(NodeId("u-ball"))->components) if (c.type == "rigidBody") c.data["mass"] = 9.0;

    f.clear();
    f.node("Crate", 1);
    f.node("Ball", 2);
    f.object("Crate", "u-crate", nullptr);
    f.object("Ball", "u-ball", nullptr);
    f.write();
    MergeReport report;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_EQ(comp(*byName(doc, "Crate"), "rigidBody"), nullptr);
    ASSERT_NE(comp(*byName(doc, "Ball"), "rigidBody"), nullptr);
    EXPECT_NEAR(mass(*byName(doc, "Ball")), 9.0, 1e-9);
}

TEST(SceneV2Merge, ChildOfMovedAssetNodeKeepsItsWorldTransform)
{
    Fixture f;
    f.node("Marker", 1);
    f.object("Marker", "u-marker", nullptr);
    f.write();
    SceneV2 doc = importedDoc(f);
    SceneNode child;
    child.id = NodeId("u-child");
    child.name = "Child";
    child.parent = NodeId("u-marker");
    child.local.translation = { 0.f, 0.5f, 0.f }; // world y = 1.5
    ASSERT_TRUE(doc.scene.addNode(child));

    f.clear();
    f.node("Marker", 10);
    f.object("Marker", "u-marker", nullptr);
    f.write();
    MergeReport report;
    ASSERT_TRUE(mergeReexportedAssets(doc, f.scene, report));
    EXPECT_NEAR(byName(doc, "Marker")->local.translation.y, 10.f, 1e-5f);
    const auto world = doc.scene.worldTransform(NodeId("u-child"));
    ASSERT_TRUE(world.has_value());
    EXPECT_NEAR((*world)[3][1], 1.5f, 1e-4f) << "non-asset child keeps its world position";
}

TEST(SceneV2Merge, InvalidReexportFailsAndLeavesDocumentUnchanged)
{
    Fixture f;
    const Json rb = body(2, 0.5);
    f.node("Crate", 1);
    f.node("Ball", 2);
    f.object("Crate", "u-crate", &rb);
    f.object("Ball", "u-ball", &rb);
    f.write();
    SceneV2 doc = importedDoc(f);
    const std::string before = doc.toJson();

    f.object("Extra", "u-crate", nullptr); // duplicate uuid
    f.node("Extra", 0);
    f.write();
    MergeReport report;
    std::string error;
    EXPECT_FALSE(mergeReexportedAssets(doc, f.scene, report, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(doc.toJson(), before);

    f.clear();
    f.node("Crate", 1); // Ball missing from the glTF
    f.object("Crate", "u-crate", &rb);
    f.object("Ball", "u-ball", &rb);
    f.write();
    EXPECT_FALSE(mergeReexportedAssets(doc, f.scene, report, &error));
    EXPECT_EQ(doc.toJson(), before);

    f.clear();
    f.node("Crate", 1);
    Json future = body(2, 0.5);
    future["version"] = 99;
    f.object("Crate", "u-crate", &future);
    f.write();
    EXPECT_FALSE(mergeReexportedAssets(doc, f.scene, report, &error));
    EXPECT_NE(error.find("not supported"), std::string::npos);
    EXPECT_EQ(doc.toJson(), before);
}

TEST(SceneV2Merge, GlbNodeWorldTransformsComposeParentChain)
{
    Fixture f;
    f.gltfNodes.push_back({ { "name", "Root" }, { "translation", { 1, 0, 0 } }, { "children", { 1 } } });
    f.gltfNodes.push_back({ { "name", "Leaf" }, { "translation", { 0, 2, 0 } }, { "scale", { 2, 2, 2 } } });
    f.write();
    std::unordered_map<std::string, Transform> worlds;
    std::string diag;
    ASSERT_TRUE(readGltfNodeWorldTransforms(f.dir / "asset.gltf", worlds, diag)) << diag;
    EXPECT_NEAR(worlds.at("Leaf").translation.x, 1.f, 1e-5f);
    EXPECT_NEAR(worlds.at("Leaf").translation.y, 2.f, 1e-5f);
    EXPECT_NEAR(worlds.at("Leaf").scale.x, 2.f, 1e-5f);
    EXPECT_FALSE(readGltfNodeWorldTransforms(f.dir / "missing.gltf", worlds, diag));
}
