#pragma once

#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Vector3d.h"

#include <vector>

namespace Phantom {
namespace Volume {

/// @brief Converts a closed triangle mesh into a point cloud sampling its
/// surface, by voxelizing a signed-distance field (Phantom::Volume::LevelSet
/// + SparseVolume) and keeping only the voxels near the zero level set. Pure
/// in-house sparse-volume implementation -- does not use OpenVDB.
class SurfaceVoxelizer
{
public:
	/// @brief Voxelizes the mesh surface and returns the world-space centers
	/// of voxels within `bandWidthInVoxels` voxels of the zero level set.
	/// @param triangles         Mesh triangles in world space.
	/// @param voxelSize         Voxel edge length used to build the signed-
	///                          distance field. Values <= 0 are clamped to 1.f.
	/// @param bandWidthInVoxels Half-width of the surface shell to keep, in
	///                          voxel units (clamped to >= 0). ~1.0 gives a
	///                          thin single-voxel-thick surface shell; larger
	///                          values thicken it.
	/// @return Points on/near the mesh surface. Empty if triangles is empty.
	static std::vector<Math::Vector3df> voxelizeSurface(
		const std::vector<Math::Triangle3df>& triangles,
		float voxelSize,
		float bandWidthInVoxels = 1.0f);
};

} // namespace Volume
} // namespace Phantom
