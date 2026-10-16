#pragma once
// CPU mesh type shared by the primitive generators, the mesh-edit conversions and the renderers
// (moved from PhantomStudio, docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2). No Vulkan dependency.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Phantom::Graphics {

	// Per-vertex data matching the viewport vertex shader (cgstudio.vert):
	//   location 0: vec3 position
	//   location 1: vec3 normal
	//   location 2: vec2 uv
	//   location 3: vec2 normalUV
	//   location 4: vec4 tangent (xyz direction, w handedness; zero xyz derives from UV)
	struct MeshVertex {
		float pos[3];
		float nrm[3];
		float uv[2];
		float normalUV[2] = { 0.f, 0.f };
		float tangent[4] = { 0.f, 0.f, 0.f, 1.f };
	};

	// GPU vertex input layouts are built from these offsets; changing the layout must be a deliberate edit.
	static_assert(offsetof(MeshVertex, pos) == 0, "MeshVertex layout");
	static_assert(offsetof(MeshVertex, nrm) == 12, "MeshVertex layout");
	static_assert(offsetof(MeshVertex, uv) == 24, "MeshVertex layout");
	static_assert(offsetof(MeshVertex, normalUV) == 32, "MeshVertex layout");
	static_assert(offsetof(MeshVertex, tangent) == 40, "MeshVertex layout");
	static_assert(sizeof(MeshVertex) == 56, "MeshVertex layout");

	struct MeshData {
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t>   indices;
	};

	struct MeshValidationResult {
		int totalTriangles = 0;
		int inconsistentTriangles = 0;
		int degenerateTriangles = 0;
		std::vector<int> badIndices;
	};

} // namespace Phantom::Graphics
