#include "LevelSet.h"

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Space/Space/DistanceCalculator.h"
#include "CGLib/Space/Space/IntersectionCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;
using namespace Phantom::Volume;

void LevelSet::setSignedDistance(const std::vector<Math::Triangle3df>& triangles, SparseVolumef& volume)
{
	constexpr float tolerance = 1.0e-6f;

	if (triangles.empty()) {
		return;
	}

	Vector3df minPos(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
	Vector3df maxPos(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

	for (const auto& tri : triangles) {
		const auto verts = tri.getVertices();
		for (const auto& v : verts) {
			minPos.x = std::min(minPos.x, v.x);
			minPos.y = std::min(minPos.y, v.y);
			minPos.z = std::min(minPos.z, v.z);
			maxPos.x = std::max(maxPos.x, v.x);
			maxPos.y = std::max(maxPos.y, v.y);
			maxPos.z = std::max(maxPos.z, v.z);
		}
	}

	const float voxelSize = volume.getVoxelSize();
	const Vector3df pad(voxelSize, voxelSize, voxelSize);
	minPos -= pad;
	maxPos += pad;

	const Coord minIndex = volume.worldToIndex(minPos);
	const Coord maxIndex = volume.worldToIndex(maxPos);

	for (int i = minIndex.x; i <= maxIndex.x; ++i) {
		for (int j = minIndex.y; j <= maxIndex.y; ++j) {
			for (int k = minIndex.z; k <= maxIndex.z; ++k) {
				volume.setValue(Coord(i, j, k), 0.0f);
			}
		}
	}

	std::vector<std::pair<Coord, float>> results;
	results.reserve(static_cast<size_t>(volume.getActiveVoxelCount()));

	volume.forEachActive([&](const Coord& idx, const Vector3df& worldPos, float) {
		// DistanceCalculator::calculate()'s 3rd argument is a snap-to-zero tolerance
		// (see DistanceCalculator.cpp: "dist <= tolerance ? 0 : dist"), NOT a running
		// best-so-far to prune against -- passing minDist there (as this used to)
		// makes the first triangle's call always satisfy dist <= infinity and return
		// 0, permanently corrupting minDist to 0 for every voxel. Use the small
		// epsilon `tolerance` for every call instead.
		float minDist = std::numeric_limits<float>::infinity();
		for (const auto& tri : triangles) {
			minDist = std::min(minDist, DistanceCalculator<float>::calculate(tri, worldPos, tolerance));
		}

		if (minDist == std::numeric_limits<float>::infinity()) {
			minDist = 0.0f;
		}

		// Jittered off the +X axis on purpose: an exactly axis-aligned ray from a
		// point that shares two coordinates with the mesh's symmetry (e.g. the
		// center of an axis-aligned box) grazes precisely along a face's diagonal
		// triangulation edge, an unstable double-count/miss case for the ray/
		// triangle parity test below. Any axis-aligned or otherwise rectilinear
		// mesh (boxes, most CAD-derived STL files) hits this routinely.
		const Ray3df ray(worldPos, Vector3df(1.0f, 1.0e-3f, 1.0e-4f));
		int hits = 0;
		for (const auto& tri : triangles) {
			for (auto t : IntersectionCalculator<float>::calculate(ray, tri, tolerance)) {
				if (t > tolerance) {
					++hits;
				}
			}
		}

		results.emplace_back(idx, (hits % 2 == 1) ? -minDist : minDist);
	});

	for (const auto& item : results) {
		volume.setValue(item.first, item.second);
	}
}

void LevelSet::setSignedDistance(const Box3df& box, SparseVolumef& volume, double thickness)
{
	const float t = static_cast<float>(thickness);
	if (t <= 0.0f) {
		return;
	}

	const auto bmin = box.getMin();
	const auto bmax = box.getMax();
	const Vector3df minPos(bmin[0] - t, bmin[1] - t, bmin[2] - t);
	const Vector3df maxPos(bmax[0] + t, bmax[1] + t, bmax[2] + t);

	const Coord min = volume.worldToIndex(minPos);
	const Coord max = volume.worldToIndex(maxPos);

	for (int i = min.x; i <= max.x; ++i) {
		for (int j = min.y; j <= max.y; ++j) {
			for (int k = min.z; k <= max.z; ++k) {
				const Coord idx(i, j, k);
				const auto p = volume.indexToWorld(idx);

				float dx = 0.0f;
				if (p.x < bmin.x) dx = bmin.x - p.x;
				else if (p.x > bmax.x) dx = p.x - bmax.x;

				float dy = 0.0f;
				if (p.y < bmin.y) dy = bmin.y - p.y;
				else if (p.y > bmax.y) dy = p.y - bmax.y;

				float dz = 0.0f;
				if (p.z < bmin.z) dz = bmin.z - p.z;
				else if (p.z > bmax.z) dz = p.z - bmax.z;

				const float outsideDist = std::sqrt(dx * dx + dy * dy + dz * dz);
				float signedDist = outsideDist;
				if (outsideDist == 0.0f) {
					const float sx = std::min(p.x - bmin.x, bmax.x - p.x);
					const float sy = std::min(p.y - bmin.y, bmax.y - p.y);
					const float sz = std::min(p.z - bmin.z, bmax.z - p.z);
					signedDist = -std::min(sx, std::min(sy, sz));
				}

				if (std::fabs(signedDist) <= t) {
					volume.setValue(idx, signedDist);
				}
			}
		}
	}
}

void LevelSet::setSignedDistance(const Triangle3df& triangle, SparseVolumef& volume)
{

}

void LevelSet::setSignedDistance(const Rectangle3df& rect,SparseVolumef& volume)
{

}
