#include "GizmoGeometry.h"
#include <cmath>
#include <cassert>

namespace Phantom::Gizmo {

static constexpr int   SHAFT_SEGS = 12;
static constexpr int   RING_SEGS  = 32;
static constexpr float kPi        = 3.14159265358979323846f;

// Helper: GizmoVertex from xyz
static inline GizmoVertex gv(float x, float y, float z) {
    GizmoVertex v;
    v.pos[0] = x; v.pos[1] = y; v.pos[2] = z;
    return v;
}

// Cylinder with bottom cap, no top cap.
// Vertices: 2*segs ring + 1 bottom center = 2*segs+1
// Indices:  segs*6 (sides) + segs*3 (bottom cap) = segs*9
static void appendCylinder(
    std::vector<GizmoVertex>& V,
    std::vector<uint32_t>&    I,
    float zBot, float zTop, float r, int segs)
{
    const auto base = static_cast<uint32_t>(V.size());

    for (int i = 0; i < segs; ++i) {
        float a = 2.f * kPi * i / segs;
        V.push_back(gv(r * cosf(a), r * sinf(a), zBot));
    }
    for (int i = 0; i < segs; ++i) {
        float a = 2.f * kPi * i / segs;
        V.push_back(gv(r * cosf(a), r * sinf(a), zTop));
    }
    V.push_back(gv(0.f, 0.f, zBot)); // bottom center

    const uint32_t cBot = base + 2 * segs;

    // Sides: segs quads = segs*2 triangles = segs*6 indices
    for (int i = 0; i < segs; ++i) {
        int j = (i + 1) % segs;
        uint32_t b0 = base + i,        b1 = base + j;
        uint32_t t0 = base + segs + i, t1 = base + segs + j;
        I.push_back(b0); I.push_back(b1); I.push_back(t1);
        I.push_back(b0); I.push_back(t1); I.push_back(t0);
    }
    // Bottom cap fan: segs triangles = segs*3 indices
    for (int i = 0; i < segs; ++i) {
        int j = (i + 1) % segs;
        I.push_back(cBot); I.push_back(base + i); I.push_back(base + j);
    }
}

// Cone with base cap.
// Vertices: segs base ring + 1 tip + 1 base center = segs+2
// Indices:  segs*3 (sides) + segs*3 (base cap) = segs*6
static void appendCone(
    std::vector<GizmoVertex>& V,
    std::vector<uint32_t>&    I,
    float zBase, float zTip, float rBase, int segs)
{
    const auto base = static_cast<uint32_t>(V.size());

    for (int i = 0; i < segs; ++i) {
        float a = 2.f * kPi * i / segs;
        V.push_back(gv(rBase * cosf(a), rBase * sinf(a), zBase));
    }
    V.push_back(gv(0.f, 0.f, zTip));  // tip
    V.push_back(gv(0.f, 0.f, zBase)); // base center

    const uint32_t tip   = base + segs;
    const uint32_t cBase = base + segs + 1;

    // Sides: segs triangles = segs*3 indices
    for (int i = 0; i < segs; ++i) {
        int j = (i + 1) % segs;
        I.push_back(base + i); I.push_back(tip); I.push_back(base + j);
    }
    // Base cap fan: segs triangles = segs*3 indices
    for (int i = 0; i < segs; ++i) {
        int j = (i + 1) % segs;
        I.push_back(cBase); I.push_back(base + j); I.push_back(base + i);
    }
}

// Flat ring in the XY plane (z=0), around the Z axis.
// Vertices: segs inner + segs outer = 2*segs
// Indices:  segs*2 quads = segs*6 indices
static void appendRing(
    std::vector<GizmoVertex>& V,
    std::vector<uint32_t>&    I,
    float rInner, float rOuter, int segs)
{
    const auto base = static_cast<uint32_t>(V.size());

    for (int i = 0; i < segs; ++i) {
        float a = 2.f * kPi * i / segs;
        V.push_back(gv(rInner * cosf(a), rInner * sinf(a), 0.f));
    }
    for (int i = 0; i < segs; ++i) {
        float a = 2.f * kPi * i / segs;
        V.push_back(gv(rOuter * cosf(a), rOuter * sinf(a), 0.f));
    }

    // Quad strip: segs*2 triangles = segs*6 indices
    for (int i = 0; i < segs; ++i) {
        int j = (i + 1) % segs;
        uint32_t i0 = base + i,        i1 = base + j;
        uint32_t o0 = base + segs + i, o1 = base + segs + j;
        I.push_back(i0); I.push_back(o0); I.push_back(o1);
        I.push_back(i0); I.push_back(o1); I.push_back(i1);
    }
}

// Axis-aligned box centered on Z axis.
// Vertices: 8
// Indices:  36 (6 faces * 2 triangles * 3)
static void appendBox(
    std::vector<GizmoVertex>& V,
    std::vector<uint32_t>&    I,
    float hw, float zBot, float zTop)
{
    const auto b = static_cast<uint32_t>(V.size());

    V.push_back(gv(-hw, -hw, zBot)); // 0
    V.push_back(gv( hw, -hw, zBot)); // 1
    V.push_back(gv( hw,  hw, zBot)); // 2
    V.push_back(gv(-hw,  hw, zBot)); // 3
    V.push_back(gv(-hw, -hw, zTop)); // 4
    V.push_back(gv( hw, -hw, zTop)); // 5
    V.push_back(gv( hw,  hw, zTop)); // 6
    V.push_back(gv(-hw,  hw, zTop)); // 7

    // Bottom (-Z)
    I.push_back(b+0); I.push_back(b+2); I.push_back(b+1);
    I.push_back(b+0); I.push_back(b+3); I.push_back(b+2);
    // Top (+Z)
    I.push_back(b+4); I.push_back(b+5); I.push_back(b+6);
    I.push_back(b+4); I.push_back(b+6); I.push_back(b+7);
    // Front (-Y)
    I.push_back(b+0); I.push_back(b+1); I.push_back(b+5);
    I.push_back(b+0); I.push_back(b+5); I.push_back(b+4);
    // Back (+Y)
    I.push_back(b+2); I.push_back(b+3); I.push_back(b+7);
    I.push_back(b+2); I.push_back(b+7); I.push_back(b+6);
    // Left (-X)
    I.push_back(b+3); I.push_back(b+0); I.push_back(b+4);
    I.push_back(b+3); I.push_back(b+4); I.push_back(b+7);
    // Right (+X)
    I.push_back(b+1); I.push_back(b+2); I.push_back(b+6);
    I.push_back(b+1); I.push_back(b+6); I.push_back(b+5);
}

GizmoBufferData buildGizmoBuffers()
{
    GizmoBufferData data;
    auto& V = data.vertices;
    auto& I = data.indices;

    // ── Translate: shaft (cylinder) + head (cone) ─────────────────────────
    {
        data.translate.firstIndex = static_cast<uint32_t>(I.size());
        appendCylinder(V, I, 0.00f, 0.78f, 0.030f, SHAFT_SEGS);
        appendCone    (V, I, 0.78f, 1.00f, 0.070f, SHAFT_SEGS);
        data.translate.indexCount =
            static_cast<uint32_t>(I.size()) - data.translate.firstIndex;
    }

    // ── Rotate: flat ring ─────────────────────────────────────────────────
    {
        data.rotate.firstIndex = static_cast<uint32_t>(I.size());
        appendRing(V, I, 0.80f, 0.95f, RING_SEGS);
        data.rotate.indexCount =
            static_cast<uint32_t>(I.size()) - data.rotate.firstIndex;
    }

    // ── Scale: shaft (cylinder) + cube handle ────────────────────────────
    {
        data.scale.firstIndex = static_cast<uint32_t>(I.size());
        appendCylinder(V, I, 0.00f, 0.88f, 0.030f, SHAFT_SEGS);
        appendBox     (V, I, 0.040f, 0.88f, 1.00f);
        data.scale.indexCount =
            static_cast<uint32_t>(I.size()) - data.scale.firstIndex;
    }

    // ── Self-verification ────────────────────────────────────────────────
    // Cylinder(segs):  2*segs+1 verts, segs*9  indices
    // Cone(segs):      segs+2   verts, segs*6  indices
    // Ring(segs):      2*segs   verts, segs*6  indices
    // Box:             8        verts, 36      indices
    //
    // Translate: (2*12+1)+(12+2) = 39 verts, (9+6)*12 = 180 indices
    // Rotate:    2*32 = 64 verts, 6*32 = 192 indices
    // Scale:     (2*12+1)+8 = 33 verts, 9*12+36 = 144 indices
    // Total:     136 verts, 516 indices

    assert(data.translate.indexCount == static_cast<uint32_t>(15 * SHAFT_SEGS));
    assert(data.rotate.indexCount    == static_cast<uint32_t>( 6 * RING_SEGS));
    assert(data.scale.indexCount     == static_cast<uint32_t>( 9 * SHAFT_SEGS + 36));

    assert(V.size() == static_cast<size_t>(
        (2 * SHAFT_SEGS + 1) + (SHAFT_SEGS + 2) +  // translate
        (2 * RING_SEGS) +                           // rotate
        (2 * SHAFT_SEGS + 1) + 8));                 // scale

    assert(I.size() == data.translate.indexCount
                     + data.rotate.indexCount
                     + data.scale.indexCount);

    return data;
}

} // namespace Phantom::Gizmo
