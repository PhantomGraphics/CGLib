#pragma once
// EditableMesh: the editing master of a mesh (docs/todo/PLAN_phantomstudio_mesh_editing.md section 3.2).
//
// Unlike a render mesh (vertices + triangles), this keeps LOGICAL topology: shared vertices, polygon
// faces (triangles / quads) and per-corner attributes (UV, optional normal). A box is 8 vertices / 12 edges /
// 6 quads; the 24 render vertices and the diagonal edges only exist after triangulation.
//
// Pure data + math: no Vulkan, no scene types, no render-mesh types, no exceptions. Persistent copies are shared as
// shared_ptr<const EditableMesh>; an edit validates a working copy and installs a new pointer.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Phantom::MeshEdit {

	struct EditableVertex {
		uint32_t id = 0;
		float    pos[3] = { 0.f, 0.f, 0.f };  // object-local
	};

	// One corner of a face: which logical vertex, plus attributes that may differ per face (UV seams, hard edges).
	struct EditableCorner {
		uint32_t vertex = 0;                  // EditableVertex::id
		float    uv[2] = { 0.f, 0.f };
		bool     hasNormal = false;           // false = flat face normal is derived
		float    normal[3] = { 0.f, 0.f, 0.f };
	};

	struct EditableFace {
		uint32_t id = 0;
		std::vector<EditableCorner> loop;     // counter-clockwise seen from the outside (cross product = outward normal)
	};

	// An edge is derived from the faces, but its id is persistent: rebuildEdges() keeps the id of every (a,b) pair that
	// survives, gives new pairs fresh ids, and never reuses an id of a removed edge.
	struct EditableEdge {
		uint32_t id = 0;
		uint32_t a = 0, b = 0;                // vertex ids, a < b
	};

	struct EditableMesh {
		std::vector<EditableVertex> vertices;
		std::vector<EditableFace>   faces;
		std::vector<EditableEdge>   edges;
		// Ids are never reused within one mesh's history, so a deleted element's id cannot alias a new one.
		uint32_t nextVertexId = 0, nextFaceId = 0, nextEdgeId = 0;
		// Identity of this geometry state: stamped from a process-wide counter by every generator, loader and edit, and
		// never rolled back by Undo (an undone mesh keeps the revision it had). Not saved. Lets pick caches, GPU buffers
		// and late results tell "same geometry" from "same pointer value reused".
		uint64_t revision = 0;
		// UV seams: edges (as sorted vertex-id pairs) that automatic unwrapping must cut along. Kept only while the pair is still an
		// edge (rebuildEdges drops the rest), sorted and unique. Not part of the geometry: revision is unaffected by marking.
		std::vector<std::pair<uint32_t, uint32_t>> seams;

		const EditableVertex* findVertex(uint32_t id) const;
		const EditableFace*   findFace(uint32_t id) const;
		const EditableEdge*   findEdge(uint32_t a, uint32_t b) const;  // order-insensitive

		uint32_t addVertex(float x, float y, float z);
		uint32_t addFace(const std::vector<EditableCorner>& loop);

		// Re-derives `edges` from `faces` (see EditableEdge).
		void rebuildEdges();

		// Checks finite coordinates, unique ids, valid references, 3..4 corners without a repeated vertex, non-degenerate
		// faces, and a consistent 2-manifold edge structure (<= 2 faces per edge, opposite direction when shared).
		// Returns false and fills *error (first problem) on failure.
		bool validate(std::string* error = nullptr) const;

		// The cheap check for an edit that moved vertices but left vertices, faces, loops and edges alone (the caller starts
		// from a validated mesh): finite positions and no zero-area face. O(faces), no allocation.
		bool validateGeometry(std::string* error = nullptr) const;
	};

	// Newell's method: for a planar loop the outward normal scaled by twice the area (also valid for slightly non-planar
	// quads). Fills n and returns its length (twice the area); 0 if a vertex of the loop is missing.
	float newellNormal(const EditableMesh& mesh, const EditableFace& face, float n[3]);

	// Next value of the process-wide revision counter (thread-safe, starts at 1).
	uint64_t nextEditableRevision();

	// Primitives generated directly from logical topology (never reverse-engineered from triangles).
	// A unit-style box (8 vertices, 6 quads, UVs per face) and an XZ plane facing +Y subdivided into divs x divs quads.
	EditableMesh makeEditableBox(float halfExtent = 0.5f);
	EditableMesh makeEditablePlane(float halfExtent = 1.0f, int divs = 1);

}
