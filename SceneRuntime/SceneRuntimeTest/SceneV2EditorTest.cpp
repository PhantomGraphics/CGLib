#include "gtest/gtest.h"

#include "../SceneRuntime/SceneV2Editor.h"

using namespace Phantom::SceneRuntime;
using namespace Phantom::SceneRuntime::Universe;

namespace {

// Roughly what Universe saves for a Blender-imported rigid body: a meshAsset reference, a
// rigidBody with its "authored" base copy, an authoredTransform, and fields Studio knows nothing
// about ("fitToBounds", "futureField").
SceneV2 makeDoc()
{
    SceneV2 doc;
    doc.assetRoot = "..";
    Phantom::Asset::AssetManifestEntry entry;
    entry.id = Phantom::Asset::AssetId("asset-1");
    entry.uri = *Phantom::Asset::AssetUri::parse("Assets/Generated/scene.glb");
    doc.assets.upsert(entry);

    SceneNode parent;
    parent.id = NodeId("uuid-root");
    parent.name = "Root";
    doc.scene.addNode(parent);

    SceneNode crate;
    crate.id = NodeId("uuid-crate");
    crate.parent = parent.id;
    crate.name = "Crate";
    crate.components.push_back({ "meshAsset", { { "assetId", "asset-1" }, { "nodeId", "Crate" },
                                                { "authoredTransform", { { "translation", { 1, 2, 3 } } } } } });
    crate.components.push_back({ "rigidBody",
        { { "mass", 2.0 }, { "friction", 0.5 }, { "restitution", 0.1 }, { "twoWay", false },
          { "fitToBounds", true }, { "futureField", { { "a", 1 } } },
          { "authored", { { "mass", 2.0 }, { "friction", 0.5 } } } } });
    doc.scene.addNode(crate);

    SceneNode crate2 = crate;
    crate2.id = NodeId("uuid-crate2");
    crate2.name = "Crate";
    doc.scene.addNode(crate2);

    SceneNode lamp;
    lamp.id = NodeId("uuid-lamp");
    lamp.name = "Lamp";
    doc.scene.addNode(lamp);
    return doc;
}

const ComponentRecord* comp(const SceneNode& n, const std::string& type)
{
    for (const auto& c : n.components) if (c.type == type) return &c;
    return nullptr;
}

} // namespace

TEST(SceneV2Editor, FindByUuidNameAndAmbiguity)
{
    SceneV2 doc = makeDoc();
    EditResult err;
    EXPECT_NE(findNodeByRef(doc, "uuid-lamp", &err), nullptr);
    EXPECT_NE(findNodeByRef(doc, "Lamp", &err), nullptr);
    EXPECT_NE(findNodeByRef(doc, "uuid-crate", &err), nullptr);

    EXPECT_EQ(findNodeByRef(doc, "Crate", &err), nullptr);
    EXPECT_EQ(err.code, "ambiguous");
    EXPECT_EQ(findNodeByRef(doc, "Nope", &err), nullptr);
    EXPECT_EQ(err.code, "not_found");
    EXPECT_EQ(findNodeByRef(doc, "", &err), nullptr);
}

TEST(SceneV2Editor, ListIsDepthFirstAndSorted)
{
    SceneV2 doc = makeDoc();
    const auto nodes = listNodes(doc);
    ASSERT_EQ(nodes.size(), 4u);
    EXPECT_EQ(nodes[0]->name, "Lamp");
    EXPECT_EQ(nodes[1]->name, "Root");
    EXPECT_EQ(nodes[2]->name, "Crate");
    EXPECT_EQ(nodes[3]->name, "Crate");
    EXPECT_EQ(nodes[2]->parent.value(), "uuid-root");
}

TEST(SceneV2Editor, DescribeIncludesAssetUriAndRigidBody)
{
    SceneV2 doc = makeDoc();
    const auto j = describeNode(doc, *doc.scene.find(NodeId("uuid-crate")));
    EXPECT_EQ(j["meshAsset"]["uri"], "Assets/Generated/scene.glb");
    EXPECT_EQ(j["rigidBody"]["mass"], 2.0);
    EXPECT_EQ(j["parent"], "uuid-root");
    EXPECT_EQ(describeDocument(doc)["nodes"].size(), 4u);
}

TEST(SceneV2Editor, TransformEditKeepsAuthoredBaseAndSurvivesSaveLoad)
{
    SceneV2 doc = makeDoc();
    TransformEdit e;
    e.translation = Phantom::Math::Vector3df(4.f, 5.f, 6.f);
    e.rotationEulerDeg = Phantom::Math::Vector3df(0.f, 90.f, 0.f);
    ASSERT_TRUE(setNodeTransform(doc, "uuid-crate", e).ok);

    bool ok = false;
    const SceneV2 loaded = SceneV2::fromJson(doc.toJson(), &ok);
    ASSERT_TRUE(ok);
    const SceneNode* n = loaded.scene.find(NodeId("uuid-crate"));
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->local.translation, (Phantom::Math::Vector3df{ 4.f, 5.f, 6.f }));
    EXPECT_EQ(describeNode(loaded, *n)["rotationEulerDeg"][1].get<float>() > 89.f, true);
    // Asset reference and authored base are untouched.
    const auto* mesh = comp(*n, "meshAsset");
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->data["assetId"], "asset-1");
    EXPECT_EQ(mesh->data["authoredTransform"]["translation"], nlohmann::json::array({ 1, 2, 3 }));
    EXPECT_EQ(loaded.assets.size(), 1u);
}

TEST(SceneV2Editor, TransformEditRejectsBadValuesWithoutChange)
{
    SceneV2 doc = makeDoc();
    const std::string before = doc.toJson();
    TransformEdit zero;
    zero.scale = Phantom::Math::Vector3df(1.f, 0.f, 1.f);
    EXPECT_EQ(setNodeTransform(doc, "Lamp", zero).code, "out_of_range");
    TransformEdit inf;
    inf.translation = Phantom::Math::Vector3df(INFINITY, 0.f, 0.f);
    EXPECT_FALSE(setNodeTransform(doc, "Lamp", inf).ok);
    EXPECT_EQ(setNodeTransform(doc, "Crate", TransformEdit{}).code, "ambiguous");
    EXPECT_EQ(doc.toJson(), before);
}

TEST(SceneV2Editor, RigidBodyFieldEditKeepsUnknownFieldsAndAuthoredCopy)
{
    SceneV2 doc = makeDoc();
    ASSERT_TRUE(setRigidBodyField(doc, "uuid-crate", "mass", "5").ok);
    ASSERT_TRUE(setRigidBodyField(doc, "uuid-crate", "friction", "0.9").ok);
    ASSERT_TRUE(setRigidBodyField(doc, "uuid-crate", "twoWay", "true").ok);

    const auto* rb = comp(*doc.scene.find(NodeId("uuid-crate")), "rigidBody");
    ASSERT_NE(rb, nullptr);
    EXPECT_EQ(rb->data["mass"], 5.0);
    EXPECT_EQ(rb->data["friction"], 0.9);
    EXPECT_EQ(rb->data["twoWay"], true);
    EXPECT_EQ(rb->data["authored"]["mass"], 2.0);   // base, so Universe's merge sees an override
    EXPECT_EQ(rb->data["fitToBounds"], true);
    EXPECT_EQ(rb->data["futureField"]["a"], 1);
}

TEST(SceneV2Editor, RigidBodyFieldEditValidation)
{
    SceneV2 doc = makeDoc();
    const std::string before = doc.toJson();
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "mass", "-1").code, "out_of_range");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "mass", "abc").code, "out_of_range");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "mass", "nan").code, "out_of_range");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "restitution", "1.5").code, "out_of_range");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "twoWay", "maybe").code, "out_of_range");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-crate", "shape", "box").code, "unknown_field");
    EXPECT_EQ(setRigidBodyField(doc, "uuid-lamp", "mass", "1").code, "no_component");
    EXPECT_EQ(setRigidBodyField(doc, "missing", "mass", "1").code, "not_found");
    EXPECT_EQ(doc.toJson(), before);
}
