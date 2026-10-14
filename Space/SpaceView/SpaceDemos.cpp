#include "SpaceDemos.h"

#include "../Space/CompactSpaceHash.h"
#include "../Space/KDTree.h"
#include "../Space/Octree.h"
#include "../Space/SignedDistanceCalculator.h"
#include "../Space/SpaceHash.h"
#include "CGLib/Math/Box3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Vector3d.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <system_error>
#include <vector>

using Phantom::Math::Box3df;
using Phantom::Math::Sphere3d;
using Phantom::Math::Vector3df;

namespace Phantom::VKSpace {

namespace {

float clampFinite(const float v, const float lo, const float hi, const float fallback) {
    if (!std::isfinite(v)) return fallback;
    return std::min(std::max(v, lo), hi);
}

int clampInt(const int v, const int lo, const int hi) { return std::min(std::max(v, lo), hi); }

// n x n x n cell centers of `box`, x slowest, z fastest (the order the hashes are filled in).
std::vector<Vector3df> latticeCenters(const Box3df& box, const int nx, const int ny, const int nz) {
    std::vector<Vector3df> points;
    points.reserve(static_cast<size_t>(nx) * ny * nz);
    const auto mn  = box.getMin();
    const auto len = box.getLength();
    for (int ix = 0; ix < nx; ++ix)
        for (int iy = 0; iy < ny; ++iy)
            for (int iz = 0; iz < nz; ++iz) {
                const float ux = (ix + 0.5f) / nx;
                const float uy = (iy + 0.5f) / ny;
                const float uz = (iz + 0.5f) / nz;
                points.emplace_back(mn + Vector3df(len.x * ux, len.y * uy, len.z * uz));
            }
    return points;
}

void addMarker(SpaceResult& out, const Vector3df& p, const float half, const glm::vec4& color) {
    out.addBoxWireframe(
        glm::vec3(p.x - half, p.y - half, p.z - half),
        glm::vec3(p.x + half, p.y + half, p.z + half),
        color);
}

} // namespace

bool parseDemoFloat(const std::string& text, float& out) {
    float v = 0.f;
    const char* const first = text.data();
    const char* const last  = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || ptr != last || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool parseDemoInt(const std::string& text, int& out) {
    int v = 0;
    const char* const first = text.data();
    const char* const last  = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || ptr != last) return false;
    out = v;
    return true;
}

// ---------------------------------------------------------------- hashes

void HashDemoParams::sanitize() {
    searchRadius = clampFinite(searchRadius, kMinRadius, kMaxRadius, HashDemoParams{}.searchRadius);
}

bool HashDemoParams::set(const std::string& name, const std::string& value) {
    float v = 0.f;
    if (name == "SearchRadius" && parseDemoFloat(value, v)) { searchRadius = v; sanitize(); return true; }
    return false;
}

HashDemoStats runSpaceHashDemo(HashDemoParams params, SpaceResult& out) {
    params.sanitize();
    const Box3df box(Vector3df(-1.f, -1.f, -1.f), Vector3df(1.f, 1.f, 1.f));
    const float radius = params.searchRadius;

    const auto points = latticeCenters(box, 6, 6, 6);

    Phantom::Space::SpaceHash hash(radius, static_cast<int>(points.size() * 2 + 1));
    for (const auto& p : points) hash.add(p);

    out.clear();

    const float half = std::min(radius * 0.2f, 0.08f);
    HashDemoStats stats;
    for (const auto& p : points) {
        const auto neighbors = hash.findNeighborIndices(p);
        stats.totalNeighbors += static_cast<int>(neighbors.size());
        // The list includes the point itself, so "more than one" means a real neighbor.
        if (neighbors.size() > 1) addMarker(out, p, half, {1.f, 1.f, 0.f, 1.f});
    }
    return stats;
}

HashDemoStats runCompactSpaceHashDemo(HashDemoParams params, SpaceResult& out) {
    params.sanitize();
    const Box3df box(Vector3df(0.f, 0.f, 0.f), Vector3df(1.f, 1.f, 1.f));
    const float radius = params.searchRadius;

    const auto points = latticeCenters(box, 6, 6, 6);

    Phantom::Space::CompactSpaceHash hash(radius, static_cast<int>(points.size() * 2 + 1));
    for (const auto& p : points) hash.add(p);

    out.clear();

    const float half = std::min(radius * 0.2f, 0.08f);
    HashDemoStats stats;
    std::vector<int> neighbors; // one buffer for all queries
    for (const auto& p : points) {
        hash.findNeighborIndices(p, neighbors);
        stats.totalNeighbors += static_cast<int>(neighbors.size());
        if (!neighbors.empty()) addMarker(out, p, half, {0.f, 1.f, 1.f, 1.f});
    }
    return stats;
}

// ---------------------------------------------------------------- KDTree

void KDTreeDemoParams::sanitize() { k = clampInt(k, kMinK, kMaxK); }

bool KDTreeDemoParams::set(const std::string& name, const std::string& value) {
    int v = 0;
    if (name == "k" && parseDemoInt(value, v)) { k = v; sanitize(); return true; }
    return false;
}

KDTreeDemoStats runKDTreeDemo(KDTreeDemoParams params, SpaceResult& out) {
    params.sanitize();
    const Box3df box(Vector3df(-1.f, -1.f, -1.f), Vector3df(1.f, 1.f, 1.f));
    const int n = 5;
    const auto len = box.getLength();

    const auto points = latticeCenters(box, n, n, n);

    Phantom::Space::KDTree tree;
    tree.build(points);

    out.clear();

    const float half = std::min({len.x / n, len.y / n, len.z / n}) * 0.2f;

    std::vector<int> nearest;
    Phantom::Space::KDTree::KNearestScratch scratch;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        addMarker(out, p, half, {0.f, 1.f, 0.f, 1.f});

        // k + 1 because the point itself is the nearest hit; it is skipped below.
        tree.findKNearestIndices(p, static_cast<size_t>(params.k) + 1, nearest, scratch);
        int linked = 0;
        for (const int j : nearest) {
            if (static_cast<size_t>(j) == i) continue;
            if (linked == params.k) break;
            const auto& np = points[static_cast<size_t>(j)];
            out.addLine(glm::vec3(p.x, p.y, p.z), glm::vec3(np.x, np.y, np.z), glm::vec4(0.2f, 1.0f, 0.2f, 0.45f));
            ++linked;
        }
    }

    KDTreeDemoStats stats;
    stats.lineIndexCount = static_cast<int>(out.lineIndices.size());
    return stats;
}

// ---------------------------------------------------------------- Octree

void OctreeDemoParams::sanitize() { maxDepth = clampInt(maxDepth, kMinDepth, kMaxDepth); }

bool OctreeDemoParams::set(const std::string& name, const std::string& value) {
    int v = 0;
    if (name == "MaxDepth" && parseDemoInt(value, v)) { maxDepth = v; sanitize(); return true; }
    return false;
}

OctreeDemoStats runOctreeDemo(OctreeDemoParams params, SpaceResult& out) {
    params.sanitize();
    using Phantom::Space::Octree;

    const Box3df box(Vector3df(-1.f, -1.f, -1.f), Vector3df(1.f, 1.f, 1.f));
    Octree tree(box);
    const int depth = params.maxDepth;

    // Levels 1..depth are drawn (root = level 1), so only depth - 1 levels of children are needed.
    std::function<void(Octree&, int)> build = [&](Octree& node, int remain) {
        if (remain <= 0) return;
        node.createChildren();
        for (auto& child : node.getChildren())
            if (child) build(*child, remain - 1);
    };
    build(tree, depth - 1);

    out.clear();

    OctreeDemoStats stats;
    std::function<void(const Octree&, int)> collect = [&](const Octree& node, int level) {
        if (level > depth) return;
        const auto b  = node.getBox();
        const auto mn = b.getMin();
        const auto mx = b.getMax();
        out.addBoxWireframe(
            glm::vec3(mn.x, mn.y, mn.z),
            glm::vec3(mx.x, mx.y, mx.z),
            {1.f, 0.5f, 0.f, 1.f});
        ++stats.nodeCount;
        for (const auto& child : node.getChildren())
            if (child) collect(*child, level + 1);
    };
    collect(tree, 1);
    return stats;
}

// ---------------------------------------------------------------- SignedDistance

void SignedDistanceDemoParams::sanitize() {
    sphereRadius = clampFinite(sphereRadius, kMinRadius, kMaxRadius, SignedDistanceDemoParams{}.sphereRadius);
    samples = clampInt(samples, kMinSamples, kMaxSamples);
}

bool SignedDistanceDemoParams::set(const std::string& name, const std::string& value) {
    float f = 0.f;
    int i = 0;
    if (name == "SphereRadius" && parseDemoFloat(value, f)) { sphereRadius = f; sanitize(); return true; }
    if (name == "Samples" && parseDemoInt(value, i)) { samples = i; sanitize(); return true; }
    return false;
}

SignedDistanceDemoStats runSignedDistanceDemo(SignedDistanceDemoParams params, SpaceResult& out) {
    params.sanitize();
    const Sphere3d<float> sphere(Vector3df(0.f, 0.f, 0.f), params.sphereRadius);
    const auto bb = sphere.getBoundingBox();

    out.clear();

    const int n = params.samples;
    SignedDistanceDemoStats stats;
    stats.minSdf = std::numeric_limits<float>::max();
    stats.maxSdf = std::numeric_limits<float>::lowest();
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k) {
                const float u = i / static_cast<float>(n);
                const float v = j / static_cast<float>(n);
                const float w = k / static_cast<float>(n);
                const auto pos = bb.getPosition(u, v, w);
                const float sdf = Phantom::Space::SignedDistanceCalculator<float>::calculate(pos, sphere);
                stats.minSdf = std::min(stats.minSdf, sdf);
                stats.maxSdf = std::max(stats.maxSdf, sdf);
                ++stats.sampleCount;
                // Inside (sdf < 0) is blue, outside red.
                const float t = std::max(0.f, std::min(1.f, -sdf / params.sphereRadius * 0.5f + 0.5f));
                out.addPoint(glm::vec3(pos.x, pos.y, pos.z), {1.f - t, 0.f, t, 1.f}, 4.f);
            }
    return stats;
}

} // namespace VKSpace
