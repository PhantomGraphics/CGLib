#include "gtest/gtest.h"

#include "../Gltf/ObjToGltfConverter.h"
#include "../Gltf/GltfAccessorView.h"
#include "../../File/File/OBJFileReader.h"

#include <sstream>

using namespace Phantom::Gltf;
using namespace Phantom::File;

namespace {

// Two triangles (fan-triangulated from the single quad face), no explicit "g" line -- OBJFileReader
// still finalizes one trailing (unnamed) group since it has faces.
OBJFile readQuad() {
    std::stringstream stream;
    stream
        << "v 0.0 0.0 0.0" << std::endl
        << "v 1.0 0.0 0.0" << std::endl
        << "v 1.0 1.0 0.0" << std::endl
        << "v 0.0 1.0 0.0" << std::endl
        << "f 1 2 3 4" << std::endl;
    OBJFileReader reader;
    reader.read(stream);
    return reader.getOBJ();
}

// Two explicitly-named groups, one triangle each.
OBJFile readTwoGroups() {
    std::stringstream stream;
    stream
        << "v 0.0 0.0 0.0" << std::endl
        << "v 1.0 0.0 0.0" << std::endl
        << "v 0.0 1.0 0.0" << std::endl
        << "v 5.0 0.0 0.0" << std::endl
        << "v 6.0 0.0 0.0" << std::endl
        << "v 5.0 1.0 0.0" << std::endl
        << "g GroupA" << std::endl
        << "f 1 2 3" << std::endl
        << "g GroupB" << std::endl
        << "f 4 5 6" << std::endl;
    OBJFileReader reader;
    reader.read(stream);
    return reader.getOBJ();
}

} // namespace

TEST(ObjToGltfConverterTest, EmptyObjYieldsEmptyDocument)
{
    OBJFile obj;
    GltfDocument doc = ObjToGltfConverter::convert(obj);
    EXPECT_TRUE(doc.meshes.empty());
    EXPECT_TRUE(doc.scenes.empty());
}

TEST(ObjToGltfConverterTest, SingleUnnamedGroupBecomesOneMeshOneNode)
{
    const OBJFile obj = readQuad();
    ASSERT_EQ(1u, obj.groups.size());
    ASSERT_EQ(1u, obj.groups[0].faces.size());

    GltfDocument doc = ObjToGltfConverter::convert(obj);
    ASSERT_EQ(1u, doc.meshes.size());
    ASSERT_EQ(1u, doc.nodes.size());
    ASSERT_EQ(1u, doc.scenes.size());
    EXPECT_EQ(0, doc.defaultScene);
    ASSERT_EQ(1u, doc.scenes[0].nodes.size());
    EXPECT_EQ(0, doc.scenes[0].nodes[0]);
    EXPECT_EQ(0, doc.nodes[0].meshIndex);
    EXPECT_TRUE(doc.meshes[0].name.empty());

    ASSERT_EQ(1u, doc.meshes[0].primitives.size());
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];

    // Quad face fan-triangulates into 2 triangles = 6 (duplicated) corners.
    GltfAccessorView posView(doc, prim.positionAccessor);
    EXPECT_EQ(6u, posView.count());
    GltfAccessorView nrmView(doc, prim.normalAccessor);
    EXPECT_EQ(6u, nrmView.count());
    GltfAccessorView uvView(doc, prim.texCoord0Accessor);
    EXPECT_EQ(6u, uvView.count());
    GltfAccessorView idxView(doc, prim.indicesAccessor);
    ASSERT_EQ(6u, idxView.count());
    for (uint32_t i = 0; i < 6; ++i)
        EXPECT_EQ(i, idxView.get<uint32_t>(i));

    // No normals/UVs in the source OBJ -> fallback values (matches GltfImporter::extractPrimitive).
    for (size_t i = 0; i < posView.count(); ++i) {
        glm::vec3 n = nrmView.get<glm::vec3>(i);
        EXPECT_FLOAT_EQ(0.f, n.x);
        EXPECT_FLOAT_EQ(1.f, n.y);
        EXPECT_FLOAT_EQ(0.f, n.z);
        glm::vec2 uv = uvView.get<glm::vec2>(i);
        EXPECT_FLOAT_EQ(0.f, uv.x);
        EXPECT_FLOAT_EQ(0.f, uv.y);
    }

    // First corner should be the OBJ's first position (0,0,0).
    glm::vec3 p0 = posView.get<glm::vec3>(0);
    EXPECT_FLOAT_EQ(0.f, p0.x);
    EXPECT_FLOAT_EQ(0.f, p0.y);
    EXPECT_FLOAT_EQ(0.f, p0.z);
}

TEST(ObjToGltfConverterTest, MultipleGroupsBecomeMultipleNodes)
{
    const OBJFile obj = readTwoGroups();
    ASSERT_EQ(2u, obj.groups.size());

    GltfDocument doc = ObjToGltfConverter::convert(obj);
    ASSERT_EQ(2u, doc.meshes.size());
    ASSERT_EQ(2u, doc.nodes.size());
    ASSERT_EQ(1u, doc.scenes.size());
    ASSERT_EQ(2u, doc.scenes[0].nodes.size());

    EXPECT_EQ("GroupA", doc.meshes[0].name);
    EXPECT_EQ("GroupB", doc.meshes[1].name);
    EXPECT_EQ("GroupA", doc.nodes[0].name);
    EXPECT_EQ("GroupB", doc.nodes[1].name);

    for (int meshIdx = 0; meshIdx < 2; ++meshIdx) {
        ASSERT_EQ(1u, doc.meshes[meshIdx].primitives.size());
        const GltfPrimitive& prim = doc.meshes[meshIdx].primitives[0];
        GltfAccessorView posView(doc, prim.positionAccessor);
        EXPECT_EQ(3u, posView.count()); // one triangle, no fan expansion needed
        GltfAccessorView idxView(doc, prim.indicesAccessor);
        EXPECT_EQ(3u, idxView.count());
    }
}

TEST(ObjToGltfConverterTest, OutOfRangePositionIndexSkipsFace)
{
    std::stringstream stream;
    stream
        << "v 0.0 0.0 0.0" << std::endl
        << "v 1.0 0.0 0.0" << std::endl
        << "v 0.0 1.0 0.0" << std::endl
        << "f 1 2 99" << std::endl; // 99 is out of range
    OBJFileReader reader;
    reader.read(stream);
    const OBJFile obj = reader.getOBJ();

    GltfDocument doc = ObjToGltfConverter::convert(obj);
    EXPECT_TRUE(doc.meshes.empty());
}
