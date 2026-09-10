#include "gtest/gtest.h"

#include "../File/GLTFFileReader.h"
#include "../File/GLTFFileWriter.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace Phantom::File;

TEST(GLTFFileWriterTest, TestWriteAndReadRoundTrip)
{
    GLTFFile src;

    GLTFMaterial material;
    material.name = "mat0";
    material.pbrMetallicRoughness.baseColorFactor = { 0.2f, 0.4f, 0.6f, 1.0f };
    src.materials.push_back(material);

    GLTFImage image;
    image.name = "img0";
    image.uri = "dummy.png";
    image.mimeType = "image/png";
    src.images.push_back(image);

    GLTFTexture texture;
    texture.name = "tex0";
    texture.imageIndex = 0;
    src.textures.push_back(texture);

    GLTFPrimitive prim;
    prim.positions.push_back({ 0.0f, 0.0f, 0.0f });
    prim.positions.push_back({ 1.0f, 0.0f, 0.0f });
    prim.positions.push_back({ 0.0f, 1.0f, 0.0f });
    prim.normals.push_back({ 0.0f, 0.0f, 1.0f });
    prim.normals.push_back({ 0.0f, 0.0f, 1.0f });
    prim.normals.push_back({ 0.0f, 0.0f, 1.0f });
    prim.texCoords.push_back({ 0.0f, 0.0f });
    prim.texCoords.push_back({ 1.0f, 0.0f });
    prim.texCoords.push_back({ 0.0f, 1.0f });
    prim.indices.push_back(0);
    prim.indices.push_back(1);
    prim.indices.push_back(2);
    prim.materialIndex = 0;
    prim.mode = GLTFPrimitiveMode::Triangles;

    GLTFMesh mesh;
    mesh.name = "mesh0";
    mesh.primitives.push_back(prim);
    src.meshes.push_back(mesh);

    GLTFNode node;
    node.name = "node0";
    node.meshIndex = 0;
    src.nodes.push_back(node);

    GLTFScene scene;
    scene.name = "scene0";
    scene.nodes.push_back(0);
    src.scenes.push_back(scene);
    src.defaultScene = 0;

    const std::filesystem::path filePath = std::filesystem::temp_directory_path() / "GLTFFileReaderWriterTest.gltf";
    GLTFFileWriter writer;
    EXPECT_TRUE(writer.write(filePath, src));

    GLTFFileReader reader;
    EXPECT_TRUE(reader.read(filePath));
    const GLTFFile dst = reader.getGLTF();

    ASSERT_EQ(1u, dst.meshes.size());
    ASSERT_EQ(1u, dst.meshes[0].primitives.size());
    ASSERT_EQ(3u, dst.meshes[0].primitives[0].positions.size());
    ASSERT_EQ(3u, dst.meshes[0].primitives[0].indices.size());
    EXPECT_EQ(0u, dst.meshes[0].primitives[0].indices[0]);
    EXPECT_EQ(1u, dst.meshes[0].primitives[0].indices[1]);
    EXPECT_EQ(2u, dst.meshes[0].primitives[0].indices[2]);
    EXPECT_EQ(0, dst.meshes[0].primitives[0].materialIndex);

    ASSERT_EQ(1u, dst.materials.size());
    EXPECT_EQ("mat0", dst.materials[0].name);
    EXPECT_FLOAT_EQ(0.2f, dst.materials[0].pbrMetallicRoughness.baseColorFactor[0]);
    EXPECT_FLOAT_EQ(0.4f, dst.materials[0].pbrMetallicRoughness.baseColorFactor[1]);
    EXPECT_FLOAT_EQ(0.6f, dst.materials[0].pbrMetallicRoughness.baseColorFactor[2]);
    EXPECT_FLOAT_EQ(1.0f, dst.materials[0].pbrMetallicRoughness.baseColorFactor[3]);

    ASSERT_EQ(1u, dst.nodes.size());
    EXPECT_EQ(0, dst.nodes[0].meshIndex);
    ASSERT_EQ(1u, dst.scenes.size());
    ASSERT_EQ(1u, dst.scenes[0].nodes.size());
    EXPECT_EQ(0, dst.scenes[0].nodes[0]);
    EXPECT_EQ(0, dst.defaultScene);

    std::remove(filePath.string().c_str());
}

// GLTFFileWriter can't emit animations, so exercise the reader against a hand-written minimal
// glTF: one node rotation-animated by a 2-keyframe LINEAR sampler. Verifies the animation
// channel/sampler parsing added for the Blender->Universe authoring loop (Phase 3).
TEST(GLTFFileReaderTest, ParsesRotationAnimationChannel)
{
    // Binary blob: input times [0,1] (2 x f32) then output quats identity, 180deg-about-Y
    // (2 x vec4 f32, xyzw). 8 + 32 = 40 bytes.
    std::vector<float> blob = {
        0.f, 1.f,
        0.f, 0.f, 0.f, 1.f,
        0.f, 1.f, 0.f, 0.f,
    };
    const auto dir = std::filesystem::temp_directory_path();
    const auto binPath  = dir / "gltf_anim_test.bin";
    const auto gltfPath = dir / "gltf_anim_test.gltf";
    {
        std::ofstream bin(binPath, std::ios::binary);
        bin.write(reinterpret_cast<const char*>(blob.data()),
                  static_cast<std::streamsize>(blob.size() * sizeof(float)));
    }

    const std::string gltf = R"({
  "asset": { "version": "2.0" },
  "nodes": [ { "name": "Spin" } ],
  "scenes": [ { "nodes": [0] } ],
  "scene": 0,
  "buffers": [ { "uri": "gltf_anim_test.bin", "byteLength": 40 } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,  "byteLength": 8 },
    { "buffer": 0, "byteOffset": 8,  "byteLength": 32 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0.0], "max": [1.0] },
    { "bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC4" }
  ],
  "animations": [ {
    "name": "Spin4s",
    "samplers": [ { "input": 0, "output": 1, "interpolation": "LINEAR" } ],
    "channels": [ { "sampler": 0, "target": { "node": 0, "path": "rotation" } } ]
  } ]
})";
    {
        std::ofstream f(gltfPath);
        f << gltf;
    }

    GLTFFileReader reader;
    ASSERT_TRUE(reader.read(gltfPath));
    const GLTFFile g = reader.getGLTF();

    ASSERT_EQ(1u, g.animations.size());
    EXPECT_EQ("Spin4s", g.animations[0].name);
    ASSERT_EQ(1u, g.animations[0].samplers.size());
    ASSERT_EQ(1u, g.animations[0].channels.size());

    const auto& s = g.animations[0].samplers[0];
    EXPECT_EQ(GLTFInterpolation::Linear, s.interpolation);
    ASSERT_EQ(2u, s.times.size());
    EXPECT_FLOAT_EQ(0.f, s.times[0]);
    EXPECT_FLOAT_EQ(1.f, s.times[1]);
    EXPECT_EQ(4, s.components);
    ASSERT_EQ(8u, s.values.size());
    EXPECT_FLOAT_EQ(1.f, s.values[3]); // identity quat w
    EXPECT_FLOAT_EQ(1.f, s.values[5]); // second quat y

    const auto& ch = g.animations[0].channels[0];
    EXPECT_EQ(0, ch.targetNode);
    EXPECT_EQ(0, ch.sampler);
    EXPECT_EQ(GLTFAnimationPath::Rotation, ch.path);

    std::remove(gltfPath.string().c_str());
    std::remove(binPath.string().c_str());
}
