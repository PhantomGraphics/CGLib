#include "../Core/MeshJoin.h"
#include <gtest/gtest.h>
#include <set>
using namespace Phantom::MeshEdit;
namespace {
EditableMesh box(float x, float h = 0.5f) {
    auto m = makeEditableBox(h);
    for (auto& v : m.vertices) v.pos[0] += x;
    return m;
}
}

TEST(MeshJoinTest, CarriesEveryShellAttributeAndSeamOver) {
    EditableMesh a = box(0.f), b = box(3.f), c = box(6.f, 0.25f);
    b.faces[0].loop[0].uv[0] = 0.25f;
    b.faces[1].loop[0].hasNormal = true;
    b.faces[1].loop[0].normal[1] = 1.f;
    const auto& e = b.edges.front();
    b.seams.push_back({ e.a, e.b });
    EditableMesh out;
    std::string why;
    ASSERT_TRUE(joinMeshes(a, { &b, &c }, out, &why)) << why;
    EXPECT_EQ(out.vertices.size(), 24u);
    EXPECT_EQ(out.faces.size(), 18u);
    EXPECT_EQ(out.edges.size(), 36u);
    EXPECT_TRUE(out.validate(&why)) << why;
    // the first part keeps its ids, the others get fresh ones
    for (size_t i = 0; i < a.vertices.size(); ++i) EXPECT_EQ(out.vertices[i].id, a.vertices[i].id);
    std::set<uint32_t> ids;
    for (const auto& v : out.vertices) ids.insert(v.id);
    EXPECT_EQ(ids.size(), 24u);
    // attributes of the joined part are unchanged
    const EditableFace& face = out.faces[a.faces.size()];
    EXPECT_FLOAT_EQ(face.loop[0].uv[0], 0.25f);
    EXPECT_TRUE(out.faces[a.faces.size() + 1].loop[0].hasNormal);
    EXPECT_FLOAT_EQ(out.faces[a.faces.size() + 1].loop[0].normal[1], 1.f);
    EXPECT_EQ(out.seams.size(), 1u);
    EXPECT_NE(out.revision, a.revision);
    // no welding: overlapping shells stay separate
    EditableMesh same;
    ASSERT_TRUE(joinMeshes(a, { &a }, same, &why)) << why;
    EXPECT_EQ(same.vertices.size(), 16u);
}

TEST(MeshJoinTest, NothingToJoinKeepsTheBase) {
    EditableMesh a = box(0.f), out;
    ASSERT_TRUE(joinMeshes(a, {}, out));
    EXPECT_EQ(out.vertices.size(), 8u);
}

TEST(MeshJoinTest, InvalidInputLeavesTheOutputAlone) {
    EditableMesh a = box(0.f), bad = box(2.f), out = box(9.f);
    bad.faces[0].loop[0].vertex = 9999;
    std::string why;
    EXPECT_FALSE(joinMeshes(a, { &bad }, out, &why));
    EXPECT_FALSE(why.empty());
    EXPECT_EQ(out.vertices.size(), 8u);
    EXPECT_FALSE(joinMeshes(a, { nullptr }, out, &why));
}
