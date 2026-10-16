#include "MeshEditPicking.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace Phantom::MeshEdit {

namespace {

struct WorldTriangle { glm::vec3 a, b, c; uint32_t face; };

glm::vec3 toWorld(const MeshPickView& v, const float p[3]) {
	return glm::vec3(v.model * glm::vec4(p[0], p[1], p[2], 1.f));
}

glm::vec3 cameraPosition(const MeshPickView& v) { return glm::vec3(glm::inverse(v.view)[3]); }

// The mesh's triangles in world space (quads split 0-1-2 / 0-2-3 like the render triangulation). Used for occlusion and face picking.
std::vector<WorldTriangle> worldTriangles(const EditableMesh& mesh, const MeshPickView& view) {
	std::vector<WorldTriangle> tris;
	for (const auto& f : mesh.faces) {
		const size_t n = f.loop.size();
		glm::vec3 p[4];
		for (size_t i = 0; i < n && i < 4; ++i) {
			const EditableVertex* v = mesh.findVertex(f.loop[i].vertex);
			p[i] = v ? toWorld(view, v->pos) : glm::vec3(0.f);
		}
		for (size_t k = 1; k + 1 < n; ++k) tris.push_back({ p[0], p[k], p[k + 1], f.id });
	}
	return tris;
}

// Moeller-Trumbore, two-sided. Returns the distance along the (normalized) direction, or a negative number for a miss.
float rayTriangle(const glm::vec3& o, const glm::vec3& d, const WorldTriangle& t) {
	const glm::vec3 e1 = t.b - t.a, e2 = t.c - t.a;
	const glm::vec3 pv = glm::cross(d, e2);
	const float det = glm::dot(e1, pv);
	if (std::fabs(det) < 1e-12f) return -1.f;
	const float inv = 1.f / det;
	const glm::vec3 tv = o - t.a;
	const float u = glm::dot(tv, pv) * inv;
	if (u < 0.f || u > 1.f) return -1.f;
	const glm::vec3 qv = glm::cross(tv, e1);
	const float v = glm::dot(d, qv) * inv;
	if (v < 0.f || u + v > 1.f) return -1.f;
	return glm::dot(e2, qv) * inv;
}

// Bounding-volume hierarchy over the world triangles (median split on the longest centroid axis). Occlusion tests for
// every vertex of a big mesh would otherwise be O(vertices x triangles).
struct TriBvh {
	struct Node { glm::vec3 lo, hi; int left = -1, right = -1, first = 0, count = 0; };
	const std::vector<WorldTriangle>* tris = nullptr;
	std::vector<int> order;
	std::vector<Node> nodes;

	void build(const std::vector<WorldTriangle>& t) {
		tris = &t;
		order.resize(t.size());
		for (size_t i = 0; i < t.size(); ++i) order[i] = static_cast<int>(i);
		nodes.clear();
		if (!t.empty()) { nodes.reserve(t.size() / 2 + 1); buildNode(0, static_cast<int>(t.size())); }
	}
	int buildNode(int first, int count) {
		const int index = static_cast<int>(nodes.size());
		nodes.emplace_back();
		glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
		glm::vec3 clo = lo, chi = hi;
		for (int i = first; i < first + count; ++i) {
			const WorldTriangle& t = (*tris)[order[i]];
			const glm::vec3 mn = glm::min(t.a, glm::min(t.b, t.c)), mx = glm::max(t.a, glm::max(t.b, t.c));
			lo = glm::min(lo, mn); hi = glm::max(hi, mx);
			const glm::vec3 c = (t.a + t.b + t.c) / 3.f;
			clo = glm::min(clo, c); chi = glm::max(chi, c);
		}
		nodes[index].lo = lo; nodes[index].hi = hi;
		if (count <= 4) { nodes[index].first = first; nodes[index].count = count; return index; }
		const glm::vec3 ext = chi - clo;
		const int axis = ext.x >= ext.y && ext.x >= ext.z ? 0 : (ext.y >= ext.z ? 1 : 2);
		const int mid = first + count / 2;
		std::nth_element(order.begin() + first, order.begin() + mid, order.begin() + first + count, [&](int l, int r) {
			const WorldTriangle& a = (*tris)[l]; const WorldTriangle& b = (*tris)[r];
			return (a.a[axis] + a.b[axis] + a.c[axis]) < (b.a[axis] + b.b[axis] + b.c[axis]); });
		const int l = buildNode(first, mid - first);
		const int r = buildNode(mid, first + count - mid);
		nodes[index].left = l; nodes[index].right = r;
		return index;
	}
	static bool boxHit(const Node& n, const glm::vec3& o, const glm::vec3& invDir, float tmax) {
		float t0 = 0.f, t1 = tmax;
		for (int k = 0; k < 3; ++k) {
			float a = (n.lo[k] - o[k]) * invDir[k], b = (n.hi[k] - o[k]) * invDir[k];
			if (a > b) std::swap(a, b);
			// Slightly inflated so rays grazing a flat (zero-thickness) box are not lost to rounding.
			t0 = std::max(t0, a - 1e-5f); t1 = std::min(t1, b + 1e-5f);
			if (t0 > t1) return false;
		}
		return true;
	}
	// Nearest hit with t in (tmin, tmax); returns the face id or false.
	bool closest(const glm::vec3& o, const glm::vec3& d, float tmin, float tmax, float& bestT, uint32_t& face, bool anyHit) const {
		if (nodes.empty()) return false;
		const glm::vec3 inv(1.f / (std::fabs(d.x) < 1e-20f ? 1e-20f : d.x), 1.f / (std::fabs(d.y) < 1e-20f ? 1e-20f : d.y), 1.f / (std::fabs(d.z) < 1e-20f ? 1e-20f : d.z));
		bestT = tmax;
		bool found = false;
		int stack[96]; int sp = 0; stack[sp++] = 0;
		while (sp > 0) {
			const Node& n = nodes[stack[--sp]];
			if (!boxHit(n, o, inv, bestT)) continue;
			if (n.left < 0) {
				for (int i = n.first; i < n.first + n.count; ++i) {
					const WorldTriangle& t = (*tris)[order[i]];
					const float h = rayTriangle(o, d, t);
					if (h > tmin && h < bestT) { bestT = h; face = t.face; found = true; if (anyHit) return true; }
				}
			} else if (sp < 94) { stack[sp++] = n.left; stack[sp++] = n.right; }
		}
		return found;
	}
};

// True if some triangle lies between the camera and `p`. Surfaces the point itself sits on do not count.
bool occluded(const TriBvh& bvh, const glm::vec3& camera, const glm::vec3& p) {
	const glm::vec3 delta = p - camera;
	const float dist = glm::length(delta);
	if (dist < 1e-9f) return false;
	const glm::vec3 dir = delta / dist;
	const float limit = dist - (dist * 1e-4f + 1e-5f);
	float t; uint32_t face;
	return bvh.closest(camera, dir, 1e-6f, limit, t, face, true);
}

float segmentDistance(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
	const glm::vec2 ab = b - a;
	const float len2 = glm::dot(ab, ab);
	if (len2 < 1e-9f) return glm::length(p - a);
	const float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f);
	return glm::length(p - (a + t * ab));
}

bool inside(const glm::vec2& p, float l, float t, float r, float b) { return p.x >= l && p.x <= r && p.y >= t && p.y <= b; }

// Liang-Barsky: does the segment touch the rectangle?
bool segmentTouchesRect(glm::vec2 a, glm::vec2 b, float l, float t, float r, float bt) {
	float t0 = 0.f, t1 = 1.f;
	const glm::vec2 d = b - a;
	const float p[4] = { -d.x, d.x, -d.y, d.y };
	const float q[4] = { a.x - l, r - a.x, a.y - t, bt - a.y };
	for (int i = 0; i < 4; ++i) {
		if (std::fabs(p[i]) < 1e-9f) { if (q[i] < 0.f) return false; continue; }
		const float u = q[i] / p[i];
		if (p[i] < 0.f) t0 = std::max(t0, u); else t1 = std::min(t1, u);
		if (t0 > t1) return false;
	}
	return true;
}

} // namespace

glm::mat4 objectModelMatrix(const std::array<float, 3>& t, const std::array<float, 3>& r, const std::array<float, 3>& s) {
	glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(t[0], t[1], t[2]));
	m = glm::rotate(m, glm::radians(r[1]), glm::vec3(0.f, 1.f, 0.f));
	m = glm::rotate(m, glm::radians(r[0]), glm::vec3(1.f, 0.f, 0.f));
	m = glm::rotate(m, glm::radians(r[2]), glm::vec3(0.f, 0.f, 1.f));
	return glm::scale(m, glm::vec3(s[0], s[1], s[2]));
}

bool projectToPixels(const MeshPickView& v, const float localPos[3], glm::vec2& pixel) {
	const glm::vec4 clip = v.proj * v.view * v.model * glm::vec4(localPos[0], localPos[1], localPos[2], 1.f);
	if (!(clip.w > 1e-6f)) return false;
	pixel.x = (clip.x / clip.w * 0.5f + 0.5f) * v.width;
	pixel.y = (clip.y / clip.w * 0.5f + 0.5f) * v.height;
	return true;
}

bool pickMeshElement(const EditableMesh& mesh, const MeshPickView& view, double x, double y,
                     const MeshPickOptions& options, uint32_t& id) {
	const glm::vec2 mouse(static_cast<float>(x), static_cast<float>(y));
	const glm::vec3 camera = cameraPosition(view);
	std::vector<WorldTriangle> tris;
	TriBvh bvh;
	if (!options.xray || options.kind == ElementKind::Face) { tris = worldTriangles(mesh, view); bvh.build(tris); }

	if (options.kind == ElementKind::Face) {
		// Front-most triangle under the cursor (a screen ray through the pixel).
		const glm::mat4 invVP = glm::inverse(view.proj * view.view);
		const float ndcX = 2.f * mouse.x / view.width - 1.f, ndcY = 2.f * mouse.y / view.height - 1.f;
		glm::vec4 n = invVP * glm::vec4(ndcX, ndcY, 0.f, 1.f), f = invVP * glm::vec4(ndcX, ndcY, 1.f, 1.f);
		const glm::vec3 nearPt = glm::vec3(n) / n.w, farPt = glm::vec3(f) / f.w;
		const glm::vec3 dir = glm::normalize(farPt - nearPt);
		float best;
		return bvh.closest(nearPt, dir, 0.f, std::numeric_limits<float>::max(), best, id, false);
	}

	struct Candidate { float dist; uint32_t id; glm::vec3 world; };
	std::vector<Candidate> candidates;
	if (options.kind == ElementKind::Vertex) {
		for (const auto& v : mesh.vertices) {
			glm::vec2 px;
			if (!projectToPixels(view, v.pos, px)) continue;
			const float d = glm::length(px - mouse);
			if (d <= options.vertexRadiusPx) candidates.push_back({ d, v.id, toWorld(view, v.pos) });
		}
	} else {
		for (const auto& e : mesh.edges) {
			const EditableVertex* a = mesh.findVertex(e.a);
			const EditableVertex* b = mesh.findVertex(e.b);
			glm::vec2 pa, pb;
			if (!a || !b || !projectToPixels(view, a->pos, pa) || !projectToPixels(view, b->pos, pb)) continue;
			const float d = segmentDistance(mouse, pa, pb);
			if (d <= options.edgeRadiusPx) candidates.push_back({ d, e.id, 0.5f * (toWorld(view, a->pos) + toWorld(view, b->pos)) });
		}
	}
	std::sort(candidates.begin(), candidates.end(), [](const Candidate& l, const Candidate& r) { return l.dist < r.dist; });
	for (const auto& c : candidates) {
		if (!options.xray && occluded(bvh, camera, c.world)) continue;
		id = c.id;
		return true;
	}
	return false;
}

std::vector<uint32_t> boxSelectMeshElements(const EditableMesh& mesh, const MeshPickView& view,
                                            double x0, double y0, double x1, double y1, const MeshPickOptions& options) {
	const bool crossing = x1 < x0;
	const float l = static_cast<float>(std::min(x0, x1)), r = static_cast<float>(std::max(x0, x1));
	const float t = static_cast<float>(std::min(y0, y1)), b = static_cast<float>(std::max(y0, y1));
	const glm::vec3 camera = cameraPosition(view);
	std::vector<WorldTriangle> tris;
	TriBvh bvh;
	if (!options.xray) { tris = worldTriangles(mesh, view); bvh.build(tris); }

	struct Projected { bool ok = false; glm::vec2 px; bool visible = true; };
	std::vector<Projected> proj(mesh.nextVertexId);
	for (const auto& v : mesh.vertices) {
		Projected p;
		p.ok = projectToPixels(view, v.pos, p.px);
		if (p.ok && !options.xray) p.visible = !occluded(bvh, camera, toWorld(view, v.pos));
		proj[v.id] = p;
	}

	std::vector<uint32_t> out;
	auto usable = [&](uint32_t vertex) { const auto& p = proj[vertex]; return p.ok && p.visible; };
	if (options.kind == ElementKind::Vertex) {
		for (const auto& v : mesh.vertices)
			if (usable(v.id) && inside(proj[v.id].px, l, t, r, b)) out.push_back(v.id);
	} else if (options.kind == ElementKind::Edge) {
		for (const auto& e : mesh.edges) {
			if (!usable(e.a) || !usable(e.b)) continue;
			const glm::vec2 pa = proj[e.a].px, pb = proj[e.b].px;
			const bool hit = crossing ? segmentTouchesRect(pa, pb, l, t, r, b)
			                          : inside(pa, l, t, r, b) && inside(pb, l, t, r, b);
			if (hit) out.push_back(e.id);
		}
	} else {
		for (const auto& f : mesh.faces) {
			bool all = true, any = false, ok = true;
			for (const auto& c : f.loop) {
				if (!usable(c.vertex)) { ok = false; break; }
				const bool in = inside(proj[c.vertex].px, l, t, r, b);
				all = all && in;
				any = any || in;
			}
			if (!ok) continue;
			if (!crossing) { if (all) out.push_back(f.id); continue; }
			bool hit = any;
			for (size_t i = 0; !hit && i < f.loop.size(); ++i)
				hit = segmentTouchesRect(proj[f.loop[i].vertex].px, proj[f.loop[(i + 1) % f.loop.size()].vertex].px, l, t, r, b);
			if (hit) out.push_back(f.id);
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

} // namespace Phantom::MeshEdit
