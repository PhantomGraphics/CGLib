#pragma once
// Primitive mesh generators, mirror and winding / degenerate-triangle validation on the CPU mesh (MeshData).
// Moved from PhantomStudio's PrimitiveBuilder (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 2); behaviour is
// unchanged. Every mesh is counter-clockwise seen from outside (the cross product follows the declared normals).
#include "MeshData.h"

namespace Phantom::Graphics {

	MeshData buildBox(float halfExtent = 0.5f);
	MeshData buildSphere(int stacks = 16, int slices = 32, float radius = 0.5f);
	MeshData buildPlane(float halfExtent = 1.0f, int divs = 1);                      // XZ plane, facing +Y
	MeshData buildCylinder(float radius = 0.5f, float height = 1.0f, int segments = 32);
	MeshData buildCone(float radius = 0.5f, float height = 1.0f, int segments = 32);
	MeshData buildTorus(float majorRadius = 0.5f, float minorRadius = 0.15f, int majorSegments = 32, int minorSegments = 16);

	// Mirrors a mesh across the given axis (0=X, 1=Y, 2=Z), flipping triangle winding so the mirrored mesh isn't back-face culled.
	MeshData mirror(const MeshData& src, int axis);

	// Checks every triangle is counter-clockwise seen from outside: its cross product points along the declared vertex
	// normals. Out-of-range indices, zero-area triangles and vertex normals that sum to zero are reported as inconsistent.
	MeshValidationResult validate(const MeshData& md);

} // namespace Phantom::Graphics
