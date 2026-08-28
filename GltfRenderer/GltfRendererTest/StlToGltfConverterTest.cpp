#include "gtest/gtest.h"

#include "../Gltf/StlToGltfConverter.h"
#include "../Gltf/GltfAccessorView.h"
#include "../../File/File/STLFileReader.h"

#include <sstream>

using namespace Phantom::Gltf;
using namespace Phantom::File;

namespace {

STLFile readTwoFacetAscii() {
    std::stringstream stream;
    stream
        << "solid two-facets" << std::endl
        << "facet normal 0.0 0.0 1.0" << std::endl
        << "  outer loop" << std::endl
        << "    vertex 0.0 0.0 1.0" << std::endl
        << "    vertex 1.0 0.0 1.0" << std::endl
        << "    vertex 0.0 1.0 1.0" << std::endl
        << "  endloop" << std::endl
        << "endfacet" << std::endl
        << "facet normal 1.0 0.0 0.0" << std::endl
        << "  outer loop" << std::endl
        << "    vertex 1.0 0.0 0.0" << std::endl
        << "    vertex 1.0 1.0 0.0" << std::endl
        << "    vertex 1.0 0.0 1.0" << std::endl
        << "  endloop" << std::endl
        << "endfacet" << std::endl
        << "endsolid" << std::endl;
    STLFileReader reader;
    reader.readAscii(stream);
    return reader.getSTL();
}

} // namespace

TEST(StlToGltfConverterTest, EmptyStlYieldsEmptyDocument)
{
    STLFile stl;
    GltfDocument doc = StlToGltfConverter::convert(stl);
    EXPECT_TRUE(doc.meshes.empty());
    EXPECT_TRUE(doc.scenes.empty());
}

TEST(StlToGltfConverterTest, AllFacetsBecomeOneMeshOneNode)
{
    const STLFile stl = readTwoFacetAscii();
    ASSERT_EQ(2u, stl.faces.size());

    GltfDocument doc = StlToGltfConverter::convert(stl);
    ASSERT_EQ(1u, doc.meshes.size());
    ASSERT_EQ(1u, doc.nodes.size());
    ASSERT_EQ(1u, doc.scenes.size());
    EXPECT_EQ(0, doc.defaultScene);
    ASSERT_EQ(1u, doc.scenes[0].nodes.size());
    EXPECT_EQ(0, doc.scenes[0].nodes[0]);
    EXPECT_EQ(0, doc.nodes[0].meshIndex);

    ASSERT_EQ(1u, doc.meshes[0].primitives.size());
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];

    GltfAccessorView posView(doc, prim.positionAccessor);
    EXPECT_EQ(6u, posView.count()); // 2 facets * 3 verts, fully expanded (no sharing)
    GltfAccessorView nrmView(doc, prim.normalAccessor);
    ASSERT_EQ(6u, nrmView.count());
    GltfAccessorView uvView(doc, prim.texCoord0Accessor);
    ASSERT_EQ(6u, uvView.count());
    GltfAccessorView idxView(doc, prim.indicesAccessor);
    ASSERT_EQ(6u, idxView.count());
    for (uint32_t i = 0; i < 6; ++i)
        EXPECT_EQ(i, idxView.get<uint32_t>(i));

    // First facet's normal is (0,0,1), shared by its 3 corners.
    for (size_t i = 0; i < 3; ++i) {
        glm::vec3 n = nrmView.get<glm::vec3>(i);
        EXPECT_FLOAT_EQ(0.f, n.x);
        EXPECT_FLOAT_EQ(0.f, n.y);
        EXPECT_FLOAT_EQ(1.f, n.z);
    }
    // UV is always (0,0) -- STL has no texture coordinates.
    for (size_t i = 0; i < uvView.count(); ++i) {
        glm::vec2 uv = uvView.get<glm::vec2>(i);
        EXPECT_FLOAT_EQ(0.f, uv.x);
        EXPECT_FLOAT_EQ(0.f, uv.y);
    }
}
