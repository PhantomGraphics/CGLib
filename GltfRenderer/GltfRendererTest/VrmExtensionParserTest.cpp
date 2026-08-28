#include "gtest/gtest.h"

#include "../Vrm/VrmExtensionParser.h"

using namespace Phantom::Gltf;
using Json = nlohmann::json;

namespace {

// nlohmann::json::parse() throws on malformed input by default; this project doesn't use
// exceptions in its own code, but these are hand-written literals that are always
// well-formed JSON, so the non-throwing overload here is purely defensive (matches
// VrmExtensionParser.cpp's own parsing convention).
Json parseJson(const char* text) {
    Json j = Json::parse(text, nullptr, /*allow_exceptions=*/false);
    EXPECT_FALSE(j.is_discarded());
    return j;
}

// Two nodes: node 0 (no mesh, e.g. "hips"), node 1 (meshIndex=0, e.g. "head") -- enough to
// exercise VRM 0.x's bind.mesh -> node resolution without needing real primitive data.
GltfDocument makeTwoNodeDoc() {
    GltfDocument doc;
    GltfNode hips;
    hips.name = "hips";
    hips.children = { 1 };
    GltfNode head;
    head.name = "head";
    head.meshIndex = 0;
    doc.nodes.push_back(hips);
    doc.nodes.push_back(head);
    GltfMesh mesh;
    mesh.name = "HeadMesh";
    doc.meshes.push_back(mesh);
    return doc;
}

} // namespace

TEST(VrmExtensionParserTest, ParsesV1Humanoid)
{
    const Json root = parseJson(R"JSON({
      "humanoid": { "humanBones": { "hips": {"node":0}, "head": {"node":1} } }
    })JSON");

    const VrmHumanoid h = VrmExtensionParser::parseHumanoid(root, VrmSpecVersion::V1);
    ASSERT_EQ(2u, h.boneNameToNode.size());
    EXPECT_EQ(0, h.boneNameToNode.at("hips"));
    EXPECT_EQ(1, h.boneNameToNode.at("head"));
}

TEST(VrmExtensionParserTest, ParsesV0Humanoid)
{
    const Json root = parseJson(R"JSON({
      "humanoid": { "humanBones": [ {"bone":"hips","node":0}, {"bone":"head","node":1} ] }
    })JSON");

    const VrmHumanoid h = VrmExtensionParser::parseHumanoid(root, VrmSpecVersion::V0);
    ASSERT_EQ(2u, h.boneNameToNode.size());
    EXPECT_EQ(0, h.boneNameToNode.at("hips"));
    EXPECT_EQ(1, h.boneNameToNode.at("head"));
}

TEST(VrmExtensionParserTest, ParsesV1PresetAndCustomExpressions)
{
    const GltfDocument doc = makeTwoNodeDoc();
    const Json root = parseJson(R"JSON({
      "expressions": {
        "preset": {
          "happy": { "morphTargetBinds": [ {"node":1,"index":0,"weight":1.0} ], "isBinary": false }
        },
        "custom": {
          "myFace": { "morphTargetBinds": [ {"node":1,"index":1,"weight":0.5} ] }
        }
      }
    })JSON");

    const auto exprs = VrmExtensionParser::parseExpressions(root, VrmSpecVersion::V1, doc);
    ASSERT_EQ(2u, exprs.size());

    const VrmExpression* happy = nullptr;
    const VrmExpression* custom = nullptr;
    for (const auto& e : exprs) {
        if (e.name == "happy") happy = &e;
        if (e.name == "myFace") custom = &e;
    }
    ASSERT_NE(nullptr, happy);
    EXPECT_EQ("happy", happy->presetName);
    ASSERT_EQ(1u, happy->binds.size());
    EXPECT_EQ(1, happy->binds[0].node);
    EXPECT_EQ(0, happy->binds[0].targetIndex);
    EXPECT_FLOAT_EQ(1.0f, happy->binds[0].weight);

    ASSERT_NE(nullptr, custom);
    EXPECT_TRUE(custom->presetName.empty());
    ASSERT_EQ(1u, custom->binds.size());
    EXPECT_FLOAT_EQ(0.5f, custom->binds[0].weight);
}

TEST(VrmExtensionParserTest, ParsesV0ExpressionsAndNormalizesWeight)
{
    const GltfDocument doc = makeTwoNodeDoc();
    const Json root = parseJson(R"JSON({
      "blendShapeMaster": {
        "blendShapeGroups": [
          {
            "name": "Blink",
            "presetName": "blink",
            "binds": [ {"mesh":1,"index":0,"weight":100.0} ],
            "isBinary": false
          }
        ]
      }
    })JSON");

    const auto exprs = VrmExtensionParser::parseExpressions(root, VrmSpecVersion::V0, doc);
    ASSERT_EQ(1u, exprs.size());
    EXPECT_EQ("Blink", exprs[0].name);
    EXPECT_EQ("blink", exprs[0].presetName);
    ASSERT_EQ(1u, exprs[0].binds.size());
    EXPECT_EQ(1, exprs[0].binds[0].node); // "mesh":1 resolves directly (node 1 owns a mesh)
    EXPECT_EQ(0, exprs[0].binds[0].targetIndex);
    EXPECT_FLOAT_EQ(1.0f, exprs[0].binds[0].weight); // 100 / 100 = 1.0
}

TEST(VrmExtensionParserTest, ResolvesV0MeshIndexBindWhenNotANode)
{
    // Only 1 node exists, so bind.mesh can't be read directly as a node index the way the
    // previous test does -- parser must fall back to searching for the node whose meshIndex
    // matches the given value.
    GltfDocument doc;
    GltfNode n;
    n.meshIndex = 0;
    doc.nodes.push_back(n);
    GltfMesh mesh;
    doc.meshes.push_back(mesh);

    const Json root = parseJson(R"JSON({
      "blendShapeMaster": {
        "blendShapeGroups": [
          { "name": "A", "binds": [ {"mesh":0,"index":2,"weight":50.0} ] }
        ]
      }
    })JSON");

    const auto exprs = VrmExtensionParser::parseExpressions(root, VrmSpecVersion::V0, doc);
    ASSERT_EQ(1u, exprs.size());
    ASSERT_EQ(1u, exprs[0].binds.size());
    EXPECT_EQ(0, exprs[0].binds[0].node);
    EXPECT_FLOAT_EQ(0.5f, exprs[0].binds[0].weight);
}

TEST(VrmExtensionParserTest, ParsesV1Meta)
{
    const Json root = parseJson(R"JSON({
      "meta": { "name": "TestAvatar", "version": "1.0", "authors": ["A", "B"], "licenseUrl": "https://vrm.dev/licenses/1.0/" }
    })JSON");

    const VrmMeta meta = VrmExtensionParser::parseMeta(root, VrmSpecVersion::V1);
    EXPECT_EQ("TestAvatar", meta.title);
    EXPECT_EQ("1.0", meta.version);
    EXPECT_EQ("A, B", meta.author);
    EXPECT_EQ("https://vrm.dev/licenses/1.0/", meta.licenseName);
}

TEST(VrmExtensionParserTest, ParsesV0Meta)
{
    const Json root = parseJson(R"JSON({
      "meta": { "title": "TestAvatar0", "version": "1.0", "author": "TestAuthor", "licenseName": "CC0" }
    })JSON");

    const VrmMeta meta = VrmExtensionParser::parseMeta(root, VrmSpecVersion::V0);
    EXPECT_EQ("TestAvatar0", meta.title);
    EXPECT_EQ("TestAuthor", meta.author);
    EXPECT_EQ("CC0", meta.licenseName);
}

TEST(VrmExtensionParserTest, MalformedHumanoidShapeDegradesToEmpty)
{
    const Json root = parseJson(R"JSON({ "humanoid": "not-an-object" })JSON");
    const VrmHumanoid h = VrmExtensionParser::parseHumanoid(root, VrmSpecVersion::V1);
    EXPECT_TRUE(h.boneNameToNode.empty());
}
