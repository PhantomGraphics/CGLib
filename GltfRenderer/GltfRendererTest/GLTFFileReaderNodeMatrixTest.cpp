#include "gtest/gtest.h"

#include "../../File/File/GLTFFileReader.h"
#include "../Gltf/GltfReader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Phantom::File;
using namespace Phantom::Gltf;

namespace {

// Node 0 uses "matrix" (translation-only: move by (2,3,4)) instead of TRS -- glTF spec forbids
// specifying both on the same node. Regression coverage for the bug where GLTFFileReader/
// GltfReader silently discarded "matrix" nodes (treating them as identity), which broke any
// skinned model whose exporter emits joint transforms as raw matrices instead of decomposed TRS
// (confirmed against real files: RiggedFigure.glb, CesiumMan.glb -- see
// internal design notes #6.2).
const char* kMatrixNodeGltf = R"JSON(
{
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [
    {
      "matrix": [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        2.0, 3.0, 4.0, 1.0
      ]
    }
  ]
}
)JSON";

std::filesystem::path writeTempGltf(const char* name, const char* json) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << json;
    return path;
}

} // namespace

TEST(GLTFFileReaderNodeMatrixTest, FileLayerReadsMatrixNode)
{
    const auto path = writeTempGltf("GLTFFileReaderNodeMatrixTest.gltf", kMatrixNodeGltf);

    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(path));
    const GLTFFile gltf = reader.getGLTF();

    ASSERT_EQ(1u, gltf.nodes.size());
    const GLTFNode& node = gltf.nodes[0];
    EXPECT_TRUE(node.hasMatrix);
    // Column-major: translation lives in matrix[12..14].
    EXPECT_FLOAT_EQ(2.0f, node.matrix[12]);
    EXPECT_FLOAT_EQ(3.0f, node.matrix[13]);
    EXPECT_FLOAT_EQ(4.0f, node.matrix[14]);

    std::remove(path.string().c_str());
}

TEST(GLTFFileReaderNodeMatrixTest, GltfLayerPropagatesMatrixToGlmMat4)
{
    const auto path = writeTempGltf("GLTFFileReaderNodeMatrixTest2.gltf", kMatrixNodeGltf);

    const auto doc = GltfReader::load(path);
    ASSERT_TRUE(doc.has_value());
    ASSERT_EQ(1u, doc->nodes.size());

    const GltfNode& node = doc->nodes[0];
    EXPECT_TRUE(node.hasMatrix);
    const glm::vec4 translation = node.matrix[3]; // glm::mat4 column 3 = translation
    EXPECT_FLOAT_EQ(2.0f, translation.x);
    EXPECT_FLOAT_EQ(3.0f, translation.y);
    EXPECT_FLOAT_EQ(4.0f, translation.z);

    std::remove(path.string().c_str());
}
