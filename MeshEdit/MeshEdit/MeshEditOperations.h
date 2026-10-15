#pragma once
// Pure shape operations on an EditableMesh (docs/todo/PLAN_phantomstudio_mesh_editing.md section 3.3/3.4).
// Every operation reads an immutable input and produces a NEW validated mesh (fresh revision); on failure the output is
// untouched and *error says why. No exceptions, no scene types.
#include "EditableMesh.h"
#include <array>
#include <string>
#include <vector>

namespace Phantom::MeshEdit {

	enum class ElementKind { Vertex, Edge, Face };

	const char* elementKindName(ElementKind kind);
	bool        elementKindFromName(const std::string& name, ElementKind& out);  // vertex|edge|face (case-insensitive)

	// True if `id` names an element of `kind` in `mesh`.
	bool hasElement(const EditableMesh& mesh, ElementKind kind, uint32_t id);
	// All element ids of `kind`, ascending.
	std::vector<uint32_t> allElementIds(const EditableMesh& mesh, ElementKind kind);

	// Expands a selection of edges / faces to the (sorted, unique) vertices they touch. False with *error if an id is missing.
	bool elementsToVertices(const EditableMesh& mesh, ElementKind kind, const std::vector<uint32_t>& ids,
	                        std::vector<uint32_t>& vertices, std::string* error = nullptr);

	// Mean position of the given vertices (the initial pivot). False if the set is empty or references a missing vertex.
	bool vertexCentroid(const EditableMesh& mesh, const std::vector<uint32_t>& vertices, std::array<float, 3>& out);

	// Moves the vertices by `delta` (object-local). Topology, ids and attributes are unchanged.
	bool translateVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& delta,
	                       EditableMesh& out, std::string* error = nullptr);
	// Proportional editing: moves the vertices by `delta` and every other vertex within `radius` (straight-line distance to the
	// nearest moved vertex) by `delta` times a falloff weight that is 1 at the selection and 0 at the radius.
	enum class Falloff { Smooth, Linear, Sharp, Constant };
	bool falloffFromName(const std::string& name, Falloff& out);   // smooth|linear|sharp|constant (case-insensitive)
	// *affected (optional) receives how many vertices outside the selection moved.
	bool translateVerticesProportional(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& delta,
	                                   float radius, Falloff falloff, EditableMesh& out, std::string* error = nullptr,
	                                   size_t* affected = nullptr);
	// Rotates the vertices about `pivot` by Euler angles in degrees (applied Y, then X, then Z like object rotation: R = Ry*Rx*Rz).
	bool rotateVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& eulerDeg,
	                    const std::array<float, 3>& pivot, EditableMesh& out, std::string* error = nullptr);
	// Scales the vertices about `pivot` per axis (object-local axes).
	bool scaleVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, const std::array<float, 3>& factor,
	                   const std::array<float, 3>& pivot, EditableMesh& out, std::string* error = nullptr);

	// Extrudes a connected, coplanar region of faces along its normal by `distance` (negative = into the solid).
	// Vertices on the region boundary are duplicated (the region's faces move onto the copies and keep their ids and UVs,
	// so the selection stays valid); vertices strictly inside the region are moved. One quad side wall is created per
	// boundary edge (UV: u = edge length, v = |distance|). Rejects: empty/missing faces, zero or non-finite distance,
	// a region that is not coplanar / not connected / has no boundary (the whole closed mesh) / pinches at a vertex.
	// *sideFaces (optional) receives the number of wall faces created.
	bool extrudeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float distance, EditableMesh& out,
	                  std::string* error = nullptr, size_t* sideFaces = nullptr);

	// Insets a connected, coplanar region of faces: the boundary is shrunk toward the inside by `thickness` (> 0, mitered
	// corners) and one coplanar ring quad is created per boundary edge. `depth` (default 0) additionally moves the inner
	// region along its normal, like an extrude. The region's faces keep their ids and UVs (so the selection stays valid and
	// is the inner region). Same rejections as extrudeFaces, plus a thickness that would invert a face, or a corner too
	// sharp to miter. *ringFaces (optional) receives the number of ring faces created.
	bool insetFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float thickness, float depth, EditableMesh& out,
	                std::string* error = nullptr, size_t* ringFaces = nullptr);

	// Bevels interior edges with one flat segment: each edge is replaced by a quad of the given `width` measured on the two
	// adjacent faces (the new edges slide along the faces' other boundary edges, so a cube edge gets a flat chamfer).
	// Limited form: both endpoints of every edge must have exactly three faces (a cube-like corner), no two selected edges
	// may share a vertex, and the slides along a shared edge must fit inside it. The third face at an endpoint gains a corner
	// and is split into quads/triangles when it would exceed four. The old endpoints are removed; ids are never reused.
	// *bevelFaces (optional) receives the ids of the new bevel quads.
	bool bevelEdges(const EditableMesh& in, const std::vector<uint32_t>& edgeIds, float width, EditableMesh& out,
	                std::string* error = nullptr, std::vector<uint32_t>* bevelFaces = nullptr);

	// Cuts the quad ring through `edgeId`: every edge of the ring (the edge, then the opposite edge of each quad, in both
	// directions until the ring closes or reaches a mesh border) gets a new vertex at `factor` (0..1, measured consistently
	// from one side of the ring, 0.5 = middle), and every quad of the ring is split into two quads. UVs are interpolated.
	// Rejects a ring that meets a non-quad face (it would leave a T-junction) and a position outside (0, 1).
	// *newVertices (optional) receives the new vertex ids, *splitFaces the number of quads split.
	bool loopCut(const EditableMesh& in, uint32_t edgeId, float factor, EditableMesh& out, std::string* error = nullptr,
	             std::vector<uint32_t>* newVertices = nullptr, size_t* splitFaces = nullptr);

	// Knife: cuts the whole mesh along the plane through `point` with `normal` (object-local). Every edge the plane crosses gets
	// a vertex (shared by the faces on both sides, with UV and normal interpolated per corner) and every face it crosses is
	// split along the cut into two pieces; a piece with five corners becomes a quad and a triangle, so faces stay 3..4 corners.
	// Vertices that already lie on the plane are used as they are. *cutVertices receives every vertex on the cut.
	// Fails -- `out` untouched -- when the plane crosses nothing or meets a face in more than two places (a non-convex face).
	bool knifePlane(const EditableMesh& in, const std::array<float, 3>& point, const std::array<float, 3>& normal, EditableMesh& out,
	                std::string* error = nullptr, std::vector<uint32_t>* cutVertices = nullptr, size_t* splitFaces = nullptr);

	// Sets the shading of the selected faces' corners. flat: drops the stored normals so the face normal is derived (and
	// follows later vertex edits). smooth: stores at every corner the area-weighted average normal of the faces around that
	// vertex whose normal is within `angleDeg` of this face's own (180 = fully smooth, smaller keeps creases). Smooth normals
	// are a snapshot: moving vertices does not update them, run smooth again. Geometry, UV and topology are unchanged.
	// *changedCorners (optional) receives how many corners changed.
	bool shadeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, bool smooth, float angleDeg, EditableMesh& out,
	                std::string* error = nullptr, size_t* changedCorners = nullptr);

	// Automatic UV unwrap of the selected faces. The faces are split into charts (connected through shared edges while each
	// face stays within `angleDeg` of the chart's average normal), each chart is projected flat onto its average plane
	// (u runs along the world axis most perpendicular to the normal, so a box face comes out upright), and all charts are
	// packed into the unit square with one common scale (equal texel density) and a gap of `margin` (fraction of the packed
	// size). Only the selected faces' corner UVs change; geometry and topology do not. UV seams appear between charts.
	// *charts (optional) receives the number of charts.
	bool unwrapFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, float angleDeg, float margin, EditableMesh& out,
	                 std::string* error = nullptr, size_t* charts = nullptr);

	// Deletes the selected elements together with the faces that depend on them: a face deletes itself; an edge deletes the
	// faces using it; a vertex deletes the faces around it. Vertices left without any face are removed too (a face delete
	// never leaves isolated vertices behind that it created). The result may be empty. Ids are never reused.
	// *removedFaces / *removedVertices (optional) receive the counts.
	bool deleteElements(const EditableMesh& in, ElementKind kind, const std::vector<uint32_t>& ids, EditableMesh& out,
	                    std::string* error = nullptr, size_t* removedFaces = nullptr, size_t* removedVertices = nullptr);

	enum class MergeTarget { Center, First, Last };  // centroid / position of the lowest id / position of the highest id
	bool mergeTargetFromName(const std::string& name, MergeTarget& out);  // center|first|last (case-insensitive)

	// Collapses `vertices` (>= 2) into one vertex placed per `target`; the survivor is the lowest id. Faces keep their
	// corners' UVs; a face that loses a corner becomes a triangle, a face that collapses to fewer than 3 distinct
	// vertices is removed, and vertices left without faces are dropped. Rejects a merge whose result is not a valid
	// manifold (e.g. it would make an edge shared by three faces).
	bool mergeVertices(const EditableMesh& in, const std::vector<uint32_t>& vertices, MergeTarget target, EditableMesh& out,
	                   std::string* error = nullptr, size_t* removedFaces = nullptr);

	// Merges vertices that lie within `distance` of each other (grouped transitively, each group merged at its centre, like
	// mergeVertices). `vertices` limits the candidates; empty = every vertex. A group that would make the mesh invalid is left
	// alone (counted in *skippedGroups). False -- `out` untouched -- when nothing could be merged.
	bool mergeByDistance(const EditableMesh& in, const std::vector<uint32_t>& vertices, float distance, EditableMesh& out,
	                     std::string* error = nullptr, size_t* mergedVertices = nullptr, size_t* skippedGroups = nullptr);

	// Bridge: joins two boundary edge loops (or two open boundary chains) of the same length with one quad per edge pair.
	// Every selected edge must be a boundary edge (used by one face). The selection must form exactly two chains that are both
	// closed or both open, with the same edge count, and that share no vertex. The new quads run opposite to the faces that own
	// the boundary edges, so the result stays consistently oriented; closed loops are matched at the rotation that minimises
	// the total distance between paired vertices. Each new corner takes its UV from the adjacent face's corner at that vertex.
	// *newFaces (optional) receives the ids of the created quads.
	bool bridgeEdges(const EditableMesh& in, const std::vector<uint32_t>& edgeIds, EditableMesh& out, std::string* error = nullptr,
	                 std::vector<uint32_t>* newFaces = nullptr);

	// Bridge between two faces: removes both (they need the same corner count and must not share a vertex) and bridges the
	// holes they leave. Every edge of both faces must have a neighbouring face, so that it becomes a boundary edge.
	bool bridgeFaces(const EditableMesh& in, const std::vector<uint32_t>& faceIds, EditableMesh& out, std::string* error = nullptr,
	                 std::vector<uint32_t>* newFaces = nullptr);

	// Dissolve: removes edges or vertices by merging the faces around them into one, keeping the surface shape. Faces are limited
	// to 3..4 corners, so only merges whose result fits are done: an edge joins its two faces (two triangles -> a quad); an
	// interior vertex joins the whole fan of faces around it (a fan of triangles of up to four faces, or a mix that still ends
	// with <= 4 outer corners). Edges / vertices that cannot be dissolved (border, too many corners, non-manifold) are skipped
	// and counted in *skipped. The merged face keeps the id of its first face and that face's corner attributes.
	// Fails -- `out` untouched -- when nothing could be dissolved. *mergedFaces receives the ids of the resulting faces.
	bool dissolveElements(const EditableMesh& in, ElementKind kind, const std::vector<uint32_t>& ids, EditableMesh& out,
	                      std::string* error = nullptr, std::vector<uint32_t>* mergedFaces = nullptr, size_t* skipped = nullptr);

	// ---- non-destructive modifiers (docs/todo/PLAN_phantomstudio_mesh_editing.md Phase 7) ----
	// A stack on an EditableMesh object: the editing master stays untouched, the evaluated mesh is what renders and exports.
	struct MeshModifier {
		enum class Type { Mirror, Subdivision };
		Type                type = Type::Mirror;
		bool                enabled = true;
		std::array<bool, 3> axis = { true, false, false };  // Mirror: object-local planes x = 0 / y = 0 / z = 0
		float               mergeDistance = 0.001f;         // Mirror: vertices this close to the plane are welded onto it
		int                 levels = 1;                     // Subdivision: Catmull-Clark steps (1..4)
		bool                smoothShading = true;           // Subdivision: store smooth corner normals on the result
	};

	// Catmull-Clark subdivision: every face becomes quads, interior vertices follow the standard rule, border vertices the
	// crease rule (1/8, 6/8, 1/8), non-manifold vertices stay. Corner UVs are averaged inside each face (seams stay seams).
	// smoothShading stores smooth corner normals on the result (otherwise it renders faceted). Rejects 0 or more than 4
	// levels, an empty mesh, and a result of more than 1,000,000 faces.
	bool subdivideMesh(const EditableMesh& in, int levels, bool smoothShading, EditableMesh& out, std::string* error = nullptr);

	// Mirrors the mesh across each flagged object-local plane in turn (x, then y, then z, so two axes give four copies).
	// Vertices within `mergeDistance` of the plane are snapped onto it and shared with their mirror image, faces lying
	// entirely in the plane (the half-model's cap) are removed so the two halves join, mirrored faces keep their corner
	// attributes (UV, normals mirrored) with the loop reversed so they still face outward. The result must validate and
	// contain faces (a mesh entirely in the plane is rejected). Geometry that straddles the plane is mirrored as is.
	bool mirrorMesh(const EditableMesh& in, const std::array<bool, 3>& axes, float mergeDistance, EditableMesh& out,
	                std::string* error = nullptr);

	// Runs the enabled modifiers in order. `out` is the evaluated mesh; false (out untouched) with the reason on failure.
	bool evaluateModifiers(const EditableMesh& in, const std::vector<MeshModifier>& modifiers, EditableMesh& out,
	                       std::string* error = nullptr);

	// Converts a world-space direction/offset into the object's local space for an object transform
	// (world = T * Ry*Rx*Rz * S, so local = S^-1 * R^T * world). False if any scale component is zero (not invertible).
	bool worldVectorToLocal(const std::array<float, 3>& rotationEulerDeg, const std::array<float, 3>& scale,
	                        const std::array<float, 3>& world, std::array<float, 3>& local, std::string* error = nullptr);

}
