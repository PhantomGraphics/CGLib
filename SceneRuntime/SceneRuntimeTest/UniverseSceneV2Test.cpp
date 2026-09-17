#include "gtest/gtest.h"

#include "../SceneRuntime/UniverseSceneV2.h"

#include <algorithm>

using namespace Phantom::SceneRuntime;
using namespace Phantom::SceneRuntime::Universe;

namespace {

bool hasComponent(const SceneNode& node, const std::string& type)
{
    return std::any_of(node.components.begin(), node.components.end(),
        [&](const ComponentRecord& c) { return c.type == type; });
}

const ComponentRecord* findComponent(const SceneNode& node, const std::string& type)
{
    for (const auto& c : node.components) {
        if (c.type == type) return &c;
    }
    return nullptr;
}

} // namespace

TEST(SceneV2, JsonRoundTrip)
{
    SceneV2 v2;
    Phantom::Asset::AssetManifestEntry entry;
    entry.id = Phantom::Asset::AssetId("asset-1");
    entry.uri = *Phantom::Asset::AssetUri::parse("Assets/Generated/room.glb");
    v2.assets.upsert(entry);

    SceneNode node;
    node.id = NodeId("node-1");
    node.name = "Room";
    ASSERT_TRUE(v2.scene.addNode(node));

    v2.physics = { { "gravity", { 0.0, -9.8, 0.0 } } };
    v2.renderSettings = { { "shadowEnabled", true } };

    const std::string json = v2.toJson();
    EXPECT_NE(json.find("\"version\":2"), std::string::npos);

    bool ok = false;
    SceneV2 loaded = SceneV2::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(loaded.assets.size(), 1u);
    EXPECT_NE(loaded.assets.find(Phantom::Asset::AssetId("asset-1")), nullptr);
    EXPECT_EQ(loaded.scene.size(), 1u);
    EXPECT_NE(loaded.scene.find(NodeId("node-1")), nullptr);
    EXPECT_EQ(loaded.physics.value("gravity", nlohmann::json::array()), v2.physics["gravity"]);
    EXPECT_TRUE(loaded.renderSettings.value("shadowEnabled", false));
}

TEST(SceneV2, FromJsonRejectsWrongVersion)
{
    bool ok = true;
    SceneV2 loaded = SceneV2::fromJson(R"({"version":1,"assets":{},"scene":{}})", &ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(loaded.scene.size(), 0u);
}

TEST(SceneV2, FromJsonRejectsGarbage)
{
    bool ok = true;
    SceneV2 loaded = SceneV2::fromJson("not json", &ok);
    EXPECT_FALSE(ok);
}

TEST(MigrateV1ToV2, PrimitiveMeshEntityMigrates)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            {
                "name": "Floor", "visible": true,
                "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] },
                "mesh": { "source": "primitive", "shape": "plane", "color": [0.5,0.5,0.55] },
                "rigidBody": { "mass": 0, "shape": "plane", "size": 0.5, "restitution": 0.3, "friction": 0.5 }
            }
        ],
        "physics": { "gravity": [0, -9.8, 0] },
        "renderSettings": { "shadowEnabled": true }
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(result.scene.scene.size(), 1u);

    std::vector<NodeId> roots = result.scene.scene.children(NodeId());
    ASSERT_EQ(roots.size(), 1u);
    const SceneNode* node = result.scene.scene.find(roots[0]);
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->id.isValid()); // freshly minted, v1 never had one
    EXPECT_EQ(node->name, "Floor");
    EXPECT_TRUE(node->enabled);

    EXPECT_TRUE(hasComponent(*node, "mesh"));
    EXPECT_TRUE(hasComponent(*node, "rigidBody"));
    EXPECT_FALSE(hasComponent(*node, "meshAsset")); // primitive has no backing asset file

    const ComponentRecord* mesh = findComponent(*node, "mesh");
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->data.value("shape", ""), "plane");

    EXPECT_EQ(result.scene.physics.value("gravity", nlohmann::json::array()), nlohmann::json({ 0, -9.8, 0 }));
    EXPECT_TRUE(result.scene.renderSettings.value("shadowEnabled", false));
}

TEST(MigrateV1ToV2, GltfMeshEntityBecomesAssetReference)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            {
                "name": "GltfHolder", "visible": true,
                "transform": { "pos": [1,0.5,-1], "rot": [0,0,0,1], "scale": [1,1,1] },
                "mesh": { "source": "gltf", "path": "scenarios/models/triangle.gltf" }
            }
        ]
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(result.scene.scene.size(), 1u);
    EXPECT_EQ(result.scene.assets.size(), 1u);

    const SceneNode* node = result.scene.scene.find(result.scene.scene.children(NodeId())[0]);
    ASSERT_NE(node, nullptr);
    const ComponentRecord* meshAsset = findComponent(*node, "meshAsset");
    ASSERT_NE(meshAsset, nullptr);
    EXPECT_FALSE(hasComponent(*node, "mesh")); // asset-backed, not inline

    const std::string assetId = meshAsset->data.value("assetId", "");
    EXPECT_FALSE(assetId.empty());
    const auto* entry = result.scene.assets.find(Phantom::Asset::AssetId(assetId));
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->uri.value(), "scenarios/models/triangle.gltf");
}

TEST(MigrateV1ToV2, EmbeddedMeshStaysInline)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            {
                "name": "EmbeddedTriangle", "visible": true,
                "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] },
                "mesh": {
                    "source": "embedded",
                    "vertices": [[0,0,0, 0,1,0, 0,0], [1,0,0, 0,1,0, 1,0], [0,0,1, 0,1,0, 0,1]],
                    "indices": [0,2,1],
                    "material": { "baseColor": [0.9,0.3,0.3], "metallic": 0.0, "roughness": 0.6 }
                }
            }
        ]
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.scene.assets.size(), 0u); // embedded has no backing asset file, by design
    const SceneNode* node = result.scene.scene.find(result.scene.scene.children(NodeId())[0]);
    ASSERT_NE(node, nullptr);
    const ComponentRecord* mesh = findComponent(*node, "mesh");
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->data.value("source", ""), "embedded");
    ASSERT_TRUE(mesh->data.contains("vertices"));
    EXPECT_EQ(mesh->data["vertices"].size(), 3u);
}

TEST(MigrateV1ToV2, ClothAndFluidBecomeComponents)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            {
                "name": "Cloth", "visible": true,
                "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] },
                "cloth": { "rows": 20, "cols": 20 }
            },
            {
                "name": "Fluid", "visible": true,
                "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] },
                "fluid": { "type": "wcsph", "spacing": 0.1 }
            }
        ]
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.scene.scene.size(), 2u);

    std::vector<NodeId> roots = result.scene.scene.children(NodeId());
    ASSERT_EQ(roots.size(), 2u);
    bool sawCloth = false, sawFluid = false;
    for (const auto& id : roots) {
        const SceneNode* node = result.scene.scene.find(id);
        if (node->name == "Cloth") {
            sawCloth = true;
            const ComponentRecord* c = findComponent(*node, "cloth");
            ASSERT_NE(c, nullptr);
            EXPECT_EQ(c->data.value("rows", 0), 20);
        } else if (node->name == "Fluid") {
            sawFluid = true;
            const ComponentRecord* c = findComponent(*node, "fluid");
            ASSERT_NE(c, nullptr);
            EXPECT_EQ(c->data.value("type", ""), "wcsph");
        }
    }
    EXPECT_TRUE(sawCloth);
    EXPECT_TRUE(sawFluid);
}

TEST(MigrateV1ToV2, TwoEntitiesGetDistinctFreshIds)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            { "name": "A", "visible": true, "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] } },
            { "name": "B", "visible": true, "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] } }
        ]
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.scene.scene.size(), 2u);
    std::vector<NodeId> roots = result.scene.scene.children(NodeId());
    ASSERT_EQ(roots.size(), 2u);
    EXPECT_NE(roots[0], roots[1]);
    EXPECT_TRUE(roots[0].isValid());
    EXPECT_TRUE(roots[1].isValid());
}

TEST(MigrateV1ToV2, AbsoluteGltfPathIsDiagnosedNotSilentlyDropped)
{
    const std::string v1 = R"({
        "version": 1,
        "entities": [
            {
                "name": "BadAsset", "visible": true,
                "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [1,1,1] },
                "mesh": { "source": "gltf", "path": "C:/absolute/model.glb" }
            }
        ]
    })";

    MigrationResult result = migrateV1ToV2(v1);
    ASSERT_TRUE(result.ok); // the document as a whole still migrates
    ASSERT_EQ(result.scene.scene.size(), 1u); // the entity itself is not dropped
    EXPECT_EQ(result.scene.assets.size(), 0u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_NE(result.diagnostics[0].find("BadAsset"), std::string::npos);

    const SceneNode* node = result.scene.scene.find(result.scene.scene.children(NodeId())[0]);
    ASSERT_NE(node, nullptr);
    EXPECT_FALSE(hasComponent(*node, "meshAsset"));
}

TEST(MigrateV1ToV2, RejectsWrongVersion)
{
    MigrationResult result = migrateV1ToV2(R"({"version":2,"entities":[]})");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST(MigrateV1ToV2, RejectsMissingEntities)
{
    MigrationResult result = migrateV1ToV2(R"({"version":1})");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.diagnostics.empty());
}

TEST(MigrateV1ToV2, RejectsGarbage)
{
    MigrationResult result = migrateV1ToV2("not json at all");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.diagnostics.empty());
}
