#include "MeshBoolean.h"
#include "CGLib/Math/Core/Vector3d.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace Phantom::MeshEdit {
namespace {
using V = Math::Vector3dd;
using Key = std::pair<uint32_t, uint32_t>;
Key edge(uint32_t a, uint32_t b) { return std::minmax(a, b); }
double length(const V& v) { return glm::length(v); }
struct Corner {
    V p{}, n{};
    double u = 0, v = 0;
    bool normal = false;
};
Corner mix(const Corner& a, const Corner& b, double t) {
    Corner c;
    c.p = a.p + (b.p - a.p) * t;
    c.u = a.u + (b.u - a.u) * t;
    c.v = a.v + (b.v - a.v) * t;
    c.normal = a.normal && b.normal;
    c.n = a.n + (b.n - a.n) * t;
    return c;
}
struct Polygon {
    std::vector<Corner> c;
    V n{};
    double w = 0;
    void flip() {
        std::reverse(c.begin(), c.end());
        n = -n;
        w = -w;
        for (auto& x : c)
            x.n = -x.n;
    }
};
struct Context {
    double eps;
    size_t limit, generated = 0, work = 0;
    std::string message;
    bool fail(const char* s) {
        if (message.empty())
            message = s;
        return false;
    }
    bool tick(size_t n = 1) {
        // Also bound adversarial pair searches and deeply fragmented BSPs.
        if (n > 50000000 || work > 50000000 - n)
            return fail("Boolean work limit exceeded");
        work += n;
        return true;
    }
    bool count(size_t n) {
        if (n > limit || generated > limit - n)
            return fail("Boolean intermediate polygon limit exceeded");
        generated += n;
        return true;
    }
};
V point(const EditableVertex& v) { return V(v.pos[0], v.pos[1], v.pos[2]); }

struct Box {
    V lo{std::numeric_limits<double>::max()}, hi{-std::numeric_limits<double>::max()};
    void add(const V& p) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    void add(const Box& b) {
        add(b.lo);
        add(b.hi);
    }
    bool overlaps(const Box& b, double e) const {
        for (int k = 0; k < 3; ++k)
            if (lo[k] > b.hi[k] + e || hi[k] < b.lo[k] - e)
                return false;
        return true;
    }
};
// Deterministic AABB tree, reused for surface intersection and edge-point queries.
struct Tree {
    struct Node {
        Box box;
        size_t begin = 0, end = 0;
        int left = -1, right = -1;
    };
    std::vector<Node> nodes;
    std::vector<size_t> order;
    const std::vector<Box>& boxes;
    explicit Tree(const std::vector<Box>& b) : boxes(b) {
        order.resize(b.size());
        std::iota(order.begin(), order.end(), size_t(0));
        if (!b.empty())
            build(0, b.size());
    }
    int build(size_t begin, size_t end) {
        int id = static_cast<int>(nodes.size());
        nodes.push_back({});
        Box box;
        for (size_t i = begin; i < end; ++i)
            box.add(boxes[order[i]]);
        nodes[id].box = box;
        nodes[id].begin = begin;
        nodes[id].end = end;
        if (end - begin > 8) {
            V span = box.hi - box.lo;
            int axis = span.y > span.x ? 1 : 0;
            if (span.z > span[axis])
                axis = 2;
            size_t mid = begin + (end - begin) / 2;
            std::nth_element(order.begin() + begin, order.begin() + mid, order.begin() + end,
                             [&](size_t a, size_t b) {
                                 double x = boxes[a].lo[axis] + boxes[a].hi[axis],
                                        y = boxes[b].lo[axis] + boxes[b].hi[axis];
                                 return x != y ? x < y : a < b;
                             });
            int l = build(begin, mid), r = build(mid, end);
            nodes[id].left = l;
            nodes[id].right = r;
        }
        return id;
    }
    template <class F> bool query(const Box& box, double eps, Context& ctx, F&& f) const {
        if (nodes.empty())
            return true;
        std::vector<int> stack{0};
        while (!stack.empty()) {
            int id = stack.back();
            stack.pop_back();
            const auto& n = nodes[id];
            if (!ctx.tick())
                return false;
            if (!n.box.overlaps(box, eps))
                continue;
            if (n.left >= 0) {
                stack.push_back(n.right);
                stack.push_back(n.left);
            } else
                for (size_t i = n.begin; i < n.end; ++i) {
                    size_t j = order[i];
                    if (boxes[j].overlaps(box, eps) && !f(j))
                        return false;
                }
        }
        return true;
    }
};
struct Triangle {
    std::array<Corner, 3> c;
    std::array<uint32_t, 3> ids;
};
Box bounds(const Triangle& t) {
    Box b;
    for (const auto& c : t.c)
        b.add(c.p);
    return b;
}

bool triangulate(const EditableMesh& m, const V& center, double scale, std::vector<Triangle>& tris,
                 Context& ctx) {
    for (const auto& f : m.faces) {
        std::vector<Corner> c;
        for (const auto& x : f.loop) {
            Corner v;
            v.p = (point(*m.findVertex(x.vertex)) - center) / scale;
            v.u = x.uv[0];
            v.v = x.uv[1];
            v.normal = x.hasNormal;
            v.n = V(x.normal[0], x.normal[1], x.normal[2]);
            c.push_back(v);
        }
        V n(0);
        for (size_t i = 0; i < c.size(); ++i)
            n += glm::cross(c[i].p, c[(i + 1) % c.size()].p);
        double nl = length(n);
        if (nl <= ctx.eps * ctx.eps)
            return ctx.fail("Boolean input has a degenerate face");
        n /= nl;
        for (const auto& v : c)
            if (std::abs(glm::dot(n, v.p - c[0].p)) > ctx.eps)
                return ctx.fail("Boolean requires planar faces");
        // Ear clipping also handles a concave quad. Keep source corner attributes.
        std::vector<size_t> loop(c.size());
        std::iota(loop.begin(), loop.end(), size_t(0));
        while (loop.size() > 3) {
            bool found = false;
            for (size_t i = 0; i < loop.size(); ++i) {
                size_t a = loop[(i + loop.size() - 1) % loop.size()], b = loop[i],
                       d = loop[(i + 1) % loop.size()];
                if (glm::dot(glm::cross(c[b].p - c[a].p, c[d].p - c[b].p), n) <= ctx.eps * ctx.eps)
                    continue;
                bool occupied = false;
                for (size_t q : loop)
                    if (q != a && q != b && q != d) {
                        bool inside = true;
                        for (auto e : {std::pair{a, b}, std::pair{b, d}, std::pair{d, a}})
                            inside = inside &&
                                     glm::dot(glm::cross(c[e.second].p - c[e.first].p, c[q].p - c[e.first].p),
                                              n) >= -ctx.eps * ctx.eps;
                        occupied = occupied || inside;
                    }
                if (occupied)
                    continue;
                tris.push_back({{c[a], c[b], c[d]}, {f.loop[a].vertex, f.loop[b].vertex, f.loop[d].vertex}});
                loop.erase(loop.begin() + i);
                found = true;
                break;
            }
            if (!found)
                return ctx.fail("Boolean cannot triangulate input face");
        }
        tris.push_back({{c[loop[0]], c[loop[1]], c[loop[2]]},
                        {f.loop[loop[0]].vertex, f.loop[loop[1]].vertex, f.loop[loop[2]].vertex}});
    }
    return true;
}

bool onSegment(const V& p, const V& a, const V& b, double eps) {
    V d = b - a;
    double dd = glm::dot(d, d);
    if (dd == 0)
        return length(p - a) <= eps;
    double t = glm::dot(p - a, d) / dd;
    return t >= -eps / std::sqrt(dd) && t <= 1 + eps / std::sqrt(dd) && length(p - (a + d * t)) <= eps;
}
bool inTriangle(const V& p, const Triangle& t, double eps) {
    V n = glm::cross(t.c[1].p - t.c[0].p, t.c[2].p - t.c[0].p);
    double nl = length(n);
    if (nl == 0)
        return false;
    n /= nl;
    for (int i = 0; i < 3; ++i) {
        V a = t.c[i].p, b = t.c[(i + 1) % 3].p;
        if (glm::dot(glm::cross(b - a, p - a), n) < -eps * length(b - a))
            return false;
    }
    return true;
}
// Collect surface intersections, including coplanar crossings. Only intersections
// on the triangles' shared topological vertex/edge are legal within one mesh.
bool pairValid(const Triangle& a, const Triangle& b, Context& ctx) {
    std::vector<V> shared;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (a.ids[i] == b.ids[j])
                shared.push_back(a.c[i].p);
    if (shared.size() == 3)
        return ctx.fail("Boolean input contains duplicate triangles");
    auto allowed = [&](const V& p) {
        if (shared.size() == 2 && onSegment(p, shared[0], shared[1], ctx.eps * 0.1))
            return true;
        for (const auto& s : shared)
            if (length(p - s) <= ctx.eps * 0.1)
                return true;
        return ctx.fail("Boolean surface self-intersects or touches without shared topology");
    };
    auto test = [&](const Triangle& x, const Triangle& y) {
        V n = glm::cross(y.c[1].p - y.c[0].p, y.c[2].p - y.c[0].p);
        double nl = length(n);
        if (nl <= ctx.eps * ctx.eps)
            return ctx.fail("Boolean has a degenerate triangle");
        n /= nl;
        for (int i = 0; i < 3; ++i) {
            V p = x.c[i].p, q = x.c[(i + 1) % 3].p;
            double dp = glm::dot(n, p - y.c[0].p), dq = glm::dot(n, q - y.c[0].p);
            if (std::abs(dp) <= ctx.eps && inTriangle(p, y, ctx.eps * 0.1) && !allowed(p))
                return false;
            if ((dp > ctx.eps && dq < -ctx.eps) || (dp < -ctx.eps && dq > ctx.eps)) {
                V r = p + (q - p) * (dp / (dp - dq));
                if (inTriangle(r, y, ctx.eps * 0.1) && !allowed(r))
                    return false;
            }
            if (std::abs(dp) <= ctx.eps && std::abs(dq) <= ctx.eps) {
                for (int j = 0; j < 3; ++j) {
                    V r = y.c[j].p, s = y.c[(j + 1) % 3].p;
                    V d = q - p, e = s - r;
                    double den = glm::dot(glm::cross(d, e), n);
                    if (std::abs(den) > ctx.eps * length(d) * length(e)) {
                        double t = glm::dot(glm::cross(r - p, e), n) / den,
                               u = glm::dot(glm::cross(r - p, d), n) / den;
                        if (t >= 0 && t <= 1 && u >= 0 && u <= 1 && !allowed(p + d * t))
                            return false;
                    }
                }
            }
        }
        return true;
    };
    return test(a, b) && test(b, a);
}

bool solidValid(const EditableMesh& m, const std::vector<Triangle>& tris, Context& ctx) {
    if (m.faces.empty())
        return m.vertices.empty() || ctx.fail("Boolean empty input contains isolated vertices");
    std::map<Key, std::vector<size_t>> uses;
    std::map<uint32_t, std::vector<size_t>> incident;
    for (size_t i = 0; i < m.faces.size(); ++i) {
        const auto& f = m.faces[i];
        for (size_t k = 0; k < f.loop.size(); ++k) {
            uint32_t a = f.loop[k].vertex, b = f.loop[(k + 1) % f.loop.size()].vertex;
            uses[edge(a, b)].push_back(i);
            incident[a].push_back(i);
        }
    }
    std::vector<std::vector<size_t>> neighbors(m.faces.size());
    std::map<uint32_t, std::vector<std::pair<size_t, size_t>>> links;
    for (const auto& [e, fs] : uses) {
        if (fs.size() != 2)
            return ctx.fail("Boolean requires a closed two-manifold mesh");
        neighbors[fs[0]].push_back(fs[1]);
        neighbors[fs[1]].push_back(fs[0]);
        links[e.first].push_back({fs[0], fs[1]});
        links[e.second].push_back({fs[0], fs[1]});
    }
    for (const auto& v : m.vertices) {
        const auto& fs = incident[v.id];
        if (fs.empty())
            return ctx.fail("Boolean input contains isolated vertices");
        std::map<size_t, std::vector<size_t>> fan;
        for (auto [a, b] : links[v.id]) {
            fan[a].push_back(b);
            fan[b].push_back(a);
        }
        std::set<size_t> seen;
        std::vector<size_t> todo{fs[0]};
        while (!todo.empty()) {
            size_t f = todo.back();
            todo.pop_back();
            if (!seen.insert(f).second)
                continue;
            if (fan[f].size() != 2)
                return ctx.fail("Boolean vertex is not manifold");
            for (size_t g : fan[f])
                todo.push_back(g);
        }
        if (seen.size() != fs.size())
            return ctx.fail("Boolean vertex has disconnected surface fans");
    }
    std::vector<Box> boxes;
    for (const auto& t : tris)
        boxes.push_back(bounds(t));
    Tree tree(boxes);
    for (size_t i = 0; i < tris.size(); ++i)
        if (!tree.query(boxes[i], ctx.eps, ctx,
                        [&](size_t j) { return j <= i || (ctx.tick() && pairValid(tris[i], tris[j], ctx)); }))
            return false;
    // Components are shells. A shell's sign must alternate with nesting depth.
    std::vector<int> component(m.faces.size(), -1);
    int count = 0;
    std::map<uint32_t, int> vertexComponent;
    for (size_t i = 0; i < m.faces.size(); ++i)
        if (component[i] < 0) {
            std::vector<size_t> todo{i};
            component[i] = count;
            while (!todo.empty()) {
                size_t f = todo.back();
                todo.pop_back();
                for (const auto& c : m.faces[f].loop)
                    vertexComponent[c.vertex] = count;
                for (size_t g : neighbors[f])
                    if (component[g] < 0) {
                        component[g] = count;
                        todo.push_back(g);
                    }
            }
            ++count;
        }
    std::vector<double> volumes(count, 0);
    std::vector<V> samples(count);
    std::vector<bool> sampled(count, false);
    for (const auto& t : tris) {
        int c = vertexComponent[t.ids[0]];
        volumes[c] += glm::dot(t.c[0].p, glm::cross(t.c[1].p, t.c[2].p)) / 6;
        if (!sampled[c]) {
            samples[c] = t.c[0].p;
            sampled[c] = true;
        }
    }
    for (int c = 0; c < count; ++c) {
        int depth = 0;
        std::vector<double> angle(count, 0);
        for (const auto& t : tris) {
            int d = vertexComponent[t.ids[0]];
            if (d == c)
                continue;
            if (!ctx.tick())
                return false;
            V a = t.c[0].p - samples[c], b = t.c[1].p - samples[c], v = t.c[2].p - samples[c];
            double la = length(a), lb = length(b), lc = length(v);
            angle[d] +=
                2 * std::atan2(glm::dot(a, glm::cross(b, v)), la * lb * lc + glm::dot(a, b) * lc +
                                                                  glm::dot(b, v) * la + glm::dot(v, a) * lb);
        }
        for (int d = 0; d < count; ++d)
            if (std::abs(angle[d]) > 6.283185307179586)
                ++depth;
        if (std::abs(volumes[c]) <= ctx.eps * ctx.eps * ctx.eps || ((volumes[c] > 0) != (depth % 2 == 0)))
            return ctx.fail("Boolean shell orientation is invalid (outer shell outward, cavity inward)");
    }
    return true;
}

// Plane BSP. Child indexes avoid recursive ownership/destruction. Tree traversal
// uses explicit stacks; a bounded build depth rejects pathological fragmentation.
struct Bsp {
    struct Node {
        V n{};
        double w = 0;
        bool plane = false;
        std::vector<Polygon> p;
        int front = -1, back = -1;
    };
    std::vector<Node> nodes{1};
    Context& ctx;
    explicit Bsp(Context& c) : ctx(c) {}
    bool split(const Polygon& p, const V& n, double w, std::vector<Polygon>& cf, std::vector<Polygon>& cb,
               std::vector<Polygon>& front, std::vector<Polygon>& back) {
        if (!ctx.tick(p.c.size()))
            return false;
        int type = 0;
        std::vector<int> types;
        for (const auto& c : p.c) {
            double d = glm::dot(n, c.p) - w;
            int t = d > ctx.eps ? 1 : d < -ctx.eps ? 2 : 0;
            type |= t;
            types.push_back(t);
        }
        if (type == 0) {
            (glm::dot(n, p.n) >= 0 ? cf : cb).push_back(p);
            return true;
        }
        if (type == 1) {
            front.push_back(p);
            return true;
        }
        if (type == 2) {
            back.push_back(p);
            return true;
        }
        Polygon f, b;
        f.n = b.n = p.n;
        f.w = b.w = p.w;
        for (size_t i = 0; i < p.c.size(); ++i) {
            size_t j = (i + 1) % p.c.size();
            const auto& a = p.c[i];
            const auto& v = p.c[j];
            if (types[i] != 2)
                f.c.push_back(a);
            if (types[i] != 1)
                b.c.push_back(a);
            if ((types[i] | types[j]) == 3) {
                double den = glm::dot(n, v.p - a.p);
                if (den == 0)
                    return ctx.fail("Boolean unstable plane intersection");
                Corner q = mix(a, v, std::clamp((w - glm::dot(n, a.p)) / den, 0.0, 1.0));
                f.c.push_back(q);
                b.c.push_back(q);
            }
        }
        if (!ctx.count(2))
            return false;
        if (f.c.size() >= 3)
            front.push_back(std::move(f));
        if (b.c.size() >= 3)
            back.push_back(std::move(b));
        return true;
    }
    bool build(std::vector<Polygon> polygons) {
        struct Job {
            int id, depth;
            std::vector<Polygon> p;
        };
        std::vector<Job> jobs;
        jobs.push_back({0, 0, std::move(polygons)});
        while (!jobs.empty()) {
            Job job = std::move(jobs.back());
            jobs.pop_back();
            if (job.p.empty())
                continue;
            if (job.depth > 512)
                return ctx.fail("Boolean BSP depth limit exceeded");
            int id = job.id;
            if (!nodes[id].plane) {
                // Score a deterministic sample of planes to avoid input-order chains.
                size_t best = 0;
                double score = std::numeric_limits<double>::max();
                size_t candidates = std::min<size_t>(16, job.p.size());
                for (size_t k = 0; k < candidates; ++k) {
                    size_t s = k * job.p.size() / candidates;
                    size_t f = 0, b = 0, cuts = 0;
                    size_t samples = std::min<size_t>(64, job.p.size());
                    for (size_t q = 0; q < samples; ++q) {
                        int mask = 0;
                        const auto& p = job.p[q * job.p.size() / samples];
                        for (const auto& v : p.c) {
                            double d = glm::dot(job.p[s].n, v.p) - job.p[s].w;
                            mask |= d > ctx.eps ? 1 : d < -ctx.eps ? 2 : 0;
                        }
                        f += mask == 1;
                        b += mask == 2;
                        cuts += mask == 3;
                    }
                    if (!ctx.tick(samples))
                        return false;
                    double value = 8.0 * cuts + std::abs(double(f) - double(b));
                    if (value < score) {
                        score = value;
                        best = s;
                    }
                }
                nodes[id].n = job.p[best].n;
                nodes[id].w = job.p[best].w;
                nodes[id].plane = true;
            }
            std::vector<Polygon> f, b;
            for (const auto& p : job.p)
                if (!split(p, nodes[id].n, nodes[id].w, nodes[id].p, nodes[id].p, f, b))
                    return false;
            if (!f.empty()) {
                int child = nodes[id].front;
                if (child < 0) {
                    child = static_cast<int>(nodes.size());
                    nodes[id].front = child;
                    nodes.push_back({});
                }
                jobs.push_back({child, job.depth + 1, std::move(f)});
            }
            if (!b.empty()) {
                int child = nodes[id].back;
                if (child < 0) {
                    child = static_cast<int>(nodes.size());
                    nodes[id].back = child;
                    nodes.push_back({});
                }
                jobs.push_back({child, job.depth + 1, std::move(b)});
            }
        }
        return true;
    }
    bool clip(std::vector<Polygon> input, std::vector<Polygon>& out) {
        struct Job {
            int id;
            std::vector<Polygon> p;
        };
        std::vector<Job> jobs;
        jobs.push_back({0, std::move(input)});
        while (!jobs.empty()) {
            Job job = std::move(jobs.back());
            jobs.pop_back();
            const auto& n = nodes[job.id];
            if (!n.plane) {
                out.insert(out.end(), std::make_move_iterator(job.p.begin()),
                           std::make_move_iterator(job.p.end()));
                continue;
            }
            std::vector<Polygon> f, b;
            for (const auto& p : job.p)
                if (!split(p, n.n, n.w, f, b, f, b))
                    return false;
            if (n.front >= 0) {
                if (!f.empty())
                    jobs.push_back({n.front, std::move(f)});
            } else
                out.insert(out.end(), std::make_move_iterator(f.begin()), std::make_move_iterator(f.end()));
            if (n.back >= 0 && !b.empty())
                jobs.push_back({n.back, std::move(b)});
        }
        return true;
    }
    bool clipTo(Bsp& b) {
        for (auto& n : nodes) {
            std::vector<Polygon> p;
            if (!b.clip(std::move(n.p), p))
                return false;
            n.p = std::move(p);
        }
        return true;
    }
    void invert() {
        for (auto& n : nodes) {
            for (auto& p : n.p)
                p.flip();
            n.n = -n.n;
            n.w = -n.w;
            std::swap(n.front, n.back);
        }
    }
    std::vector<Polygon> all() {
        std::vector<Polygon> p;
        for (auto& n : nodes)
            p.insert(p.end(), n.p.begin(), n.p.end());
        return p;
    }
};
std::vector<Polygon> polygons(const std::vector<Triangle>& ts) {
    std::vector<Polygon> p;
    for (const auto& t : ts) {
        Polygon q;
        q.c.assign(t.c.begin(), t.c.end());
        q.n = glm::normalize(glm::cross(t.c[1].p - t.c[0].p, t.c[2].p - t.c[0].p));
        q.w = glm::dot(q.n, q.c[0].p);
        p.push_back(std::move(q));
    }
    return p;
}

bool reconstruct(const std::vector<Polygon>& polygons, const EditableMesh& a, const V& center, double scale,
                 const BooleanOptions& options, EditableMesh& out, Context& ctx) {
    EditableMesh m;
    m.nextVertexId = a.nextVertexId;
    m.nextFaceId = a.nextFaceId;
    m.nextEdgeId = a.nextEdgeId;
    std::vector<V> positions;
    using Cell = std::array<int64_t, 3>;
    std::map<Cell, std::vector<size_t>> grid;
    auto vertex = [&](const V& p, size_t& index) {
        Cell cell;
        for (int k = 0; k < 3; ++k)
            cell[k] = static_cast<int64_t>(std::floor(p[k] / ctx.eps));
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z) {
                    auto it = grid.find({cell[0] + x, cell[1] + y, cell[2] + z});
                    if (it == grid.end())
                        continue;
                    for (size_t j : it->second) {
                        if (!ctx.tick())
                            return false;
                        double d = length(positions[j] - p);
                        if (d <= ctx.eps * 0.01) {
                            index = j;
                            return true;
                        }
                        if (d <= ctx.eps)
                            return ctx.fail("Boolean distinct vertices are closer than the precision limit");
                    }
                }
        if (m.nextVertexId == UINT32_MAX)
            return ctx.fail("Boolean vertex id overflow");
        V v = p * scale + center;
        index = positions.size();
        positions.push_back(p);
        grid[cell].push_back(index);
        m.addVertex(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
        return true;
    };
    std::vector<std::vector<size_t>> ids;
    ids.reserve(polygons.size());
    for (const auto& p : polygons) {
        std::vector<size_t> loop;
        for (const auto& c : p.c) {
            size_t i = 0;
            if (!vertex(c.p, i))
                return false;
            if (loop.empty() || loop.back() != i)
                loop.push_back(i);
        }
        if (loop.size() > 1 && loop.front() == loop.back())
            loop.pop_back();
        if (loop.size() < 3)
            return ctx.fail("Boolean polygon collapsed during welding");
        ids.push_back(std::move(loop));
    }
    std::vector<Box> boxes;
    for (const auto& p : positions) {
        Box b;
        b.add(p);
        boxes.push_back(b);
    }
    Tree tree(boxes);
    for (size_t pi = 0; pi < polygons.size(); ++pi) {
        const auto& p = polygons[pi];
        const auto& loop = ids[pi];
        // Welding does not normally remove a corner. Reject rather than lose UV provenance.
        if (loop.size() != p.c.size())
            return ctx.fail("Boolean polygon has indistinguishable corners");
        std::vector<std::pair<size_t, Corner>> boundary;
        for (size_t k = 0; k < loop.size(); ++k) {
            size_t i = loop[k], j = loop[(k + 1) % loop.size()];
            V d = positions[j] - positions[i];
            double dd = glm::dot(d, d);
            if (dd <= ctx.eps * ctx.eps)
                return ctx.fail("Boolean edge is below the precision limit");
            Box b;
            b.add(positions[i]);
            b.add(positions[j]);
            std::vector<std::pair<double, size_t>> splits{{0, i}};
            if (!tree.query(b, ctx.eps * 0.01, ctx, [&](size_t q) {
                    if (q == i || q == j)
                        return true;
                    if (!ctx.tick())
                        return false;
                    double t = glm::dot(positions[q] - positions[i], d) / dd;
                    if (t > 0 && t < 1 && length(positions[q] - (positions[i] + d * t)) <= ctx.eps * 0.01)
                        splits.push_back({t, q});
                    return true;
                }))
                return false;
            std::sort(splits.begin(), splits.end());
            for (auto [t, q] : splits)
                boundary.push_back({q, mix(p.c[k], p.c[(k + 1) % loop.size()], t)});
        }
        auto corner = [&](size_t i, const Corner& c) {
            EditableCorner r;
            r.vertex = m.vertices[i].id;
            r.uv[0] = static_cast<float>(c.u);
            r.uv[1] = static_cast<float>(c.v);
            double nl = length(c.n);
            r.hasNormal = c.normal && nl > ctx.eps;
            if (r.hasNormal)
                for (int k = 0; k < 3; ++k)
                    r.normal[k] = static_cast<float>(c.n[k] / nl);
            return r;
        };
        auto face = [&](size_t i, const Corner& ci, size_t j, const Corner& cj, size_t k, const Corner& ck) {
            if (m.faces.size() >= options.maxOutputFaces || m.nextFaceId == UINT32_MAX)
                return ctx.fail("Boolean output face/id limit exceeded");
            if (length(glm::cross(positions[j] - positions[i], positions[k] - positions[i])) <=
                ctx.eps * ctx.eps)
                return ctx.fail("Boolean output has a subprecision triangle");
            m.addFace({corner(i, ci), corner(j, cj), corner(k, ck)});
            return true;
        };
        if (boundary.size() == 3) {
            if (!face(boundary[0].first, boundary[0].second, boundary[1].first, boundary[1].second,
                      boundary[2].first, boundary[2].second))
                return false;
        } else {
            // Convex BSP fragments: a centre fan preserves every collinear boundary
            // vertex, unlike a first-corner fan which can reintroduce T-junctions.
            Corner c;
            c.normal = true;
            for (const auto& v : p.c) {
                c.p += v.p;
                c.n += v.n;
                c.u += v.u;
                c.v += v.v;
                c.normal = c.normal && v.normal;
            }
            double n = double(p.c.size());
            c.p /= n;
            c.n /= n;
            c.u /= n;
            c.v /= n;
            size_t mid = 0;
            if (!vertex(c.p, mid))
                return false;
            for (size_t k = 0; k < boundary.size(); ++k) {
                const auto& x = boundary[k];
                const auto& y = boundary[(k + 1) % boundary.size()];
                if (!face(mid, c, x.first, x.second, y.first, y.second))
                    return false;
            }
        }
    }
    // Remove vertices not used by any surviving fragment, without recycling ids.
    std::set<uint32_t> used;
    for (const auto& f : m.faces)
        for (const auto& c : f.loop)
            used.insert(c.vertex);
    m.vertices.erase(std::remove_if(m.vertices.begin(), m.vertices.end(),
                                    [&](const auto& v) { return !used.count(v.id); }),
                     m.vertices.end());
    if (m.faces.size() > (UINT32_MAX - m.nextEdgeId) / 3)
        return ctx.fail("Boolean edge id limit exceeded");
    m.rebuildEdges();
    std::string error;
    if (!m.validate(&error)) {
        ctx.message = "Boolean output validation: " + error;
        return false;
    }
    std::vector<Triangle> ts;
    if (!triangulate(m, center, scale, ts, ctx) || !solidValid(m, ts, ctx))
        return false;
    m.revision = nextEditableRevision();
    out = std::move(m);
    return true;
}
} // namespace

bool booleanMeshes(const EditableMesh& a, const EditableMesh& b, BooleanOperation operation,
                   EditableMesh& out, const BooleanOptions& options, std::string* error) {
    Context ctx{options.relativeTolerance, options.maxIntermediatePolygons};
    auto finish = [&](bool ok) {
        if (!ok)
            std::fprintf(stderr, "[MeshBoolean] %s\n", ctx.message.c_str());
        if (error)
            *error = ok ? std::string() : ctx.message;
        return ok;
    };
    if (!std::isfinite(ctx.eps) || ctx.eps < 1e-12 || ctx.eps > 1e-3 || !options.maxInputTriangles ||
        !ctx.limit || !options.maxOutputFaces)
        return finish(
            ctx.fail("Boolean options are invalid (tolerance must be 1e-12..1e-3, limits positive)"));
    if (operation != BooleanOperation::Union && operation != BooleanOperation::Difference &&
        operation != BooleanOperation::Intersection)
        return finish(ctx.fail("Unknown Boolean operation"));
    size_t input = 0;
    for (const auto* m : {&a, &b}) {
        std::string reason;
        if (!m->validate(&reason)) {
            ctx.message = "Boolean input validation: " + reason;
            return finish(false);
        }
        for (const auto& f : m->faces) {
            size_t n = f.loop.size() - 2;
            if (n > options.maxInputTriangles - input)
                return finish(ctx.fail("Boolean input triangle limit exceeded"));
            input += n;
        }
    }
    Box box;
    for (const auto* m : {&a, &b})
        for (const auto& v : m->vertices)
            box.add(point(v));
    V center(0);
    double scale = 1;
    if (!a.vertices.empty() || !b.vertices.empty()) {
        center = (box.lo + box.hi) * 0.5;
        scale = length(box.hi - box.lo);
        if (!(scale > 0))
            return finish(ctx.fail("Boolean input has no extent"));
    }
    std::vector<Triangle> ta, tb;
    if (!triangulate(a, center, scale, ta, ctx) || !triangulate(b, center, scale, tb, ctx) ||
        !solidValid(a, ta, ctx) || !solidValid(b, tb, ctx))
        return finish(false);
    if (!ctx.count(ta.size() + tb.size()))
        return finish(false);
    std::vector<Polygon> result;
    if (ta.empty() || tb.empty()) {
        if (operation == BooleanOperation::Union)
            result = polygons(ta.empty() ? tb : ta);
        else if (operation == BooleanOperation::Difference)
            result = polygons(ta);
    } else {
        Bsp x(ctx), y(ctx);
        if (!x.build(polygons(ta)) || !y.build(polygons(tb)))
            return finish(false);
        if (operation == BooleanOperation::Union) {
            if (!x.clipTo(y) || !y.clipTo(x))
                return finish(false);
            y.invert();
            if (!y.clipTo(x))
                return finish(false);
            y.invert();
            if (!x.build(y.all()))
                return finish(false);
        } else if (operation == BooleanOperation::Difference) {
            x.invert();
            if (!x.clipTo(y) || !y.clipTo(x))
                return finish(false);
            y.invert();
            if (!y.clipTo(x))
                return finish(false);
            y.invert();
            if (!x.build(y.all()))
                return finish(false);
            x.invert();
        } else {
            x.invert();
            if (!y.clipTo(x))
                return finish(false);
            y.invert();
            if (!x.clipTo(y) || !y.clipTo(x) || !x.build(y.all()))
                return finish(false);
            x.invert();
        }
        result = x.all();
    }
    return finish(reconstruct(result, a, center, scale, options, out, ctx));
}
} // namespace Phantom::MeshEdit
