#include "MeshEditOperations.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

namespace Phantom::MeshEdit {

namespace {

void fail(std::string* error, const std::string& message) {
	if (error) *error = message;
}

bool finite3(const std::array<float, 3>& v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

using Mat3 = std::array<std::array<double, 3>, 3>;

Mat3 mul(const Mat3& a, const Mat3& b) {
	Mat3 r{};
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			for (int k = 0; k < 3; ++k) r[i][j] += a[i][k] * b[k][j];
	return r;
}

// R = Ry * Rx * Rz, the same order as the object transform (glm::eulerAngleYXZ).
Mat3 rotationYXZ(const std::array<float, 3>& deg) {
	const double kDeg = 3.14159265358979323846 / 180.0;
	const double x = deg[0] * kDeg, y = deg[1] * kDeg, z = deg[2] * kDeg;
	const double cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z), sz = std::sin(z);
	const Mat3 rx = {{ { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } }};
	const Mat3 ry = {{ { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } }};
	const Mat3 rz = {{ { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } }};
	return mul(mul(ry, rx), rz);
}

// Applies `edit(position)` to every listed vertex of a copy, then validates and stamps a new revision.
template <typename Edit>
bool editVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, EditableMesh& out, std::string* error,
                  Edit edit) {
	if (vertices.empty()) { fail(error, "no vertices selected"); return false; }
	EditableMesh work = in;
	for (uint32_t id : vertices) {
		auto it = std::lower_bound(work.vertices.begin(), work.vertices.end(), id,
		                           [](const EditableVertex& v, uint32_t value) { return v.id < value; });
		if (it == work.vertices.end() || it->id != id) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
		edit(it->pos);
		if (!std::isfinite(it->pos[0]) || !std::isfinite(it->pos[1]) || !std::isfinite(it->pos[2])) {
			fail(error, "the result has a non-finite coordinate");
			return false;
		}
	}
	// Only positions changed (`work` is a copy of a validated mesh), so topology need not be re-checked.
	if (!work.validateGeometry(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	return true;
}

} // namespace

const char* elementKindName(ElementKind kind) {
	switch (kind) {
	case ElementKind::Vertex: return "vertex";
	case ElementKind::Edge:   return "edge";
	case ElementKind::Face:   return "face";
	}
	return "vertex";
}

bool elementKindFromName(const std::string& name, ElementKind& out) {
	std::string l = name;
	for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (l == "vertex" || l == "vertices") { out = ElementKind::Vertex; return true; }
	if (l == "edge" || l == "edges")      { out = ElementKind::Edge;   return true; }
	if (l == "face" || l == "faces")      { out = ElementKind::Face;   return true; }
	return false;
}

bool hasElement(const EditableMesh& mesh, ElementKind kind, uint32_t id) {
	switch (kind) {
	case ElementKind::Vertex: return mesh.findVertex(id) != nullptr;
	case ElementKind::Face:   return mesh.findFace(id) != nullptr;
	case ElementKind::Edge:
		for (const auto& e : mesh.edges) if (e.id == id) return true;
		return false;
	}
	return false;
}

std::vector<uint32_t> allElementIds(const EditableMesh& mesh, ElementKind kind) {
	std::vector<uint32_t> ids;
	switch (kind) {
	case ElementKind::Vertex: for (const auto& v : mesh.vertices) ids.push_back(v.id); break;
	case ElementKind::Face:   for (const auto& f : mesh.faces) ids.push_back(f.id); break;
	case ElementKind::Edge:   for (const auto& e : mesh.edges) ids.push_back(e.id); break;
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

bool elementsToVertices(const EditableMesh& mesh, ElementKind kind, const std::vector<uint32_t>& ids,
                        std::vector<uint32_t>& vertices, std::string* error) {
	std::set<uint32_t> set;
	for (uint32_t id : ids) {
		switch (kind) {
		case ElementKind::Vertex:
			if (!mesh.findVertex(id)) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
			set.insert(id);
			break;
		case ElementKind::Face: {
			const EditableFace* f = mesh.findFace(id);
			if (!f) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }
			for (const auto& c : f->loop) set.insert(c.vertex);
			break;
		}
		case ElementKind::Edge: {
			bool found = false;
			for (const auto& e : mesh.edges)
				if (e.id == id) { set.insert(e.a); set.insert(e.b); found = true; break; }
			if (!found) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
			break;
		}
		}
	}
	vertices.assign(set.begin(), set.end());
	return true;
}

bool vertexCentroid(const EditableMesh& mesh, const std::vector<uint32_t>& vertices, std::array<float, 3>& out) {
	if (vertices.empty()) return false;
	double sum[3] = { 0, 0, 0 };
	for (uint32_t id : vertices) {
		const EditableVertex* v = mesh.findVertex(id);
		if (!v) return false;
		for (int k = 0; k < 3; ++k) sum[k] += v->pos[k];
	}
	for (int k = 0; k < 3; ++k) out[k] = static_cast<float>(sum[k] / vertices.size());
	return true;
}

bool translateVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& delta,
                       EditableMesh& out, std::string* error) {
	if (!finite3(delta)) { fail(error, "the offset is not finite"); return false; }
	return editVertices(in, vertices, out, error, [&](float* p) { for (int k = 0; k < 3; ++k) p[k] += delta[k]; });
}

bool falloffFromName(const std::string& name, Falloff& out) {
	std::string l = name;
	for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (l == "smooth")   { out = Falloff::Smooth;   return true; }
	if (l == "linear")   { out = Falloff::Linear;   return true; }
	if (l == "sharp")    { out = Falloff::Sharp;    return true; }
	if (l == "constant") { out = Falloff::Constant; return true; }
	return false;
}

bool translateVerticesProportional(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& delta,
                                   float radius, Falloff falloff, EditableMesh& out, std::string* error, size_t* affected) {
	if (vertices.empty()) { fail(error, "no vertices selected"); return false; }
	if (!std::isfinite(radius) || radius <= 0.f) { fail(error, "radius must be a positive number"); return false; }
	if (!finite3(delta)) { fail(error, "the offset has a non-finite component"); return false; }
	const std::set<uint32_t> selected(vertices.begin(), vertices.end());
	std::vector<const EditableVertex*> sources;
	for (uint32_t id : selected) {
		const EditableVertex* v = in.findVertex(id);
		if (!v) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
		sources.push_back(v);
	}
	EditableMesh work = in;
	size_t moved = 0;
	for (auto& v : work.vertices) {
		double w = 1.0;
		if (!selected.count(v.id)) {
			double best = 1e300;
			for (const EditableVertex* s : sources) {
				const double dx = double(v.pos[0]) - s->pos[0], dy = double(v.pos[1]) - s->pos[1], dz = double(v.pos[2]) - s->pos[2];
				best = std::min(best, dx * dx + dy * dy + dz * dz);
			}
			const double t = std::sqrt(best) / radius;
			if (t >= 1.0) continue;
			switch (falloff) {
			case Falloff::Smooth:   w = 1.0 - (3.0 * t * t - 2.0 * t * t * t); break;
			case Falloff::Linear:   w = 1.0 - t; break;
			case Falloff::Sharp:    w = (1.0 - t) * (1.0 - t); break;
			case Falloff::Constant: w = 1.0; break;
			}
			if (w <= 0.0) continue;
			++moved;
		}
		for (int k = 0; k < 3; ++k) v.pos[k] += static_cast<float>(delta[k] * w);
	}
	if (!work.validateGeometry(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (affected) *affected = moved;
	return true;
}

bool rotateVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& eulerDeg,
                    const std::array<float, 3>& pivot, EditableMesh& out, std::string* error) {
	if (!finite3(eulerDeg) || !finite3(pivot)) { fail(error, "the rotation is not finite"); return false; }
	const Mat3 r = rotationYXZ(eulerDeg);
	return editVertices(in, vertices, out, error, [&](float* p) {
		const double d[3] = { p[0] - double(pivot[0]), p[1] - double(pivot[1]), p[2] - double(pivot[2]) };
		for (int i = 0; i < 3; ++i)
			p[i] = static_cast<float>(pivot[i] + r[i][0] * d[0] + r[i][1] * d[1] + r[i][2] * d[2]);
	});
}

bool scaleVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& factor,
                   const std::array<float, 3>& pivot, EditableMesh& out, std::string* error) {
	if (!finite3(factor) || !finite3(pivot)) { fail(error, "the scale is not finite"); return false; }
	return editVertices(in, vertices, out, error, [&](float* p) {
		for (int k = 0; k < 3; ++k) p[k] = pivot[k] + (p[k] - pivot[k]) * factor[k];
	});
}

namespace {

// Shared by extrude (inset = 0) and inset (inset > 0, distance = depth along the normal, may be 0): the region's boundary
// vertices are duplicated, the region moves onto the copies, and one quad wall/ring is created per boundary edge.
bool extrudeRegion(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float distance, float inset, EditableMesh& out,
                   std::string* error, size_t* sideFaces) {
	if (faceIds.empty()) { fail(error, "no faces selected"); return false; }

	std::set<uint32_t> region(faceIds.begin(), faceIds.end());
	for (uint32_t id : region)
		if (!in.findFace(id)) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }

	// Directed edges of the region; a boundary edge is one whose reverse is not used by another region face.
	std::set<std::pair<uint32_t, uint32_t>> directed;
	for (uint32_t id : region) {
		const auto& loop = in.findFace(id)->loop;
		for (size_t i = 0; i < loop.size(); ++i) directed.insert({ loop[i].vertex, loop[(i + 1) % loop.size()].vertex });
	}
	std::vector<std::pair<uint32_t, uint32_t>> boundary;  // in face order, a -> b as walked by the region face
	std::map<uint32_t, int> boundaryOut;                  // vertex -> number of boundary edges leaving it
	for (uint32_t id : region) {
		const auto& loop = in.findFace(id)->loop;
		for (size_t i = 0; i < loop.size(); ++i) {
			const uint32_t a = loop[i].vertex, b = loop[(i + 1) % loop.size()].vertex;
			if (directed.count({ b, a })) continue;
			boundary.push_back({ a, b });
			++boundaryOut[a];
		}
	}
	if (boundary.empty()) { fail(error, "the selected faces have no boundary (they cover a closed surface), so there is nothing to extrude"); return false; }
	for (const auto& kv : boundaryOut)
		if (kv.second > 1) { fail(error, "the selected region touches itself at vertex " + std::to_string(kv.first) + " (non-manifold), extrude is not supported there"); return false; }

	// Connected through shared edges.
	{
		std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edgeFaces;
		for (uint32_t id : region) {
			const auto& loop = in.findFace(id)->loop;
			for (size_t i = 0; i < loop.size(); ++i) {
				const uint32_t a = loop[i].vertex, b = loop[(i + 1) % loop.size()].vertex;
				edgeFaces[{ std::min(a, b), std::max(a, b) }].push_back(id);
			}
		}
		std::set<uint32_t> seen{ *region.begin() };
		std::vector<uint32_t> stack{ *region.begin() };
		while (!stack.empty()) {
			const uint32_t id = stack.back(); stack.pop_back();
			const auto& loop = in.findFace(id)->loop;
			for (size_t i = 0; i < loop.size(); ++i) {
				const uint32_t a = loop[i].vertex, b = loop[(i + 1) % loop.size()].vertex;
				for (uint32_t other : edgeFaces[{ std::min(a, b), std::max(a, b) }])
					if (seen.insert(other).second) stack.push_back(other);
			}
		}
		if (seen.size() != region.size()) { fail(error, "the selected faces are not connected; extrude one connected region at a time"); return false; }
	}

	// Common normal: every face must point (almost) the same way.
	double sum[3] = { 0, 0, 0 };
	std::vector<std::array<double, 3>> normals;
	for (uint32_t id : region) {
		const auto& loop = in.findFace(id)->loop;
		std::array<double, 3> n{ 0, 0, 0 };
		for (size_t i = 0; i < loop.size(); ++i) {
			const EditableVertex* p = in.findVertex(loop[i].vertex);
			const EditableVertex* q = in.findVertex(loop[(i + 1) % loop.size()].vertex);
			n[0] += (double(p->pos[1]) - q->pos[1]) * (double(p->pos[2]) + q->pos[2]);
			n[1] += (double(p->pos[2]) - q->pos[2]) * (double(p->pos[0]) + q->pos[0]);
			n[2] += (double(p->pos[0]) - q->pos[0]) * (double(p->pos[1]) + q->pos[1]);
		}
		for (int k = 0; k < 3; ++k) sum[k] += n[k];
		normals.push_back(n);
	}
	const double sumLen = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
	if (!(sumLen > 1e-12)) { fail(error, "the selected faces have no common direction to extrude along"); return false; }
	const double dir[3] = { sum[0] / sumLen, sum[1] / sumLen, sum[2] / sumLen };
	for (const auto& n : normals) {
		const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (!(len > 1e-12) || (n[0] * dir[0] + n[1] * dir[1] + n[2] * dir[2]) / len < 0.9999) {
			fail(error, "the selected faces are not coplanar; extrude a flat region (or one face at a time)");
			return false;
		}
	}
	const float offset[3] = { float(dir[0] * distance), float(dir[1] * distance), float(dir[2] * distance) };

	// Inset: every boundary vertex moves in-plane toward the region's inside so each boundary edge shifts by `inset`
	// (mitered corner: along the bisector of the two inward edge normals, scaled by 1/cos(half angle)).
	std::map<uint32_t, std::array<float, 3>> insetOffset;
	if (inset > 0.f) {
		std::map<uint32_t, uint32_t> nextOf, prevOf;
		for (const auto& e : boundary) { nextOf[e.first] = e.second; prevOf[e.second] = e.first; }
		auto inward = [&](uint32_t a, uint32_t b, double r[3]) {
			const EditableVertex* p = in.findVertex(a);
			const EditableVertex* q = in.findVertex(b);
			const double e[3] = { double(q->pos[0]) - p->pos[0], double(q->pos[1]) - p->pos[1], double(q->pos[2]) - p->pos[2] };
			// The region is on the left of a boundary edge walked counter-clockwise: inward = normal x edge.
			r[0] = dir[1] * e[2] - dir[2] * e[1];
			r[1] = dir[2] * e[0] - dir[0] * e[2];
			r[2] = dir[0] * e[1] - dir[1] * e[0];
			const double len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
			if (!(len > 1e-12)) return false;
			for (int k = 0; k < 3; ++k) r[k] /= len;
			return true;
		};
		for (const auto& kv : nextOf) {
			const uint32_t v = kv.first;
			auto prev = prevOf.find(v);
			double i1[3], i2[3];
			if (prev == prevOf.end() || !inward(prev->second, v, i1) || !inward(v, kv.second, i2)) {
				fail(error, "the region boundary at vertex " + std::to_string(v) + " is degenerate, inset is not supported there");
				return false;
			}
			double d[3] = { i1[0] + i2[0], i1[1] + i2[1], i1[2] + i2[2] };
			const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
			if (!(len > 1e-9)) { fail(error, "the region boundary folds back at vertex " + std::to_string(v) + ", inset is not supported there"); return false; }
			for (int k = 0; k < 3; ++k) d[k] /= len;
			const double cosHalf = d[0] * i1[0] + d[1] * i1[1] + d[2] * i1[2];
			if (cosHalf < 0.2) { fail(error, "the corner at vertex " + std::to_string(v) + " is too sharp to inset"); return false; }
			const double scale = double(inset) / cosHalf;
			insetOffset[v] = { float(d[0] * scale), float(d[1] * scale), float(d[2] * scale) };
		}
	}

	EditableMesh work = in;
	// Boundary vertices are copied; interior vertices of the region are simply moved.
	std::set<uint32_t> boundaryVertices;
	for (const auto& e : boundary) { boundaryVertices.insert(e.first); boundaryVertices.insert(e.second); }
	std::set<uint32_t> regionVertices;
	for (uint32_t id : region)
		for (const auto& c : in.findFace(id)->loop) regionVertices.insert(c.vertex);
	std::map<uint32_t, uint32_t> top;  // original vertex -> vertex the region now uses
	for (uint32_t v : regionVertices) {
		const EditableVertex* source = in.findVertex(v);
		float p[3] = { source->pos[0] + offset[0], source->pos[1] + offset[1], source->pos[2] + offset[2] };
		auto in2 = insetOffset.find(v);
		if (in2 != insetOffset.end()) for (int k = 0; k < 3; ++k) p[k] += in2->second[k];
		if (boundaryVertices.count(v)) {
			top[v] = work.addVertex(p[0], p[1], p[2]);
		} else {
			auto it = std::lower_bound(work.vertices.begin(), work.vertices.end(), v,
			                           [](const EditableVertex& e, uint32_t value) { return e.id < value; });
			for (int k = 0; k < 3; ++k) it->pos[k] = p[k];
			top[v] = v;
		}
	}
	for (auto& face : work.faces)
		if (region.count(face.id))
			for (auto& c : face.loop) c.vertex = top[c.vertex];

	if (inset > 0.f) {
		// A thickness beyond the region's inradius turns faces inside out: reject instead of producing a bow-tie.
		// (A square inset past its centre is a 180-degree rotation with the same normal, so also compare edge directions.)
		for (const auto& e : boundary) {
			const EditableVertex* a = in.findVertex(e.first);
			const EditableVertex* b = in.findVertex(e.second);
			const EditableVertex* na = work.findVertex(top[e.first]);
			const EditableVertex* nb = work.findVertex(top[e.second]);
			double dot = 0;
			for (int k = 0; k < 3; ++k) dot += (double(nb->pos[k]) - na->pos[k]) * (double(b->pos[k]) - a->pos[k]);
			if (dot <= 1e-12) { fail(error, "the inset thickness is too large for this region"); return false; }
		}
		for (uint32_t id : region) {
			const auto& loop = work.findFace(id)->loop;
			double n[3] = { 0, 0, 0 };
			for (size_t i = 0; i < loop.size(); ++i) {
				const EditableVertex* p = work.findVertex(loop[i].vertex);
				const EditableVertex* q = work.findVertex(loop[(i + 1) % loop.size()].vertex);
				n[0] += (double(p->pos[1]) - q->pos[1]) * (double(p->pos[2]) + q->pos[2]);
				n[1] += (double(p->pos[2]) - q->pos[2]) * (double(p->pos[0]) + q->pos[0]);
				n[2] += (double(p->pos[0]) - q->pos[0]) * (double(p->pos[1]) + q->pos[1]);
			}
			if (n[0] * dir[0] + n[1] * dir[1] + n[2] * dir[2] <= 1e-12) {
				fail(error, "the inset thickness is too large for face " + std::to_string(id));
				return false;
			}
		}
	}

	const float height = std::sqrt(distance * distance + inset * inset);
	for (const auto& e : boundary) {
		const EditableVertex* a = in.findVertex(e.first);
		const EditableVertex* b = in.findVertex(e.second);
		const float dx = b->pos[0] - a->pos[0], dy = b->pos[1] - a->pos[1], dz = b->pos[2] - a->pos[2];
		const float width = std::sqrt(dx * dx + dy * dy + dz * dz);
		// a, b, b', a': outward for a positive distance, and automatically facing the pocket for a negative one.
		const uint32_t ids[4] = { e.first, e.second, top[e.second], top[e.first] };
		const float uv[4][2] = { { 0.f, 0.f }, { width, 0.f }, { width, height }, { 0.f, height } };
		std::vector<EditableCorner> loop(4);
		for (int k = 0; k < 4; ++k) { loop[k].vertex = ids[k]; loop[k].uv[0] = uv[k][0]; loop[k].uv[1] = uv[k][1]; }
		work.addFace(loop);
	}
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (sideFaces) *sideFaces = boundary.size();
	return true;
}

} // namespace

bool extrudeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float distance, EditableMesh& out,
                  std::string* error, size_t* sideFaces) {
	if (!std::isfinite(distance) || distance == 0.f) { fail(error, "the extrude distance must be a non-zero finite number"); return false; }
	return extrudeRegion(in, faceIds, distance, 0.f, out, error, sideFaces);
}

bool insetFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float thickness, float depth, EditableMesh& out,
                std::string* error, size_t* ringFaces) {
	if (!std::isfinite(thickness) || thickness <= 0.f) { fail(error, "the inset thickness must be a positive finite number"); return false; }
	if (!std::isfinite(depth)) { fail(error, "the inset depth is not finite"); return false; }
	return extrudeRegion(in, faceIds, depth, thickness, out, error, ringFaces);
}

// All-edge chamfer for closed convex, three-valent meshes (boxes, including non-cubic dimensions).
// One inset corner per original face/vertex; edge strips and triangular corner caps close the surface.
static bool bevelAllEdges(const EditableMesh& in, float width, EditableMesh& out,
                          std::string* error, std::vector<uint32_t>* bevelFaces) {
	if (!in.validate(error)) return false;
	EditableMesh work = in;
	std::map<std::pair<uint32_t, uint32_t>, EditableCorner> corners;
	std::map<uint32_t, std::vector<uint32_t>> incident;
	for (const auto& f : in.faces) for (const auto& c : f.loop) incident[c.vertex].push_back(f.id);
	for (const auto& [v, faces] : incident)
		if (faces.size() != 3) { fail(error, "all-edge bevel needs three faces at every vertex"); return false; }
	for (const auto& f : in.faces) {
		float normal[3];
		const float area = newellNormal(in, f, normal);
		for (auto& n : normal) n /= area;
		const auto* anchor = in.findVertex(f.loop.front().vertex);
		for (const auto& v : in.vertices) {
			double side = 0;
			for (int k = 0; k < 3; ++k) side += (double(v.pos[k]) - anchor->pos[k])*normal[k];
			if (side > 1e-6) { fail(error, "all-edge bevel needs a convex outward-oriented mesh"); return false; }
		}
		for (size_t i = 0; i < f.loop.size(); ++i) {
			const auto& c = f.loop[i];
			const auto& prev = f.loop[(i + f.loop.size() - 1) % f.loop.size()];
			const auto& next = f.loop[(i + 1) % f.loop.size()];
			const auto* p = in.findVertex(c.vertex);
			const auto* a = in.findVertex(prev.vertex);
			const auto* b = in.findVertex(next.vertex);
			double u[3], v[3], lu = 0, lv = 0;
			for (int k = 0; k < 3; ++k) { u[k] = a->pos[k] - p->pos[k]; v[k] = b->pos[k] - p->pos[k]; lu += u[k]*u[k]; lv += v[k]*v[k]; }
			lu = std::sqrt(lu); lv = std::sqrt(lv);
			if (lu < 1e-12 || lv < 1e-12) { fail(error, "degenerate bevel corner"); return false; }
			for (int k = 0; k < 3; ++k) { u[k] /= lu; v[k] /= lv; }
			const double dot = u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
			const double sin = std::sqrt(std::max(0.0, 1.0 - dot*dot));
			const double turn = (v[1]*u[2]-v[2]*u[1])*normal[0] + (v[2]*u[0]-v[0]*u[2])*normal[1] + (v[0]*u[1]-v[1]*u[0])*normal[2];
			if (sin < 1e-6 || turn <= 0) { fail(error, "all-edge bevel needs convex face corners"); return false; }
			// width is the perpendicular offset from both incident edges in this face.
			const double d = width / sin;
			const double along = d * (1.0 + dot);
			if (!(along < 0.499 * std::min(lu, lv))) { fail(error, "the bevel width is too large for a face"); return false; }
			EditableCorner result = c;
			result.hasNormal = false;
			result.vertex = work.addVertex(float(p->pos[0] + d*(u[0]+v[0])), float(p->pos[1] + d*(u[1]+v[1])), float(p->pos[2] + d*(u[2]+v[2])));
			for (int k = 0; k < 2; ++k) result.uv[k] = float(c.uv[k] + d/lu*(prev.uv[k]-c.uv[k]) + d/lv*(next.uv[k]-c.uv[k]));
			corners[{f.id, c.vertex}] = result;
		}
	}
	for (auto& f : work.faces) for (auto& c : f.loop) c = corners.at({f.id, c.vertex});
	std::vector<uint32_t> created;
	// At each old vertex, directed strip boundaries determine the opposite direction for its cap.
	std::map<uint32_t, std::map<uint32_t, uint32_t>> capNext;
	for (const auto& e : in.edges) {
		const EditableFace* forward = nullptr; const EditableFace* reverse = nullptr;
		for (const auto& f : in.faces) for (size_t i = 0; i < f.loop.size(); ++i) {
			const auto a = f.loop[i].vertex, b = f.loop[(i+1)%f.loop.size()].vertex;
			if (a == e.a && b == e.b) forward = &f;
			if (a == e.b && b == e.a) reverse = &f;
		}
		if (!forward || !reverse) { fail(error, "all-edge bevel needs a closed mesh"); return false; }
		auto a = corners.at({forward->id, e.a}), b = corners.at({forward->id, e.b});
		auto c = corners.at({reverse->id, e.a}), d = corners.at({reverse->id, e.b});
		created.push_back(work.addFace({b, a, c, d}));
		capNext[e.a][c.vertex] = a.vertex;
		capNext[e.b][b.vertex] = d.vertex;
	}
	for (const auto& [vertex, next] : capNext) {
		if (next.size() != 3) { fail(error, "invalid bevel corner fan"); return false; }
		std::vector<EditableCorner> loop;
		const uint32_t first = next.begin()->first;
		uint32_t v = first;
		for (int k = 0; k < 3; ++k) {
			EditableCorner c; c.vertex = v; c.uv[0] = k == 1 ? 1.f : 0.f; c.uv[1] = k == 2 ? 1.f : 0.f;
			loop.push_back(c);
			const auto it = next.find(v);
			if (it == next.end()) { fail(error, "invalid bevel corner fan"); return false; }
			v = it->second;
		}
		if (v != first) { fail(error, "invalid bevel corner fan"); return false; }
		created.push_back(work.addFace(loop));
	}
	work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(), [&](const EditableVertex& v) { return incident.count(v.id) > 0; }), work.vertices.end());
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (bevelFaces) *bevelFaces = std::move(created);
	return true;
}

bool bevelEdges(const EditableMesh& in, const std::vector<uint32_t>& edgeIds, float width, EditableMesh& out,
                std::string* error, std::vector<uint32_t>* bevelFaces) {
	if (edgeIds.empty()) { fail(error, "no edges selected"); return false; }
	if (!std::isfinite(width) || width <= 0.f) { fail(error, "the bevel width must be a positive finite number"); return false; }
	const std::set<uint32_t> requested(edgeIds.begin(), edgeIds.end());
	if (requested.size() == in.edges.size() && std::all_of(in.edges.begin(), in.edges.end(), [&](const EditableEdge& e) { return requested.count(e.id) > 0; }))
		return bevelAllEdges(in, width, out, error, bevelFaces);

	// Per endpoint of a selected edge: the two faces of the edge (f1 walks a -> b, f2 walks b -> a), this vertex's
	// neighbours n1 / n2 in them (other than the partner), the slide fractions along vertex->n1 / vertex->n2, and the
	// replacement vertices v1 (on f1) / v2 (on f2).
	struct Info {
		uint32_t partner = 0, f1 = 0, f2 = 0;
		uint32_t n1 = 0, n2 = 0;
		float    s1 = 0.f, s2 = 0.f;
		uint32_t v1 = 0, v2 = 0;
	};
	std::map<uint32_t, Info> info;
	struct Selected { uint32_t a, b; };
	std::vector<Selected> selected;

	auto faceHasDirected = [&](const EditableFace& f, uint32_t a, uint32_t b) {
		for (size_t i = 0; i < f.loop.size(); ++i)
			if (f.loop[i].vertex == a && f.loop[(i + 1) % f.loop.size()].vertex == b) return true;
		return false;
	};
	auto neighbourOther = [&](const EditableFace& f, uint32_t v, uint32_t partner) {
		for (size_t i = 0; i < f.loop.size(); ++i) {
			if (f.loop[i].vertex != v) continue;
			const uint32_t prev = f.loop[(i + f.loop.size() - 1) % f.loop.size()].vertex;
			const uint32_t next = f.loop[(i + 1) % f.loop.size()].vertex;
			return prev == partner ? next : prev;
		}
		return partner;
	};
	auto faceCount = [&](uint32_t v) {
		size_t n = 0;
		for (const auto& f : in.faces)
			for (const auto& c : f.loop) if (c.vertex == v) { ++n; break; }
		return n;
	};

	const std::set<uint32_t> uniqueEdges(edgeIds.begin(), edgeIds.end());
	for (uint32_t id : uniqueEdges) {
		const EditableEdge* edge = nullptr;
		for (const auto& e : in.edges) if (e.id == id) { edge = &e; break; }
		if (!edge) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
		const uint32_t a = edge->a, b = edge->b;
		uint32_t f1 = 0, f2 = 0;
		bool have1 = false, have2 = false;
		for (const auto& f : in.faces) {
			if (faceHasDirected(f, a, b)) { f1 = f.id; have1 = true; }
			else if (faceHasDirected(f, b, a)) { f2 = f.id; have2 = true; }
		}
		if (!have1 || !have2) { fail(error, "edge " + std::to_string(id) + " is a border edge, bevel needs two faces"); return false; }
		if (info.count(a) || info.count(b)) { fail(error, "selected edges share a vertex, bevel supports separate edges only"); return false; }
		if (faceCount(a) != 3 || faceCount(b) != 3) {
			fail(error, "edge " + std::to_string(id) + " ends at a vertex that does not have exactly three faces, bevel is not supported there");
			return false;
		}
		selected.push_back({ a, b });
		for (int end = 0; end < 2; ++end) {
			Info i;
			const uint32_t v = end ? b : a;
			i.partner = end ? a : b;
			i.f1 = f1; i.f2 = f2;
			i.n1 = neighbourOther(*in.findFace(f1), v, i.partner);
			i.n2 = neighbourOther(*in.findFace(f2), v, i.partner);
			info[v] = i;
		}
	}

	// Slide distances and the new vertex positions.
	std::map<std::pair<uint32_t, uint32_t>, double> used;  // undirected neighbouring edge -> total fraction taken
	EditableMesh work = in;
	for (auto& kv : info) {
		const uint32_t v = kv.first;
		Info& i = kv.second;
		const EditableVertex* pv = in.findVertex(v);
		const EditableVertex* pp = in.findVertex(i.partner);
		for (int side = 0; side < 2; ++side) {
			const uint32_t n = side ? i.n2 : i.n1;
			const EditableVertex* pn = in.findVertex(n);
			double u[3], w[3];
			for (int k = 0; k < 3; ++k) { u[k] = double(pp->pos[k]) - pv->pos[k]; w[k] = double(pn->pos[k]) - pv->pos[k]; }
			const double lu = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
			const double lw = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
			if (!(lu > 1e-12) || !(lw > 1e-12)) { fail(error, "a degenerate edge next to vertex " + std::to_string(v)); return false; }
			const double cx = u[1] * w[2] - u[2] * w[1], cy = u[2] * w[0] - u[0] * w[2], cz = u[0] * w[1] - u[1] * w[0];
			const double sinT = std::sqrt(cx * cx + cy * cy + cz * cz) / (lu * lw);
			if (!(sinT > 1e-6)) { fail(error, "the faces at vertex " + std::to_string(v) + " are degenerate, bevel is not supported there"); return false; }
			const double fraction = (double(width) / sinT) / lw;
			double& total = used[{ std::min(v, n), std::max(v, n) }];
			total += fraction;
			if (!(total < 0.999)) { fail(error, "the bevel width is too large for the edge next to vertex " + std::to_string(v)); return false; }
			(side ? i.s2 : i.s1) = float(fraction);
			const uint32_t created = work.addVertex(float(pv->pos[0] + w[0] * fraction), float(pv->pos[1] + w[1] * fraction),
			                                       float(pv->pos[2] + w[2] * fraction));
			(side ? i.v2 : i.v1) = created;
		}
	}

	auto lerpCorner = [](const EditableCorner& from, const EditableCorner& to, float t, uint32_t vertex) {
		EditableCorner c = from;
		c.vertex = vertex;
		c.uv[0] = from.uv[0] + (to.uv[0] - from.uv[0]) * t;
		c.uv[1] = from.uv[1] + (to.uv[1] - from.uv[1]) * t;
		return c;
	};

	// Rewrite every face that touches an old endpoint.
	std::vector<std::pair<size_t, std::vector<std::vector<EditableCorner>>>> rewritten;  // face index -> pieces
	for (size_t fi = 0; fi < in.faces.size(); ++fi) {
		const EditableFace& f = in.faces[fi];
		bool touched = false;
		for (const auto& c : f.loop) if (info.count(c.vertex)) touched = true;
		if (!touched) continue;
		std::vector<EditableCorner> loop;
		const size_t n = f.loop.size();
		for (size_t i = 0; i < n; ++i) {
			const EditableCorner& c = f.loop[i];
			auto it = info.find(c.vertex);
			if (it == info.end()) { loop.push_back(c); continue; }
			const Info& inf = it->second;
			const EditableCorner& prev = f.loop[(i + n - 1) % n];
			const EditableCorner& next = f.loop[(i + 1) % n];
			if (f.id == inf.f1) {
				const EditableCorner& toward = prev.vertex == inf.n1 ? prev : next;
				loop.push_back(lerpCorner(c, toward, inf.s1, inf.v1));
			} else if (f.id == inf.f2) {
				const EditableCorner& toward = prev.vertex == inf.n2 ? prev : next;
				loop.push_back(lerpCorner(c, toward, inf.s2, inf.v2));
			} else {
				// The third face runs ... n2 -> a -> n1 ... at the edge's first end and ... n1 -> b -> n2 ... at the second;
				// the vertex becomes two corners, the one toward the previous neighbour first.
				if (prev.vertex == inf.n2 && next.vertex == inf.n1) {
					loop.push_back(lerpCorner(c, prev, inf.s2, inf.v2));
					loop.push_back(lerpCorner(c, next, inf.s1, inf.v1));
				} else if (prev.vertex == inf.n1 && next.vertex == inf.n2) {
					loop.push_back(lerpCorner(c, prev, inf.s1, inf.v1));
					loop.push_back(lerpCorner(c, next, inf.s2, inf.v2));
				} else {
					fail(error, "the faces around vertex " + std::to_string(c.vertex) + " are not consistently oriented");
					return false;
				}
			}
		}
		// Quads and triangles only: peel quads off the front of a longer loop.
		std::vector<std::vector<EditableCorner>> pieces;
		while (loop.size() > 4) {
			pieces.push_back({ loop[0], loop[1], loop[2], loop[3] });
			loop.erase(loop.begin() + 1, loop.begin() + 3);
		}
		pieces.push_back(std::move(loop));
		rewritten.push_back({ fi, std::move(pieces) });
	}
	for (auto& r : rewritten) {
		work.faces[r.first].loop = std::move(r.second[0]);
		for (size_t k = 1; k < r.second.size(); ++k) work.addFace(r.second[k]);
	}

	// The bevel quads: b1 a1 a2 b2 (b1->a1 reverses f1's a1->b1, a2->b2 reverses f2's b2->a2).
	std::vector<uint32_t> bevelIds;
	for (const auto& s : selected) {
		const Info& ia = info[s.a];
		const Info& ib = info[s.b];
		const EditableVertex* a1 = work.findVertex(ia.v1);
		const EditableVertex* a2 = work.findVertex(ia.v2);
		const EditableVertex* pa = in.findVertex(s.a);
		const EditableVertex* pb = in.findVertex(s.b);
		auto dist = [](const float* p, const float* q) {
			return std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
		};
		const float length = dist(pa->pos, pb->pos), across = dist(a1->pos, a2->pos);
		const uint32_t ids[4] = { ib.v1, ia.v1, ia.v2, ib.v2 };
		const float uv[4][2] = { { length, 0.f }, { 0.f, 0.f }, { 0.f, across }, { length, across } };
		std::vector<EditableCorner> loop(4);
		for (int k = 0; k < 4; ++k) { loop[k].vertex = ids[k]; loop[k].uv[0] = uv[k][0]; loop[k].uv[1] = uv[k][1]; }
		bevelIds.push_back(work.addFace(loop));
	}

	work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(),
		[&](const EditableVertex& v) { return info.count(v.id) != 0; }), work.vertices.end());
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (bevelFaces) *bevelFaces = bevelIds;
	return true;
}

bool loopCut(const EditableMesh& in, uint32_t edgeId, float factor, EditableMesh& out, std::string* error,
             std::vector<uint32_t>* newVertices, size_t* splitFaces) {
	if (!std::isfinite(factor) || !(factor > 0.f) || !(factor < 1.f)) { fail(error, "the loop cut position must be strictly between 0 and 1"); return false; }
	const EditableEdge* start = nullptr;
	for (const auto& e : in.edges) if (e.id == edgeId) { start = &e; break; }
	if (!start) { fail(error, "edge " + std::to_string(edgeId) + " does not exist"); return false; }

	using Key = std::pair<uint32_t, uint32_t>;
	auto key = [](uint32_t a, uint32_t b) { return Key{ std::min(a, b), std::max(a, b) }; };
	auto position = [](const EditableFace& f, uint32_t s, uint32_t e) {  // index i with {loop[i], loop[i+1]} == {s, e}, or -1
		for (size_t i = 0; i < f.loop.size(); ++i) {
			const uint32_t p = f.loop[i].vertex, q = f.loop[(i + 1) % f.loop.size()].vertex;
			if ((p == s && q == e) || (p == e && q == s)) return int(i);
		}
		return -1;
	};
	auto otherFace = [&](uint32_t s, uint32_t e, uint32_t except) -> const EditableFace* {
		for (const auto& f : in.faces) if (f.id != except && position(f, s, e) >= 0) return &f;
		return nullptr;
	};

	// Ring edges as parallel-directed pairs (s -> e), and the faces they cross, each with the loop index of its first cut edge.
	struct Cut { uint32_t s, e; };
	std::vector<Cut> cuts;
	std::set<Key> seen;
	std::map<uint32_t, int> faceStart;  // face id -> index i of the cut edge (i, i+1); the other is (i+2, i+3)
	cuts.push_back({ start->a, start->b });
	seen.insert(key(start->a, start->b));

	std::vector<const EditableFace*> firstFaces;
	for (const auto& f : in.faces) if (position(f, start->a, start->b) >= 0) firstFaces.push_back(&f);
	if (firstFaces.empty() || firstFaces.size() > 2) { fail(error, "the edge does not have one or two faces"); return false; }

	bool closed = false;
	for (const EditableFace* first : firstFaces) {
		uint32_t s = start->a, e = start->b;
		const EditableFace* face = first;
		while (face) {
			if (face->loop.size() != 4) { fail(error, "loop cut needs quad faces; face " + std::to_string(face->id) + " is not a quad"); return false; }
			const int i = position(*face, s, e);
			if (faceStart.count(face->id)) { closed = true; break; }  // the walk came back around
			faceStart[face->id] = i;
			const auto& q = face->loop;
			uint32_t ns, ne;
			if (q[i].vertex == s) { ns = q[(i + 3) % 4].vertex; ne = q[(i + 2) % 4].vertex; }
			else                  { ns = q[(i + 2) % 4].vertex; ne = q[(i + 3) % 4].vertex; }
			// `first` is the face on one side: the new edge continues on that side only.
			if (seen.count(key(ns, ne))) { closed = true; break; }
			seen.insert(key(ns, ne));
			// Edges found while walking from the second face go to the front conceptually; order does not matter for cutting.
			cuts.push_back({ ns, ne });
			s = ns; e = ne;
			face = otherFace(s, e, face->id);
		}
		if (closed) break;
	}

	// The faces cut: every face that has two cut edges opposite each other. Recompute from the final cut set so the
	// start face on each side is included even when the ring closed immediately.
	std::map<Key, uint32_t> midpoint;
	EditableMesh work = in;
	std::vector<uint32_t> created;
	for (const auto& c : cuts) {
		const EditableVertex* a = in.findVertex(c.s);
		const EditableVertex* b = in.findVertex(c.e);
		const uint32_t id = work.addVertex(a->pos[0] + (b->pos[0] - a->pos[0]) * factor, a->pos[1] + (b->pos[1] - a->pos[1]) * factor,
		                                  a->pos[2] + (b->pos[2] - a->pos[2]) * factor);
		midpoint[key(c.s, c.e)] = id;
		created.push_back(id);
	}
	std::map<Key, std::pair<uint32_t, float>> factorFrom;  // undirected edge -> (vertex the factor is measured from, factor)
	for (const auto& c : cuts) factorFrom[key(c.s, c.e)] = { c.s, factor };

	auto cornerOn = [&](const EditableCorner& from, const EditableCorner& to, uint32_t mid) {
		const auto& ff = factorFrom[key(from.vertex, to.vertex)];
		const float t = ff.first == from.vertex ? ff.second : 1.f - ff.second;
		EditableCorner c = from;
		c.vertex = mid;
		c.uv[0] = from.uv[0] + (to.uv[0] - from.uv[0]) * t;
		c.uv[1] = from.uv[1] + (to.uv[1] - from.uv[1]) * t;
		if (from.hasNormal && to.hasNormal) {
			double n[3], len = 0;
			for (int k = 0; k < 3; ++k) { n[k] = from.normal[k] + (to.normal[k] - from.normal[k]) * t; len += n[k] * n[k]; }
			len = std::sqrt(len);
			if (len > 1e-12) for (int k = 0; k < 3; ++k) c.normal[k] = float(n[k] / len);
			else c.hasNormal = false;
		} else c.hasNormal = false;
		return c;
	};

	size_t split = 0;
	std::vector<std::vector<EditableCorner>> extra;
	for (auto& face : work.faces) {
		const EditableFace* orig = in.findFace(face.id);
		if (!orig || orig->loop.size() != 4) continue;
		// Find a pair of opposite edges that are both cut.
		int at = -1;
		for (int i = 0; i < 2 && at < 0; ++i) {
			const auto& q = orig->loop;
			if (midpoint.count(key(q[i].vertex, q[(i + 1) % 4].vertex)) && midpoint.count(key(q[(i + 2) % 4].vertex, q[(i + 3) % 4].vertex))) at = i;
		}
		if (at < 0) continue;
		const auto& q = orig->loop;
		const EditableCorner &c0 = q[at], &c1 = q[(at + 1) % 4], &c2 = q[(at + 2) % 4], &c3 = q[(at + 3) % 4];
		const EditableCorner m1 = cornerOn(c0, c1, midpoint[key(c0.vertex, c1.vertex)]);
		const EditableCorner m1b = cornerOn(c1, c0, midpoint[key(c0.vertex, c1.vertex)]);
		const EditableCorner m2 = cornerOn(c2, c3, midpoint[key(c2.vertex, c3.vertex)]);
		const EditableCorner m2b = cornerOn(c3, c2, midpoint[key(c2.vertex, c3.vertex)]);
		// [c0, m1, m2, c3] keeps the id, [m1, c1, c2, m2] is new. The midpoint corner is taken from the nearer end per piece.
		face.loop = { c0, m1, m2b, c3 };
		extra.push_back({ m1b, c1, c2, m2 });
		++split;
	}
	for (const auto& loop : extra) work.addFace(loop);
	// Every cut edge must belong to two split faces (or one, on a mesh border), otherwise a T-junction would remain.
	{
		std::map<Key, int> uses;
		for (const auto& f : in.faces) {
			if (f.loop.size() != 4) continue;
			for (int i = 0; i < 2; ++i)
				if (midpoint.count(key(f.loop[i].vertex, f.loop[(i + 1) % 4].vertex)) &&
				    midpoint.count(key(f.loop[(i + 2) % 4].vertex, f.loop[(i + 3) % 4].vertex))) {
					++uses[key(f.loop[i].vertex, f.loop[(i + 1) % 4].vertex)];
					++uses[key(f.loop[(i + 2) % 4].vertex, f.loop[(i + 3) % 4].vertex)];
				}
		}
		for (const auto& kv : midpoint) {
			int neighbours = 0;
			for (const auto& f : in.faces) if (position(f, kv.first.first, kv.first.second) >= 0) ++neighbours;
			if (uses[kv.first] != neighbours) { fail(error, "the edge ring does not close cleanly (a face next to it is not cut)"); return false; }
		}
	}
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (newVertices) *newVertices = created;
	if (splitFaces) *splitFaces = split;
	return true;
}

bool knifePlane(const EditableMesh& in, const std::array<float, 3>& point, const std::array<float, 3>& normal, EditableMesh& out,
                std::string* error, std::vector<uint32_t>* cutVertices, size_t* splitFaces) {
	using Key = std::pair<uint32_t, uint32_t>;
	auto key = [](uint32_t a, uint32_t b) { return a < b ? Key{ a, b } : Key{ b, a }; };
	double len = 0;
	for (float c : normal) { if (!std::isfinite(c)) { fail(error, "the plane normal must be finite"); return false; } len += double(c) * c; }
	for (float c : point) if (!std::isfinite(c)) { fail(error, "the plane point must be finite"); return false; }
	if (len < 1e-18) { fail(error, "the plane normal must not be zero"); return false; }
	len = std::sqrt(len);
	const double n[3] = { normal[0] / len, normal[1] / len, normal[2] / len };
	constexpr double kEps = 1.0e-5;
	std::map<uint32_t, double> dist;
	for (const auto& v : in.vertices)
		dist[v.id] = n[0] * (v.pos[0] - point[0]) + n[1] * (v.pos[1] - point[1]) + n[2] * (v.pos[2] - point[2]);
	auto side = [&](uint32_t id) { const double d = dist[id]; return d > kEps ? 1 : d < -kEps ? -1 : 0; };

	EditableMesh work = in;
	std::map<Key, uint32_t> crossing;   // undirected edge -> the vertex inserted where the plane crosses it
	auto crossVertex = [&](uint32_t a, uint32_t b) {
		const Key k = key(a, b);
		const auto it = crossing.find(k);
		if (it != crossing.end()) return it->second;
		const EditableVertex* va = in.findVertex(a);
		const EditableVertex* vb = in.findVertex(b);
		const double t = dist[a] / (dist[a] - dist[b]);   // measured from a
		const uint32_t id = work.addVertex(float(va->pos[0] + (vb->pos[0] - va->pos[0]) * t), float(va->pos[1] + (vb->pos[1] - va->pos[1]) * t),
		                                  float(va->pos[2] + (vb->pos[2] - va->pos[2]) * t));
		crossing[k] = id;
		return id;
	};

	struct Node { EditableCorner corner; bool onPlane = false; };
	std::set<uint32_t> cut;
	size_t split = 0;
	std::vector<std::vector<EditableCorner>> extra;
	for (auto& face : work.faces) {
		const EditableFace* orig = in.findFace(face.id);
		if (!orig) continue;   // (only original faces are in `work.faces` at this point)
		bool anyPlus = false, anyMinus = false;
		for (const auto& c : orig->loop) { const int s = side(c.vertex); anyPlus = anyPlus || s > 0; anyMinus = anyMinus || s < 0; }
		if (!anyPlus || !anyMinus) continue;

		// The loop with a corner inserted wherever the plane crosses an edge.
		std::vector<Node> ring;
		const auto& loop = orig->loop;
		for (size_t i = 0; i < loop.size(); ++i) {
			const EditableCorner& from = loop[i];
			const EditableCorner& to = loop[(i + 1) % loop.size()];
			ring.push_back({ from, side(from.vertex) == 0 });
			if (side(from.vertex) * side(to.vertex) < 0) {
				const double t = dist[from.vertex] / (dist[from.vertex] - dist[to.vertex]);
				EditableCorner c = from;
				c.vertex = crossVertex(from.vertex, to.vertex);
				c.uv[0] = float(from.uv[0] + (to.uv[0] - from.uv[0]) * t);
				c.uv[1] = float(from.uv[1] + (to.uv[1] - from.uv[1]) * t);
				if (from.hasNormal && to.hasNormal) {
					double m[3], l = 0;
					for (int k = 0; k < 3; ++k) { m[k] = from.normal[k] + (to.normal[k] - from.normal[k]) * t; l += m[k] * m[k]; }
					l = std::sqrt(l);
					if (l > 1e-12) for (int k = 0; k < 3; ++k) c.normal[k] = float(m[k] / l); else c.hasNormal = false;
				} else c.hasNormal = false;
				ring.push_back({ c, true });
			}
		}
		std::vector<size_t> onPlane;
		for (size_t i = 0; i < ring.size(); ++i) if (ring[i].onPlane) onPlane.push_back(i);
		if (onPlane.size() != 2) { fail(error, "the plane meets face " + std::to_string(face.id) + " in " + std::to_string(onPlane.size()) + " places; only convex faces can be cut"); return false; }

		auto piece = [&](size_t from, size_t to) {
			std::vector<EditableCorner> p;
			for (size_t i = from;; i = (i + 1) % ring.size()) { p.push_back(ring[i].corner); if (i == to) break; }
			return p;
		};
		std::vector<std::vector<EditableCorner>> pieces;
		for (auto& p : { piece(onPlane[0], onPlane[1]), piece(onPlane[1], onPlane[0]) }) {
			// Faces stay triangles or quads: a five-corner piece becomes a quad and a triangle.
			std::vector<EditableCorner> rest = p;
			while (rest.size() > 4) {
				pieces.push_back({ rest[0], rest[1], rest[2], rest[3] });
				rest.erase(rest.begin() + 1, rest.begin() + 3);   // [0, 3, 4, ...]
			}
			pieces.push_back(rest);
		}
		face.loop = pieces[0];
		for (size_t i = 1; i < pieces.size(); ++i) extra.push_back(pieces[i]);
		for (const Node& node : ring) if (node.onPlane) cut.insert(node.corner.vertex);
		++split;
	}
	if (split == 0) { fail(error, "the plane does not cross any face"); return false; }
	for (const auto& loop : extra) work.addFace(loop);
	work.rebuildEdges();
	if (!work.validate(error)) { if (error) *error = "the cut would produce an invalid mesh: " + *error; return false; }
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (cutVertices) cutVertices->assign(cut.begin(), cut.end());
	if (splitFaces) *splitFaces = split;
	return true;
}

bool unwrapFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float angleDeg, float margin, EditableMesh& out,
                 std::string* error, size_t* charts) {
	if (faceIds.empty()) { fail(error, "no faces selected"); return false; }
	if (!std::isfinite(angleDeg) || angleDeg <= 0.f || angleDeg > 180.f) { fail(error, "the chart angle must be between 0 and 180 degrees"); return false; }
	if (!std::isfinite(margin) || margin < 0.f || margin > 0.25f) { fail(error, "the margin must be between 0 and 0.25"); return false; }
	const std::set<uint32_t> selected(faceIds.begin(), faceIds.end());
	for (uint32_t id : selected)
		if (!in.findFace(id)) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }

	struct Info { std::array<double, 3> n{ 0, 0, 0 }; double area = 0; };  // unit normal and area
	std::map<uint32_t, Info> info;
	std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edgeFaces;
	for (const auto& f : in.faces) {
		if (!selected.count(f.id)) continue;
		float n[3];
		const float twice = newellNormal(in, f, n);
		if (!(twice > 1e-12f)) { fail(error, "face " + std::to_string(f.id) + " has no area"); return false; }
		info[f.id] = { { n[0] / twice, n[1] / twice, n[2] / twice }, 0.5 * twice };
		for (size_t i = 0; i < f.loop.size(); ++i) {
			uint32_t a = f.loop[i].vertex, b = f.loop[(i + 1) % f.loop.size()].vertex;
			if (a > b) std::swap(a, b);
			edgeFaces[{ a, b }].push_back(f.id);
		}
	}
	std::map<uint32_t, std::vector<uint32_t>> neighbours;
	for (const auto& e : edgeFaces) {
		if (std::binary_search(in.seams.begin(), in.seams.end(), e.first)) continue;   // a marked seam separates charts
		for (uint32_t x : e.second) for (uint32_t y : e.second) if (x != y) neighbours[x].push_back(y);
	}

	// Charts: flood fill, comparing each candidate with the chart's running average normal.
	const double kPi = 3.14159265358979323846;
	const double limit = std::cos(double(angleDeg) * kPi / 180.0) - 1e-9;
	struct Chart {
		std::vector<uint32_t> faces;
		std::array<double, 3> n{ 0, 0, 0 };
		double u[3]{}, v[3]{};
		double w = 0, h = 0, x = 0, y = 0, minU = 0, minV = 0;
	};
	std::vector<Chart> list;
	std::set<uint32_t> done;
	for (const auto& f : in.faces) {
		if (!selected.count(f.id) || done.count(f.id)) continue;
		Chart chart;
		std::vector<uint32_t> stack{ f.id };
		done.insert(f.id);
		while (!stack.empty()) {
			const uint32_t cur = stack.back(); stack.pop_back();
			chart.faces.push_back(cur);
			const Info& ci = info[cur];
			for (int k = 0; k < 3; ++k) chart.n[k] += ci.n[k] * ci.area;
			for (uint32_t nb : neighbours[cur]) {
				if (done.count(nb)) continue;
				const double len = std::sqrt(chart.n[0] * chart.n[0] + chart.n[1] * chart.n[1] + chart.n[2] * chart.n[2]);
				const Info& ni = info[nb];
				const double d = len > 1e-12 ? (chart.n[0] * ni.n[0] + chart.n[1] * ni.n[1] + chart.n[2] * ni.n[2]) / len : 1.0;
				if (d < limit) continue;
				done.insert(nb);
				stack.push_back(nb);
			}
		}
		list.push_back(std::move(chart));
	}

	// Planar projection per chart. u = ref x n, v = n x u (u x v = n: the chart reads upright and unmirrored from outside).
	for (auto& c : list) {
		double len = std::sqrt(c.n[0] * c.n[0] + c.n[1] * c.n[1] + c.n[2] * c.n[2]);
		if (!(len > 1e-12)) { c.n = info[c.faces[0]].n; len = 1.0; }
		for (int k = 0; k < 3; ++k) c.n[k] /= len;
		double ref[3] = { 0, 1, 0 };
		if (std::fabs(c.n[1]) > 0.9) { ref[1] = 0; ref[2] = 1; }
		double u[3] = { ref[1] * c.n[2] - ref[2] * c.n[1], ref[2] * c.n[0] - ref[0] * c.n[2], ref[0] * c.n[1] - ref[1] * c.n[0] };
		const double ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
		for (int k = 0; k < 3; ++k) c.u[k] = u[k] / ul;
		c.v[0] = c.n[1] * c.u[2] - c.n[2] * c.u[1];
		c.v[1] = c.n[2] * c.u[0] - c.n[0] * c.u[2];
		c.v[2] = c.n[0] * c.u[1] - c.n[1] * c.u[0];
		double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
		for (uint32_t fid : c.faces)
			for (const auto& corner : in.findFace(fid)->loop) {
				const EditableVertex* p = in.findVertex(corner.vertex);
				const double s = p->pos[0] * c.u[0] + p->pos[1] * c.u[1] + p->pos[2] * c.u[2];
				const double t = p->pos[0] * c.v[0] + p->pos[1] * c.v[1] + p->pos[2] * c.v[2];
				lo[0] = std::min(lo[0], s); hi[0] = std::max(hi[0], s);
				lo[1] = std::min(lo[1], t); hi[1] = std::max(hi[1], t);
			}
		c.minU = lo[0]; c.minV = lo[1];
		c.w = hi[0] - lo[0]; c.h = hi[1] - lo[1];
	}

	// Shelf packing in model units; the gap is a fraction of the packed size so it survives the final scale.
	double totalArea = 0, maxW = 0;
	for (const auto& c : list) { totalArea += std::max(c.w, 1e-6) * std::max(c.h, 1e-6); maxW = std::max(maxW, c.w); }
	const double gap = double(margin) * std::sqrt(totalArea);
	std::vector<size_t> order(list.size());
	for (size_t i = 0; i < order.size(); ++i) order[i] = i;
	std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return list[a].h > list[b].h; });
	const double shelfWidth = std::max(maxW + gap, std::sqrt(totalArea) * 1.15 + gap);
	double cx = gap * 0.5, cy = gap * 0.5, rowH = 0, usedW = 0;
	for (size_t i : order) {
		Chart& c = list[i];
		if (cx > gap * 0.5 && cx + c.w + gap > shelfWidth) { cx = gap * 0.5; cy += rowH + gap; rowH = 0; }
		c.x = cx; c.y = cy;
		cx += c.w + gap;
		rowH = std::max(rowH, c.h);
		usedW = std::max(usedW, cx);
	}
	const double usedH = cy + rowH + gap * 0.5;
	const double scale = 1.0 / std::max({ usedW, usedH, 1e-9 });

	EditableMesh work = in;
	std::map<uint32_t, const Chart*> chartOf;
	for (const auto& c : list) for (uint32_t fid : c.faces) chartOf[fid] = &c;
	for (auto& f : work.faces) {
		auto it = chartOf.find(f.id);
		if (it == chartOf.end()) continue;
		const Chart& c = *it->second;
		for (auto& corner : f.loop) {
			const EditableVertex* p = in.findVertex(corner.vertex);
			const double s = p->pos[0] * c.u[0] + p->pos[1] * c.u[1] + p->pos[2] * c.u[2];
			const double t = p->pos[0] * c.v[0] + p->pos[1] * c.v[1] + p->pos[2] * c.v[2];
			corner.uv[0] = float((c.x + (s - c.minU)) * scale);
			corner.uv[1] = float((c.y + (t - c.minV)) * scale);
		}
	}
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (charts) *charts = list.size();
	return true;
}

bool markSeams(const EditableMesh& in, const std::vector<uint32_t>& edgeIds, bool mark, EditableMesh& out,
               std::string* error, size_t* changed) {
	if (edgeIds.empty()) { fail(error, "no edges selected"); return false; }
	EditableMesh work = in;
	size_t count = 0;
	for (uint32_t id : std::set<uint32_t>(edgeIds.begin(), edgeIds.end())) {
		const EditableEdge* e = nullptr;
		for (const auto& cand : work.edges) if (cand.id == id) { e = &cand; break; }
		if (!e) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
		const std::pair<uint32_t, uint32_t> key{ e->a, e->b };
		auto it = std::lower_bound(work.seams.begin(), work.seams.end(), key);
		const bool present = it != work.seams.end() && *it == key;
		if (mark && !present) { work.seams.insert(it, key); ++count; }
		else if (!mark && present) { work.seams.erase(it); ++count; }
	}
	if (count == 0) { fail(error, mark ? "the selected edges are already seams" : "none of the selected edges is a seam"); return false; }
	work.revision = nextEditableRevision();   // caches keyed on the revision (the edit overlay) must redraw the seams
	out = std::move(work);
	if (changed) *changed = count;
	return true;
}

bool shadeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, bool smooth, float angleDeg, EditableMesh& out,
                std::string* error, size_t* changedCorners) {
	if (faceIds.empty()) { fail(error, "no faces selected"); return false; }
	if (!std::isfinite(angleDeg) || angleDeg < 0.f || angleDeg > 180.f) { fail(error, "the smoothing angle must be between 0 and 180 degrees"); return false; }
	const std::set<uint32_t> selected(faceIds.begin(), faceIds.end());
	for (uint32_t id : selected)
		if (!in.findFace(id)) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }

	EditableMesh work = in;
	size_t changed = 0;
	if (!smooth) {
		for (auto& f : work.faces) {
			if (!selected.count(f.id)) continue;
			for (auto& c : f.loop)
				if (c.hasNormal) { c.hasNormal = false; c.normal[0] = c.normal[1] = c.normal[2] = 0.f; ++changed; }
		}
	} else {
		// Area-weighted face normals (Newell vector: length = twice the area), then per corner the average of the faces at
		// that vertex whose normal is within the angle of this face's own normal (180 = fully smooth, small = keeps creases).
		std::map<uint32_t, std::array<double, 3>> areaNormal;
		std::map<uint32_t, std::vector<uint32_t>> facesAt;
		for (const auto& f : in.faces) {
			std::array<double, 3> n{ 0, 0, 0 };
			for (size_t i = 0; i < f.loop.size(); ++i) {
				const EditableVertex* p = in.findVertex(f.loop[i].vertex);
				const EditableVertex* q = in.findVertex(f.loop[(i + 1) % f.loop.size()].vertex);
				n[0] += (double(p->pos[1]) - q->pos[1]) * (double(p->pos[2]) + q->pos[2]);
				n[1] += (double(p->pos[2]) - q->pos[2]) * (double(p->pos[0]) + q->pos[0]);
				n[2] += (double(p->pos[0]) - q->pos[0]) * (double(p->pos[1]) + q->pos[1]);
				facesAt[f.loop[i].vertex].push_back(f.id);
			}
			areaNormal[f.id] = n;
		}
		const double kPi = 3.14159265358979323846;
		const double limit = std::cos(double(angleDeg) * kPi / 180.0) - 1e-9;
		auto length = [](const std::array<double, 3>& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
		for (auto& f : work.faces) {
			if (!selected.count(f.id)) continue;
			const auto& own = areaNormal[f.id];
			const double ownLen = length(own);
			if (!(ownLen > 1e-12)) continue;
			for (auto& c : f.loop) {
				std::array<double, 3> sum{ 0, 0, 0 };
				for (uint32_t gid : facesAt[c.vertex]) {
					const auto& g = areaNormal[gid];
					const double gl = length(g);
					if (!(gl > 1e-12)) continue;
					if ((own[0] * g[0] + own[1] * g[1] + own[2] * g[2]) / (ownLen * gl) < limit) continue;
					for (int k = 0; k < 3; ++k) sum[k] += g[k];
				}
				const double len = length(sum);
				const bool before = c.hasNormal;
				float prev[3] = { c.normal[0], c.normal[1], c.normal[2] };
				if (!(len > 1e-12) || (sum[0] * own[0] + sum[1] * own[1] + sum[2] * own[2]) / (len * ownLen) > 0.99999) {
					c.hasNormal = false;  // identical to the flat normal: keep deriving it
					c.normal[0] = c.normal[1] = c.normal[2] = 0.f;
				} else {
					c.hasNormal = true;
					for (int k = 0; k < 3; ++k) c.normal[k] = float(sum[k] / len);
				}
				if (before != c.hasNormal || (c.hasNormal && (prev[0] != c.normal[0] || prev[1] != c.normal[1] || prev[2] != c.normal[2]))) ++changed;
			}
		}
	}
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (changedCorners) *changedCorners = changed;
	return true;
}

namespace {

bool mirrorOneAxis(const EditableMesh& in, int a, float merge, EditableMesh& out, std::string* error) {
	EditableMesh work = in;
	std::map<uint32_t, uint32_t> twin;
	std::set<uint32_t> onPlane;
	for (size_t i = 0; i < in.vertices.size(); ++i) {
		const EditableVertex& v = in.vertices[i];
		if (std::fabs(v.pos[a]) <= merge) {
			twin[v.id] = v.id;
			onPlane.insert(v.id);
			work.vertices[i].pos[a] = 0.f;  // snap so the seam is exactly closed
		} else {
			float p[3] = { v.pos[0], v.pos[1], v.pos[2] };
			p[a] = -p[a];
			twin[v.id] = work.addVertex(p[0], p[1], p[2]);
		}
	}
	std::set<uint32_t> caps;  // faces lying entirely in the plane: the half-model's cap, removed so the halves join
	for (const auto& f : in.faces) {
		bool allOnPlane = true;
		for (const auto& c : f.loop) if (!onPlane.count(c.vertex)) { allOnPlane = false; break; }
		if (allOnPlane) { caps.insert(f.id); continue; }
		std::vector<EditableCorner> loop;
		for (size_t k = f.loop.size(); k-- > 0;) {
			EditableCorner c = f.loop[k];
			c.vertex = twin[c.vertex];
			if (c.hasNormal) c.normal[a] = -c.normal[a];
			loop.push_back(c);
		}
		work.addFace(loop);
	}
	work.faces.erase(std::remove_if(work.faces.begin(), work.faces.end(),
		[&](const EditableFace& f) { return caps.count(f.id) != 0; }), work.faces.end());
	if (work.faces.empty()) { fail(error, "the mirror left no faces (the whole mesh lies in the mirror plane)"); return false; }
	std::set<uint32_t> used;
	for (const auto& f : work.faces) for (const auto& c : f.loop) used.insert(c.vertex);
	work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(),
		[&](const EditableVertex& v) { return !used.count(v.id); }), work.vertices.end());
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	out = std::move(work);
	return true;
}

} // namespace

bool mirrorMesh(const EditableMesh& in, const std::array<bool, 3>& axes, float mergeDistance, EditableMesh& out, std::string* error) {
	if (!std::isfinite(mergeDistance) || mergeDistance < 0.f) { fail(error, "the mirror merge distance must be a non-negative number"); return false; }
	if (!axes[0] && !axes[1] && !axes[2]) { fail(error, "the mirror modifier needs at least one axis"); return false; }
	EditableMesh current = in;
	for (int a = 0; a < 3; ++a) {
		if (!axes[a]) continue;
		EditableMesh next;
		std::string why;
		if (!mirrorOneAxis(current, a, mergeDistance, next, &why)) {
			fail(error, std::string("mirror ") + "xyz"[a] + ": " + why);
			return false;
		}
		current = std::move(next);
	}
	current.revision = nextEditableRevision();
	out = std::move(current);
	return true;
}

bool evaluateModifiers(const EditableMesh& in, const std::vector<MeshModifier>& modifiers, EditableMesh& out, std::string* error) {
	EditableMesh current = in;
	bool any = false;
	for (const auto& m : modifiers) {
		if (!m.enabled) continue;
		EditableMesh next;
		switch (m.type) {
		case MeshModifier::Type::Mirror:
			if (!mirrorMesh(current, m.axis, m.mergeDistance, next, error)) return false;
			break;
		case MeshModifier::Type::Subdivision:
			if (!subdivideMesh(current, m.levels, m.smoothShading, next, error)) return false;
			break;
		}
		current = std::move(next);
		any = true;
	}
	if (!any) { current = in; }
	out = std::move(current);
	return true;
}

namespace {

// One Catmull-Clark step. Every n-gon becomes n quads [corner', edge point, face point, previous edge point]; original vertex
// ids survive (moved), new vertices are appended. Corner UVs are averaged inside each face, so seams stay seams.
bool subdivideOnce(const EditableMesh& in, EditableMesh& out, std::string* error) {
	using Key = std::pair<uint32_t, uint32_t>;
	auto key = [](uint32_t a, uint32_t b) { return Key{ std::min(a, b), std::max(a, b) }; };
	auto pos = [&](uint32_t id) { return in.findVertex(id)->pos; };

	std::vector<std::array<double, 3>> facePoint(in.faces.size());
	std::map<Key, std::vector<size_t>> edgeFaces;
	std::map<uint32_t, std::vector<size_t>> vertexFaces;
	for (size_t fi = 0; fi < in.faces.size(); ++fi) {
		const auto& loop = in.faces[fi].loop;
		std::array<double, 3> sum{ 0, 0, 0 };
		for (size_t i = 0; i < loop.size(); ++i) {
			const float* p = pos(loop[i].vertex);
			for (int k = 0; k < 3; ++k) sum[k] += p[k];
			edgeFaces[key(loop[i].vertex, loop[(i + 1) % loop.size()].vertex)].push_back(fi);
			vertexFaces[loop[i].vertex].push_back(fi);
		}
		for (int k = 0; k < 3; ++k) facePoint[fi][k] = sum[k] / loop.size();
	}

	EditableMesh work;
	work.nextVertexId = in.nextVertexId;
	work.vertices = in.vertices;

	// Edge points: average of both ends and both face points inside the surface, the plain midpoint on a border.
	std::map<Key, uint32_t> edgePoint;
	std::map<Key, std::array<double, 3>> edgeMid;
	for (const auto& kv : edgeFaces) {
		const float* a = pos(kv.first.first);
		const float* b = pos(kv.first.second);
		std::array<double, 3> mid{ (double(a[0]) + b[0]) / 2, (double(a[1]) + b[1]) / 2, (double(a[2]) + b[2]) / 2 };
		edgeMid[kv.first] = mid;
		std::array<double, 3> p = mid;
		if (kv.second.size() == 2)
			for (int k = 0; k < 3; ++k) p[k] = (double(a[k]) + b[k] + facePoint[kv.second[0]][k] + facePoint[kv.second[1]][k]) / 4;
		edgePoint[kv.first] = work.addVertex(float(p[0]), float(p[1]), float(p[2]));
	}

	// Moved original vertices (interior rule, border crease rule, or unchanged on non-manifold spots).
	std::map<uint32_t, std::set<uint32_t>> neighbours;
	for (const auto& kv : edgeFaces) {
		neighbours[kv.first.first].insert(kv.first.second);
		neighbours[kv.first.second].insert(kv.first.first);
	}
	for (size_t i = 0; i < in.vertices.size(); ++i) {
		const uint32_t v = in.vertices[i].id;
		const float* p = in.vertices[i].pos;
		const auto& around = neighbours[v];
		std::vector<uint32_t> border;
		bool nonManifold = false;
		for (uint32_t n : around) {
			const size_t count = edgeFaces[key(v, n)].size();
			if (count == 1) border.push_back(n);
			else if (count != 2) nonManifold = true;
		}
		if (nonManifold || around.size() < 2) continue;
		std::array<double, 3> q{ p[0], p[1], p[2] };
		if (border.empty()) {
			const double n = double(around.size());
			std::array<double, 3> F{ 0, 0, 0 }, R{ 0, 0, 0 };
			for (size_t fi : vertexFaces[v]) for (int k = 0; k < 3; ++k) F[k] += facePoint[fi][k] / vertexFaces[v].size();
			for (uint32_t nb : around) for (int k = 0; k < 3; ++k) R[k] += edgeMid[key(v, nb)][k] / around.size();
			for (int k = 0; k < 3; ++k) q[k] = (F[k] + 2 * R[k] + (n - 3) * p[k]) / n;
		} else if (border.size() == 2) {
			const float* a = pos(border[0]);
			const float* b = pos(border[1]);
			for (int k = 0; k < 3; ++k) q[k] = (6.0 * p[k] + a[k] + b[k]) / 8.0;
		} else continue;
		for (int k = 0; k < 3; ++k) work.vertices[i].pos[k] = float(q[k]);
	}

	// Faces: one quad per corner.
	for (size_t fi = 0; fi < in.faces.size(); ++fi) {
		const auto& loop = in.faces[fi].loop;
		const size_t n = loop.size();
		const uint32_t fv = work.addVertex(float(facePoint[fi][0]), float(facePoint[fi][1]), float(facePoint[fi][2]));
		float faceUv[2] = { 0.f, 0.f };
		for (const auto& c : loop) { faceUv[0] += c.uv[0] / n; faceUv[1] += c.uv[1] / n; }
		for (size_t i = 0; i < n; ++i) {
			const EditableCorner& cur = loop[i];
			const EditableCorner& next = loop[(i + 1) % n];
			const EditableCorner& prev = loop[(i + n - 1) % n];
			std::vector<EditableCorner> quad(4);
			quad[0].vertex = cur.vertex;                     quad[0].uv[0] = cur.uv[0];                     quad[0].uv[1] = cur.uv[1];
			quad[1].vertex = edgePoint[key(cur.vertex, next.vertex)];
			quad[1].uv[0] = (cur.uv[0] + next.uv[0]) / 2;    quad[1].uv[1] = (cur.uv[1] + next.uv[1]) / 2;
			quad[2].vertex = fv;                             quad[2].uv[0] = faceUv[0];                     quad[2].uv[1] = faceUv[1];
			quad[3].vertex = edgePoint[key(prev.vertex, cur.vertex)];
			quad[3].uv[0] = (cur.uv[0] + prev.uv[0]) / 2;    quad[3].uv[1] = (cur.uv[1] + prev.uv[1]) / 2;
			work.addFace(quad);
		}
	}
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	out = std::move(work);
	return true;
}

} // namespace

bool subdivideMesh(const EditableMesh& in, int levels, bool smoothShading, EditableMesh& out, std::string* error) {
	if (levels < 1 || levels > 4) { fail(error, "subdivision levels must be between 1 and 4"); return false; }
	if (in.faces.empty()) { fail(error, "there is nothing to subdivide"); return false; }
	const size_t kMaxFaces = 1000000;
	size_t projected = 0;
	for (const auto& f : in.faces) projected += f.loop.size();
	for (int i = 1; i < levels; ++i) projected *= 4;
	if (projected > kMaxFaces) { fail(error, "the subdivided mesh would have more than 1,000,000 faces; use fewer levels"); return false; }
	EditableMesh current = in;
	for (int i = 0; i < levels; ++i) {
		EditableMesh next;
		if (!subdivideOnce(current, next, error)) return false;
		current = std::move(next);
	}
	if (smoothShading) {
		std::vector<uint32_t> all;
		for (const auto& f : current.faces) all.push_back(f.id);
		EditableMesh shaded;
		if (!shadeFaces(current, all, true, 180.f, shaded, error)) return false;
		current = std::move(shaded);
	}
	current.revision = nextEditableRevision();
	out = std::move(current);
	return true;
}

bool deleteElements(const EditableMesh& in, ElementKind kind, const std::vector<uint32_t>& ids, EditableMesh& out,
                    std::string* error, size_t* removedFaces, size_t* removedVertices) {
	if (ids.empty()) { fail(error, "no elements selected"); return false; }
	std::set<uint32_t> dropFaces;
	for (uint32_t id : ids) {
		switch (kind) {
		case ElementKind::Face:
			if (!in.findFace(id)) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }
			dropFaces.insert(id);
			break;
		case ElementKind::Vertex:
			if (!in.findVertex(id)) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
			for (const auto& f : in.faces)
				for (const auto& c : f.loop) if (c.vertex == id) { dropFaces.insert(f.id); break; }
			break;
		case ElementKind::Edge: {
			const EditableEdge* edge = nullptr;
			for (const auto& e : in.edges) if (e.id == id) { edge = &e; break; }
			if (!edge) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
			for (const auto& f : in.faces) {
				bool a = false, b = false;
				for (const auto& c : f.loop) { a = a || c.vertex == edge->a; b = b || c.vertex == edge->b; }
				if (a && b) {
					// Both endpoints in the face is not enough for a triangle/quad diagonal: require a consecutive pair.
					for (size_t i = 0; i < f.loop.size(); ++i) {
						const uint32_t p = f.loop[i].vertex, q = f.loop[(i + 1) % f.loop.size()].vertex;
						if ((p == edge->a && q == edge->b) || (p == edge->b && q == edge->a)) { dropFaces.insert(f.id); break; }
					}
				}
			}
			break;
		}
		}
	}
	EditableMesh work = in;
	std::set<uint32_t> touched;
	for (const auto& f : in.faces)
		if (dropFaces.count(f.id)) for (const auto& c : f.loop) touched.insert(c.vertex);
	work.faces.erase(std::remove_if(work.faces.begin(), work.faces.end(),
		[&](const EditableFace& f) { return dropFaces.count(f.id) != 0; }), work.faces.end());
	std::set<uint32_t> used;
	for (const auto& f : work.faces) for (const auto& c : f.loop) used.insert(c.vertex);
	size_t vertexCount = 0;
	work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(), [&](const EditableVertex& v) {
		const bool drop = touched.count(v.id) && !used.count(v.id);
		vertexCount += drop;
		return drop;
	}), work.vertices.end());
	work.rebuildEdges();
	if (!work.validate(error)) return false;
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (removedFaces) *removedFaces = dropFaces.size();
	if (removedVertices) *removedVertices = vertexCount;
	return true;
}

bool mergeTargetFromName(const std::string& name, MergeTarget& out) {
	std::string l = name;
	for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (l == "center" || l == "centre") { out = MergeTarget::Center; return true; }
	if (l == "first")                   { out = MergeTarget::First;  return true; }
	if (l == "last")                    { out = MergeTarget::Last;   return true; }
	return false;
}

bool mergeVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, MergeTarget target, EditableMesh& out,
                   std::string* error, size_t* removedFaces) {
	std::set<uint32_t> group(vertices.begin(), vertices.end());
	if (group.size() < 2) { fail(error, "merge needs at least two distinct vertices"); return false; }
	for (uint32_t id : group)
		if (!in.findVertex(id)) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }

	const uint32_t survivor = *group.begin();
	std::array<float, 3> position{};
	if (target == MergeTarget::Center) {
		if (!vertexCentroid(in, std::vector<uint32_t>(group.begin(), group.end()), position)) { fail(error, "cannot compute the centre"); return false; }
	} else {
		const EditableVertex* source = in.findVertex(target == MergeTarget::First ? *group.begin() : *group.rbegin());
		position = { source->pos[0], source->pos[1], source->pos[2] };
	}

	EditableMesh work = in;
	std::set<uint32_t> touched;
	std::vector<EditableFace> kept;
	size_t dropped = 0;
	for (auto& face : work.faces) {
		bool affected = false;
		for (auto& c : face.loop) if (group.count(c.vertex)) { affected = true; touched.insert(c.vertex); c.vertex = survivor; }
		if (!affected) { kept.push_back(std::move(face)); continue; }
		// Remove consecutive duplicates (cyclic); any vertex still repeated means the face is degenerate.
		std::vector<EditableCorner> loop;
		for (const auto& c : face.loop)
			if (loop.empty() || loop.back().vertex != c.vertex) loop.push_back(c);
		while (loop.size() > 1 && loop.front().vertex == loop.back().vertex) loop.pop_back();
		std::set<uint32_t> distinct;
		for (const auto& c : loop) distinct.insert(c.vertex);
		if (loop.size() < 3 || distinct.size() != loop.size()) { ++dropped; continue; }
		face.loop = std::move(loop);
		kept.push_back(std::move(face));
	}
	work.faces = std::move(kept);
	for (auto& v : work.vertices)
		if (v.id == survivor) { v.pos[0] = position[0]; v.pos[1] = position[1]; v.pos[2] = position[2]; }
	std::set<uint32_t> used;
	for (const auto& f : work.faces) for (const auto& c : f.loop) used.insert(c.vertex);
	work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(), [&](const EditableVertex& v) {
		return touched.count(v.id) && !used.count(v.id);
	}), work.vertices.end());
	work.rebuildEdges();
	if (!work.validate(error)) {
		if (error) *error = "the merge would produce an invalid mesh: " + *error;
		return false;
	}
	// The faces around the merged vertex must still form one fan; two fans meeting at a point are a non-manifold vertex.
	{
		std::vector<const EditableFace*> around;
		for (const auto& f : work.faces)
			for (const auto& c : f.loop) if (c.vertex == survivor) { around.push_back(&f); break; }
		auto neighbours = [&](const EditableFace& f, uint32_t other) {
			for (size_t i = 0; i < f.loop.size(); ++i) {
				const uint32_t p = f.loop[i].vertex, q = f.loop[(i + 1) % f.loop.size()].vertex;
				if (p == survivor && q == other) return true;
				if (q == survivor && p == other) return true;
			}
			return false;
		};
		if (!around.empty()) {
			std::set<const EditableFace*> seen{ around.front() };
			std::vector<const EditableFace*> stack{ around.front() };
			while (!stack.empty()) {
				const EditableFace* f = stack.back(); stack.pop_back();
				for (const auto& c : f->loop) {
					if (c.vertex == survivor || !neighbours(*f, c.vertex)) continue;
					for (const EditableFace* g : around)
						if (neighbours(*g, c.vertex) && seen.insert(g).second) stack.push_back(g);
				}
			}
			if (seen.size() != around.size()) { fail(error, "the merge would join two separate fans at one vertex (non-manifold); merge neighbouring vertices instead"); return false; }
		}
	}
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (removedFaces) *removedFaces = dropped;
	return true;
}

bool mergeByDistance(const EditableMesh& in, const std::vector<uint32_t>& vertices, float distance, EditableMesh& out,
                     std::string* error, size_t* mergedVertices, size_t* skippedGroups) {
	if (!std::isfinite(distance) || distance < 0.f) { fail(error, "distance must be a non-negative number"); return false; }
	std::vector<const EditableVertex*> candidates;
	if (vertices.empty()) {
		for (const auto& v : in.vertices) candidates.push_back(&v);
	} else {
		for (uint32_t id : std::set<uint32_t>(vertices.begin(), vertices.end())) {
			const EditableVertex* v = in.findVertex(id);
			if (!v) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
			candidates.push_back(v);
		}
	}
	// Union-find over candidates closer than `distance` (a grid of cells `distance` wide limits the comparisons to neighbours).
	std::vector<size_t> parent(candidates.size());
	for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
	auto root = [&](size_t i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
	const float cell = std::max(distance, 1.0e-6f);
	using Cell = std::array<long long, 3>;
	std::map<Cell, std::vector<size_t>> grid;
	auto cellOf = [&](const EditableVertex& v) { return Cell{ (long long)std::floor(v.pos[0] / cell), (long long)std::floor(v.pos[1] / cell), (long long)std::floor(v.pos[2] / cell) }; };
	for (size_t i = 0; i < candidates.size(); ++i) {
		const Cell c = cellOf(*candidates[i]);
		for (long long dx = -1; dx <= 1; ++dx) for (long long dy = -1; dy <= 1; ++dy) for (long long dz = -1; dz <= 1; ++dz) {
			const auto it = grid.find(Cell{ c[0] + dx, c[1] + dy, c[2] + dz });
			if (it == grid.end()) continue;
			for (size_t j : it->second) {
				const float ex = candidates[i]->pos[0] - candidates[j]->pos[0], ey = candidates[i]->pos[1] - candidates[j]->pos[1], ez = candidates[i]->pos[2] - candidates[j]->pos[2];
				if (ex * ex + ey * ey + ez * ez <= distance * distance) parent[root(i)] = root(j);
			}
		}
		grid[c].push_back(i);
	}
	std::map<size_t, std::vector<uint32_t>> groups;
	for (size_t i = 0; i < candidates.size(); ++i) groups[root(i)].push_back(candidates[i]->id);

	EditableMesh work = in;
	size_t merged = 0, skipped = 0;
	for (auto& entry : groups) {
		if (entry.second.size() < 2) continue;
		EditableMesh next;
		std::string why;
		// A group whose merge would make the mesh invalid (e.g. non-manifold) is left alone.
		if (mergeVertices(work, entry.second, MergeTarget::Center, next, &why)) { work = std::move(next); merged += entry.second.size(); }
		else ++skipped;
	}
	if (merged > 0 && work.faces.empty()) { fail(error, "merging that many vertices would leave no faces"); return false; }
	if (merged == 0) {
		fail(error, skipped ? "the vertices within the distance cannot be merged without making the mesh invalid" : "no vertices are within that distance of each other");
		return false;
	}
	out = std::move(work);
	if (mergedVertices) *mergedVertices = merged;
	if (skippedGroups) *skippedGroups = skipped;
	return true;
}

bool bridgeEdges(const EditableMesh& in, const std::vector<uint32_t>& edgeIds, EditableMesh& out, std::string* error,
                 std::vector<uint32_t>* newFaces) {
	const std::set<uint32_t> ids(edgeIds.begin(), edgeIds.end());
	if (ids.size() < 2) { fail(error, "bridge needs the edges of two loops (select more edges)"); return false; }

	// Every directed use of an edge by a face, keyed by the unordered vertex pair.
	struct Use { size_t face, corner; };
	std::map<std::pair<uint32_t, uint32_t>, std::vector<Use>> uses;
	for (size_t fi = 0; fi < in.faces.size(); ++fi) {
		const auto& loop = in.faces[fi].loop;
		for (size_t ci = 0; ci < loop.size(); ++ci) {
			const uint32_t p = loop[ci].vertex, q = loop[(ci + 1) % loop.size()].vertex;
			uses[{ std::min(p, q), std::max(p, q) }].push_back({ fi, ci });
		}
	}

	// The selected boundary edges, directed the way their owning face runs.
	std::map<uint32_t, uint32_t> next, prev;
	std::map<uint32_t, EditableCorner> cornerOut, cornerIn;
	for (uint32_t id : ids) {
		const EditableEdge* edge = nullptr;
		for (const auto& e : in.edges) if (e.id == id) { edge = &e; break; }
		if (!edge) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
		const auto it = uses.find({ edge->a, edge->b });
		if (it == uses.end() || it->second.size() != 1) { fail(error, "edge " + std::to_string(id) + " is not a boundary edge (bridge joins open edges)"); return false; }
		const auto& loop = in.faces[it->second[0].face].loop;
		const size_t ci = it->second[0].corner;
		const EditableCorner& from = loop[ci];
		const EditableCorner& to = loop[(ci + 1) % loop.size()];
		if (next.count(from.vertex) || prev.count(to.vertex)) { fail(error, "the selected edges branch at a vertex; select two simple loops"); return false; }
		next[from.vertex] = to.vertex;
		prev[to.vertex] = from.vertex;
		cornerOut[from.vertex] = from;
		cornerIn[to.vertex] = to;
	}
	auto cornerAt = [&](uint32_t vertex) {
		const auto o = cornerOut.find(vertex);
		EditableCorner c = o != cornerOut.end() ? o->second : cornerIn.at(vertex);
		c.hasNormal = false;
		c.normal[0] = c.normal[1] = c.normal[2] = 0.f;
		return c;
	};

	struct Chain { std::vector<uint32_t> verts; bool closed = false; };
	std::vector<Chain> chains;
	std::set<uint32_t> visited;
	for (const auto& entry : next) {
		if (prev.count(entry.first)) continue;                 // not the start of an open chain
		Chain chain;
		for (uint32_t v = entry.first;; v = next.at(v)) {
			chain.verts.push_back(v);
			visited.insert(v);
			if (!next.count(v)) break;
		}
		chains.push_back(std::move(chain));
	}
	for (const auto& entry : next) {
		if (visited.count(entry.first)) continue;              // what remains are closed loops
		Chain chain;
		chain.closed = true;
		uint32_t v = entry.first;
		do { chain.verts.push_back(v); visited.insert(v); v = next.at(v); } while (v != entry.first);
		chains.push_back(std::move(chain));
	}
	if (chains.size() != 2) { fail(error, "the selected edges form " + std::to_string(chains.size()) + " chain(s); bridge needs exactly two"); return false; }
	if (chains[0].closed != chains[1].closed) { fail(error, "bridge needs two closed loops or two open chains, not one of each"); return false; }
	if (chains[0].verts.size() != chains[1].verts.size()) {
		fail(error, "the two chains have different lengths (" + std::to_string(chains[0].verts.size()) + " and " + std::to_string(chains[1].verts.size()) + " vertices)");
		return false;
	}
	for (uint32_t v : chains[0].verts)
		if (std::find(chains[1].verts.begin(), chains[1].verts.end(), v) != chains[1].verts.end()) { fail(error, "the two chains share a vertex"); return false; }

	const std::vector<uint32_t>& a = chains[0].verts;
	std::vector<uint32_t> b = chains[1].verts;
	std::reverse(b.begin(), b.end());                           // the new quads run against the owning faces, so one side is reversed
	const size_t count = a.size();
	size_t shift = 0;
	if (chains[0].closed) {
		double best = 1e300;
		for (size_t s = 0; s < count; ++s) {
			double sum = 0;
			for (size_t i = 0; i < count; ++i) {
				const EditableVertex* p = in.findVertex(a[i]);
				const EditableVertex* q = in.findVertex(b[(i + s) % count]);
				for (int k = 0; k < 3; ++k) { const double d = double(p->pos[k]) - q->pos[k]; sum += d * d; }
			}
			if (sum < best) { best = sum; shift = s; }
		}
	}

	EditableMesh work = in;
	std::vector<uint32_t> created;
	const size_t quads = chains[0].closed ? count : count - 1;
	for (size_t i = 0; i < quads; ++i) {
		const uint32_t a0 = a[i], a1 = a[(i + 1) % count];
		const uint32_t b0 = b[(i + shift) % count], b1 = b[(i + shift + 1) % count];
		created.push_back(work.addFace({ cornerAt(a1), cornerAt(a0), cornerAt(b0), cornerAt(b1) }));
	}
	work.rebuildEdges();
	if (!work.validate(error)) {
		if (error) *error = "the bridge would produce an invalid mesh: " + *error;
		return false;
	}
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (newFaces) *newFaces = std::move(created);
	return true;
}

bool bridgeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, EditableMesh& out, std::string* error,
                 std::vector<uint32_t>* newFaces) {
	const std::set<uint32_t> ids(faceIds.begin(), faceIds.end());
	if (ids.size() != 2) { fail(error, "bridge between faces needs exactly two faces"); return false; }
	std::vector<const EditableFace*> faces;
	for (uint32_t id : ids) {
		const EditableFace* f = in.findFace(id);
		if (!f) { fail(error, "face " + std::to_string(id) + " does not exist"); return false; }
		faces.push_back(f);
	}
	if (faces[0]->loop.size() != faces[1]->loop.size()) { fail(error, "the two faces have different corner counts"); return false; }
	for (const auto& c : faces[0]->loop)
		for (const auto& d : faces[1]->loop)
			if (c.vertex == d.vertex) { fail(error, "the two faces share a vertex"); return false; }

	EditableMesh holes = in;
	holes.faces.erase(std::remove_if(holes.faces.begin(), holes.faces.end(),
		[&](const EditableFace& f) { return ids.count(f.id) != 0; }), holes.faces.end());
	holes.rebuildEdges();
	std::vector<uint32_t> edges;
	for (const EditableFace* f : faces)
		for (size_t i = 0; i < f->loop.size(); ++i) {
			const EditableEdge* e = holes.findEdge(f->loop[i].vertex, f->loop[(i + 1) % f->loop.size()].vertex);
			if (!e) { fail(error, "every edge of the faces needs a neighbouring face, so that the hole has an edge to bridge"); return false; }
			edges.push_back(e->id);
		}
	return bridgeEdges(holes, edges, out, error, newFaces);
}

bool dissolveElements(const EditableMesh& in, ElementKind kind, const std::vector<uint32_t>& ids, EditableMesh& out,
                      std::string* error, std::vector<uint32_t>* mergedFaces, size_t* skipped) {
	if (kind == ElementKind::Face) { fail(error, "dissolve works on edges or vertices (faces: use delete)"); return false; }
	if (ids.empty()) { fail(error, "no elements selected"); return false; }
	EditableMesh work = in;
	std::set<uint32_t> merged;
	size_t skippedCount = 0, done = 0;

	// Rotates a face's loop so that it starts at the corner of `vertex`.
	auto rotated = [](const EditableFace& f, uint32_t vertex, std::vector<EditableCorner>& loop) {
		loop.clear();
		size_t start = f.loop.size();
		for (size_t i = 0; i < f.loop.size(); ++i) if (f.loop[i].vertex == vertex) start = i;
		if (start == f.loop.size()) return false;
		for (size_t i = 0; i < f.loop.size(); ++i) loop.push_back(f.loop[(start + i) % f.loop.size()]);
		return true;
	};
	auto eraseFace = [&](uint32_t id) {
		work.faces.erase(std::remove_if(work.faces.begin(), work.faces.end(), [&](const EditableFace& f) { return f.id == id; }), work.faces.end());
		merged.erase(id);
	};
	auto findFace = [&](uint32_t id) -> EditableFace* {
		for (auto& f : work.faces) if (f.id == id) return &f;
		return nullptr;
	};

	std::vector<std::pair<uint32_t, uint32_t>> pairs;   // edges as vertex pairs (face ids change, vertex ids do not)
	if (kind == ElementKind::Edge) {
		for (uint32_t id : std::set<uint32_t>(ids.begin(), ids.end())) {
			const EditableEdge* e = nullptr;
			for (const auto& c : in.edges) if (c.id == id) { e = &c; break; }
			if (!e) { fail(error, "edge " + std::to_string(id) + " does not exist"); return false; }
			pairs.push_back({ e->a, e->b });
		}
		for (const auto& p : pairs) {
			std::vector<uint32_t> users;
			for (const auto& f : work.faces)
				for (size_t i = 0; i < f.loop.size(); ++i) {
					const uint32_t a = f.loop[i].vertex, b = f.loop[(i + 1) % f.loop.size()].vertex;
					if ((a == p.first && b == p.second) || (a == p.second && b == p.first)) users.push_back(f.id);
				}
			if (users.size() != 2) { ++skippedCount; continue; }
			EditableFace* f = findFace(users[0]);
			EditableFace* g = findFace(users[1]);
			if (f->loop.size() + g->loop.size() - 2 > 4) { ++skippedCount; continue; }
			// f runs a->b, g runs b->a (opposite, as validated): f rotated to start at b, g rotated to start at a.
			uint32_t a = p.first, b = p.second;
			for (size_t i = 0; i < f->loop.size(); ++i)
				if (f->loop[i].vertex == p.first && f->loop[(i + 1) % f->loop.size()].vertex == p.second) { a = p.first; b = p.second; break; }
				else if (f->loop[i].vertex == p.second && f->loop[(i + 1) % f->loop.size()].vertex == p.first) { a = p.second; b = p.first; break; }
			std::vector<EditableCorner> fl, gl;
			if (!rotated(*f, b, fl) || !rotated(*g, a, gl)) { ++skippedCount; continue; }
			// fl = [b, ..., a]; gl = [a, ..., b]: keep fl whole, then gl without its first (a) and last (b) corner.
			std::vector<EditableCorner> loop = fl;
			for (size_t i = 1; i + 1 < gl.size(); ++i) loop.push_back(gl[i]);
			const uint32_t keep = f->id;
			f->loop = std::move(loop);
			eraseFace(users[1]);
			merged.insert(keep);
			++done;
		}
	} else {
		for (uint32_t id : std::set<uint32_t>(ids.begin(), ids.end())) {
			if (!work.findVertex(id)) {
				if (!in.findVertex(id)) { fail(error, "vertex " + std::to_string(id) + " does not exist"); return false; }
				++skippedCount;   // already removed by an earlier dissolve
				continue;
			}
			// The fan of faces around the vertex, each as [v, p1, ..., pm].
			std::vector<std::pair<uint32_t, std::vector<EditableCorner>>> fan;
			for (const auto& f : work.faces) {
				std::vector<EditableCorner> loop;
				if (rotated(f, id, loop)) fan.push_back({ f.id, std::move(loop) });
			}
			if (fan.size() < 3) { ++skippedCount; continue; }   // a border vertex, or a vertex of just two faces
			// Chain the faces: the face after `i` starts its first neighbour where face i ends.
			std::vector<size_t> order{ 0 };
			std::set<size_t> used{ 0 };
			bool closed = false;
			while (true) {
				const uint32_t tail = fan[order.back()].second.back().vertex;
				size_t nextFace = fan.size();
				for (size_t j = 0; j < fan.size(); ++j)
					if (!used.count(j) && fan[j].second[1].vertex == tail) { nextFace = j; break; }
				if (nextFace == fan.size()) { closed = fan[order.front()].second[1].vertex == tail; break; }
				order.push_back(nextFace);
				used.insert(nextFace);
			}
			if (!closed || order.size() != fan.size()) { ++skippedCount; continue; }
			std::vector<EditableCorner> loop;
			for (size_t i : order) {
				const auto& l = fan[i].second;
				for (size_t k = 1; k + 1 < l.size(); ++k) loop.push_back(l[k]);
			}
			if (loop.size() < 3 || loop.size() > 4) { ++skippedCount; continue; }
			const uint32_t keep = fan[order.front()].first;
			findFace(keep)->loop = std::move(loop);
			for (size_t i = 1; i < order.size(); ++i) eraseFace(fan[order[i]].first);
			merged.insert(keep);
			work.vertices.erase(std::remove_if(work.vertices.begin(), work.vertices.end(), [&](const EditableVertex& v) { return v.id == id; }), work.vertices.end());
			++done;
		}
	}
	if (done == 0) { fail(error, "nothing could be dissolved (border elements, or the merged face would have more than four corners)"); return false; }
	work.rebuildEdges();
	if (!work.validate(error)) {
		if (error) *error = "the dissolve would produce an invalid mesh: " + *error;
		return false;
	}
	work.revision = nextEditableRevision();
	out = std::move(work);
	if (mergedFaces) mergedFaces->assign(merged.begin(), merged.end());
	if (skipped) *skipped = skippedCount;
	return true;
}

bool worldVectorToLocal(const std::array<float, 3>& rotationEulerDeg, const std::array<float, 3>& scale,
                        const std::array<float, 3>& world, std::array<float, 3>& local, std::string* error) {
	for (float s : scale)
		if (!std::isfinite(s) || std::fabs(s) < 1e-12f) {
			fail(error, "the object has a zero scale on an axis, so a world-space offset cannot be applied; use space=local or fix the scale");
			return false;
		}
	const Mat3 r = rotationYXZ(rotationEulerDeg);
	for (int i = 0; i < 3; ++i) {
		// (R^T * world)[i] = sum_j R[j][i] * world[j]
		const double v = r[0][i] * world[0] + r[1][i] * world[1] + r[2][i] * world[2];
		local[i] = static_cast<float>(v / scale[i]);
	}
	return true;
}

} // namespace Phantom::MeshEdit
