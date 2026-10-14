#pragma once

#include "World.h"

#include <string>

namespace Phantom::VKSpace {

/// @file
/// CPU side of the SpaceView demos: each function builds a synthetic sample, runs one Space algorithm and
/// writes the wireframe/points into a SpaceResult. No ImGui, no Vulkan -- the ImGui panels only edit the
/// parameters and request a run, and the command dispatcher goes through the very same panel run(), so
/// the GUI and the scenario commands cannot drift apart. SpaceTest links this file to check the output
/// without a window (SpaceTest/SpaceDemoTest.cpp).
///
/// Parameters are always used through sanitize(): out-of-range values are clamped (never rejected) so a
/// scripted `SetParam` cannot request an unbounded amount of work.

/// Parses the whole of `text` as a finite float / an int. Rejects empty text, trailing characters, NaN and
/// Inf (std::from_chars accepts "nan" and "inf"). Returns false and leaves `out` unchanged on failure.
bool parseDemoFloat(const std::string& text, float& out);
bool parseDemoInt(const std::string& text, int& out);

// ---------------------------------------------------------------- SpaceHash / CompactSpaceHash

struct HashDemoParams {
    static constexpr float kMinRadius = 0.01f;
    static constexpr float kMaxRadius = 2.0f;

    float searchRadius = 0.35f;

    void sanitize();
    /// Sets a parameter by its command name ("SearchRadius"). False for an unknown name or unparsable value.
    bool set(const std::string& name, const std::string& value);
};

struct HashDemoStats {
    int totalNeighbors = 0; ///< Neighbor references summed over the 6x6x6 sample lattice.
};

/// SpaceHash over a 6x6x6 lattice in [-1,1]^3; boxes mark points that have another point in range.
HashDemoStats runSpaceHashDemo(HashDemoParams params, SpaceResult& out);
/// CompactSpaceHash over a 6x6x6 lattice in [0,1]^3; boxes mark points with any neighbor reference.
HashDemoStats runCompactSpaceHashDemo(HashDemoParams params, SpaceResult& out);

// ---------------------------------------------------------------- KDTree

struct KDTreeDemoParams {
    static constexpr int kMinK = 1;
    static constexpr int kMaxK = 64;

    int k = 8;

    void sanitize();
    bool set(const std::string& name, const std::string& value); ///< "k"
};

struct KDTreeDemoStats {
    int lineIndexCount = 0;
};

/// 5x5x5 lattice in [-1,1]^3; every point is linked to its k nearest other points (found with KDTree).
KDTreeDemoStats runKDTreeDemo(KDTreeDemoParams params, SpaceResult& out);

// ---------------------------------------------------------------- Octree

struct OctreeDemoParams {
    static constexpr int kMinDepth = 1;
    // Depth d draws (8^d - 1) / 7 boxes: depth 7 is ~300k. The slider used to allow 8, and the old code also
    // built one more (undrawn) level than it showed -- ~19M nodes at depth 8, i.e. gigabytes.
    static constexpr int kMaxDepth = 7;

    int maxDepth = 1;

    void sanitize();
    bool set(const std::string& name, const std::string& value); ///< "MaxDepth"
};

struct OctreeDemoStats {
    int nodeCount = 0;
};

/// Subdivides [-1,1]^3 uniformly and draws the box of every node on levels 1..maxDepth (the root is level 1).
OctreeDemoStats runOctreeDemo(OctreeDemoParams params, SpaceResult& out);

// ---------------------------------------------------------------- SignedDistance

struct SignedDistanceDemoParams {
    static constexpr float kMinRadius = 0.05f;
    static constexpr float kMaxRadius = 1.9f; // keeps every sample inside [-2,2]
    static constexpr int kMinSamples = 2;
    static constexpr int kMaxSamples = 100;

    float sphereRadius = 0.5f;
    int samples = 10;

    void sanitize();
    bool set(const std::string& name, const std::string& value); ///< "SphereRadius", "Samples"
};

struct SignedDistanceDemoStats {
    int sampleCount = 0;
    float minSdf = 0.f;
    float maxSdf = 0.f;
};

/// Samples the signed distance to a sphere on a `samples`^3 grid over its bounding box; one point each.
SignedDistanceDemoStats runSignedDistanceDemo(SignedDistanceDemoParams params, SpaceResult& out);

} // namespace VKSpace
