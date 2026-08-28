#include "gtest/gtest.h"

#include "../../File/File/GLTFFileReader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Phantom::File;

namespace {

// Single triangle, one morph target (a POSITION-only delta moving the third vertex up by 0.5),
// mesh.weights=[0.25], and a made-up unrecognized root extension to exercise
// GLTFFile::rootExtensionsJson passthrough (this layer must not know or care what "VRM" is --
// see GltfReader/VrmReader for the layers that interpret extension content).
const char* kMorphTargetGltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "mesh": 0 } ],
  "meshes": [
    {
      "name": "MorphTri",
      "primitives": [
        {
          "attributes": { "POSITION": 0 },
          "targets": [ { "POSITION": 1 } ]
        }
      ],
      "weights": [0.25]
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
    {
      "byteLength": 72,
      "uri": "data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD8AAAAA"
    }
  ],
  "extensions": {
    "TEST_custom_extension": { "answer": 42 }
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

TEST(GLTFFileReaderMorphTargetTest, ParsesMorphTargetPositionDeltasAndWeights)
{
    const auto path = writeTempGltf("GLTFFileReaderMorphTargetTest_morph.gltf", kMorphTargetGltf);

    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(path));
    const GLTFFile gltf = reader.getGLTF();

    ASSERT_EQ(1u, gltf.meshes.size());
    ASSERT_EQ(1u, gltf.meshes[0].primitives.size());
    const GLTFPrimitive& prim = gltf.meshes[0].primitives[0];

    ASSERT_EQ(3u, prim.positions.size());
    ASSERT_EQ(1u, prim.targets.size());
    ASSERT_EQ(3u, prim.targets[0].size());
    EXPECT_FLOAT_EQ(0.0f, prim.targets[0][0].x);
    EXPECT_FLOAT_EQ(0.0f, prim.targets[0][0].y);
    EXPECT_FLOAT_EQ(0.0f, prim.targets[0][1].y);
    EXPECT_FLOAT_EQ(0.0f, prim.targets[0][2].x);
    EXPECT_FLOAT_EQ(0.5f, prim.targets[0][2].y);
    EXPECT_FLOAT_EQ(0.0f, prim.targets[0][2].z);

    ASSERT_EQ(1u, gltf.meshes[0].morphWeights.size());
    EXPECT_FLOAT_EQ(0.25f, gltf.meshes[0].morphWeights[0]);

    ASSERT_EQ(1u, gltf.rootExtensionsJson.size());
    EXPECT_EQ("TEST_custom_extension", gltf.rootExtensionsJson[0].first);
    EXPECT_NE(std::string::npos, gltf.rootExtensionsJson[0].second.find("42"));

    std::remove(path.string().c_str());
}
