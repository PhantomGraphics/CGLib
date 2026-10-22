#include "PlanarMeshCleanup.h"
#include "PlanarTriangulation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace Phantom::MeshEdit {
namespace {
using V = Math::Vector3dd;
using Edge = std::pair<uint32_t, uint32_t>;
Edge edge(uint32_t a, uint32_t b) { return std::minmax(a, b); }
struct Geometry {
    std::map<uint32_t, V> p;
    double eps, attributeEps;
    const V& point(const EditableCorner& c) const { return p.find(c.vertex)->second; }
    V normal(const EditableFace& f) const {
        return glm::normalize(glm::cross(point(f.loop[1]) - point(f.loop[0]),
                                         point(f.loop[2]) - point(f.loop[0])));
    }
    bool close(double a, double b) const {
        return std::abs(a - b) <= attributeEps * (1 + std::max(std::abs(a), std::abs(b)));
    }
    // Checking every corner against one representative affine field protects
    // interpolation gradients as well as seams (endpoint equality is insufficient).
    bool compatible(const EditableFace& reference, const EditableFace& other) const {
        const V n = normal(reference), on = normal(other);
        if (glm::dot(n, on) <= 0 || glm::length(glm::cross(n, on)) > eps) return false;
        const auto& a = reference.loop[0];
        const auto& b = reference.loop[1];
        const auto& c = reference.loop[2];
        const V u = point(b) - point(a), v = point(c) - point(a), cross = glm::cross(u, v);
        const double area = glm::dot(cross, n);
        for (const auto& q : other.loop) {
            const V d = point(q) - point(a);
            if (std::abs(glm::dot(d, n)) > eps) return false;
            const double y = glm::dot(glm::cross(d, v), n) / area;
            const double z = glm::dot(glm::cross(u, d), n) / area;
            const double x = 1 - y - z;
            for (int k = 0; k < 2; ++k)
                if (!close(x * a.uv[k] + y * b.uv[k] + z * c.uv[k], q.uv[k])) return false;
            if (a.hasNormal != b.hasNormal || a.hasNormal != c.hasNormal || a.hasNormal != q.hasNormal)
                return false;
            if (q.hasNormal)
                for (int k = 0; k < 3; ++k)
                    if (!close(x * a.normal[k] + y * b.normal[k] + z * c.normal[k], q.normal[k]))
                        return false;
        }
        return true;
    }
};
using Uses = std::map<Edge, std::vector<size_t>>;
using Incident = std::map<uint32_t, std::vector<size_t>>;
Incident vertexUses(const EditableMesh& m) {
    Incident result;
    for (size_t i = 0; i < m.faces.size(); ++i)
        for (const auto& c : m.faces[i].loop) result[c.vertex].push_back(i);
    return result;
}
Uses edgeUses(const EditableMesh& m) {
    Uses uses;
    for (size_t i = 0; i < m.faces.size(); ++i)
        for (size_t k = 0; k < 3; ++k)
            uses[edge(m.faces[i].loop[k].vertex, m.faces[i].loop[(k + 1) % 3].vertex)].push_back(i);
    return uses;
}
bool spend(size_t& work, size_t amount) {
    if (amount >= work) { work = 0; return false; }
    work -= amount;
    return true;
}
bool boundary(const EditableMesh& m, const std::vector<size_t>& faces,
              std::vector<EditableCorner>& loop) {
    std::map<Edge, std::vector<std::pair<EditableCorner, EditableCorner>>> uses;
    for (size_t f : faces)
        for (size_t k = 0; k < 3; ++k) {
            const auto& a = m.faces[f].loop[k];
            const auto& b = m.faces[f].loop[(k + 1) % 3];
            uses[edge(a.vertex, b.vertex)].push_back({a, b});
        }
    std::map<uint32_t, std::pair<EditableCorner, uint32_t>> next;
    for (const auto& [key, u] : uses) {
        if (u.size() != 1) continue;
        if (!next.emplace(u[0].first.vertex, std::pair{u[0].first, u[0].second.vertex}).second)
            return false; // disconnected/non-disk vertex fan: leave it alone
    }
    if (next.size() < 3) return false;
    uint32_t first = next.begin()->first, current = first;
    do {
        const auto it = next.find(current);
        if (it == next.end() || loop.size() == next.size()) return false;
        loop.push_back(it->second.first);
        current = it->second.second;
    } while (current != first);
    return loop.size() == next.size();
}
bool rebuild(EditableMesh& m) {
    if (m.faces.size() > (UINT32_MAX - m.nextEdgeId) / 3) return false;
    m.rebuildEdges();
    return m.validate();
}

// Return false only for a fatal budget/id failure. An unsafe local candidate is
// skipped, keeping its original triangles. Every affected patch participates in
// one candidate so removing a vertex along a crease cannot make a T-junction.
bool removeVertex(EditableMesh& mesh, uint32_t id, const Geometry& g, const Uses& uses, const Incident& vertexFaces,
                  const std::set<Edge>& protectedEdges, const std::set<uint32_t>& protectedVertices,
                  size_t& work, bool& changed) {
    if (protectedVertices.count(id)) return true;
    const auto found = vertexFaces.find(id);
    if (found == vertexFaces.end()) return true;
    const auto& incident = found->second;
    if (!spend(work, incident.size())) return false;
    std::set<size_t> remaining(incident.begin(), incident.end());
    std::vector<std::vector<EditableCorner>> replacements;
    while (!remaining.empty()) {
        const size_t seed = *remaining.begin();
        remaining.erase(seed);
        std::vector<size_t> group{seed};
        for (size_t i = 0; i < group.size(); ++i) {
            for (const auto& c : mesh.faces[group[i]].loop) {
                if (c.vertex == id) continue;
                const auto e = edge(id, c.vertex);
                if (protectedEdges.count(e)) continue;
                const auto u = uses.find(e);
                if (u == uses.end()) continue;
                for (size_t f : u->second) {
                    if (!spend(work, 1)) return false;
                    if (remaining.count(f) && g.compatible(mesh.faces[seed], mesh.faces[f]) &&
                        g.compatible(mesh.faces[f], mesh.faces[seed])) {
                        remaining.erase(f);
                        group.push_back(f);
                    }
                }
            }
        }
        std::vector<EditableCorner> loop;
        if (!boundary(mesh, group, loop)) return true;
        const auto removed = std::find_if(loop.begin(), loop.end(),
                                         [&](const auto& c) { return c.vertex == id; });
        if (removed != loop.end()) {
            if (loop.size() <= 3) return true;
            const size_t k = static_cast<size_t>(removed - loop.begin());
            const V a = g.point(loop[(k + loop.size() - 1) % loop.size()]);
            const V b = g.point(loop[(k + 1) % loop.size()]);
            const V d = b - a, p = g.point(*removed);
            const double dd = glm::dot(d, d);
            if (!(dd > g.eps * g.eps)) return true;
            const double t = glm::dot(p - a, d) / dd;
            if (t <= 0 || t >= 1 ||
                glm::length(p - (a + t * d)) > g.eps) return true;
            loop.erase(removed);
        }
        std::vector<V> points;
        for (const auto& c : loop) points.push_back(g.point(c));
        std::vector<std::array<size_t, 3>> triangles;
        if (!detail::triangulatePlanarBoundary(points, g.normal(mesh.faces[seed]), g.eps, work, triangles))
            return work != 0;
        for (const auto& t : triangles) {
            const V a = points[t[0]], b = points[t[1]], c = points[t[2]];
            const double longest = std::max({glm::length(b - a), glm::length(c - b), glm::length(a - c)});
            // A near-collinear boundary may have acquired float roundoff when
            // Boolean's double intersections were stored. Do not replace a fan
            // with a sliver that another surface would classify as an edge touch.
            if (glm::length(glm::cross(b - a, c - a)) <= g.eps * longest) return true;
            replacements.push_back({loop[t[0]], loop[t[1]], loop[t[2]]});
        }
    }
    if (replacements.size() >= incident.size()) return true;
    if (replacements.size() > UINT32_MAX - mesh.nextFaceId) return false;
    if (!spend(work, mesh.faces.size() + mesh.vertices.size())) return false;
    EditableMesh candidate = mesh;
    const std::set<size_t> removed(incident.begin(), incident.end());
    candidate.faces.clear();
    for (size_t i = 0; i < mesh.faces.size(); ++i)
        if (!removed.count(i)) candidate.faces.push_back(mesh.faces[i]);
    for (const auto& loop : replacements) candidate.addFace(loop);
    candidate.vertices.erase(std::remove_if(candidate.vertices.begin(), candidate.vertices.end(),
                                            [&](const auto& v) { return v.id == id; }), candidate.vertices.end());
    if (!rebuild(candidate)) return true;
    mesh = std::move(candidate);
    changed = true;
    return true;
}
double minimumAngle(const Geometry& g, const std::vector<EditableCorner>& triangle) {
    double result = 4;
    for (size_t i = 0; i < 3; ++i) {
        const V a = g.point(triangle[(i + 1) % 3]) - g.point(triangle[i]);
        const V b = g.point(triangle[(i + 2) % 3]) - g.point(triangle[i]);
        result = std::min(result, std::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b)));
    }
    return result;
}
bool improveTriangles(EditableMesh& m, const Geometry& g, const std::set<Edge>& protectedEdges,
                      size_t& work, bool& changed) {
    // Disjoint pairs per pass avoid stale adjacency. Strictly improving flips,
    // sorted edge iteration, and four passes give bounded deterministic work.
    for (int pass = 0; pass < 4; ++pass) {
        if (!spend(work, m.faces.size())) return false;
        const auto uses = edgeUses(m);
        std::set<size_t> touched;
        std::vector<std::vector<EditableCorner>> added;
        for (const auto& [e, fs] : uses) {
            if (!spend(work, 1)) return false;
            if (fs.size() != 2 || protectedEdges.count(e) || touched.count(fs[0]) || touched.count(fs[1])) continue;
            const auto& f = m.faces[fs[0]];
            const auto& h = m.faces[fs[1]];
            if (!g.compatible(f, h) || !g.compatible(h, f)) continue;
            size_t k = 0;
            while (edge(f.loop[k].vertex, f.loop[(k + 1) % 3].vertex) != e) ++k;
            const auto a = f.loop[k], b = f.loop[(k + 1) % 3], c = f.loop[(k + 2) % 3];
            EditableCorner d;
            for (const auto& q : h.loop)
                if (q.vertex != e.first && q.vertex != e.second) d = q;
            if (uses.count(edge(c.vertex, d.vertex))) continue;
            std::vector<EditableCorner> x{c, d, b}, y{d, c, a};
            const V n = g.normal(f);
            auto area = [&](const auto& t) {
                return glm::dot(glm::cross(g.point(t[1]) - g.point(t[0]), g.point(t[2]) - g.point(t[0])), n);
            };
            auto longest = [&](const auto& t) {
                return std::max({glm::length(g.point(t[1]) - g.point(t[0])),
                                 glm::length(g.point(t[2]) - g.point(t[1])),
                                 glm::length(g.point(t[0]) - g.point(t[2]))});
            };
            if (area(x) <= g.eps * longest(x) || area(y) <= g.eps * longest(y)) continue;
            const double before = std::min(minimumAngle(g, f.loop), minimumAngle(g, h.loop));
            const double after = std::min(minimumAngle(g, x), minimumAngle(g, y));
            if (after <= before + 1e-8) continue;
            touched.insert(fs[0]); touched.insert(fs[1]);
            added.push_back(std::move(x)); added.push_back(std::move(y));
        }
        if (added.empty()) break;
        if (added.size() > UINT32_MAX - m.nextFaceId) return false;
        EditableMesh candidate = m;
        candidate.faces.clear();
        for (size_t i = 0; i < m.faces.size(); ++i)
            if (!touched.count(i)) candidate.faces.push_back(m.faces[i]);
        for (const auto& t : added) candidate.addFace(t);
        if (!spend(work, candidate.faces.size() + candidate.vertices.size())) return false;
        if (!rebuild(candidate)) break;
        m = std::move(candidate);
        changed = true;
    }
    return true;
}
}

bool cleanupPlanarMesh(const EditableMesh& input, EditableMesh& output,
                       const PlanarCleanupOptions& options, std::string* error) {
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!std::isfinite(options.relativeTolerance) || options.relativeTolerance < 1e-12 ||
        options.relativeTolerance > 1e-3 || !std::isfinite(options.attributeTolerance) ||
        options.attributeTolerance < 0 || options.attributeTolerance > 1e-3 || !options.maxWork)
        return fail("Planar cleanup options are invalid");
    size_t work = options.maxWork;
    if (!spend(work, input.faces.size() + input.vertices.size()) ||
        !spend(work, options.protectedEdges.size()) || !spend(work, input.seams.size()))
        return fail("Planar cleanup work limit exceeded");
    if (!input.validate(error)) return false;
    for (const auto& f : input.faces)
        if (f.loop.size() != 3) return fail("Planar cleanup requires triangles");
    V lo(std::numeric_limits<double>::max()), hi(-std::numeric_limits<double>::max());
    for (const auto& v : input.vertices) {
        const V p(v.pos[0], v.pos[1], v.pos[2]);
        lo = glm::min(lo, p); hi = glm::max(hi, p);
    }
    const V center = input.vertices.empty() ? V(0) : (lo + hi) * 0.5;
    const double scale = input.vertices.empty() ? 1 : std::max(glm::length(hi - lo), 1e-30);
    Geometry g{{}, options.relativeTolerance, options.attributeTolerance};
    for (const auto& v : input.vertices)
        g.p.emplace(v.id, (V(v.pos[0], v.pos[1], v.pos[2]) - center) / scale);
    std::set<Edge> protectedEdges;
    for (const auto& e : input.seams) protectedEdges.insert(edge(e.first, e.second));
    const auto initialUses = edgeUses(input);
    for (const auto& e : options.protectedEdges) {
        const auto key = edge(e.first, e.second);
        if (!initialUses.count(key)) return fail("Planar cleanup protected edge does not exist");
        protectedEdges.insert(key);
    }
    std::set<uint32_t> protectedVertices;
    for (const auto& e : protectedEdges) {
        protectedVertices.insert(e.first);
        protectedVertices.insert(e.second);
    }
    EditableMesh m = input;
    bool changed = false, removed = true;
    while (removed) {
        removed = false;
        if (!spend(work, m.faces.size())) return fail("Planar cleanup work limit exceeded");
        auto uses = edgeUses(m);
        auto vertexFaces = vertexUses(m);
        // Stable input ID order. After a successful removal refresh adjacency;
        // a subsequent round revisits vertices whose fan was previously blocked.
        std::vector<uint32_t> ids;
        for (const auto& v : m.vertices) ids.push_back(v.id);
        for (uint32_t id : ids) {
            bool local = false;
            if (!removeVertex(m, id, g, uses, vertexFaces, protectedEdges, protectedVertices, work, local))
                return fail(work ? "Planar cleanup id limit exceeded" : "Planar cleanup work limit exceeded");
            if (local) {
                removed = changed = true;
                if (!spend(work, m.faces.size())) return fail("Planar cleanup work limit exceeded");
                uses = edgeUses(m);
                vertexFaces = vertexUses(m);
            }
        }
    }
    if (!improveTriangles(m, g, protectedEdges, work, changed))
        return fail(work ? "Planar cleanup id limit exceeded" : "Planar cleanup work limit exceeded");
    if (!m.validate(error)) return false;
    if (changed) m.revision = nextEditableRevision();
    output = std::move(m);
    if (error) error->clear();
    return true;
}
}
