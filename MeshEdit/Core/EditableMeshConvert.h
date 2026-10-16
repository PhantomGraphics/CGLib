#pragma once
// Conversions between the render mesh (Graphics::MeshData) and EditableMesh.
// Moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2).
#include "EditableMesh.h"
#include "../../Graphics/Core/MeshData.h"

#include <string>
#include <vector>

namespace Phantom::MeshEdit {

	// Converts render data (triangles, one vertex per corner) into logical topology: vertices at identical positions are
	// welded, every triangle becomes a face whose corners keep the source UV and, where the source shading is not flat,
	// its normal (UV seams and hard edges survive as corner attributes). Zero-area triangles are dropped (counted in
	// *droppedTriangles). Fails with a reason when the result is not a valid 2-manifold (overlapping parts sharing
	// positions, inconsistent winding): the caller keeps the original object.
	bool makeEditableFromMeshData(const Graphics::MeshData& src, EditableMesh& out, std::string* error = nullptr,
	                              int* droppedTriangles = nullptr);

	// Triangulates once (quads are split 0-1-2 / 0-2-3) into render data: one render vertex per corner.
	// Triangles keep the loop order: counter-clockwise from outside (cross product along the declared normal).
	// If triangleFace is given, triangleFace[t] = EditableFace::id of triangle t.
	// `validated` = the caller already validated this mesh (skips the O(n log n) validate()).
	bool editableToMeshData(const EditableMesh& mesh, Graphics::MeshData& out, std::vector<uint32_t>* triangleFace = nullptr,
	                        std::string* error = nullptr, bool validated = false);

} // namespace Phantom::MeshEdit
