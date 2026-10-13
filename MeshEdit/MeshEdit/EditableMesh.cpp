#include "EditableMesh.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace Phantom::MeshEdit {

namespace {

bool finite3(const float* v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

void fail(std::string* error, const std::string& message) {
	if (error) *error = message;
}

std::pair<uint32_t, uint32_t> undirected(uint32_t a, uint32_t b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); }

} // namespace

// Newell's method: for a planar loop this is the outward normal scaled by twice the area; also valid for slightly
// non-planar quads. Returns its length (twice the area).
float newellNormal(const EditableMesh& m, const EditableFace& f, float n[3]) {
	n[0] = n[1] = n[2] = 0.f;
	const size_t count = f.loop.size();
	for (size_t i = 0; i < count; ++i) {
		const EditableVertex* p = m.findVertex(f.loop[i].vertex);
		const EditableVertex* q = m.findVertex(f.loop[(i + 1) % count].vertex);
		if (!p || !q) return 0.f;
		n[0] += (p->pos[1] - q->pos[1]) * (p->pos[2] + q->pos[2]);
		n[1] += (p->pos[2] - q->pos[2]) * (p->pos[0] + q->pos[0]);
		n[2] += (p->pos[0] - q->pos[0]) * (p->pos[1] + q->pos[1]);
	}
	return std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

const EditableVertex* EditableMesh::findVertex(uint32_t id) const {
	// Vertices are appended in id order and may only be removed, so ids are sorted: binary search.
	auto it = std::lower_bound(vertices.begin(), vertices.end(), id,
	                           [](const EditableVertex& v, uint32_t value) { return v.id < value; });
	return it != vertices.end() && it->id == id ? &*it : nullptr;
}

const EditableFace* EditableMesh::findFace(uint32_t id) const {
	auto it = std::lower_bound(faces.begin(), faces.end(), id,
	                           [](const EditableFace& f, uint32_t value) { return f.id < value; });
	return it != faces.end() && it->id == id ? &*it : nullptr;
}

const EditableEdge* EditableMesh::findEdge(uint32_t a, uint32_t b) const {
	const auto key = undirected(a, b);
	for (const auto& e : edges)
		if (e.a == key.first && e.b == key.second) return &e;
	return nullptr;
}

uint32_t EditableMesh::addVertex(float x, float y, float z) {
	EditableVertex v;
	v.id = nextVertexId++;
	v.pos[0] = x; v.pos[1] = y; v.pos[2] = z;
	vertices.push_back(v);
	return v.id;
}

uint32_t EditableMesh::addFace(const std::vector<EditableCorner>& loop) {
	EditableFace f;
	f.id = nextFaceId++;
	f.loop = loop;
	faces.push_back(std::move(f));
	return faces.back().id;
}

void EditableMesh::rebuildEdges() {
	std::map<std::pair<uint32_t, uint32_t>, uint32_t> known;
	for (const auto& e : edges) known[{ e.a, e.b }] = e.id;

	std::vector<EditableEdge> rebuilt;
	std::set<std::pair<uint32_t, uint32_t>> seen;
	for (const auto& f : faces) {
		const size_t count = f.loop.size();
		for (size_t i = 0; i < count; ++i) {
			const auto key = undirected(f.loop[i].vertex, f.loop[(i + 1) % count].vertex);
			if (!seen.insert(key).second) continue;
			EditableEdge e;
			e.a = key.first; e.b = key.second;
			auto it = known.find(key);
			e.id = it != known.end() ? it->second : nextEdgeId++;
			rebuilt.push_back(e);
		}
	}
	edges = std::move(rebuilt);
}

bool EditableMesh::validate(std::string* error) const {
	std::set<uint32_t> vertexIds;
	uint32_t previous = 0;
	bool first = true;
	for (const auto& v : vertices) {
		if (!finite3(v.pos)) { fail(error, "vertex " + std::to_string(v.id) + " has a non-finite position"); return false; }
		if (v.id >= nextVertexId) { fail(error, "vertex id " + std::to_string(v.id) + " is not below nextVertexId"); return false; }
		if (!first && v.id <= previous) { fail(error, "vertex ids are not strictly increasing at " + std::to_string(v.id)); return false; }
		previous = v.id; first = false;
		vertexIds.insert(v.id);
	}

	std::map<std::pair<uint32_t, uint32_t>, int> undirectedUse;
	std::set<std::pair<uint32_t, uint32_t>> directedUse;
	first = true;
	for (const auto& f : faces) {
		const std::string label = "face " + std::to_string(f.id);
		if (f.id >= nextFaceId) { fail(error, label + " id is not below nextFaceId"); return false; }
		if (!first && f.id <= previous) { fail(error, "face ids are not strictly increasing at " + std::to_string(f.id)); return false; }
		previous = f.id; first = false;
		if (f.loop.size() < 3 || f.loop.size() > 4) { fail(error, label + " must be a triangle or a quad"); return false; }
		std::set<uint32_t> inLoop;
		for (const auto& c : f.loop) {
			if (!vertexIds.count(c.vertex)) { fail(error, label + " references missing vertex " + std::to_string(c.vertex)); return false; }
			if (!inLoop.insert(c.vertex).second) { fail(error, label + " repeats vertex " + std::to_string(c.vertex)); return false; }
			if (!std::isfinite(c.uv[0]) || !std::isfinite(c.uv[1])) { fail(error, label + " has a non-finite UV"); return false; }
			if (c.hasNormal && !finite3(c.normal)) { fail(error, label + " has a non-finite normal"); return false; }
		}
		float n[3];
		if (!(newellNormal(*this, f, n) > 1e-12f)) { fail(error, label + " is degenerate (zero area)"); return false; }

		const size_t count = f.loop.size();
		for (size_t i = 0; i < count; ++i) {
			const uint32_t a = f.loop[i].vertex, b = f.loop[(i + 1) % count].vertex;
			if (!directedUse.insert({ a, b }).second) {
				fail(error, "edge " + std::to_string(a) + "->" + std::to_string(b) + " is used twice in the same direction (inconsistent winding)");
				return false;
			}
			if (++undirectedUse[undirected(a, b)] > 2) {
				fail(error, "edge " + std::to_string(a) + "-" + std::to_string(b) + " is shared by more than two faces (non-manifold)");
				return false;
			}
		}
	}

	// The edge table must be exactly the derived edge set with unique ids.
	std::set<uint32_t> edgeIds;
	std::set<std::pair<uint32_t, uint32_t>> tableEdges;
	for (const auto& e : edges) {
		if (e.a >= e.b) { fail(error, "edge " + std::to_string(e.id) + " is not stored with a < b"); return false; }
		if (e.id >= nextEdgeId) { fail(error, "edge id " + std::to_string(e.id) + " is not below nextEdgeId"); return false; }
		if (!edgeIds.insert(e.id).second) { fail(error, "duplicate edge id " + std::to_string(e.id)); return false; }
		if (!tableEdges.insert({ e.a, e.b }).second) { fail(error, "duplicate edge " + std::to_string(e.a) + "-" + std::to_string(e.b)); return false; }
	}
	if (tableEdges.size() != undirectedUse.size()) { fail(error, "edge table does not match the faces"); return false; }
	for (const auto& kv : undirectedUse)
		if (!tableEdges.count(kv.first)) { fail(error, "edge table is missing an edge used by a face"); return false; }
	return true;
}

bool EditableMesh::validateGeometry(std::string* error) const {
	for (const auto& v : vertices)
		if (!finite3(v.pos)) { fail(error, "vertex " + std::to_string(v.id) + " has a non-finite position"); return false; }
	for (const auto& f : faces) {
		float n[3];
		if (!(newellNormal(*this, f, n) > 1e-12f)) { fail(error, "face " + std::to_string(f.id) + " is degenerate (zero area)"); return false; }
	}
	return true;
}

uint64_t nextEditableRevision() {
	static std::atomic<uint64_t> counter{ 0 };
	return ++counter;
}

EditableMesh makeEditableBox(float h) {
	EditableMesh m;
	m.revision = nextEditableRevision();
	// 0:(-,-,-) 1:(+,-,-) 2:(+,+,-) 3:(-,+,-) 4:(-,-,+) 5:(+,-,+) 6:(+,+,+) 7:(-,+,+)
	m.addVertex(-h, -h, -h); m.addVertex( h, -h, -h); m.addVertex( h,  h, -h); m.addVertex(-h,  h, -h);
	m.addVertex(-h, -h,  h); m.addVertex( h, -h,  h); m.addVertex( h,  h,  h); m.addVertex(-h,  h,  h);

	// Each loop is counter-clockwise seen from outside: +X, -X, +Y, -Y, +Z, -Z (same order as buildBox).
	static const uint32_t loops[6][4] = {
		{ 1, 2, 6, 5 }, { 0, 4, 7, 3 }, { 3, 7, 6, 2 }, { 0, 1, 5, 4 }, { 4, 5, 6, 7 }, { 0, 3, 2, 1 },
	};
	static const float uvs[4][2] = { { 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };
	for (const auto& l : loops) {
		std::vector<EditableCorner> loop(4);
		for (int k = 0; k < 4; ++k) {
			loop[k].vertex = l[k];
			loop[k].uv[0] = uvs[k][0]; loop[k].uv[1] = uvs[k][1];
		}
		m.addFace(loop);
	}
	m.rebuildEdges();
	return m;
}

EditableMesh makeEditablePlane(float h, int divs) {
	EditableMesh m;
	m.revision = nextEditableRevision();
	if (divs < 1) divs = 1;
	const float step = (2.f * h) / divs;
	for (int i = 0; i <= divs; ++i)          // i: z index
		for (int j = 0; j <= divs; ++j)      // j: x index
			m.addVertex(-h + j * step, 0.f, -h + i * step);
	const int w = divs + 1;
	for (int i = 0; i < divs; ++i) {
		for (int j = 0; j < divs; ++j) {
			// (x0,z0) -> (x0,z1) -> (x1,z1) -> (x1,z0): counter-clockwise seen from +Y.
			const int idx[4] = { i * w + j, (i + 1) * w + j, (i + 1) * w + j + 1, i * w + j + 1 };
			const int ui[4] = { j, j, j + 1, j + 1 }, vi[4] = { i, i + 1, i + 1, i };
			std::vector<EditableCorner> loop(4);
			for (int k = 0; k < 4; ++k) {
				loop[k].vertex = static_cast<uint32_t>(idx[k]);
				loop[k].uv[0] = static_cast<float>(ui[k]) / divs;
				loop[k].uv[1] = static_cast<float>(vi[k]) / divs;
			}
			m.addFace(loop);
		}
	}
	m.rebuildEdges();
	return m;
}

} // namespace Phantom::MeshEdit
