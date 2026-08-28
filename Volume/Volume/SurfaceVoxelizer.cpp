#include "SurfaceVoxelizer.h"

#include "LevelSet.h"
#include "SparseVolumeTree/SparseVolume.h"

#include <algorithm>
#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Volume;

namespace {
// Sentinel signed distance for voxels never touched by LevelSet::setSignedDistance()
// (i.e. outside the mesh's padded AABB), mirroring Physics::MeshBoundaryShape's
// kFarOutside convention so such voxels never satisfy the surface-band test below.
constexpr float kFarOutside = 1.0e6f;
}

std::vector<Vector3df> SurfaceVoxelizer::voxelizeSurface(
	const std::vector<Triangle3df>& triangles,
	float voxelSize,
	float bandWidthInVoxels)
{
	std::vector<Vector3df> points;
	if (triangles.empty()) {
		return points;
	}

	const float vs = voxelSize > 0.0f ? voxelSize : 1.0f;
	const float band = std::max(bandWidthInVoxels, 0.0f) * vs;

	SparseVolumef volume(kFarOutside);
	volume.setVoxelSize(vs);

	LevelSet levelSet;
	levelSet.setSignedDistance(triangles, volume);

	points.reserve(static_cast<size_t>(volume.getActiveVoxelCount()));
	volume.forEachActive([&](const Coord&, const Vector3df& worldPos, float dist) {
		if (std::fabs(dist) <= band) {
			points.push_back(worldPos);
		}
	});

	return points;
}
