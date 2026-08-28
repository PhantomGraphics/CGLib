#include "gtest/gtest.h"

#include "../Vrm/VrmReader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Phantom::Gltf;

namespace {

// Two nodes (hips -> head), one mesh (on "head") with one morph target, one material. Buffer is
// the same 72-byte base-position + morph-delta blob used by GLTFFileReaderMorphTargetTest (base
// triangle + a delta that moves vertex 2 up by 0.5 on Y).
const char* kVrm0Gltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "extensionsUsed": ["VRM"],
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    { "name": "hips", "children": [1] },
    { "name": "head", "mesh": 0 }
  ],
  "meshes": [
    {
      "name": "HeadMesh",
      "primitives": [
        { "attributes": { "POSITION": 0 }, "targets": [ { "POSITION": 1 } ], "material": 0 }
      ],
      "weights": [0.0]
    }
  ],
  "materials": [
    {
      "name": "Body",
      "pbrMetallicRoughness": { "baseColorFactor": [1.0, 1.0, 1.0, 1.0], "metallicFactor": 1.0, "roughnessFactor": 1.0 }
    }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-0.5, -0.5, 0.0], "max": [0.5, 0.5, 0.0] },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0.0, 0.0, 0.0], "max": [0.0, 0.5, 0.0] }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 36 }
  ],
  "buffers": [
    { "byteLength": 72, "uri": "data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD8AAAAA" }
  ],
  "extensions": {
    "VRM": {
      "exporterVersion": "TestGen-1.0",
      "meta": { "title": "TestAvatar0", "version": "1.0", "author": "TestAuthor", "licenseName": "CC0" },
      "humanoid": { "humanBones": [ {"bone":"hips","node":0}, {"bone":"head","node":1} ] },
      "blendShapeMaster": {
        "blendShapeGroups": [
          { "name": "Blink", "presetName": "blink", "binds": [ {"mesh":1,"index":0,"weight":100.0} ], "isBinary": false }
        ]
      },
      "materialProperties": [
        { "shader": "VRM/MToon", "vectorProperties": { "_Color": [0.1, 0.2, 0.3, 1.0] }, "textureProperties": {} }
      ]
    }
  }
}
)JSON";

const char* kVrm1Gltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "extensionsUsed": ["VRMC_vrm"],
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    { "name": "hips", "children": [1] },
    { "name": "head", "mesh": 0 }
  ],
  "meshes": [
    {
      "name": "HeadMesh",
      "primitives": [
        { "attributes": { "POSITION": 0 }, "targets": [ { "POSITION": 1 } ], "material": 0 }
      ],
      "weights": [0.0]
    }
  ],
  "materials": [
    {
      "name": "Body",
      "pbrMetallicRoughness": { "baseColorFactor": [1.0, 0.0, 0.0, 1.0], "metallicFactor": 0.8, "roughnessFactor": 0.2 },
      "extensions": { "VRMC_materials_mtoon": { "specVersion": "1.0" } }
    }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-0.5, -0.5, 0.0], "max": [0.5, 0.5, 0.0] },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0.0, 0.0, 0.0], "max": [0.0, 0.5, 0.0] }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 36 }
  ],
  "buffers": [
    { "byteLength": 72, "uri": "data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD8AAAAA" }
  ],
  "extensions": {
    "VRMC_vrm": {
      "specVersion": "1.0",
      "meta": { "name": "TestAvatar1", "version": "1.0", "authors": ["TestAuthor"], "licenseUrl": "https://vrm.dev/licenses/1.0/" },
      "humanoid": { "humanBones": { "hips": {"node":0}, "head": {"node":1} } },
      "expressions": {
        "preset": {
          "happy": { "morphTargetBinds": [ {"node":1,"index":0,"weight":1.0} ], "isBinary": false }
        },
        "custom": {}
      }
    }
  }
}
)JSON";

std::filesystem::path writeTempGltf(const char* name, const char* json) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << json;
    return path;
}

} // namespace

TEST(VrmReaderTest, LoadsVrm0EndToEnd)
{
    const auto path = writeTempGltf("VrmReaderTest_vrm0.gltf", kVrm0Gltf);
    const auto vrmDoc = VrmReader::load(path);
    ASSERT_TRUE(vrmDoc.has_value());

    EXPECT_EQ(VrmSpecVersion::V0, vrmDoc->specVersion);

    ASSERT_EQ(1u, vrmDoc->gltf.meshes.size());
    ASSERT_EQ(1u, vrmDoc->gltf.meshes[0].primitives.size());
    EXPECT_EQ(1u, vrmDoc->gltf.meshes[0].primitives[0].targets.size());

    ASSERT_EQ(2u, vrmDoc->humanoid.boneNameToNode.size());
    EXPECT_EQ(0, vrmDoc->humanoid.boneNameToNode.at("hips"));
    EXPECT_EQ(1, vrmDoc->humanoid.boneNameToNode.at("head"));

    ASSERT_EQ(1u, vrmDoc->expressions.size());
    EXPECT_EQ("Blink", vrmDoc->expressions[0].name);
    EXPECT_EQ("blink", vrmDoc->expressions[0].presetName);
    ASSERT_EQ(1u, vrmDoc->expressions[0].binds.size());
    EXPECT_EQ(1, vrmDoc->expressions[0].binds[0].node);

    EXPECT_EQ("TestAvatar0", vrmDoc->meta.title);
    EXPECT_EQ("TestAuthor", vrmDoc->meta.author);
    EXPECT_EQ("CC0", vrmDoc->meta.licenseName);

    // VRM 0.x MToon fallback: base color read from materialProperties[]._Color (the underlying
    // glTF material's own baseColorFactor, {1,1,1,1}, is NOT what should end up here).
    ASSERT_EQ(1u, vrmDoc->gltf.materials.size());
    const auto& baseColor = vrmDoc->gltf.materials[0].pbrMetallicRoughness.baseColorFactor;
    EXPECT_FLOAT_EQ(0.1f, baseColor.r);
    EXPECT_FLOAT_EQ(0.2f, baseColor.g);
    EXPECT_FLOAT_EQ(0.3f, baseColor.b);
    EXPECT_FLOAT_EQ(0.0f, vrmDoc->gltf.materials[0].pbrMetallicRoughness.metallicFactor);
    EXPECT_FLOAT_EQ(1.0f, vrmDoc->gltf.materials[0].pbrMetallicRoughness.roughnessFactor);

    std::remove(path.string().c_str());
}

TEST(VrmReaderTest, LoadsVrm1EndToEnd)
{
    const auto path = writeTempGltf("VrmReaderTest_vrm1.gltf", kVrm1Gltf);
    const auto vrmDoc = VrmReader::load(path);
    ASSERT_TRUE(vrmDoc.has_value());

    EXPECT_EQ(VrmSpecVersion::V1, vrmDoc->specVersion);

    ASSERT_EQ(2u, vrmDoc->humanoid.boneNameToNode.size());
    EXPECT_EQ(0, vrmDoc->humanoid.boneNameToNode.at("hips"));
    EXPECT_EQ(1, vrmDoc->humanoid.boneNameToNode.at("head"));

    ASSERT_EQ(1u, vrmDoc->expressions.size());
    EXPECT_EQ("happy", vrmDoc->expressions[0].name);
    EXPECT_EQ("happy", vrmDoc->expressions[0].presetName);
    ASSERT_EQ(1u, vrmDoc->expressions[0].binds.size());
    EXPECT_EQ(1, vrmDoc->expressions[0].binds[0].node);
    EXPECT_EQ(0, vrmDoc->expressions[0].binds[0].targetIndex);
    EXPECT_FLOAT_EQ(1.0f, vrmDoc->expressions[0].binds[0].weight);

    EXPECT_EQ("TestAvatar1", vrmDoc->meta.title);
    EXPECT_EQ("TestAuthor", vrmDoc->meta.author);
    EXPECT_EQ("https://vrm.dev/licenses/1.0/", vrmDoc->meta.licenseName);

    // VRM 1.0 MToon fallback: base color is left as the material's own baseColorFactor (MToon
    // 1.0 reuses it directly, no duplicate color field in the extension) -- only
    // metallic/roughness get forced to a matte look.
    ASSERT_EQ(1u, vrmDoc->gltf.materials.size());
    const auto& baseColor = vrmDoc->gltf.materials[0].pbrMetallicRoughness.baseColorFactor;
    EXPECT_FLOAT_EQ(1.0f, baseColor.r);
    EXPECT_FLOAT_EQ(0.0f, baseColor.g);
    EXPECT_FLOAT_EQ(0.0f, baseColor.b);
    EXPECT_FLOAT_EQ(0.0f, vrmDoc->gltf.materials[0].pbrMetallicRoughness.metallicFactor);
    EXPECT_FLOAT_EQ(1.0f, vrmDoc->gltf.materials[0].pbrMetallicRoughness.roughnessFactor);

    std::remove(path.string().c_str());
}

TEST(VrmReaderTest, PlainGltfWithNoVrmExtensionStillLoads)
{
    const char* plain = R"JSON(
    {
      "asset": { "version": "2.0" },
      "scene": 0,
      "scenes": [ { "nodes": [0] } ],
      "nodes": [ { "mesh": 0 } ],
      "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0 } } ] } ],
      "accessors": [
        { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-0.5,-0.5,0.0], "max": [0.5,0.5,0.0] }
      ],
      "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 36 } ],
      "buffers": [ { "byteLength": 36, "uri": "data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAA" } ]
    }
    )JSON";
    const auto path = writeTempGltf("VrmReaderTest_plain.gltf", plain);

    const auto vrmDoc = VrmReader::load(path);
    ASSERT_TRUE(vrmDoc.has_value());
    EXPECT_EQ(VrmSpecVersion::Unknown, vrmDoc->specVersion);
    EXPECT_TRUE(vrmDoc->humanoid.boneNameToNode.empty());
    EXPECT_TRUE(vrmDoc->expressions.empty());
    ASSERT_EQ(1u, vrmDoc->gltf.meshes.size());

    std::remove(path.string().c_str());
}
