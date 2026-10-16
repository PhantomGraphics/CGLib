#include "EditableMeshConvert.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <string>

// Conversions between the render mesh (Graphics::MeshData: one vertex per corner, triangles) and EditableMesh (logical
// topology). Moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2).
namespace Phantom::MeshEdit {

using Graphics::MeshData;
using Graphics::MeshVertex;

namespace {

bool finite3(const float* v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

void fail(std::string* error, const std::string& message) {
	if (error) *error = message;
}

}  // namespace

bool makeEditableFromMeshData(const MeshData& src, EditableMesh& out, std::string* error, int* droppedTriangles) {
	if (droppedTriangles) *droppedTriangles = 0;
	if (src.indices.empty() || src.indices.size() % 3 != 0) { fail(error, "the mesh has no triangles"); return false; }
	EditableMesh m;
	// Weld positions closer than a tiny fraction of the model size (parametric primitives close their seams with
	// float noise, e.g. cos(2*pi) != cos(0)); grid cells of that size, 27 neighbours searched.
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	for (const auto& sv : src.vertices) {
		if (!finite3(sv.pos)) { fail(error, "the mesh has a non-finite vertex"); return false; }
		for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], sv.pos[k]); hi[k] = std::max(hi[k], sv.pos[k]); }
	}
	const float diag = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
	const float eps = std::max(diag * 1e-5f, 1e-12f);
	std::map<std::array<long long, 3>, std::vector<uint32_t>> grid;
	std::vector<uint32_t> vertexOf(src.vertices.size(), 0);
	for (size_t i = 0; i < src.vertices.size(); ++i) {
		const auto& p = src.vertices[i].pos;
		std::array<long long, 3> cell;
		for (int k = 0; k < 3; ++k) cell[k] = static_cast<long long>(std::floor(p[k] / eps));
		bool found = false;
		for (int dx = -1; dx <= 1 && !found; ++dx) for (int dy = -1; dy <= 1 && !found; ++dy) for (int dz = -1; dz <= 1 && !found; ++dz) {
			auto it = grid.find({ cell[0] + dx, cell[1] + dy, cell[2] + dz });
			if (it == grid.end()) continue;
			for (uint32_t id : it->second) {
				const EditableVertex* q = m.findVertex(id);
				const float d0 = q->pos[0] - p[0], d1 = q->pos[1] - p[1], d2 = q->pos[2] - p[2];
				if (d0 * d0 + d1 * d1 + d2 * d2 <= eps * eps) { vertexOf[i] = id; found = true; break; }
			}
		}
		if (!found) { vertexOf[i] = m.addVertex(p[0], p[1], p[2]); grid[cell].push_back(vertexOf[i]); }
	}
	// Which way do the source triangles wind relative to their own normals? Counter-clockwise from outside (along) is
	// the norm; a source wound the other way is reversed so the loops are always counter-clockwise from outside.
	long along = 0, against = 0;
	for (size_t t = 0; t + 2 < src.indices.size(); t += 3) {
		const auto& a = src.vertices[src.indices[t]]; const auto& b = src.vertices[src.indices[t + 1]]; const auto& c = src.vertices[src.indices[t + 2]];
		float e1[3], e2[3], n[3];
		for (int k = 0; k < 3; ++k) { e1[k] = b.pos[k] - a.pos[k]; e2[k] = c.pos[k] - a.pos[k]; }
		n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
		const float d = n[0] * (a.nrm[0] + b.nrm[0] + c.nrm[0]) + n[1] * (a.nrm[1] + b.nrm[1] + c.nrm[1]) + n[2] * (a.nrm[2] + b.nrm[2] + c.nrm[2]);
		if (d > 0.f) ++along; else if (d < 0.f) ++against;
	}
	const bool sourceIsCcw = along > against;
	int dropped = 0;
	for (size_t t = 0; t + 2 < src.indices.size(); t += 3) {
		uint32_t idx[3] = { src.indices[t], src.indices[t + 1], src.indices[t + 2] };
		if (idx[0] >= src.vertices.size() || idx[1] >= src.vertices.size() || idx[2] >= src.vertices.size()) { fail(error, "an index is out of range"); return false; }
		const uint32_t v[3] = { vertexOf[idx[0]], vertexOf[idx[1]], vertexOf[idx[2]] };
		if (v[0] == v[1] || v[1] == v[2] || v[0] == v[2]) { ++dropped; continue; }
		// Loop order: counter-clockwise from outside, which is also the render order. A source wound the other way round
		// has its triangle reversed.
		static const int reversedOrder[3] = { 0, 2, 1 }, sameOrder[3] = { 0, 1, 2 };
		const int* order = sourceIsCcw ? sameOrder : reversedOrder;
		std::vector<EditableCorner> loop(3);
		for (int k = 0; k < 3; ++k) {
			const MeshVertex& sv = src.vertices[idx[order[k]]];
			loop[k].vertex = v[order[k]];
			loop[k].uv[0] = sv.uv[0]; loop[k].uv[1] = sv.uv[1];
		}
		const uint32_t id = m.addFace(loop);
		// Keep the source normal on corners that are not simply the face normal (smooth or hand-set shading).
		EditableFace& f = m.faces.back();
		float fn[3];
		if (newellNormal(m, f, fn) > 0.f) {
			const float len = std::sqrt(fn[0] * fn[0] + fn[1] * fn[1] + fn[2] * fn[2]);
			for (int k = 0; k < 3; ++k) {
				const MeshVertex& sv = src.vertices[idx[order[k]]];
				const float dot = (fn[0] * sv.nrm[0] + fn[1] * sv.nrm[1] + fn[2] * sv.nrm[2]) / len;
				const float nl = std::sqrt(sv.nrm[0] * sv.nrm[0] + sv.nrm[1] * sv.nrm[1] + sv.nrm[2] * sv.nrm[2]);
				if (nl > 0.f && dot / nl < 0.999f) {
					f.loop[k].hasNormal = true;
					for (int a = 0; a < 3; ++a) f.loop[k].normal[a] = sv.nrm[a];
				}
			}
		}
		(void)id;
	}
	if (m.faces.empty()) { fail(error, "every triangle is degenerate"); return false; }
	// Drop vertices no face uses (their triangles were degenerate), keeping ids increasing.
	std::set<uint32_t> used;
	for (const auto& f : m.faces) for (const auto& c : f.loop) used.insert(c.vertex);
	m.vertices.erase(std::remove_if(m.vertices.begin(), m.vertices.end(), [&](const EditableVertex& v) { return !used.count(v.id); }), m.vertices.end());
	m.rebuildEdges();
	m.revision = nextEditableRevision();
	std::string why;
	if (!m.validate(&why)) { fail(error, "the mesh is not a clean manifold surface (" + why + ")"); return false; }
	if (droppedTriangles) *droppedTriangles = dropped;
	out = std::move(m);
	return true;
}

bool editableToMeshData(const EditableMesh& mesh, MeshData& out, std::vector<uint32_t>* triangleFace, std::string* error, bool validated) {
	if (!validated && !mesh.validate(error)) return false;
	out = MeshData{};
	if (triangleFace) triangleFace->clear();
	for (const auto& f : mesh.faces) {
		float n[3];
		const float twiceArea = newellNormal(mesh, f, n);
		const float inv = twiceArea > 0.f ? 1.f / twiceArea : 0.f;
		const uint32_t base = static_cast<uint32_t>(out.vertices.size());
		for (const auto& c : f.loop) {
			const EditableVertex* v = mesh.findVertex(c.vertex);
			MeshVertex mv{};
			mv.pos[0] = v->pos[0]; mv.pos[1] = v->pos[1]; mv.pos[2] = v->pos[2];
			for (int k = 0; k < 3; ++k) mv.nrm[k] = c.hasNormal ? c.normal[k] : n[k] * inv;
			mv.uv[0] = c.uv[0]; mv.uv[1] = c.uv[1];
			out.vertices.push_back(mv);
		}
		const uint32_t count = static_cast<uint32_t>(f.loop.size());
		for (uint32_t k = 1; k + 1 < count; ++k) {
			// (0,k,k+1) is counter-clockwise from outside, which is what the renderer treats as a front face.
			out.indices.push_back(base);
			out.indices.push_back(base + k);
			out.indices.push_back(base + k + 1);
			if (triangleFace) triangleFace->push_back(f.id);
		}
	}
	return true;
}

} // namespace Phantom::MeshEdit
