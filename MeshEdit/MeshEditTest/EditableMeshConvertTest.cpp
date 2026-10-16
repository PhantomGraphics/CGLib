#include <gtest/gtest.h>

#include "../Core/EditableMeshConvert.h"

#include <cmath>
#include <string>

// Render-mesh <-> EditableMesh conversion, moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2).
using namespace Phantom::MeshEdit;
using Phantom::Graphics::MeshData;
using Phantom::Graphics::MeshVertex;

namespace {
// Total area and the signed count of triangles facing along (+) / against (-) their declared normals.
void areaAndFacing(const MeshData& m, double& area, long& outward) {
    area = 0; outward = 0;
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const auto& a = m.vertices[m.indices[t]]; const auto& b = m.vertices[m.indices[t + 1]]; const auto& c = m.vertices[m.indices[t + 2]];
        double e1[3], e2[3], n[3];
        for (int k = 0; k < 3; ++k) { e1[k] = b.pos[k] - a.pos[k]; e2[k] = c.pos[k] - a.pos[k]; }
        n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        area += 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        const double d = n[0] * a.nrm[0] + n[1] * a.nrm[1] + n[2] * a.nrm[2];
        if (d > 1e-12) ++outward; else if (d < -1e-12) --outward;
    }
}
}

TEST(EditableMeshConvert, UvSeamsAndHardEdgesSurviveAsCornerAttributes) {
    // Two triangles forming a quad, plus a seam: the shared edge has different UVs on each side and a hard normal.
    MeshData m;
    auto v = [&](float x, float y, float nx, float ny, float nz, float u, float w) { MeshVertex mv{}; mv.pos[0] = x; mv.pos[1] = y; mv.nrm[0] = nx; mv.nrm[1] = ny; mv.nrm[2] = nz; mv.uv[0] = u; mv.uv[1] = w; m.vertices.push_back(mv); };
    // glTF-style winding: counter-clockwise seen from +Z, normal +Z.
    v(0, 0, 0, 0, 1, 0, 0); v(1, 0, 0, 0, 1, 1, 0); v(1, 1, 0, 0, 1, 1, 1);
    v(0, 0, 0, 0.6f, 0.8f, 0.5f, 0.5f); v(1, 1, 0, 0.6f, 0.8f, 0.9f, 0.9f); v(0, 1, 0, 0, 1, 0, 1);
    m.indices = { 0, 1, 2, 3, 4, 5 };
    EditableMesh e; std::string error;
    ASSERT_TRUE(makeEditableFromMeshData(m, e, &error)) << error;
    EXPECT_EQ(4u, e.vertices.size());
    EXPECT_EQ(2u, e.faces.size());
    EXPECT_EQ(5u, e.edges.size());
    // The shared corner at (0,0) has different UVs in the two faces.
    float uv0[2] = { -1, -1 }, uv1[2] = { -1, -1 };
    for (const auto& c : e.faces[0].loop) if (e.findVertex(c.vertex)->pos[0] == 0.f && e.findVertex(c.vertex)->pos[1] == 0.f) { uv0[0] = c.uv[0]; uv0[1] = c.uv[1]; }
    for (const auto& c : e.faces[1].loop) if (e.findVertex(c.vertex)->pos[0] == 0.f && e.findVertex(c.vertex)->pos[1] == 0.f) { uv1[0] = c.uv[0]; uv1[1] = c.uv[1]; }
    EXPECT_FLOAT_EQ(0.f, uv0[0]); EXPECT_FLOAT_EQ(0.5f, uv1[0]);
    // The tilted normal is kept on the second face; the flat first face stores none.
    bool firstFlat = true, secondHard = false;
    for (const auto& c : e.faces[0].loop) if (c.hasNormal) firstFlat = false;
    for (const auto& c : e.faces[1].loop) if (c.hasNormal) secondHard = true;
    EXPECT_TRUE(firstFlat);
    EXPECT_TRUE(secondHard);
    // Rendering reproduces the source triangles in the source order.
    MeshData back;
    ASSERT_TRUE(editableToMeshData(e, back, nullptr, &error)) << error;
    ASSERT_EQ(6u, back.indices.size());
    double a0, a1; long f0, f1;
    areaAndFacing(m, a0, f0); areaAndFacing(back, a1, f1);
    EXPECT_NEAR(a0, a1, 1e-6);
    EXPECT_GT(f1, 0);  // still front-facing the same way as the source
}

TEST(EditableMeshConvert, NonManifoldAndEmptyMeshesAreRefusedWithAReason) {
    // Three triangles sharing one edge.
    MeshData m;
    auto v = [&](float x, float y, float z) { MeshVertex mv{}; mv.pos[0] = x; mv.pos[1] = y; mv.pos[2] = z; mv.nrm[2] = 1; m.vertices.push_back(mv); };
    v(0, 0, 0); v(1, 0, 0); v(0, 1, 0); v(0, -1, 0); v(0, 0, 1);
    m.indices = { 0, 1, 2, 1, 0, 3, 0, 1, 4 };
    EditableMesh e; std::string error;
    EXPECT_FALSE(makeEditableFromMeshData(m, e, &error));
    EXPECT_NE(std::string::npos, error.find("manifold"));
    EXPECT_FALSE(makeEditableFromMeshData(MeshData{}, e, &error));
}
