#include "RigSkinning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace Phantom::Animation::RigSkinning {

namespace {

using Pair = std::pair<uint32_t, double>; // bone id, weight

bool finiteMat(const glm::mat4& m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

std::vector<Pair> toPairs(const VertexInfluence& v) {
    std::vector<Pair> out;
    for (int i = 0; i < 4; ++i)
        if (v.boneId[i] != 0) out.emplace_back(v.boneId[i], static_cast<double>(v.weight[i]));
    return out;
}

uint32_t strongestBone(const VertexInfluence& v) {
    uint32_t best = 0;
    float bestW = -1.f;
    for (int i = 0; i < 4; ++i) {
        if (v.boneId[i] != 0 && v.weight[i] > bestW) { bestW = v.weight[i]; best = v.boneId[i]; }
    }
    return best;
}

// Merge duplicate ids, drop invalid entries, keep the top 4, normalize. Returns false if nothing positive remains.
bool normalizePairs(std::vector<Pair>& p, const AuthoredRig& rig, VertexInfluence& out) {
    std::map<uint32_t, double> merged;
    for (const auto& [id, w] : p) {
        if (!rig.find(id) || !std::isfinite(w) || w <= 0.0) continue;
        merged[id] += w;
    }
    std::vector<Pair> v(merged.begin(), merged.end());
    std::stable_sort(v.begin(), v.end(), [](const Pair& a, const Pair& b) { return a.second > b.second; }); // ties keep id order
    if (v.size() > 4) v.resize(4);
    double sum = 0.0;
    for (const auto& e : v) sum += e.second;
    if (!(sum > 0.0)) return false;
    out = VertexInfluence{};
    for (size_t i = 0; i < v.size(); ++i) {
        out.boneId[i] = v[i].first;
        out.weight[i] = static_cast<float>(v[i].second / sum);
    }
    return true;
}

double segmentDistanceSq(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    const glm::dvec3 dp(p), da(a), db(b);
    const glm::dvec3 ab = db - da;
    const double len2 = glm::dot(ab, ab);
    double t = len2 > 0.0 ? glm::dot(dp - da, ab) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const glm::dvec3 d = dp - (da + ab * t);
    return glm::dot(d, d);
}

} // namespace

std::vector<uint32_t> computeSourceIndices(const std::vector<glm::vec3>& positions, uint32_t& sourceCount) {
    std::map<std::array<uint32_t, 3>, uint32_t> seen;
    std::vector<uint32_t> out(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        std::array<uint32_t, 3> key{};
        for (int k = 0; k < 3; ++k) {
            float f = positions[i][k];
            if (f == 0.f) f = 0.f; // fold -0 into +0
            std::memcpy(&key[k], &f, sizeof(float));
        }
        auto it = seen.find(key);
        if (it == seen.end()) it = seen.emplace(key, static_cast<uint32_t>(seen.size())).first;
        out[i] = it->second;
    }
    sourceCount = static_cast<uint32_t>(seen.size());
    return out;
}

RigMesh makeRigMesh(std::vector<SkinVertex> vertices, std::vector<uint32_t> indices, int materialIndex) {
    RigMesh m;
    std::vector<glm::vec3> pos;
    pos.reserve(vertices.size());
    for (const auto& v : vertices) pos.push_back(v.position);
    m.sourceIndex = computeSourceIndices(pos, m.sourceCount);
    m.vertices = std::move(vertices);
    m.indices = std::move(indices);
    m.materialIndex = materialIndex;
    return m;
}

RigResult transformToRigSpace(RigMesh& mesh, const glm::mat4& meshToRig) {
    if (!finiteMat(meshToRig)) return RigResult::failure("mesh transform is not finite");
    const glm::mat3 lin(meshToRig);
    const double det = glm::determinant(glm::dmat3(lin));
    if (!std::isfinite(det) || std::fabs(det) < 1.0e-12) return RigResult::failure("mesh transform is not invertible");
    const glm::mat3 normalMat = glm::transpose(glm::inverse(lin));
    for (auto& v : mesh.vertices) {
        v.position = glm::vec3(meshToRig * glm::vec4(v.position, 1.f));
        const glm::vec3 n = normalMat * v.normal;
        const float l = glm::length(n);
        v.normal = l > 0.f ? n / l : v.normal;
    }
    if (det < 0.0) {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    }
    return RigResult::success();
}

RigAdjacency buildAdjacency(const RigMesh& mesh) {
    RigAdjacency adj(mesh.sourceCount);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t s[3];
        for (int k = 0; k < 3; ++k) {
            const uint32_t idx = mesh.indices[i + k];
            s[k] = idx < mesh.sourceIndex.size() ? mesh.sourceIndex[idx] : UINT32_MAX;
        }
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = s[k], b = s[(k + 1) % 3];
            if (a == UINT32_MAX || b == UINT32_MAX || a == b) continue;
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
    }
    for (auto& n : adj) {
        std::sort(n.begin(), n.end());
        n.erase(std::unique(n.begin(), n.end()), n.end());
    }
    return adj;
}

std::vector<glm::vec3> sourcePositions(const RigMesh& mesh) {
    std::vector<glm::vec3> out(mesh.sourceCount, glm::vec3(0.f));
    std::vector<uint8_t> set(mesh.sourceCount, 0);
    for (size_t i = 0; i < mesh.vertices.size() && i < mesh.sourceIndex.size(); ++i) {
        const uint32_t s = mesh.sourceIndex[i];
        if (s < out.size() && !set[s]) { out[s] = mesh.vertices[i].position; set[s] = 1; }
    }
    return out;
}

bool sanitize(VertexInfluence& v, const AuthoredRig& rig, uint32_t fallbackBone) {
    auto pairs = toPairs(v);
    VertexInfluence out;
    if (normalizePairs(pairs, rig, out)) { v = out; return true; }
    if (fallbackBone == 0 || !rig.find(fallbackBone)) return false;
    v = VertexInfluence{};
    v.boneId[0] = fallbackBone;
    v.weight[0] = 1.f;
    return true;
}

RigResult autoBind(const AuthoredRig& rig, const RigMesh& mesh, RigBinding& out) {
    if (rig.empty()) return RigResult::failure("rig has no bones");
    RigResult valid = rig.validate();
    if (!valid) return valid;
    const std::vector<glm::vec3> pos = sourcePositions(mesh);
    for (const auto& p : pos)
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return RigResult::failure("mesh has non-finite vertices");

    RigBinding result;
    result.rigRevision = rig.revision();
    result.weights.resize(pos.size());
    std::vector<std::pair<double, uint32_t>> dist(rig.size());
    for (size_t vi = 0; vi < pos.size(); ++vi) {
        for (size_t bi = 0; bi < rig.size(); ++bi) {
            const AuthoredBone& b = rig.bones()[bi];
            dist[bi] = {segmentDistanceSq(pos[vi], b.head, b.tail), b.id};
        }
        const size_t n = std::min<size_t>(4, dist.size());
        std::partial_sort(dist.begin(), dist.begin() + n, dist.end()); // distance, then bone id
        std::vector<Pair> pairs;
        for (size_t k = 0; k < n; ++k) pairs.emplace_back(dist[k].second, 1.0 / std::max(dist[k].first, 1.0e-12));
        VertexInfluence inf;
        if (!normalizePairs(pairs, rig, inf)) return RigResult::failure("automatic weights failed");
        result.weights[vi] = inf;
    }
    out = std::move(result);
    return RigResult::success();
}

RigResult autoWeights(const AuthoredRig& rig, const RigMesh& mesh, const AutoWeightOptions& options, AutoWeightResult& out) {
    if (options.method == AutoWeightOptions::Method::Distance) {
        AutoWeightResult legacy;
        RigResult r = autoBind(rig, mesh, legacy.binding);
        if (!r) return r;
        out = std::move(legacy);
        return RigResult::success();
    }
    if (rig.empty()) return RigResult::failure("rig has no bones");
    RigResult valid = rig.validate();
    if (!valid) return valid;
    if (options.maxBones < 1 || options.maxBones > 4) return RigResult::failure("maxBones must be 1 to 4");
    if (!std::isfinite(options.maxDistance) || options.maxDistance < 0.f) return RigResult::failure("maxDistance must be zero (no limit) or a positive number");
    if (!std::isfinite(options.centerTolerance) || options.centerTolerance < 0.f) return RigResult::failure("invalid centre tolerance");
    std::map<uint32_t, int> side; // bone id -> +1 left (+X), -1 right (-X)
    for (uint32_t id : options.leftBones) {
        if (!rig.find(id)) return RigResult::failure("the left group names a bone that does not exist");
        side[id] = 1;
    }
    for (uint32_t id : options.rightBones) {
        if (!rig.find(id)) return RigResult::failure("the right group names a bone that does not exist");
        if (side.count(id) && side[id] == 1) return RigResult::failure("a bone cannot be in both the left and the right group");
        side[id] = -1;
    }
    const std::vector<glm::vec3> pos = sourcePositions(mesh);
    for (const auto& p : pos)
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return RigResult::failure("mesh has non-finite vertices");

    const double limitSq = options.maxDistance > 0.f ? static_cast<double>(options.maxDistance) * options.maxDistance : -1.0;
    AutoWeightResult result;
    result.binding.rigRevision = rig.revision();
    result.binding.weights.resize(pos.size());
    std::vector<std::pair<double, uint32_t>> eligible, everything;
    for (size_t vi = 0; vi < pos.size(); ++vi) {
        const float x = pos[vi].x;
        const int vertexSide = x > options.centerTolerance ? 1 : x < -options.centerTolerance ? -1 : 0;
        eligible.clear();
        everything.clear();
        for (const AuthoredBone& b : rig.bones()) {
            const double d = segmentDistanceSq(pos[vi], b.head, b.tail);
            everything.emplace_back(d, b.id);
            const auto it = side.find(b.id);
            const bool blocked = vertexSide != 0 && it != side.end() && it->second != vertexSide;
            if (!blocked && (limitSq < 0.0 || d <= limitSq)) eligible.emplace_back(d, b.id);
        }
        const bool reached = !eligible.empty();
        std::vector<std::pair<double, uint32_t>>& pool = reached ? eligible : everything;
        const size_t n = std::min<size_t>(reached ? static_cast<size_t>(options.maxBones) : 1, pool.size());
        std::partial_sort(pool.begin(), pool.begin() + n, pool.end()); // distance, then bone id
        std::vector<Pair> pairs;
        for (size_t k = 0; k < n; ++k) pairs.emplace_back(pool[k].second, 1.0 / std::max(pool[k].first, 1.0e-12));
        VertexInfluence inf;
        if (!normalizePairs(pairs, rig, inf)) return RigResult::failure("automatic weights failed");
        result.binding.weights[vi] = inf;
        if (!reached) result.unassigned.push_back(static_cast<uint32_t>(vi));
    }
    out = std::move(result);
    return RigResult::success();
}

std::vector<uint32_t> connectedComponents(const RigAdjacency& adjacency, const std::vector<uint32_t>& seeds) {
    std::vector<uint8_t> seen(adjacency.size(), 0);
    std::vector<uint32_t> stack;
    for (uint32_t s : seeds)
        if (s < adjacency.size() && !seen[s]) { seen[s] = 1; stack.push_back(s); }
    while (!stack.empty()) {
        const uint32_t v = stack.back();
        stack.pop_back();
        for (uint32_t n : adjacency[v])
            if (!seen[n]) { seen[n] = 1; stack.push_back(n); }
    }
    std::vector<uint32_t> out;
    for (uint32_t i = 0; i < seen.size(); ++i)
        if (seen[i]) out.push_back(i);
    return out;
}

RigResult rigidBind(const AuthoredRig& rig, const RigMesh& mesh, uint32_t boneId, RigBinding& out) {
    if (!rig.find(boneId)) return RigResult::failure("bone does not exist");
    RigBinding result;
    result.rigRevision = rig.revision();
    VertexInfluence inf;
    inf.boneId[0] = boneId;
    inf.weight[0] = 1.f;
    result.weights.assign(mesh.sourceCount, inf);
    out = std::move(result);
    return RigResult::success();
}

RigResult setWeights(RigBinding& binding, const AuthoredRig& rig, const std::vector<uint32_t>& sourceVertices,
                     const std::vector<std::pair<uint32_t, float>>& influences) {
    for (uint32_t s : sourceVertices)
        if (s >= binding.weights.size()) return RigResult::failure("vertex index out of range");
    for (const auto& [id, w] : influences) {
        if (!rig.find(id)) return RigResult::failure("bone does not exist");
        if (!std::isfinite(w)) return RigResult::failure("non-finite weight");
    }
    std::vector<Pair> pairs;
    for (const auto& [id, w] : influences) pairs.emplace_back(id, w);
    VertexInfluence inf;
    const bool ok = normalizePairs(pairs, rig, inf);
    std::vector<VertexInfluence> next = binding.weights;
    for (uint32_t s : sourceVertices) {
        if (ok) next[s] = inf;
        else if (!sanitize(next[s] = VertexInfluence{}, rig, strongestBone(binding.weights[s]))) return RigResult::failure("weights are all zero");
    }
    binding.weights = std::move(next);
    return RigResult::success();
}

RigResult normalizeAll(RigBinding& binding, const AuthoredRig& rig) {
    std::vector<VertexInfluence> next = binding.weights;
    for (size_t i = 0; i < next.size(); ++i) {
        if (!sanitize(next[i], rig, strongestBone(binding.weights[i])))
            return RigResult::failure("vertex has no valid weight");
    }
    binding.weights = std::move(next);
    return RigResult::success();
}

std::vector<BrushSample> selectInRadius(const std::vector<glm::vec3>& positions, const glm::vec3& center, float radius,
                                        const std::vector<uint8_t>* visible) {
    std::vector<BrushSample> out;
    if (!(radius > 0.f) || !std::isfinite(radius)) return out;
    for (size_t i = 0; i < positions.size(); ++i) {
        if (visible && (i >= visible->size() || !(*visible)[i])) continue;
        const float t = glm::distance(positions[i], center) / radius;
        if (t >= 1.f) continue;
        const float f = 1.f - t * t;
        out.push_back({static_cast<uint32_t>(i), f * f});
    }
    return out;
}

RigResult applyBrush(RigBinding& binding, const AuthoredRig& rig, const RigAdjacency& adjacency, BrushMode mode,
                     uint32_t boneId, float strength, const std::vector<BrushSample>& samples) {
    if (!std::isfinite(strength) || strength < 0.f) return RigResult::failure("invalid brush strength");
    if (mode != BrushMode::Smooth && !rig.find(boneId)) return RigResult::failure("bone does not exist");
    for (const auto& s : samples)
        if (s.sourceVertex >= binding.weights.size()) return RigResult::failure("vertex index out of range");

    // Every sample reads the weights as they were before the brush (a snapshot), and the results are written together at the end: only
    // the touched vertices are copied, so a dab on a large mesh does not copy the whole weight array.
    const std::vector<VertexInfluence>& before = binding.weights;
    std::vector<std::pair<uint32_t, VertexInfluence>> updates;
    updates.reserve(samples.size());

    for (const auto& s : samples) {
        const VertexInfluence& cur = before[s.sourceVertex];
        const double amount = std::clamp(static_cast<double>(strength) * s.falloff, 0.0, 1.0);
        std::vector<Pair> pairs;

        if (mode == BrushMode::Smooth) {
            std::map<uint32_t, double> avg;
            const auto* nbrs = s.sourceVertex < adjacency.size() ? &adjacency[s.sourceVertex] : nullptr;
            if (!nbrs || nbrs->empty()) continue;
            for (uint32_t n : *nbrs) {
                if (n >= before.size()) continue;
                for (int k = 0; k < 4; ++k)
                    if (before[n].boneId[k] != 0) avg[before[n].boneId[k]] += before[n].weight[k] / double(nbrs->size());
            }
            std::map<uint32_t, double> blend;
            for (int k = 0; k < 4; ++k)
                if (cur.boneId[k] != 0) blend[cur.boneId[k]] += (1.0 - amount) * cur.weight[k];
            for (const auto& [id, w] : avg) blend[id] += amount * w;
            pairs.assign(blend.begin(), blend.end());
        } else {
            double own = 0.0, others = 0.0;
            for (int k = 0; k < 4; ++k) {
                if (cur.boneId[k] == boneId) own += cur.weight[k];
                else if (cur.boneId[k] != 0) others += cur.weight[k];
            }
            const double target = mode == BrushMode::Add ? std::min(1.0, own + amount) : std::max(0.0, own - amount);
            const double remain = 1.0 - target;
            for (int k = 0; k < 4; ++k) {
                if (cur.boneId[k] == 0 || cur.boneId[k] == boneId) continue;
                pairs.emplace_back(cur.boneId[k], others > 0.0 ? cur.weight[k] / others * remain : 0.0);
            }
            pairs.emplace_back(boneId, target);
        }

        VertexInfluence inf;
        if (normalizePairs(pairs, rig, inf)) {
            updates.emplace_back(s.sourceVertex, inf);
        } else {
            VertexInfluence fallback;
            if (!sanitize(fallback, rig, strongestBone(cur))) return RigResult::failure("weights are all zero");
            updates.emplace_back(s.sourceVertex, fallback);
        }
    }
    for (const auto& [vertex, inf] : updates) binding.weights[vertex] = inf;
    return RigResult::success();
}

bool isBindingValid(const RigBinding& binding, const AuthoredRig& rig, const RigMesh& mesh) {
    if (binding.rigRevision != rig.revision()) return false;
    if (binding.weights.size() != mesh.sourceCount) return false;
    std::vector<uint8_t> known(rig.nextId(), 0);   // bone ids are below nextId
    for (const auto& b : rig.bones()) if (b.id < known.size()) known[b.id] = 1;
    for (const auto& v : binding.weights) {
        double sum = 0.0;
        for (int k = 0; k < 4; ++k) {
            if (v.weight[k] < 0.f || !std::isfinite(v.weight[k])) return false;
            if (v.weight[k] > 0.f && (v.boneId[k] >= known.size() || !known[v.boneId[k]])) return false;
            sum += v.weight[k];
        }
        if (std::fabs(sum - 1.0) > 1.0e-4) return false;
    }
    return true;
}

RigResult buildSkinnedMesh(const AuthoredRig& rig, const RigBinding& binding, const RigMesh& mesh, Skeleton& skeleton,
                           SkinnedMesh& out) {
    if (!isBindingValid(binding, rig, mesh)) return RigResult::failure("binding does not match the rig or mesh");
    if (mesh.sourceIndex.size() != mesh.vertices.size()) return RigResult::failure("mesh source indices are inconsistent");
    for (uint32_t idx : mesh.indices)
        if (idx >= mesh.vertices.size()) return RigResult::failure("mesh index out of range");

    Skeleton sk;
    std::vector<uint32_t> indexToId;
    RigResult r = rig.toSkeleton(sk, &indexToId);
    if (!r) return r;
    std::map<uint32_t, int> idToIndex;
    for (size_t i = 0; i < indexToId.size(); ++i) idToIndex[indexToId[i]] = static_cast<int>(i);

    SkinnedMesh sm;
    sm.indices = mesh.indices;
    sm.materialIndex = mesh.materialIndex;
    sm.vertices = mesh.vertices;
    for (size_t i = 0; i < sm.vertices.size(); ++i) {
        const VertexInfluence& inf = binding.weights[mesh.sourceIndex[i]];
        glm::ivec4 bi(0);
        glm::vec4 bw(0.f);
        for (int k = 0; k < 4; ++k) {
            if (inf.boneId[k] == 0 || inf.weight[k] <= 0.f) continue;
            bi[k] = idToIndex[inf.boneId[k]];
            bw[k] = inf.weight[k];
        }
        sm.vertices[i].boneIndices = bi;
        sm.vertices[i].boneWeights = bw;
    }
    skeleton = std::move(sk);
    out = std::move(sm);
    return RigResult::success();
}

} // namespace Phantom::Animation::RigSkinning
