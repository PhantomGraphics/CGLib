#include "gtest/gtest.h"

#include "../File/GLTFFileReader.h"
#include "../File/GLTFFileWriter.h"

#include <cstdio>

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
