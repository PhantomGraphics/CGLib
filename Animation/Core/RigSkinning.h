#pragma once

#include "AuthoredRig.h"
#include "SkinnedMesh.h"

#include <cstdint>
#include <vector>

namespace Phantom::Animation {

// Weights of one source vertex: up to 4 (bone id, weight) pairs. Unused slots have id 0 / weight 0. After any edit made
// through RigSkinning the weights are non-negative, refer to existing bones and sum to 1.
struct VertexInfluence {
    uint32_t boneId[4]{0, 0, 0, 0};
    float    weight[4]{0.f, 0.f, 0.f, 0.f};
};

// Weights for a whole mesh. Indexed by *source* vertex: render vertices that were split for UV / normal reasons share
// one source vertex (RigMesh::sourceIndex), so they always carry identical weights and cannot tear along a seam.
struct RigBinding {
    std::vector<VertexInfluence> weights;
    uint64_t                     rigRevision = 0; // AuthoredRig::revision() the binding was made for
};

// Render mesh in rig space (bind pose). SkinVertex bone fields are ignored here; they are produced by buildSkinnedMesh.
struct RigMesh {
    std::vector<SkinVertex> vertices;
    std::vector<uint32_t>   indices;
    std::vector<uint32_t>   sourceIndex; // per render vertex -> source vertex
    uint32_t                sourceCount = 0;
    int                     materialIndex = -1;
};

// Source-vertex adjacency (edge neighbours, sorted, no duplicates).
using RigAdjacency = std::vector<std::vector<uint32_t>>;

enum class BrushMode { Add, Subtract, Smooth };

struct BrushSample {
    uint32_t sourceVertex = 0;
    float    falloff = 1.f; // 0..1
};

// Settings of the limited automatic weights. Method::Distance is the original behaviour (every other field is ignored).
// Method::Limited picks, for each vertex, the nearest bones (distance to the head-tail segment) that are allowed on its side of
// the rig's X = 0 plane and no farther than maxDistance, at most maxBones of them, weighted by inverse squared distance.
struct AutoWeightOptions {
    enum class Method { Distance = 0, Limited = 1 };
    Method method = Method::Distance;
    float  maxDistance = 0.f;          // rig units; 0 = no distance limit
    int    maxBones = 4;               // candidate bones per vertex, 1..4
    // Side groups, as bone ids. A leftBones bone only influences vertices on the +X side of the X = 0 plane, a rightBones bone only the -X
    // side. Bones in neither group (the centre line) and vertices within centerTolerance of the plane are not restricted.
    std::vector<uint32_t> leftBones;
    std::vector<uint32_t> rightBones;
    float  centerTolerance = 1.0e-4f;
};

// A vertex that no allowed bone reaches keeps a nearest-bone fallback in `binding` (so the mesh still draws) and is listed in `unassigned`
// (source vertex indices, ascending); callers decide whether to accept it. Distance never reports unassigned vertices.
struct AutoWeightResult {
    RigBinding            binding;
    std::vector<uint32_t> unassigned;
};

namespace RigSkinning {

// Groups render vertices that share an identical position into one source vertex. Returns the per-vertex source index
// and writes the number of source vertices to sourceCount.
std::vector<uint32_t> computeSourceIndices(const std::vector<glm::vec3>& positions, uint32_t& sourceCount);

// Builds a RigMesh from render vertices + indices, welding by position.
RigMesh makeRigMesh(std::vector<SkinVertex> vertices, std::vector<uint32_t> indices, int materialIndex = -1);

// Moves a mesh into rig space with an affine matrix (normals by the inverse transpose; triangle winding is flipped when
// the matrix mirrors). Fails, leaving the mesh unchanged, for non-finite or non-invertible matrices.
RigResult transformToRigSpace(RigMesh& mesh, const glm::mat4& meshToRig);

RigAdjacency buildAdjacency(const RigMesh& mesh);
std::vector<glm::vec3> sourcePositions(const RigMesh& mesh);

// Distance-based automatic weights: the nearest (up to) 4 bones by distance to the head-tail segment, weighted by inverse
// squared distance. Ties are broken by bone id; a zero distance stays finite.
RigResult autoBind(const AuthoredRig& rig, const RigMesh& mesh, RigBinding& out);

// Checks the options against the rig, then computes the weights. Fails (outputs untouched) for an empty rig, non-finite vertices, unknown bones,
// a bone in both groups, maxBones outside 1..4 or a negative / non-finite distance or tolerance.
RigResult autoWeights(const AuthoredRig& rig, const RigMesh& mesh, const AutoWeightOptions& options, AutoWeightResult& out);

// The source vertices connected to `seeds` through mesh edges (seeds included), ascending.
std::vector<uint32_t> connectedComponents(const RigAdjacency& adjacency, const std::vector<uint32_t>& seeds);

// Every vertex fully weighted to one bone.
RigResult rigidBind(const AuthoredRig& rig, const RigMesh& mesh, uint32_t boneId, RigBinding& out);

// Replaces the weights of the listed source vertices (pairs of bone id, weight), then sanitizes them. An all-zero result
// falls back to the vertex's previous strongest bone.
RigResult setWeights(RigBinding& binding, const AuthoredRig& rig, const std::vector<uint32_t>& sourceVertices,
                     const std::vector<std::pair<uint32_t, float>>& influences);

// Sanitizes one influence set (drop unknown bones / negative / non-finite, merge duplicates, keep the top 4, normalize).
// fallbackBone is used when nothing positive remains. Returns false if no valid result was possible.
bool sanitize(VertexInfluence& v, const AuthoredRig& rig, uint32_t fallbackBone);
RigResult normalizeAll(RigBinding& binding, const AuthoredRig& rig);

// Brush samples for every source vertex within radius of center (quadratic falloff). `visible` (optional, one flag per
// source vertex) restricts the brush to surface that is actually seen.
std::vector<BrushSample> selectInRadius(const std::vector<glm::vec3>& positions, const glm::vec3& center, float radius,
                                        const std::vector<uint8_t>* visible = nullptr);

// Add / Subtract move the weight of boneId by strength*falloff (others compensate proportionally); Smooth blends each
// vertex toward the average of its neighbours. Smooth reads a snapshot, so the result does not depend on sample order.
RigResult applyBrush(RigBinding& binding, const AuthoredRig& rig, const RigAdjacency& adjacency, BrushMode mode,
                     uint32_t boneId, float strength, const std::vector<BrushSample>& samples);

// Whether the binding still matches the rig and mesh.
bool isBindingValid(const RigBinding& binding, const AuthoredRig& rig, const RigMesh& mesh);

// Produces the runtime skeleton and skinned mesh. At the rest pose skinning reproduces the rig-space vertices exactly.
RigResult buildSkinnedMesh(const AuthoredRig& rig, const RigBinding& binding, const RigMesh& mesh, Skeleton& skeleton,
                           SkinnedMesh& out);

} // namespace RigSkinning
} // namespace Phantom::Animation
