#include "MeshReduction.h"
#include "PlanarTriangulation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <iterator>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <tuple>

namespace Phantom::MeshEdit {
namespace {
using V = Math::Vector3dd;
using Edge = std::pair<size_t, size_t>;
Edge edge(size_t a, size_t b) { return std::minmax(a, b); }
constexpr double epsilon = 1e-9;
struct Budget {
    size_t left, used = 0;
    bool tick(size_t n = 1) {
        if (n > left) { used += left; left = 0; return false; }
        left -= n; used += n; return true;
    }
};
struct Quadric {
    double q[4][4]{};
    void add(const Quadric& other) {
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) q[i][j] += other.q[i][j];
    }
    double bilinear(const V& a, double aw, const V& b, double bw) const {
        const double x[4]{a.x, a.y, a.z, aw}, y[4]{b.x, b.y, b.z, bw};
        double result = 0;
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) result += x[i] * q[i][j] * y[j];
        return result;
    }
};
struct Vertex {
    V p{};
    Quadric q;
    std::set<size_t> faces;
    uint32_t id = 0;
    size_t version = 0;
    bool locked = false, alive = true;
};
struct Face {
    std::array<size_t, 3> v{};
    std::array<EditableCorner, 3> c{};
    bool alive = true;
};
using Triangle = std::array<V, 3>;
Triangle points(const Face& f, const std::vector<Vertex>& vertices) {
    return {vertices[f.v[0]].p, vertices[f.v[1]].p, vertices[f.v[2]].p};
}
V cross(const Triangle& t) { return glm::cross(t[1] - t[0], t[2] - t[0]); }
struct Box {
    V lo{std::numeric_limits<double>::max()}, hi{-std::numeric_limits<double>::max()};
    void add(const V& p) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    void add(const Box& b) { if (b.lo.x <= b.hi.x) { add(b.lo); add(b.hi); } }
    bool overlaps(const Box& b) const {
        for (int k = 0; k < 3; ++k) if (lo[k] > b.hi[k] + epsilon || hi[k] < b.lo[k] - epsilon) return false;
        return lo.x <= hi.x && b.lo.x <= b.hi.x;
    }
};
Box bounds(const Triangle& t) { Box b; for (const auto& p : t) b.add(p); return b; }
bool onSegment(const V& p, const V& a, const V& b) {
    const V d = b - a;
    const double dd = glm::dot(d, d);
    if (dd == 0) return glm::length(p - a) <= epsilon;
    const double t = glm::dot(p - a, d) / dd;
    return t >= 0 && t <= 1 && glm::length(p - (a + t * d)) <= epsilon;
}
bool inside(const V& p, const Triangle& t, const V& n) {
    for (size_t i = 0; i < 3; ++i) {
        const V d = t[(i + 1) % 3] - t[i];
        if (glm::dot(glm::cross(d, p - t[i]), n) < -epsilon * glm::length(d)) return false;
    }
    return true;
}
// Only the shared topological vertex/edge is an allowed surface intersection.
bool intersects(const Face& a, const Triangle& x, const Face& b, const Triangle& y) {
    std::vector<V> shared;
    for (size_t i = 0; i < 3; ++i) for (size_t j = 0; j < 3; ++j)
        if (a.v[i] == b.v[j]) shared.push_back(x[i]);
    if (shared.size() == 3) return true;
    auto allowed = [&](const V& p) {
        if (shared.size() == 2 && onSegment(p, shared[0], shared[1])) return true;
        for (const auto& s : shared) if (glm::length(p - s) <= epsilon) return true;
        return false;
    };
    auto test = [&](const Triangle& s, const Triangle& t) {
        const V n = glm::normalize(cross(t));
        for (size_t i = 0; i < 3; ++i) {
            const V p = s[i], q = s[(i + 1) % 3];
            const double dp = glm::dot(n, p - t[0]), dq = glm::dot(n, q - t[0]);
            if (std::abs(dp) <= epsilon && inside(p, t, n) && !allowed(p)) return true;
            if ((dp > epsilon && dq < -epsilon) || (dp < -epsilon && dq > epsilon)) {
                const V hit = p + (q - p) * (dp / (dp - dq));
                if (inside(hit, t, n) && !allowed(hit)) return true;
            }
            if (std::abs(dp) <= epsilon && std::abs(dq) <= epsilon)
                for (size_t j = 0; j < 3; ++j) {
                    const V r = t[j], d = q - p, e = t[(j + 1) % 3] - r;
                    const double denominator = glm::dot(glm::cross(d, e), n);
                    if (std::abs(denominator) <= epsilon * glm::length(d) * glm::length(e)) continue;
                    const double u = glm::dot(glm::cross(r - p, e), n) / denominator;
                    const double v = glm::dot(glm::cross(r - p, d), n) / denominator;
                    if (u >= 0 && u <= 1 && v >= 0 && v <= 1 && !allowed(p + u * d)) return true;
                }
        }
        return false;
    };
    return test(x, y) || test(y, x);
}
// Fixed face slots, refitted leaves: no all-mesh rebuild after an edge collapse.
struct Tree {
    struct Node { Box box; int parent = -1, left = -1, right = -1; size_t face = 0; };
    std::vector<Node> nodes;
    std::vector<int> leaf;
    std::vector<size_t> order;
    const std::vector<Face>& faces;
    const std::vector<Vertex>& vertices;
    Budget& budget;
    Tree(const std::vector<Face>& f, const std::vector<Vertex>& v, Budget& b) : faces(f), vertices(v), budget(b) {
        leaf.resize(f.size(), -1); order.resize(f.size());
        std::iota(order.begin(), order.end(), size_t(0));
        if (!f.empty()) build(0, f.size(), -1);
    }
    int build(size_t begin, size_t end, int parent) {
        if (!budget.tick(end - begin)) return -1;
        const int id = static_cast<int>(nodes.size());
        nodes.push_back({}); nodes[id].parent = parent;
        for (size_t i = begin; i < end; ++i) nodes[id].box.add(bounds(points(faces[order[i]], vertices)));
        if (end - begin == 1) { nodes[id].face = order[begin]; leaf[order[begin]] = id; return id; }
        const V span = nodes[id].box.hi - nodes[id].box.lo;
        int axis = span.y > span.x ? 1 : 0; if (span.z > span[axis]) axis = 2;
        const size_t mid = begin + (end - begin) / 2;
        std::nth_element(order.begin() + begin, order.begin() + mid, order.begin() + end, [&](size_t a, size_t b) {
            const auto pa = points(faces[a], vertices), pb = points(faces[b], vertices);
            const double x = pa[0][axis] + pa[1][axis] + pa[2][axis], y = pb[0][axis] + pb[1][axis] + pb[2][axis];
            return x != y ? x < y : a < b;
        });
        const int left = build(begin, mid, id), right = build(mid, end, id);
        nodes[id].left = left; nodes[id].right = right;
        return left < 0 || right < 0 ? -1 : id;
    }
    bool refit(size_t face) {
        int id = leaf[face];
        if (id < 0) return false;
        nodes[id].box = faces[face].alive ? bounds(points(faces[face], vertices)) : Box{};
        while ((id = nodes[id].parent) >= 0) {
            if (!budget.tick()) return false;
            nodes[id].box = {}; nodes[id].box.add(nodes[nodes[id].left].box); nodes[id].box.add(nodes[nodes[id].right].box);
        }
        return true;
    }
    template<class F> bool query(const Box& box, F callback) {
        if (nodes.empty()) return true;
        std::vector<int> stack{0};
        while (!stack.empty()) {
            if (!budget.tick()) return false;
            const int id = stack.back(); stack.pop_back();
            const auto& n = nodes[id];
            if (!n.box.overlaps(box)) continue;
            if (n.left < 0) { if (faces[n.face].alive && !callback(n.face)) return false; }
            else { stack.push_back(n.right); stack.push_back(n.left); }
        }
        return true;
    }
};
struct Candidate {
    double cost = 0, t = 0;
    V p{};
    size_t a = 0, b = 0, va = 0, vb = 0;
    bool operator<(const Candidate& other) const {
        return std::tie(cost, a, b) > std::tie(other.cost, other.a, other.b);
    }
};
bool sameAttributes(const EditableCorner& a, const EditableCorner& b) {
    auto close = [](float x, float y) { return std::abs(double(x) - y) <= 1e-6 * (1 + std::max(std::abs(double(x)), std::abs(double(y)))); };
    if (a.hasNormal != b.hasNormal) return false;
    for (int k = 0; k < 2; ++k) if (!close(a.uv[k], b.uv[k])) return false;
    if (a.hasNormal) for (int k = 0; k < 3; ++k) if (!close(a.normal[k], b.normal[k])) return false;
    return true;
}
const EditableCorner& corner(const Face& f, size_t vertex) {
    for (size_t k = 0; k < 3; ++k) if (f.v[k] == vertex) return f.c[k];
    return f.c[0]; // callers only request incident vertices
}
std::set<size_t> neighbors(size_t id, const std::vector<Vertex>& v, const std::vector<Face>& f) {
    std::set<size_t> result;
    for (size_t i : v[id].faces) for (size_t q : f[i].v) if (q != id) result.insert(q);
    return result;
}
}

bool reduceMesh(const EditableMesh& input, const MeshReductionOptions& options,
                EditableMesh& output, MeshReductionReport& report, std::string* error) {
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!std::isfinite(options.ratio) || options.ratio <= 0 || options.ratio > 1 ||
        (options.targetTriangles && options.ratio != .5) || !options.maxInputTriangles || !options.maxWork)
        return fail("Invalid reduction options (choose ratio in (0,1] or a positive triangle target)");
    Budget budget{options.maxWork};
    if (!budget.tick(input.vertices.size() + input.faces.size())) return fail("Reduction work limit exceeded");
    if (!input.validate(error)) return false;
    MeshReductionReport result;
    result.inputVertices = input.vertices.size();
    for (const auto& f : input.faces) {
        if (f.loop.size() - 2 > options.maxInputTriangles - result.inputTriangles) return fail("Reduction input triangle limit exceeded");
        result.inputTriangles += f.loop.size() - 2;
    }
    result.targetTriangles = options.targetTriangles ? options.targetTriangles :
        std::max(size_t(1), static_cast<size_t>(std::floor(double(result.inputTriangles) * options.ratio)));
    V lo(std::numeric_limits<double>::max()), hi(-std::numeric_limits<double>::max());
    for (const auto& v : input.vertices) { const V p(v.pos[0], v.pos[1], v.pos[2]); lo = glm::min(lo, p); hi = glm::max(hi, p); }
    const V center = input.vertices.empty() ? V(0) : (lo + hi) * .5;
    const double scale = input.vertices.empty() ? 1 : glm::length(hi - lo);
    if (!(scale > 0) && !input.vertices.empty()) return fail("Reduction input has no extent");
    std::vector<Vertex> vertices;
    std::map<uint32_t, size_t> indices;
    for (const auto& v : input.vertices) {
        indices.emplace(v.id, vertices.size());
        Vertex work; work.id = v.id; work.p = (V(v.pos[0], v.pos[1], v.pos[2]) - center) / scale;
        vertices.push_back(std::move(work));
    }
    std::vector<Face> faces;
    for (const auto& f : input.faces) {
        std::vector<V> p;
        for (const auto& c : f.loop) p.push_back(vertices[indices.find(c.vertex)->second].p);
        V n(0); for (size_t k = 0; k < p.size(); ++k) n += glm::cross(p[k], p[(k + 1) % p.size()]);
        const double length = glm::length(n);
        if (length <= epsilon * epsilon) return fail("Reduction input has a degenerate face");
        n /= length;
        for (const auto& q : p) if (std::abs(glm::dot(n, q - p[0])) > 1e-7) return fail("Reduction requires planar quads");
        std::vector<std::array<size_t, 3>> triangles;
        if (!detail::triangulatePlanarBoundary(p, n, epsilon, budget.left, triangles)) return fail("Reduction triangulation failed or work limit exceeded");
        // Account for the shared triangulator's direct budget consumption.
        budget.used = options.maxWork - budget.left;
        for (const auto& t : triangles) {
            Face face;
            for (size_t k = 0; k < 3; ++k) { face.c[k] = f.loop[t[k]]; face.v[k] = indices.find(face.c[k].vertex)->second; }
            const auto index = faces.size(); faces.push_back(face);
            for (size_t v : face.v) vertices[v].faces.insert(index);
            const double plane[4]{n.x, n.y, n.z, -glm::dot(n, p[0])};
            Quadric q;
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) q.q[i][j] = plane[i] * plane[j];
            for (size_t v : face.v) vertices[v].q.add(q);
        }
    }
    std::map<Edge, std::vector<size_t>> edges;
    for (size_t i = 0; i < faces.size(); ++i) for (size_t k = 0; k < 3; ++k)
        edges[edge(faces[i].v[k], faces[i].v[(k + 1) % 3])].push_back(i);
    const std::set<std::pair<uint32_t, uint32_t>> seams(input.seams.begin(), input.seams.end());
    for (const auto& [e, uses] : edges) {
        if (!budget.tick()) return fail("Reduction work limit exceeded");
        bool protect = uses.size() != 2 || seams.count(std::minmax(vertices[e.first].id, vertices[e.second].id));
        if (uses.size() == 2) {
            const auto& a = faces[uses[0]]; const auto& b = faces[uses[1]];
            protect = protect || !sameAttributes(corner(a, e.first), corner(b, e.first)) ||
                !sameAttributes(corner(a, e.second), corner(b, e.second)) ||
                corner(a, e.first).hasNormal != corner(a, e.second).hasNormal ||
                glm::dot(glm::normalize(cross(points(a, vertices))), glm::normalize(cross(points(b, vertices)))) <= std::sqrt(.5);
        }
        if (protect) vertices[e.first].locked = vertices[e.second].locked = true;
    }
    // Edge-manifold validation alone does not reject disconnected vertex fans.
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& fs = vertices[i].faces;
        if (fs.empty()) return fail("Reduction input contains isolated vertices");
        std::set<size_t> seen; std::vector<size_t> pending{*fs.begin()};
        while (!pending.empty()) {
            if (!budget.tick()) return fail("Reduction work limit exceeded");
            const size_t f = pending.back(); pending.pop_back();
            if (!seen.insert(f).second) continue;
            for (size_t v : faces[f].v) if (v != i)
                for (size_t next : edges.find(edge(i, v))->second) if (!seen.count(next)) pending.push_back(next);
        }
        if (seen.size() != fs.size()) return fail("Reduction input has disconnected vertex fans");
    }
    Tree tree(faces, vertices, budget);
    if (!budget.left) return fail("Reduction work limit exceeded");
    for (size_t i = 0; i < faces.size(); ++i) {
        bool intersection = false;
        const auto p = points(faces[i], vertices);
        const bool ok = tree.query(bounds(p), [&](size_t j) {
            if (j <= i) return true;
            if (!budget.tick()) return false;
            intersection = intersects(faces[i], p, faces[j], points(faces[j], vertices));
            return !intersection;
        });
        if (!ok) return fail(intersection ? "Reduction input self-intersects or touches without shared topology" : "Reduction work limit exceeded");
    }
    std::priority_queue<Candidate> queue;
    auto enqueue = [&](size_t a, size_t b) {
        if (a > b) std::swap(a, b);
        if (vertices[a].locked || vertices[b].locked || !vertices[a].alive || !vertices[b].alive) return;
        Quadric q = vertices[a].q; q.add(vertices[b].q);
        const V start = vertices[a].p, d = vertices[b].p - start;
        const double denominator = q.bilinear(d, 0, d, 0);
        double t = denominator > 1e-20 ? std::clamp(-q.bilinear(d, 0, start, 1) / denominator, 0., 1.) : .5;
        // Always compare the endpoints; a nearly singular quadric is common on planes.
        double cost = q.bilinear(start + t * d, 1, start + t * d, 1);
        for (double endpoint : {0., 1.}) {
            const double c = q.bilinear(start + endpoint * d, 1, start + endpoint * d, 1);
            if (c < cost - 1e-15) { cost = c; t = endpoint; }
        }
        V p = (start + t * d) * scale + center;
        for (int k = 0; k < 3; ++k) p[k] = static_cast<float>(p[k]);
        p = (p - center) / scale; // validate the actual float positions that will be committed
        cost = std::max(0., q.bilinear(p, 1, p, 1));
        if (std::isfinite(cost)) queue.push({cost, t, p, a, b, vertices[a].version, vertices[b].version});
    };
    for (const auto& [e, uses] : edges) enqueue(e.first, e.second);
    size_t count = faces.size();
    while (count > result.targetTriangles && !queue.empty()) {
        if (!budget.tick()) return fail("Reduction work limit exceeded");
        const auto c = queue.top(); queue.pop();
        const size_t a = c.a, b = c.b;
        if (!vertices[a].alive || !vertices[b].alive || vertices[a].version != c.va || vertices[b].version != c.vb) continue;
        const auto na = neighbors(a, vertices, faces), nb = neighbors(b, vertices, faces);
        std::set<size_t> common;
        std::set_intersection(na.begin(), na.end(), nb.begin(), nb.end(), std::inserter(common, common.end()));
        std::vector<size_t> shared;
        std::set_intersection(vertices[a].faces.begin(), vertices[a].faces.end(), vertices[b].faces.begin(), vertices[b].faces.end(), std::back_inserter(shared));
        if (shared.size() != 2) continue;
        std::set<size_t> opposite;
        for (size_t f : shared) for (size_t v : faces[f].v) if (v != a && v != b) opposite.insert(v);
        if (common != opposite) continue; // link condition preserves genus and manifold connectivity
        std::set<size_t> affected = vertices[a].faces;
        affected.insert(vertices[b].faces.begin(), vertices[b].faces.end());
        if (!budget.tick(affected.size())) return fail("Reduction work limit exceeded");
        const auto ca = corner(faces[shared[0]], a), cb = corner(faces[shared[0]], b);
        EditableCorner blended = ca;
        for (int k = 0; k < 2; ++k) blended.uv[k] = static_cast<float>((1 - c.t) * ca.uv[k] + c.t * cb.uv[k]);
        blended.hasNormal = ca.hasNormal && cb.hasNormal;
        if (blended.hasNormal) {
            V n;
            for (int k = 0; k < 3; ++k) n[k] = (1 - c.t) * ca.normal[k] + c.t * cb.normal[k];
            if (glm::length(n) <= epsilon) continue;
            n = glm::normalize(n); for (int k = 0; k < 3; ++k) blended.normal[k] = static_cast<float>(n[k]);
        }
        std::vector<std::pair<size_t, Face>> changed;
        std::vector<Triangle> changedPoints;
        std::set<std::array<size_t, 3>> unique;
        bool valid = true;
        for (size_t i : affected) {
            if (i == shared[0] || i == shared[1]) continue;
            Face f = faces[i];
            auto p = points(f, vertices);
            const V before = cross(p);
            for (size_t k = 0; k < 3; ++k) if (f.v[k] == a || f.v[k] == b) { f.v[k] = a; f.c[k] = blended; p[k] = c.p; }
            const V after = cross(p);
            const double area = glm::length(after);
            const double longest = std::max({glm::length(p[1] - p[0]), glm::length(p[2] - p[1]), glm::length(p[0] - p[2])});
            auto key = f.v; std::sort(key.begin(), key.end());
            if (area <= epsilon * longest || glm::dot(before, after) <= .5 * glm::length(before) * area || !unique.insert(key).second) { valid = false; break; }
            changed.push_back({i, f}); changedPoints.push_back(p);
        }
        if (!valid) continue;
        for (size_t i = 0; i < changed.size() && valid; ++i) {
            const bool ok = tree.query(bounds(changedPoints[i]), [&](size_t j) {
                if (affected.count(j)) return true;
                if (!budget.tick()) return false;
                if (intersects(changed[i].second, changedPoints[i], faces[j], points(faces[j], vertices))) { valid = false; return false; }
                return true;
            });
            if (!ok && !budget.left) return fail("Reduction work limit exceeded");
            for (size_t j = 0; j < i && valid; ++j) {
                if (!budget.tick()) return fail("Reduction work limit exceeded");
                if (intersects(changed[i].second, changedPoints[i], changed[j].second, changedPoints[j])) valid = false;
            }
        }
        if (!valid) continue;
        std::set<size_t> nearby = na; nearby.insert(nb.begin(), nb.end()); nearby.insert(a);
        for (size_t i : affected) for (size_t v : faces[i].v) vertices[v].faces.erase(i);
        faces[shared[0]].alive = faces[shared[1]].alive = false;
        for (const auto& [i, f] : changed) { faces[i] = f; for (size_t v : f.v) vertices[v].faces.insert(i); }
        vertices[a].p = c.p; vertices[a].q.add(vertices[b].q); vertices[b].alive = false;
        for (size_t i : affected) if (!tree.refit(i)) return fail("Reduction work limit exceeded");
        for (size_t v : nearby) ++vertices[v].version;
        for (size_t v : nearby) if (vertices[v].alive)
            for (size_t n : neighbors(v, vertices, faces)) {
                if (!budget.tick()) return fail("Reduction work limit exceeded");
                enqueue(v, n);
            }
        count -= 2; ++result.collapses;
    }
    result.outputTriangles = count; result.changed = result.collapses != 0;
    result.targetReached = count <= result.targetTriangles;
    result.stop = result.targetReached ? ReductionStop::TargetReached : ReductionStop::ProtectedTopology;
    EditableMesh made = input;
    if (result.changed) {
        if (count > UINT32_MAX - made.nextFaceId || count > (UINT32_MAX - made.nextEdgeId) / 3)
            return fail("Reduction face/edge id limit exceeded");
        made.vertices.clear(); made.faces.clear();
        for (const auto& v : vertices) if (v.alive) {
            EditableVertex p; p.id = v.id;
            const V position = v.p * scale + center;
            for (int k = 0; k < 3; ++k) p.pos[k] = static_cast<float>(position[k]);
            made.vertices.push_back(p);
        }
        for (const auto& f : faces) if (f.alive) {
            std::vector<EditableCorner> corners(f.c.begin(), f.c.end());
            for (size_t k = 0; k < 3; ++k) corners[k].vertex = vertices[f.v[k]].id;
            made.addFace(corners);
        }
        made.rebuildEdges();
        if (!made.validate(error)) return false;
        made.revision = nextEditableRevision();
    }
    result.outputVertices = made.vertices.size(); result.work = budget.used;
    output = std::move(made); report = result;
    if (error) error->clear();
    return true;
}
}
