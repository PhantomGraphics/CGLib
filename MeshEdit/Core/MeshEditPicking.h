#pragma once
// Screen-space picking of EditableMesh elements (docs/todo/PLAN_phantomstudio_mesh_editing.md section 3.3).
// Pure CPU: no Vulkan, no scene. The caller supplies the object's model matrix and the camera used to draw it.
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/glm.hpp>

#include "MeshEditOperations.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Phantom::MeshEdit {

	// Camera convention: proj already has the Vulkan Y flip (as PhantomStudio's renderer applies it), so
	// ndc.y * 0.5 + 0.5 scaled by the height is the pixel row (origin top-left).
	struct MeshPickView {
		glm::mat4 model{ 1.f };
		glm::mat4 view{ 1.f };
		glm::mat4 proj{ 1.f };
		float width = 1.f, height = 1.f;
	};

	struct MeshPickOptions {
		ElementKind kind = ElementKind::Vertex;
		// false (default): elements hidden behind the mesh's own surface cannot be picked. true: see-through.
		bool xray = false;
		float vertexRadiusPx = 12.f;  // how close a click must be to a vertex
		float edgeRadiusPx = 8.f;     // ... to an edge
	};

	// Model matrix of an object transform: Translate * Ry * Rx * Rz * Scale (angles in degrees), as the PhantomStudio renderer.
	glm::mat4 objectModelMatrix(const std::array<float, 3>& translation, const std::array<float, 3>& rotationEulerDeg,
	                            const std::array<float, 3>& scale);

	// Projects a local point to pixels. False if it is behind the camera.
	bool projectToPixels(const MeshPickView& v, const float localPos[3], glm::vec2& pixel);

	// The element under (x, y), or false. Vertices/edges: nearest within the radius; faces: the front-most triangle hit.
	bool pickMeshElement(const EditableMesh& mesh, const MeshPickView& view, double x, double y,
	                     const MeshPickOptions& options, uint32_t& id);

	// Elements selected by the rectangle (x0,y0)-(x1,y1) in pixels. Dragging right-to-left (x1 < x0) is a
	// "crossing" selection (an edge/face counts if any part is inside); left-to-right is a "window" selection (all of
	// its vertices must be inside). Vertices are the same either way. Hidden elements are skipped unless xray.
	std::vector<uint32_t> boxSelectMeshElements(const EditableMesh& mesh, const MeshPickView& view,
	                                            double x0, double y0, double x1, double y1, const MeshPickOptions& options);

}
