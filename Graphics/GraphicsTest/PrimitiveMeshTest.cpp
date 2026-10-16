#include <gtest/gtest.h>

#include "../Core/PrimitiveMesh.h"

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

// Primitive generators, mirror and validate moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2).
using namespace Phantom::Graphics;

namespace {

// Twice the area below which a triangle is a collapsed pole fan (UV sphere poles), whose sign is float noise.
constexpr float kMinArea = 1e-6f;

struct Vec { float x, y, z; };
Vec sub(const float* a, const float* b) { return { a[0] - b[0], a[1] - b[1], a[2] - b[2] }; }
Vec cross(Vec a, Vec b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(Vec a) { return std::sqrt(dot(a, a)); }

// Triangles whose cross product points against the sum of their declared vertex normals (degenerate ones are skipped).
int againstDeclaredNormals(const MeshData& mesh) {
    int bad = 0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1]];
        const auto& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec n = cross(sub(b.pos, a.pos), sub(c.pos, a.pos));
        if (length(n) < kMinArea) continue;
        const Vec declared = { a.nrm[0] + b.nrm[0] + c.nrm[0], a.nrm[1] + b.nrm[1] + c.nrm[1], a.nrm[2] + b.nrm[2] + c.nrm[2] };
        if (dot(n, declared) <= 0.f) ++bad;
    }
    return bad;
}

// Triangles of a convex solid centred on the origin whose cross product points toward the centre. Needs no declared
// normals, so it also catches a mesh whose normals and winding are both wrong the same way.
int pointingInward(const MeshData& mesh) {
    int bad = 0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1]];
        const auto& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec n = cross(sub(b.pos, a.pos), sub(c.pos, a.pos));
        if (length(n) < kMinArea) continue;
        const Vec centroid = { (a.pos[0] + b.pos[0] + c.pos[0]) / 3.f, (a.pos[1] + b.pos[1] + c.pos[1]) / 3.f,
                               (a.pos[2] + b.pos[2] + c.pos[2]) / 3.f };
        if (dot(n, centroid) <= 0.f) ++bad;
    }
    return bad;
}


using Builder = std::function<MeshData()>;
const std::array<std::pair<const char*, Builder>, 6> kShapes = {{
    {"box", [] { return buildBox(); }}, {"sphere", [] { return buildSphere(); }}, {"plane", [] { return buildPlane(); }},
    {"cylinder", [] { return buildCylinder(); }}, {"cone", [] { return buildCone(); }}, {"torus", [] { return buildTorus(); }}}};

} // namespace

TEST(PrimitiveMesh, EveryPrimitiveFollowsItsDeclaredNormals) {
    for (const auto& [name, build] : kShapes) {
        const MeshData mesh = build();
        ASSERT_FALSE(mesh.indices.empty()) << name;
        EXPECT_EQ(againstDeclaredNormals(mesh), 0) << name << ": triangles wound against their declared normals";
        // validate() also counts collapsed triangles (the UV sphere's pole fans) as inconsistent; nothing else may be.
        const auto check = validate(mesh);
        EXPECT_EQ(check.inconsistentTriangles, check.degenerateTriangles) << name << ": validate()";
    }
}

TEST(PrimitiveMesh, ConvexSolidsFaceOutwardFromTheirCentre) {
    // Box, sphere, cylinder and cone (centred on the origin along Y for cone: its centroid sits inside the solid, and the
    // base cap is a plane through y = -h/2, so the origin test is still valid for every triangle).
    for (const auto& [name, build] : kShapes) {
        if (std::string(name) == "plane" || std::string(name) == "torus") continue;
        EXPECT_EQ(pointingInward(build()), 0) << name << ": a triangle's front side faces the inside";
    }
}

TEST(PrimitiveMesh, ValidateReportsAnInsideOutMesh) {
    MeshData box = buildBox(0.5f);
    ASSERT_EQ(validate(box).inconsistentTriangles, 0);
    for (size_t i = 0; i + 2 < box.indices.size(); i += 3) std::swap(box.indices[i + 1], box.indices[i + 2]);
    EXPECT_EQ(validate(box).inconsistentTriangles, 12);
    EXPECT_EQ(againstDeclaredNormals(box), 12);
}

TEST(PrimitiveMesh, MirroredMeshKeepsFacingOutward) {
    for (int axis = 0; axis < 3; ++axis) {
        const MeshData mirrored = mirror(buildSphere(), axis);
        EXPECT_EQ(againstDeclaredNormals(mirrored), 0) << "axis " << axis;
        const MeshData mirroredBox = mirror(buildBox(0.5f), axis);
        EXPECT_EQ(againstDeclaredNormals(mirroredBox), 0) << "axis " << axis;
    }
}
